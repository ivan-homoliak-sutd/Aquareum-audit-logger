#include "ecl-host.h"
// #include "ecledger_u.h"

#include "common.h"
#include "data_types.h"
#include "utils.h"

// eEVM
#include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/processor.h"
#include "eEVM/transaction.h"
#include "eEVM/util.h"
#include <fmt/format_header_only.h>
// #include "eEVM/simple/simpleglobalstate.h"

#include "aleth-mp3/Common.h"

#include <openssl/sha.h>

ECLedger::ECLedger(){};

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


/////////////////// AUX ///////////////////

void sign_tx(eevm::PersistantTransaction* tx, uint8_t (&SK_sender)[ECC_SK_SIZE], secp256k1_context& ctx)
{
    auto inp4hash = tx->asDataForHash();

    debug_print("sign_tx: [.", false);

    eevm::KeccakHash tx_hash = eevm::keccak_256(inp4hash);

    debug_print(".", false);

    secp256k1_ecdsa_signature tx_sig;
    int ret = secp256k1_ecdsa_sign(&ctx, &tx_sig, tx_hash.data(), SK_sender, NULL, NULL);
    if (1 != ret) {
        error_print("Error when signing hello world TX.");
    }
    debug_print(".", false);

    memcpy(tx->signature, tx_sig.data, SIG_SIZE_PB);
    debug_print("OK]\n", false);
}

/////////////////// Transaction creation ///////////////////

eevm::PersistantTransaction* ECLedger::createHelloWorldTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                          secp256k1_context& ctx)
{
    // Construct address for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Create random addresses for contract
    std::vector<uint8_t> raw_address(20);  // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Create code
    std::string hello_world("[ENCLAVE]: Executed smart contract that prints this msg!");
    const eevm::Code code = create_printStr_bytecode(hello_world);

    uint64_t nonce = 0;  // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, to, nonce, 0, code);
    sign_tx(tx, SK_sender, ctx);

    return tx;
}

eevm::PersistantTransaction* ECLedger::createSumTx(int a, int b,
                                                   secp256k1_pubkey& PK_sender,
                                                   uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                   secp256k1_context& ctx)
{
    // Parse args
    const uint256_t arg_a = eevm::to_uint256(std::to_string(a));
    const uint256_t arg_b = eevm::to_uint256(std::to_string(b));

    // Create addresses for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Create random addresses for contract
    std::vector<uint8_t> raw_address(20);  // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Create summing bytecode
    const eevm::Code code = create_a_plus_b_bytecode(arg_a, arg_b);

    // Construct a transaction object
    uint64_t nonce = 0;  // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, to, nonce, 0, code);
    sign_tx(tx, SK_sender, ctx);

    return tx;
}

// NOTE: it supports only 32B uint arguments of a constructor
eevm::PersistantTransaction* ECLedger::createDeploymentTX(const nlohmann::json& contract_definition,
                                                          secp256k1_pubkey& PK_sender,
                                                          uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                          secp256k1_context& ctx)
{
    // Construct address for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Create RANDOM address for contract (TODO: later derive it from nonce of AccountState of sender)
    std::vector<uint8_t> raw_address(20);  // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address contract_address = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Get the binary constructor of the contract and its parameters
    auto contract_ctor_code = eevm::to_bytes(contract_definition["bin"]);

    for (auto& ctor_param : contract_definition["ctor"]) {
        debug_print(fmt::format("\t parsing ctor parameter: {} {} => {} ", string(ctor_param["type"]), string(ctor_param["name"]), string(ctor_param["value"])));
        if (string(ctor_param["type"]) != "uint256")
            throw std::logic_error(fmt::format("Unsupported type of parameter in contract's constructor: '{}'", string(ctor_param["type"])));
        append_arg(contract_ctor_code, u256(std::stoul(string(ctor_param["value"]))));
    }
    // debug_print("--2");

    uint64_t nonce = 0;  // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, contract_address, nonce, 0, contract_ctor_code);
    sign_tx(tx, SK_sender, ctx);
    // debug_print("--3");

    return tx;
}

eevm::PersistantTransaction* ECLedger::createIncCounterTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                          secp256k1_context& ctx)
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
    sign_tx(tx, SK_sender, ctx);

    return tx;
}

eevm::PersistantTransaction* ECLedger::createNewAccountTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                          secp256k1_context& ctx,
                                                          const Address& newAddr, unsigned initBalance)
{
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);
    uint64_t nonce = 0;  // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, newAddr, nonce, initBalance, {});
    sign_tx(tx, SK_sender, ctx);
    return tx;
}

int ECLedger::executeTX(eevm::PersistantTransaction* tx)
{
    debug_print("Executing Tx in HOST...");
    auto lh = eevm::NullLogHandler();
    auto etx = eevm::Transaction(reinterpret_cast<eevm::Address*>(&tx->origin),
                                 reinterpret_cast<eevm::Address*>(&tx->to),
                                 lh, tx->code, tx->value, tx->nonce, tx->gas_price, tx->gas_limit, (uint8_t*)tx->signature);

    // get or create the account
    const eevm::SimpleAccountState contract = this->m_gs.get(etx.to);

    // execute the TX
    eevm::Processor<eevm::SimpleAccount, eevm::SimpleStorage> p(this->m_gs);
    const eevm::ExecResult e = p.run(etx, etx.origin, contract, {}, 0, nullptr);

    // Check the response
    if (e.er != eevm::ExitReason::returned) {
        std::cout << fmt::format("[HOST:] Unexpected return code: {}", (size_t)e.er) << std::endl;
        return ERR_EVM_WRONG_RET_CODE;
    }
    return RET_SUCCESS;
}


// sha256 with openSSL library
// unsigned char tx_hash[HASH_SIZE];
// SHA256_CTX ctx_sha256;
// SHA256_Init(&ctx_sha256);
// SHA256_Update(&ctx_sha256, tx->origin, sizeof(uint256_t)); // NOTE: hopefully it reads internal data of Address class
// SHA256_Update(&ctx_sha256, tx->value, sizeof(uint64_t));
// SHA256_Update(&ctx_sha256, tx->code, sizeof(tx->code.value_type) * tx->code.size);
// SHA256_Update(&ctx_sha256, tx->gas_price, sizeof(uint64_t));
// SHA256_Update(&ctx_sha256, tx->gas_limit, sizeof(uint64_t));
// SHA256_Final(tx_hash, &ctx_sha256);