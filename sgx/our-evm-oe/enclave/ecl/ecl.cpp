#include "ecl.h"
#include "ecledger_t.h"

#include "common.h"
#include "data_types.h"
#include "signing.h"

// eEVM
#include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/processor.h"

/**
 * This is only tmp method since it fully maintains global state within the enclave.
 */
int ECLedger::execute_tx_simplestate_internal(PersistantTxProxy_T* tx,
                                              const uint8_t* code,
                                              size_t code_size)
{
    TRACE_ENCLAVE("execute_tx_simplestate_internal invoked");

    // create eevm::Tx object from the proxy and code
    auto c = std::vector<uint8_t>(std::move(code), code + code_size);
    auto lh = eevm::NullLogHandler();

    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(tx->origin),
                                 reinterpret_cast<eevm::Address*>(tx->to),
                                 lh, c, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, (uint8_t*)tx->signature);

    // Deploy contract to simple global state (internal to enclave)
    const eevm::SimpleAccountState contract = this->simple_gs.create(etx.to, 0, c);

    TRACE_ENCLAVE("running processor...");

    // Create processor
    eevm::T_Processor p(this->simple_gs);

    // Execute code. All executions are associated with a TX. This TX is called by sender, executing the code in contract,
    // with empty input
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(etx, etx.origin, contract, {}, 0, &tr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        tr.print_last_n(std::cout, 100);
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return ERR_EVM_WRONG_RET_CODE;
    }
    tr.print_last_n(std::cout, 100);

    const std::string response(reinterpret_cast<const char*>(e.output.data()), e.output.size());
    TRACE_ENCLAVE("output as str: %s", response.c_str());

    const uint256_t result_bi = eevm::from_big_endian(e.output.data(), 32);
    TRACE_ENCLAVE("output as 32B hex: %s", eevm::to_lower_hex_string(result_bi).c_str());

    return RET_SUCCESS;
}

/**
 * Considers the full MP3 global state transferred from the host part here.
 */
