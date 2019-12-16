#include "ecl-host.h"
// #include "ecledger_u.h"

#include "common.h"
#include "data_types.h"
#include "utils.h"

// eEVM
#include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/transaction.h"
#include "eEVM/util.h"
// #include "eEVM/processor.h"
// #include "eEVM/simple/simpleglobalstate.h"

#include <openssl/sha.h>

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

ECLedger::ECLedger(){};

eevm::PersistantTransaction* ECLedger::createHelloWorldTX(secp256k1_pubkey& PK_sender,
                                                          uint8_t (&SK_sender)[ECC_SK_SIZE],
                                                          secp256k1_context& ctx) {

    // Create addresses for sender using his PK
    const eevm::Address sender = eevm::from_big_endian(PK_sender.data, PB_ADDR_SIZE);

    // Create random addresses for contract
    std::vector<uint8_t> raw_address(20); // addrress has 20 Bytes
    std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
    const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());

    // Create code
    std::string hello_world("[ENCLAVE]: Executed smart contract that prints this msg!");
    const eevm::Code code = create_bytecode(hello_world);

    uint64_t nonce = 0; // TODO: this is temporary (it should be extracted from evm)
    auto tx = new eevm::PersistantTransaction(sender, to, nonce, 0, code);

    auto inp4hash = tx->asDataForHash();

    eevm::KeccakHash tx_hash = eevm::keccak_256(inp4hash);

    secp256k1_ecdsa_signature tx_sig;
    int ret = secp256k1_ecdsa_sign(&ctx, &tx_sig, tx_hash.data(), SK_sender, NULL, NULL);
    if (1 != ret) {
        error_print("Error when signing hello world TX.");
    }
    int i = 0;
    memcpy(tx->signature, tx_sig.data, SIG_SIZE_PB);

    return tx;
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