#include "ecl.h"
#include "ecledger_t.h"

#include "common.h"
#include "data_types.h"
#include "signing-PB/signing.h"

// eEVM
#include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/processor.h"
#include "eEVM/simple/simpleglobalstate.h"

int ECLedger::execute_tx(PersistantTxProxy_T* tx, const uint8_t* code, size_t code_size) {

    // create eevm::Tx object from the proxy and code
    auto c = std::vector<uint8_t>(std::move(code), code + code_size);
    auto lh = eevm::NullLogHandler();

    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(tx->origin),
                                 reinterpret_cast<eevm::Address*>(tx->to),
                                 lh, c, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, tx->signature);

    // Deploy contract to global state
    const eevm::AccountState contract = this->gs.create(etx.to, 0, c);

    // Create processor
    eevm::Processor p(this->gs);

    // Execute code. All executions are associated with a TX. This TX is called by sender, executing the code in contract,
    // with empty input (and no trace collection)
    const eevm::ExecResult e = p.run(etx, etx.origin, contract, {}, 0, nullptr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return ERR_EVM_WRONG_RET_CODE;
    }

    const std::string response(reinterpret_cast<const char*>(e.output.data()));
    TRACE_ENCLAVE("output: %s", response.c_str());
    return RET_SUCCESS;
}

std::vector<uint8_t> create_bytecode(const std::string& s) {
    std::vector<uint8_t> code;
    constexpr uint8_t mdest = 0x0;
    const uint8_t rsize = s.size() + 1;

    // Store each byte in evm memory
    uint8_t mcurrent = mdest;
    for (const char& c : s) {
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(c);
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(mcurrent++); // IH: this represents the address in the memory, starting from 0;
        code.push_back(eevm::Opcode::MSTORE8);
    }

    // Return
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(rsize); // the size to read from memory (i.e., length of string)
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(mdest); // starting from memory 0x00
    code.push_back(eevm::Opcode::RETURN);

    return code;
}

int ECLedger::execute_hello_world() {

    // Create random addresses for sender and contract
    std::vector<uint8_t> raw_address(20); // addrress has 20 Bytes
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
    const eevm::AccountState contract = gs.create(to, 0, code);

    // Create transaction
    eevm::NullLogHandler ignore;
    eevm::Transaction tx(sender, to, ignore);

    // Create processor
    eevm::Processor p(gs);

    // Execute code. All executions are associated with a transaction. This
    // transaction is called by sender, executing the code in contract, with empty
    // input (and no trace collection)
    const eevm::ExecResult e = p.run(tx, sender, contract, {}, 0, nullptr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[ENCLAVE:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return 2;
    }

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

////////////////////////////////////////////////////////////

void push_uint256(std::vector<uint8_t>& code, const uint256_t& n) {
    code.push_back(eevm::Opcode::PUSH32); // Append opcode

    // Resize code array
    const size_t pre_size = code.size();
    code.resize(pre_size + 32);

    // Serialize number into code array
    eevm::to_big_endian(n, code.data() + pre_size); // IH: store n to real memory pointed by code.data() + pre_size
}

std::vector<uint8_t> create_a_plus_b_bytecode(const uint256_t& a, const uint256_t& b) {
    std::vector<uint8_t> code;
    constexpr uint8_t mdest = 0x0;  //< Memory start address for result
    constexpr uint8_t rsize = 0x20; //< Size of result

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

int ECLedger::execute_sum_a_b(int a, int b) {
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
    const eevm::AccountState contract = gs.create(to, 0, code);

    if (verbose) {
        std::cout << fmt::format(
                         "[ENCLAVE:] Target address {} contains the following bytecode:\n {}",
                         eevm::to_checksum_address(to),
                         eevm::to_hex_string(contract.acc.get_code()))
                  << std::endl;
    }

    // Construct a transaction object
    eevm::NullLogHandler ignore; //< Ignore any logs produced by this transaction
    std::cout << "[ENCLAVE]: Creating Transaction" << std::endl;
    eevm::Transaction tx(sender, to, ignore);

    std::cout << "[ENCLAVE]: Creating eEVM Processor" << std::endl;

    // Construct processor
    eevm::Processor p(gs);

    if (verbose)
        std::cout << fmt::format("[ENCLAVE:] Executing a transaction from {} to {}", eevm::to_checksum_address(sender),
                                 eevm::to_checksum_address(to))
                  << std::endl;

    // Run transaction
    eevm::Trace tr;
    const eevm::ExecResult e = p.run(
        tx,
        sender,
        contract,
        {}, //< No input - the arguments are hard-coded in the contract
        0,  //< No gas value
        &tr //< Record execution trace
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
