#pragma once

#include "common.h"
#include "eEVM/normal/normalGlobalState.h"
#include "eEVM/transaction.h"
#include "secp256k1.h"
#include <nlohmann/json.hpp>

using namespace eevm;

class ECLedger {
    NormalGlobalState m_gs;  // the full global state of the ECL ledger

public:
    ECLedger();

    PersistantTransaction* createHelloWorldTX(secp256k1_pubkey& PK_sender,
                                              uint8_t (&SK_sender)[ECC_SK_SIZE],
                                              secp256k1_context& ctx);

    PersistantTransaction* createSumTx(int a, int b,
                                       secp256k1_pubkey& PK_sender,
                                       uint8_t (&SK_sender)[ECC_SK_SIZE],
                                       secp256k1_context& ctx);

    PersistantTransaction* createIncCounterTX(secp256k1_pubkey& PK_sender,
                                              uint8_t (&SK_sender)[ECC_SK_SIZE],
                                              secp256k1_context& ctx);

    PersistantTransaction* createDeploymentTX(const nlohmann::json& cdef,
                                              secp256k1_pubkey& PK_sender,
                                              uint8_t (&SK_sender)[ECC_SK_SIZE],
                                              secp256k1_context& ctx);
};
