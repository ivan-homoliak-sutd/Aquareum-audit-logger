#include "ecl-host.h"
// #include "ecledger_u.h"

#include "common.h"
#include "data_types.h"
#include "utils.h"

// eEVM
// #include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/processor.h"
#include "eEVM/transaction.h"
#include "eEVM/util.h"
#include <fmt/format_header_only.h>
// #include "eEVM/simple/simpleglobalstate.h"

#include "aleth-mp3/Common.h"

/////////////////// bytecode generation ///////////////////

std::vector<uint8_t> create_printStr_bytecode(const std::string& s)
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

std::vector<uint8_t> create_inc_counter_bytecode()
{
    std::vector<uint8_t> code;
    // constexpr uint8_t mdest = 0x0;   //< Memory start address for result
    // constexpr uint8_t rsize = 0x20;  //< Size of result

    // TODO

    return code;
}

void append_arg(std::vector<uint8_t>& code, const uint256_t& arg)
{
    // ABI encode a function call with a uint256_t (or Address) argument.
    // ABI-encoding for more complicated types is more complicated.
    const auto pre_size = code.size();
    code.resize(pre_size + 32u);
    eevm::to_big_endian(arg, code.data() + pre_size);
}

/////////////////// Transaction creation ///////////////////

eevm::PersistantTransaction* ECLedger::createHelloWorldTX(OperAccount& sender, size_t nonce)
{
    // Deterministically compute address for contract from nonce and address of sender
    std::vector<uint8_t> raw_address(20);
    const eevm::Address contract_address = eevm::generate_address(sender.addr, nonce);

    // Create code
    std::string hello_world("[ENCLAVE]: Executed smart contract that prints this msg!");
    const eevm::Code code = create_printStr_bytecode(hello_world);

    auto tx = new eevm::PersistantTransaction(sender.addr, contract_address, nonce, 0, code);
    this->m_ecc->sign_data(tx->asDataForHash(), sender.SK, tx->signature);
    return tx;
}

eevm::PersistantTransaction* ECLedger::createSumTx(int a, int b,
                                                   secp256k1_pubkey& PK_sender,
                                                   uint8_t* SK_sender,
                                                   size_t nonce)
{
    // Parse args
    const uint256_t arg_a = eevm::to_uint256(std::to_string(a));
    const uint256_t arg_b = eevm::to_uint256(std::to_string(b));

    // Create addresses for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Deterministically compute address for contract from nonce and address of sender
    std::vector<uint8_t> raw_address(20);
    const eevm::Address to = eevm::generate_address(operAddr, nonce);

    // Create summing bytecode
    const eevm::Code code = create_a_plus_b_bytecode(arg_a, arg_b);

    // Construct a transaction object
    auto tx = new eevm::PersistantTransaction(sender, to, nonce, 0, code);
    this->m_ecc->sign_data(tx->asDataForHash(), SK_sender, tx->signature);

    return tx;
}

// NOTE: it supports only 32B uint arguments of a constructor
eevm::PersistantTransaction* ECLedger::createDeploymentTX(const ContrDefinition& contract_definition,
                                                          OperAccount& sender,
                                                          size_t nonce,
                                                          uint64_t value)
{
    // Deterministically compute address for contract from nonce and address of sender
    std::vector<uint8_t> raw_address(20);
    const eevm::Address contract_address = eevm::generate_address(sender.addr, nonce);

    // Get the binary constructor of the contract and its parameters
    auto contract_ctor_code = contract_definition.bin;  // copy here

    for (auto&& par : contract_definition.ctor_params) {
        if ("uint256" == par.type) {
            append_arg(contract_ctor_code, par.value);
        } else if ("address" == par.type) {
            append_arg(contract_ctor_code, par.value);
        } else
            throw std::logic_error(fmt::format("Unsupported type of parameter in passed: '{}'", par.type));
    }

    auto tx = new eevm::PersistantTransaction(sender.addr, contract_address, nonce, value, contract_ctor_code);
    this->m_ecc->sign_data(tx->asDataForHash(), sender.SK, tx->signature);

    return tx;
}

eevm::PersistantTransaction* ECLedger::createCallFunctionTX(const OperAccount& sender,
                                                            const eevm::Address to,
                                                            const std::vector<u256> params,
                                                            const Bytes& function_hex_ptr,
                                                            const size_t nonce,
                                                            const uint64_t value)
{
    auto function_call = function_hex_ptr;  // copy vector

    // append all passed arguments to function call pointer
    for (auto& p : params) {
        append_arg(function_call, p);
    }

    auto tx = new eevm::PersistantTransaction(sender.addr, to, nonce, value, function_call);
    this->m_ecc->sign_data(tx->asDataForHash(), sender.SK, tx->signature);

    return tx;
}


