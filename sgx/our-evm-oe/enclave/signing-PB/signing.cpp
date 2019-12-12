#include "signing-PB/signing.h"
#include "ecledger_t.h"
#include "errcodes.h"
#include "secp256k1.h"

#include <openenclave/enclave.h>

int generate_keypair_PB(KeyPairPB_T* keypair) {

    // 1) compute SK of PB by random generation
    if (OE_OK != oe_random(keypair->SK_PB, 32)) {
        return ERR_RAND_FAILED;
    }

    // 2) compute PK of PB
    static secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN); // if sth needs to be verified here, then add also | SECP256K1_CONTEXT_VERIFY
    if (1 != secp256k1_ec_pubkey_create(ctx, &keypair->PK_PB, keypair->SK_PB)) {
        return ERR_KEYPAIR_GEN_FAILED;
    }
    return 0;
}
