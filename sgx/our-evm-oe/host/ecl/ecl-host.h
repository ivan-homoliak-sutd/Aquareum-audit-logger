#pragma once

#include "common.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/transaction.h"
#include "secp256k1.h"
#include "signing.h"
#include <nlohmann/json.hpp>

using namespace eevm;

class ECLedger {
public:
    ECC* m_ecc;  // ECC signing wrapper

    inline ECLedger(ECC* e)
      : m_ecc(e){};


    NormalGlobalState m_gs;  // the full global state of the ECL ledger

    PersistantTransaction* createHelloWorldTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender);

    PersistantTransaction* createSumTx(int a, int b,
                                       secp256k1_pubkey& PK_sender,
                                       uint8_t* SK_sender);

    PersistantTransaction* createIncCounterTX(secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender);

    PersistantTransaction* createDeploymentTX(const nlohmann::json& cdef,
                                              secp256k1_pubkey& PK_sender,
                                              uint8_t* SK_sender);

    eevm::PersistantTransaction* createNewAccountTX(secp256k1_pubkey& PK_sender,
                                                    uint8_t* SK_sender,
                                                    const Address& newAddr,
                                                    unsigned initBalance);

    int executeTX(eevm::PersistantTransaction* tx);
};