eevm::PersistantTransaction* ECLedger::createIncCounterTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t* SK_sender)
{
    // Construct address for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Create random addresses for contract (TODO: later derive it from the account of sender)
    std::vector<uint8_t> raw_address(20);  // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Create summing bytecode
    const eevm::Code code = create_inc_counter_bytecode();

    // Construct a transaction object
    uint64_t nonce = 0;  // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, to, nonce, 0, code);
    this->m_ecc->sign_data(tx->asDataForHash(), SK_sender, tx->signature);

    return tx;
}

eevm::PersistantTransaction* ECLedger::createNewAccountTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t* SK_sender,
                                                          const Address& newAddr,
                                                          unsigned initBalance,
                                                          size_t nonce)
{
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);
    auto tx = new eevm::PersistantTransaction(sender, newAddr, nonce, initBalance, EMPTY_CODE);
    this->m_ecc->sign_data(tx->asDataForHash(), SK_sender, tx->signature);
    return tx;
}

int ECLedger::executeTX(eevm::PersistantTransaction* tx)
{
    debug_print("Executing Tx in HOST...");

    // 0) Check whether sender exists (Operator is an exception)
    if (tx->origin != this->operAddr && !m_gs.exists(tx->origin)) {
        TRACE_HOST("Sender of TX does not exist.");
        return ERR_EVM_SENDER_DOES_NOT_EXIST;
    }

    // 1) Create eevm::Tx object from the persistant TX and code
    // auto lh = eevm::NullLogHandler();
    auto lh = eevm::VectorLogHandler();
    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(&tx->origin),
                                 reinterpret_cast<eevm::Address*>(&tx->to),
                                 lh, tx->code, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, (uint8_t*)tx->signature);

    TRACE_HOST("TX with val = %ld from = %s to = %s",
               etx.value, (eevm::to_hex_string(etx.origin) + std::string((etx.origin == this->operAddr) ? " (OPERATOR)" : "")).c_str(),
               eevm::to_hex_string(etx.to).c_str());

    // 2a) If no code is present in TX, execute just simple transfer
    if (EMPTY_CODE_OBJ == etx.get_code_ref()) {
        return this->_execute_transfer_tx(etx);
    }
    debug_print("Executing CONTRACT in HOST...");

    // 2b) If code is present, then (deploy contract if does not exist and) ececute TX with the code
    auto senderAccnt = m_gs.get(etx.origin);
    bool contrDeployed = false;
    eevm::SimpleAccountState* contrState;
    if (!m_gs.exists(etx.to)) {
        auto expectedAddr = eevm::generate_address(etx.origin, senderAccnt.acc.get_nonce());
        if (etx.to != expectedAddr) {  // check correct address derivation from sender's addr and nonce
            TRACE_HOST("Contract address does not match the sender's address and his nonce");
            return ERR_EVM_WRONG_CONTR_ADDR;
        }
        TRACE_HOST("Creating a new state for a contract %s", eevm::to_hex_string(etx.to).c_str());
        auto cs = m_gs.create(etx.to, etx.value, etx.code);  // insert account state of contract
        contrState = new eevm::SimpleAccountState(cs);
        contrDeployed = true;
    } else {
        TRACE_HOST("Contract already exists => fetching its state.");
        auto cs = m_gs.get(etx.to);
        contrState = new eevm::SimpleAccountState(cs);
    }

    // 3) update the balance before we execute the code
    auto senderBalBefore = senderAccnt.acc.get_balance();  // TODO: check whether EEVM is not doing it !!!
    auto senderDeducted = (etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value);
    auto& senderStorage = m_gs.getStorages().at(etx.origin);
    if (intx::uint256(0u) != senderDeducted) {  // skip update when zero value call is present
        auto senderAccntAfter = m_gs.update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(), senderAccnt.acc.get_nonce(), senderStorage), senderStorage});
        assert(senderAccntAfter.acc.get_balance() == senderBalBefore + senderDeducted);
    }

    // 4) Create processor & Run code of TX
    TRACE_HOST("running processor...");
    std::unordered_map<eevm::Address, eevm::SimpleAccountState> updated_accounts;  // processor will fill this list if needed, and then we need to sync to MP3 gs
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(m_gs, updated_accounts);
    eevm::Trace tr;
    eevm::ExecResult e = p.run(etx, etx.origin, *contrState, (contrDeployed) ? EMPTY_CODE_OBJ : etx.code, etx.value, &tr);

    // 5)  Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("Unexpected return code: {}", (size_t)e.er) << std::endl;
        tr.print_last_n(std::cout, 10);
        if (lh.logs.size())  // print LOG events emmitted in EVM
            TRACE_ENCLAVE("Emmited log events in EVM:\n %s", eevm::txlog_to_json_str(etx.log_handler).c_str());
        delete contrState;
        return ERR_EVM_WRONG_RET_CODE;
    }
    if (lh.logs.size())  // print LOG events emmitted in EVM
        TRACE_ENCLAVE("Emmited log events in EVM:\n %s", eevm::txlog_to_json_str(etx.log_handler).c_str());

    const std::string response(reinterpret_cast<const char*>(e.output.data()), e.output.size());
    TRACE_HOST("output as str: %s", response.c_str());
    const uint256_t result_bi = eevm::from_big_endian(e.output.data(), 32);
    TRACE_HOST("output as 32B hex: %s", eevm::to_lower_hex_string(result_bi).c_str());

    // 6) if deployment of contract was made, then update the code of the contract to contain the effect of execution
    if (contrDeployed) {
        contrState->acc.set_code(std::move(e.output));
    }

    // 7) update the storage hash of the account of contract called
    contrState->acc.set_stHash(contrState->st.hash());
    m_gs.update(etx.to, {eevm::SimpleAccount(etx.to, etx.value, contrState->acc.get_code_ref(), contrState->acc.get_nonce(), contrState->st), contrState->st});

    // 8) Sync all (foreign) account states modified by the eEVM processor.
    for (auto& i : updated_accounts) {
        auto& as = i.second;
        TRACE_ENCLAVE("Updating (FOREIGN) account: %s", eevm::address_to_hex_string(as.acc.get_address()).c_str());
        // throw std::logic_error("Not tested yet!");
        as.acc.set_stHash(as.st.hash());
        m_gs.update(i.first, {eevm::SimpleAccount(
                                  as.acc.get_address(),
                                  as.acc.get_balance(),
                                  as.acc.get_code_ref(),
                                  as.acc.get_nonce(),
                                  as.st),
                              as.st});
    }

    // 9) Update the nonce of the sender
    auto newNonce = senderAccnt.acc.get_nonce() + 1;
    m_gs.update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, senderAccnt.acc.get_code_ref(), newNonce, senderStorage), senderStorage});  // update MP3 for sender

    // TODO: if some contract is created by TX call of existing contract, then EVM must increment nonce of sending contract (check it) !!!

    delete contrState;
    return RET_SUCCESS;
}

