#pragma once

#include "common.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/transaction.h"
#include "secp256k1.h"
#include "signing.h"
#include <nlohmann/json.hpp>

using namespace eevm;


using Bytes = std::vector<uint8_t>;

struct CtorPar {
    std::string name;
    std::string type;
    u256 value;

    CtorPar(const std::string& name, const std::string& type, const u256& value)
      : name(name), type(type), value(value)
    {}
};

struct ContrDefinition {
    std::vector<byte> bin;                                 // code of the contract
    std::vector<std::pair<std::string, Bytes>> endpoints;  // functions available
    std::vector<CtorPar> ctor_params;                      // parameters of ctors (with defaut values)
};

class ECLedger {
public:
    NormalGlobalState m_gs;  // the full global state of the ECL ledger

    ECC* m_ecc;  // ECC signing wrapper

    eevm::Address operAddr;


    inline ECLedger(ECC* e)
      : m_ecc(e){};

    PersistantTransaction* createHelloWorldTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender,
                                              size_t nonce);

    PersistantTransaction* createSumTx(int a, int b,
                                       secp256k1_pubkey& PK_sender,
                                       uint8_t* SK_sender,
                                       size_t nonce);

    PersistantTransaction* createIncCounterTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender);

    PersistantTransaction* createDeploymentTX(const ContrDefinition& cdef,
                                              secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender,
                                              size_t nonce,
                                              uint64_t value);

    eevm::PersistantTransaction* createNewAccountTX(secp256k1_pubkey& PK_sender,
                                                    uint8_t* SK_sender,
                                                    const Address& newAddr,
                                                    unsigned initBalance,
                                                    size_t nonce);

    int executeTX(eevm::PersistantTransaction* tx);

private:
    int _execute_transfer_tx(eevm::Transaction& etx);
};
