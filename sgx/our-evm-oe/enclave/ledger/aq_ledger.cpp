#include "aq_ledger.h"
#include "aqledger_t.h"

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
int AQLedger::execute_tx_simplestate_internal(PersistantTxProxy_T* tx,
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
    auto contract = this->simple_gs.create(etx.to, 0, c);

    TRACE_ENCLAVE("running processor...");

    // Create processor
    std::unordered_map<eevm::Address, eevm::SimpleAccountState> updated_accounts;
    eevm::T_Processor p(this->simple_gs, updated_accounts);

    // Execute code. All executions are associated with a TX. This TX is called by sender, executing the code in contract,
    // with empty input
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(etx, etx.origin, contract, {}, 0, &tr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        tr.print_last_n(std::cout, 10);
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return ERR_EVM_WRONG_RET_CODE;
    }
    // tr.print_last_n(std::cout, 10);

    const std::string response(reinterpret_cast<const char*>(e.output.data()), e.output.size());
    TRACE_ENCLAVE("output as str: %s", response.c_str());

#ifdef TRACING_ENABLED
    const uint256_t output_result = eevm::from_big_endian(e.output.data(), 32);
    TRACE_ENCLAVE("output as 32B hex: %s", eevm::to_lower_hex_string(output_result).c_str());
#endif

    // Sync all (foreign) account states modified by the eEVM processor.
    for (auto& i : updated_accounts) {
        auto& as = i.second;
        TRACE_ENCLAVE("Updating (FOREIGN) account: %s", eevm::address_to_hex_string(as.acc.get_address()).c_str());
        // throw std::logic_error("Not tested yet!");
        as.acc.set_stHash(as.st.hash());
        simple_gs.update(i.first, {as.acc, as.st});
    }

    return RET_SUCCESS;
}

/**
 * Considers the full MP3 global state transferred from the host part here
 * but also works for partial state if no DB entries required to execute tx are missing.
 */