int ECLedger::_execute_transfer_tx(eevm::Transaction& etx)
{
    TRACE_HOST("Simple transfer");

    // 1) Verify signature of TX
    auto inp4hash = etx.asDataForHash();
    eevm::KeccakHash txHash = eevm::keccak_256(inp4hash);
    bool correct = m_ecc->verify_sig((const secp256k1_ecdsa_recoverable_signature*)etx.signature,
                                     txHash.data(),
                                     etx.origin);
    if (!correct) {
        error_print("Signature verifiation of a TX failed.");
        return ERROR_SIGNATURE_VERIFY_FAIL;
    }

    // allow account creation for operator (if it does not exist)
    auto snderAcState = (etx.origin == this->operAddr && !m_gs.exists(etx.origin)) ? m_gs.create(etx.origin, 0u, EMPTY_CODE_OBJ) : m_gs.get(etx.origin);

    // 2) increment the nonce and adjust the balance of the sender
    debug_print(fmt::format("Code size of sender account is {} ", snderAcState.acc.get_code_ref().size()));
    if (EMPTY_CODE_OBJ == snderAcState.acc.get_code_ref()) {  // according to ETH Yellow paper, increment only if code of sender is empty (i.e., normal account)
        snderAcState.acc.set_nonce(snderAcState.acc.get_nonce() + 1);
    }
    auto& storage = m_gs.getStorages().at(etx.origin);  // just copy the old storage
    auto& code = snderAcState.acc.get_code_ref();
    if (etx.origin != this->operAddr && (etx.value > snderAcState.acc.get_balance())) {
        error_print(fmt::format("The account {} does not have enough balance.", address_to_hex_string(etx.origin)));
        return ERR_EVM_LOW_BALANCE;
    }
    // if TX was made by the operator then do not check his balance and just add the value to the sender
    auto senderBalBefore = snderAcState.acc.get_balance();
    auto senderDeducted = ((etx.origin == this->operAddr) ? intx::uint256(0u) : intx::uint256(etx.value));
    auto accSndUpdated = m_gs.update(etx.origin, {eevm::SimpleAccount(etx.origin, senderBalBefore - senderDeducted, code, snderAcState.acc.get_nonce(), storage), storage});
    assert(accSndUpdated.acc.get_balance() == senderBalBefore + senderDeducted);

    // 3) add value to the target account
    auto recvAcState = (!m_gs.exists(etx.to)) ? m_gs.create(etx.to, 0u, EMPTY_CODE_OBJ) : m_gs.get(etx.to);  // cretate target account if it does not exist
    storage = m_gs.getStorages().at(etx.to);                                                                 // just copy the old storage
    code = recvAcState.acc.get_code_ref();
    auto recvBalanceBefore = recvAcState.acc.get_balance();
    auto recvAcStateAfter = m_gs.update(etx.to, {eevm::SimpleAccount(etx.to, recvBalanceBefore + intx::uint256(etx.value), code, recvAcState.acc.get_nonce(), storage), storage});
    assert(recvAcStateAfter.acc.get_balance() == recvBalanceBefore + intx::uint256(etx.value));

    return RET_SUCCESS;
}