int ECLedger::execute_tx_mp3state_full(eevm::NormalGlobalState* gs, PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size)
{
    TRACE_ENCLAVE("execute_tx_mp3state_full invoked");

    // 0) Check whether sender exists (Operator is an exception)
    auto sender = reinterpret_cast<eevm::Address*>(tx->origin);
    if (*sender != this->operAddr && !gs->exists(*sender)) {
        TRACE_ENCLAVE("Sender of TX does not exist.");
        return ERR_EVM_SENDER_DOES_NOT_EXIST;
    }

    // 1) Create eevm::Tx object from the proxy and code
    auto c = std::vector<uint8_t>(std::move(code), code + code_size);
    auto lh = eevm::NullLogHandler();
    // auto lh = eevm::VectorLogHandler();
    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(tx->origin),
                                 reinterpret_cast<eevm::Address*>(tx->to),
                                 lh, c, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, (uint8_t*)tx->signature);

    TRACE_ENCLAVE("TX with val = %ld from = %s and to = %s",
                  etx.value, (eevm::to_hex_string(etx.origin) + std::string((etx.origin == this->operAddr) ? " (OPERATOR)" : "")).c_str(),
                  eevm::to_hex_string(etx.to).c_str());

    // 2a) If no code is present in TX, execute just simple transfer
    if (EMPTY_CODE_OBJ == etx.get_code_ref()) {
        return this->_execute_transfer_tx(gs, etx);
    }

    // 2b) If code is present, then (deploy contract if does not exist and) ececute TX with the code
    auto senderAccnt = gs->get(etx.origin, false);
    bool contractCreation = false;
    auto newNonce = senderAccnt.acc.get_nonce() + 1;
    eevm::SimpleAccountState contrState;
    if (!gs->exists(etx.to)) {
        auto expectedAddr = eevm::generate_address(etx.origin, senderAccnt.acc.get_nonce());
        if (etx.to != expectedAddr) {  // check correct address derivation from sender's addr and nonce
            TRACE_ENCLAVE("Contract address does not match the sender's address and his nonce");
            return ERR_EVM_WRONG_CONTR_ADDR;
        }
        TRACE_ENCLAVE("Creating a new state for a contract %s", eevm::to_hex_string(etx.to).c_str());
        auto cs = gs->create(etx.to, etx.value, etx.code);  // insert account state of contract
        contrState = std::move(cs);
        contractCreation = true;
    } else {
        TRACE_ENCLAVE("Contract already exists => fetching its state.");
        auto cs = gs->get(etx.to, false);
        contrState = std::move(cs);
    }

    // 3) update the balance before we execute the code
    auto senderBalBefore = senderAccnt.acc.get_balance();  // TODO: check whether EEVM is not doing it !!!
    auto senderDeducted = (etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value);
    auto& senderStorage = gs->getStorages().at(etx.origin);
    if (intx::uint256(0u) == senderDeducted) {                                                                                                                    // skip update when zero value call is present
        senderAccnt = gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(), newNonce, senderStorage), senderStorage});  // update MP3 for sender
    } else {
        assert(senderAccnt.acc.get_balance() == senderBalBefore + senderDeducted);
    }

    // 4) Create processor & Run code of TX
    TRACE_ENCLAVE("running processor...");
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(*gs);
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(etx, etx.origin, contrState, {}, etx.value, &tr);

    // 5) Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        tr.print_last_n(std::cout, 10);
        // TRACE_ENCLAVE("Log handler of TX:\n %s", eevm::txlog_to_json_str(etx.log_handler).c_str());
        return ERR_EVM_WRONG_RET_CODE;
    }
    const std::string response(reinterpret_cast<const char*>(e.output.data()), e.output.size());
    TRACE_ENCLAVE("output as str: %s", response.c_str());
    const uint256_t result_bi = eevm::from_big_endian(e.output.data(), 32);
    TRACE_ENCLAVE("output as 32B hex: %s", eevm::to_lower_hex_string(result_bi).c_str());

    // 6) Update the nonce of the sender either it is: a) a simple account, or b) a contract that just deployed another contract
    senderAccnt = gs->get(etx.origin, false);
    if (EMPTY_CODE_OBJ == senderAccnt.acc.get_code_ref() || contractCreation) {
        auto newNonce = senderAccnt.acc.get_nonce() + 1;
        senderAccnt = gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(), newNonce, senderStorage), senderStorage});  // update MP3 for sender
    }

    // TODO: if some contract is created by TX call of existing contract, then EVM must increment nonce of sending contract (check it) !!!

    return RET_SUCCESS;
}

int ECLedger::_execute_transfer_tx(eevm::NormalGlobalState* gs, eevm::Transaction& etx)
{
    TRACE_ENCLAVE("Simple tranfer");

    // 1) Verify signature of TX
    auto inp4hash = etx.asDataForHash();
    eevm::KeccakHash txHash = eevm::keccak_256(inp4hash);
    if (!this->ecc.verify_sig((const secp256k1_ecdsa_recoverable_signature*)etx.signature, txHash.data(), etx.origin)) {
        TRACE_ENCLAVE("Signature verifiation of a TX failed.");
        return ERROR_SIGNATURE_VERIFY_FAIL;
    }

    // 2) Increment the nonce and the balance of the sender
    auto accnState = gs->get(etx.origin, (etx.origin == this->operAddr) ? true : false);  // allow account creation for operator
    if (EMPTY_CODE_OBJ == accnState.acc.get_code_ref()) {                                 // according to ETH Yellow paper, increment only if code is empty
        TRACE_ENCLAVE("--incrementing nonce");
        accnState.acc.set_nonce(accnState.acc.get_nonce() + 1);
    }
    auto& code = accnState.acc.get_code_ref();
    if (etx.origin != this->operAddr && (etx.value > accnState.acc.get_balance())) {
        TRACE_ENCLAVE("The account %s does not have enough balance.", eevm::address_to_hex_string(etx.origin).c_str());
        return ERR_EVM_LOW_BALANCE;
    }
    // if TX was made by the operator then do not check his balance and just add the value to the sender
    auto senderBalBefore = accnState.acc.get_balance();
    auto senderDeducted = (etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value);
    auto& senderStorage = gs->getStorages().at(etx.origin);
    accnState = gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, code, accnState.acc.get_nonce(), senderStorage), senderStorage});  // update MP3 for sender
    assert(accnState.acc.get_balance() == senderBalBefore + senderDeducted);

    // 3) add value to the target account
    auto recvAcState = gs->get(etx.to, true);      // alow creation of a target account here
    auto& storage = gs->getStorages().at(etx.to);  // just copy the old storage
    auto recvBalanceBefore = recvAcState.acc.get_balance();
    code = recvAcState.acc.get_code_ref();
    recvAcState = gs->update(etx.to, {eevm::SimpleAccount(etx.to, recvBalanceBefore + intx::uint256(etx.value), code, recvAcState.acc.get_nonce(), storage), storage});
    assert(recvAcState.acc.get_balance() == recvBalanceBefore + intx::uint256(etx.value));

    return RET_SUCCESS;
}