int32_t AQLedger::execute_tx_mp3state_full(eevm::FragmentedGlobalState* gs, PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size,
                                           MerkleTreeArray* txs_hashes, uint8_t* output_result)
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
    // auto lh = eevm::NullLogHandler();
    auto lh = eevm::VectorLogHandler();
    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(tx->origin),
                                 reinterpret_cast<eevm::Address*>(tx->to),
                                 lh, c, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, (uint8_t*)tx->signature);

    TRACE_ENCLAVE("TX with val = %ld from = %s to = %s",
                  etx.value, (eevm::to_hex_string(etx.origin) + std::string((etx.origin == this->operAddr) ? " (OPERATOR)" : "")).c_str(),
                  eevm::to_hex_string(etx.to).c_str());

    // 2) Verify signature of TX
    auto inp4hash = etx.asDataForHash();
    eevm::KeccakHash txHash = eevm::keccak_256(inp4hash);
    if (txs_hashes)               // no return before this point (we save TX hash here)
        txs_hashes->add(txHash);  // save the hash of TX for later aggregation
    if (!this->ecc.verify_sig((const secp256k1_ecdsa_recoverable_signature*)etx.signature, txHash.data(), etx.origin)) {
        TRACE_ENCLAVE("Signature verifiation of a TX failed.");
        return ERROR_SIGNATURE_VERIFY_FAIL;
    }

    // 3a) If no code is present in TX, execute just simple transfer
    if (EMPTY_CODE_OBJ == etx.get_code_ref()) {
        return this->_execute_transfer_tx(gs, etx);
    }
    // TODO: ensure that in production, Operator can create only simple accounts (without code) to avoid "inflation" bugs from constructors
    // assert(etx.origin != this->operAddr);

    // 3b) If some code is present, then deploy contract if does not exist and fetch its account state
    auto senderAccnt = gs->get(etx.origin);
    bool contrDeployed = false;

    eevm::SimpleAccountState* contrState;
    if (!gs->exists(etx.to)) {
        auto expectedAddr = eevm::generate_address(etx.origin, senderAccnt.acc.get_nonce());
        if (etx.to != expectedAddr) {  // check correct address derivation from sender's addr and her nonce
            TRACE_ENCLAVE("Contract address does not match the sender's address and his nonce");
            return ERR_EVM_WRONG_CONTR_ADDR;
        }
        TRACE_ENCLAVE("Creating a new state entry for a contract with addr %s", eevm::to_hex_string(etx.to).c_str());
        auto cs = gs->create(etx.to, etx.value, etx.code);  // insert account state of contract
        contrState = new eevm::SimpleAccountState(std::move(cs));
        contrDeployed = true;
    } else {
        TRACE_ENCLAVE("Contract already exists => fetching its state.");
        auto cs = gs->get(etx.to);
        contrState = new eevm::SimpleAccountState(std::move(cs));
    }

    // 4) update the balance of sender before we execute the code (to avoid inflation bugs)
    auto senderBalBefore = senderAccnt.acc.get_balance();  // TODO: check whether EEVM is not doing it !!!
    auto senderDeducted = (etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value);
    auto& senderStorage = gs->getStorage(etx.origin);
    if (intx::uint256(0u) != senderDeducted) {  // skip update when zero value call is present
        auto senderAccntUpdated = gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(),
                                                                              senderAccnt.acc.get_nonce(), senderStorage),
                                                          senderStorage});
        assert(senderAccntUpdated.acc.get_balance() == senderBalBefore + senderDeducted);
    }

    // 5) Create processor & Run code of TX
    // TODO: it should not modify MP3 GS (make it const) - however, GSOverlay::getRef() in Processor needs to be fixed first
    TRACE_ENCLAVE("running processor.. (contr addr = %s)", eevm::address_to_hex_string(contrState->acc.get_address()).c_str());
    std::unordered_map<eevm::Address, eevm::SimpleAccountState> updated_accounts;  // processor will fill this list if needed, and then we need to sync to MP3 gs
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(*gs, updated_accounts);
    eevm::Trace tr;

    // Use empty input for contract deployment
    eevm::ExecResult e = p.run(etx, etx.origin, *contrState, (contrDeployed) ? EMPTY_CODE_OBJ : etx.code, etx.value, &tr);

    // 6) Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        tr.print_last_n(std::cout, 10);
        if (lh.logs.size())  // print LOG events emmitted in EVM
            TRACE_ENCLAVE("Emmited log events in EVM:\n %s", eevm::txlog_to_json_str(etx.log_handler).c_str());
        delete contrState;
        return ERR_EVM_WRONG_RET_CODE;
    }
    if (NULL != output_result)
        memcpy(output_result, e.output.data(), 32);  // store output to the host memory

#ifdef TRACING_ENABLED
    if (lh.logs.size())  // print LOG events emmitted in EVM
        TRACE_ENCLAVE("Emmited log events in EVM:\n %s", eevm::txlog_to_json_str(etx.log_handler).c_str());
    const std::string response(reinterpret_cast<const char*>(e.output.data()), e.output.size());
    const uint256_t output_result_bi = eevm::from_big_endian(e.output.data(), 32);
    TRACE_ENCLAVE("output as str: %s", response.c_str());
    TRACE_ENCLAVE("output as 32B hex: %s", eevm::to_hex_string(output_result_bi).c_str());
