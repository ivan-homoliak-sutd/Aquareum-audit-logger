#pragma once

#include "data_types.h"
// #include "secp256k1.h"
#include "eEVM/address.h"
#include "secp256k1_recovery.h"


class ECC {
    static secp256k1_context* m_ctx;

public:
    inline ECC()
    {
        if (NULL == m_ctx)
            m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    }

    inline ~ECC()
    {
        if (NULL != m_ctx)
            free(m_ctx);
    }

    int compute_PK_from_SK(KeyPairPB_T* keypair);

    int sign_data(const std::vector<uint8_t>& data, const uint8_t* SK, uint8_t* sig);

    int sign_hash(secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const uint8_t* seckey);

    bool verify_sig(const secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const eevm::Address addr);
};