//////////////////////////////// Hardcoded 'printing' of hello world smart contract //////////////////////////////////////

std::vector<uint8_t> create_bytecode(const std::string& s)
{
    std::vector<uint8_t> code;
    constexpr uint8_t mdest = 0x0;
    const uint8_t rsize = s.size() + 1;

    // Store each byte in evm memory
    uint8_t mcurrent = mdest;
    for (const char& c : s) {
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(c);
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(mcurrent++);  // IH: this represents the address in the memory, starting from 0;
        code.push_back(eevm::Opcode::MSTORE8);
    }

    // Return
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(rsize);  // the size to read from memory (i.e., length of string)
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(mdest);  // starting from memory 0x00
    code.push_back(eevm::Opcode::RETURN);

    return code;
}

int ECLedger::execute_hello_world()
{
    // Create random addresses for sender and contract
    std::vector<uint8_t> raw_address(20);  // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });

    const eevm::Address sender = eevm::from_big_endian(raw_address.data(), raw_address.size());

    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Create global state
    eevm::SimpleGlobalState gs;

    // Create code
    std::string hello_world("[ENCLAVE]: Executed smart contract that prints this msg!");
    const eevm::Code code = create_bytecode(hello_world);

    // Deploy contract to global state
    const eevm::SimpleAccountState contract = gs.create(to, 0, code);

    // Create transaction
    // eevm::NullLogHandler ignore;
    auto lh = eevm::VectorLogHandler();
    eevm::Transaction tx(sender, to, lh);

    // Create processor
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(gs);

    // Execute code. All executions are associated with a transaction. This
    // transaction is called by sender, executing the code in contract, with empty
    // input (and no trace collection)
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(tx, sender, contract, {}, 0, &tr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        tr.print_last_n(std::cout, 10);
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return 2;
    }
    tr.print_last_n(std::cout, 10);
    TRACE_ENCLAVE("Log handler of TX:\n %s", eevm::txlog_to_json_str(tx.log_handler).c_str());

    // Create string from response data, and print it
    const std::string response(reinterpret_cast<const char*>(e.output.data()));
    if (response != hello_world) {
        throw std::runtime_error(fmt::format(
            "[ENCLAVE:]  Incorrect result.\n Expected: {}\n Actual: {}", hello_world, response));
        return 3;
    }

    std::cout << response << std::endl;

    return 0;
}

//////////////////////////// Hardcoded summing contract execution  ////////////////////////////////

void push_uint256(std::vector<uint8_t>& code, const uint256_t& n)
{
    code.push_back(eevm::Opcode::PUSH32);  // Append opcode

    // Resize code array
    const size_t pre_size = code.size();
    code.resize(pre_size + 32);

    // Serialize number into code array
    eevm::to_big_endian(n, code.data() + pre_size);  // IH: store n to real memory pointed by code.data() + pre_size
}