#endif

    // 7) if deployment of contract was made, then update the code of the contract to contain the effect of execution
    if (contrDeployed) {
        contrState->acc.set_code(std::move(e.output));
    }

    // 8) update the storage hash (and nonce) of the account of contract called. Note that nonce of MP3 was already modified by processor.
    contrState->acc.set_stHash(contrState->st.hash());
    gs->update(etx.to, {eevm::SimpleAccount(etx.to, etx.value, contrState->acc.get_code_ref(), contrState->acc.get_nonce(), contrState->st), contrState->st});

    // 9) (If any) sync all foreign account states modified by the eEVM processor (i.e., internal contract calls - by internal transactions)
    // PARALLEL:  We should lock(mutex) all corresponding MP3 before calling executeTX() - use some access list?;
    // So far all these were modified only in a cache of eEVM Processor.
    for (auto& i : updated_accounts) {
        auto& as = i.second;
        TRACE_ENCLAVE("Updating (FOREIGN) account: %s", eevm::address_to_hex_string(as.acc.get_address()).c_str());
        // throw std::logic_error("Not tested yet!");
        as.acc.set_stHash(as.st.hash());
        gs->update(i.first, {eevm::SimpleAccount(
                                 as.acc.get_address(),
                                 as.acc.get_balance(),
                                 as.acc.get_code_ref(),
                                 as.acc.get_nonce(),
                                 as.st),
                             as.st});
    }

    // 10) Update the nonce of the sender
    auto newNonce = senderAccnt.acc.get_nonce() + 1;
    gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(), newNonce, senderStorage), senderStorage});  // update MP3 for sender

    delete contrState;
    return RET_SUCCESS;
}

int AQLedger::_execute_transfer_tx(eevm::NormalGlobalState* gs, eevm::Transaction& etx)
{
    TRACE_ENCLAVE("Simple transfer");

    // allow account creation for operator (if it does not exist)
    auto accnState = (etx.origin == this->operAddr && !gs->exists(etx.origin)) ? gs->create(etx.origin, 0u, EMPTY_CODE_OBJ) : gs->get(etx.origin);

    // 1) Increment the nonce and the balance of the sender
    if (EMPTY_CODE_OBJ == accnState.acc.get_code_ref()) {  // according to ETH Yellow paper, increment only if code is empty
        TRACE_ENCLAVE("--incrementing nonce");
        accnState.acc.set_nonce(accnState.acc.get_nonce() + 1);
    }
    // 2) check ballance
    auto& code = accnState.acc.get_code_ref();
    if (etx.origin != this->operAddr && (etx.value > accnState.acc.get_balance())) {
        TRACE_ENCLAVE("The account %s does not have enough balance.", eevm::address_to_hex_string(etx.origin).c_str());
        return ERR_EVM_LOW_BALANCE;
    }
    // if TX was made by the operator then do not check his balance and just add the value to the sender
    auto senderBalBefore = accnState.acc.get_balance();
    auto senderDeducted = (etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value);
    auto& senderStorage = gs->getStorage(etx.origin);
    auto accSndUpdated = gs->update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, code, accnState.acc.get_nonce(), senderStorage), senderStorage});  // update MP3 for sender
    assert(accSndUpdated.acc.get_balance() == senderBalBefore - senderDeducted);

    // 3) add value to the target account
    auto recvAcState = (!gs->exists(etx.to)) ? gs->create(etx.to, 0u, EMPTY_CODE_OBJ) : gs->get(etx.to);  // create target account if it does not exist
    auto& storage = gs->getStorage(etx.to);                                                               // just copy the old storage
    auto recvBalanceBefore = recvAcState.acc.get_balance();
    code = recvAcState.acc.get_code_ref();
    auto recvAcStateAfter = gs->update(etx.to, {eevm::SimpleAccount(etx.to, recvBalanceBefore + intx::uint256(etx.value), code, recvAcState.acc.get_nonce(), storage), storage});
    assert(recvAcStateAfter.acc.get_balance() == recvBalanceBefore + intx::uint256(etx.value));

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

int AQLedger::execute_hello_world()
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
    eevm::SimpleAccountState contract = gs.create(to, 0, code);

    // Create transaction
    // eevm::NullLogHandler ignore;
    auto lh = eevm::VectorLogHandler();
    eevm::Transaction tx(sender, to, lh);

    // Create processor
    std::unordered_map<eevm::Address, eevm::SimpleAccountState> updated_accounts;  // we will ignore it after
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(gs, updated_accounts);

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
    // tr.print_last_n(std::cout, 10);
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

int AQLedger::execute_sum_a_b(int a, int b)
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
    eevm::SimpleAccountState contract = gs.create(to, 0, code);

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
    std::unordered_map<eevm::Address, eevm::SimpleAccountState> updated_accounts;  // we will ignore it after
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(gs, updated_accounts);

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
