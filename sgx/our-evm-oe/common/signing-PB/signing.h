#pragma once

#include "data_types.h"
// #include "secp256k1.h"
#include "eEVM/address.h"
#include "secp256k1_recovery.h"

class ECC {
public:
    static secp256k1_context* s_ctx;

    secp256k1_context* m_ctx;

    inline ECC()
    {
        m_ctx = ECC::s_ctx;
    }

    // inline ~ECC() // IH: this should be equipped with shared_pointer
    // {
    //     if (NULL != m_ctx)
    //         free(m_ctx);
    // }

    int compute_PK_from_SK(KeyPairPB_T* keypair);

    int sign_data(const std::vector<uint8_t>& data, const uint8_t* SK, uint8_t* sig);

    int sign_hash(secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const uint8_t* seckey);

    bool verify_sig(const secp256k1_ecdsa_recoverable_signature* rsig, const uint8_t* msg32, const eevm::Address addr);
};