std::vector<uint8_t> create_a_plus_b_bytecode(const uint256_t& a, const uint256_t& b)
{
    std::vector<uint8_t> code;
    constexpr uint8_t mdest = 0x0;   //< Memory start address for result
    constexpr uint8_t rsize = 0x20;  //< Size of result

    // Push args and ADD
    push_uint256(code, a);
    push_uint256(code, b);
    code.push_back(eevm::Opcode::ADD);

    // Store result
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(mdest);
    code.push_back(eevm::Opcode::MSTORE);

    // Return
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(rsize);
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(mdest);
    code.push_back(eevm::Opcode::RETURN);

    return code;
}

int ECLedger::execute_sum_a_b(int a, int b)
{
    // Validate args, read verbose option
    bool verbose = true;
    srand(time(nullptr));

    // Parse args
    const uint256_t arg_a = eevm::to_uint256(std::to_string(a));
    const uint256_t arg_b = eevm::to_uint256(std::to_string(b));

    if (verbose)
        std::cout << fmt::format("[ENCLAVE:] Calculating {} + {}", eevm::to_lower_hex_string(arg_a), eevm::to_lower_hex_string(arg_b)) << std::endl;

    std::cout << "[ENCLAVE]: Starting summing smart contract..." << std::endl;

    // Invent a random address to use as sender
    std::vector<uint8_t> raw_address(20);
    std::generate(raw_address.begin(), raw_address.end(), []() { return rand(); });
    const eevm::Address sender = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Generate a target address for the summing contract (this COULD be random,
    // but here we use the scheme for Contract Creation specified in the Yellow Paper)
    const eevm::Address to = eevm::generate_address(sender, 0);

    // Create summing bytecode
    const eevm::Code code = create_a_plus_b_bytecode(arg_a, arg_b);

    // Construct global state
    eevm::SimpleGlobalState gs;

    // Populate the global state with the constructed contract
    const eevm::SimpleAccountState contract = gs.create(to, 0, code);

    if (verbose) {
        std::cout << fmt::format(
                         "[ENCLAVE:] Target address {} contains the following bytecode:\n {}",
                         eevm::to_checksum_address(to),
                         eevm::to_hex_string(contract.acc.get_code()))
                  << std::endl;
    }

    // Construct a transaction object
    eevm::NullLogHandler ignore;  //< Ignore any logs produced by this transaction
    std::cout << "[ENCLAVE]: Creating Transaction" << std::endl;
    eevm::Transaction tx(sender, to, ignore);

    std::cout << "[ENCLAVE]: Creating eEVM Processor" << std::endl;

    // Construct processor
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(gs);

    if (verbose)
        std::cout << fmt::format("[ENCLAVE:] Executing a transaction from {} to {}", eevm::to_checksum_address(sender), eevm::to_checksum_address(to))
                  << std::endl;

    // Run transaction
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(
        tx,
        sender,
        contract,
        {},  //< No input - the arguments are hard-coded in the contract
        0,   //< No gas value
        &tr  //< Record execution trace
    );

    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return 2;
    }

    if (verbose)
        std::cout << fmt::format("[ENCLAVE:] Execution completed, and returned a result of {} bytes", e.output.size()) << std::endl;

    const uint256_t result = eevm::from_big_endian(e.output.data(), e.output.size());

    std::cout << "[ENCLAVE:]" << fmt::format("{} + {} = {}", eevm::to_lower_hex_string(arg_a), eevm::to_lower_hex_string(arg_b), eevm::to_lower_hex_string(result)) << std::endl;

    return 0;
}


////////////////////////////// Static Methods //////////////////////////////


/**
 * Constructs  NormalGlobalState object from parameters passed to ecall.
 */
// static int construct_full_state(eevm::NormalGlobalState& out_gs, const uint8_t* db_keys, size_t db_keys_size,
//                                 const uint8_t* db_values, const size_t* values_sizes, size_t db_values_sizes_size,
//                                 uint8_t* const storages, const size_t* storages_sizes, size_t storages_sizes_size)
// {
// }
