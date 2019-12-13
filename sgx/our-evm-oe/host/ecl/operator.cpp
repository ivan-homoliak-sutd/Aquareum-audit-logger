

#include <string>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "common.h"
#include "operator.h"
#include "secp256k1.h"
#include "utils.h"
#include "secp256k1.h"

using namespace ecl;

Operator::Operator(secp256k1_pubkey * _enc_PK) {

    memcpy(this->PK_E_PB.data, _enc_PK->data, ECC_PK_SIZE);
    info_print(string("PK_E_PB = ") + to_hex_str(_enc_PK->data, ECC_PK_SIZE));

    // 1) compute SK of PB by random generation
    int rc = RAND_bytes(this->SK_O, ECC_SK_SIZE);
    if (rc != 0) {
        unsigned long err = ERR_get_error();
        error_print(string("RAND_pseudo_bytes failed") + std::to_string(err));
    }

    // 2) compute PK of PB
    static secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN); // if sth needs to be verified here, then add also | SECP256K1_CONTEXT_VERIFY
    if (1 != secp256k1_ec_pubkey_create(ctx, &this->PK_O, (const uint8_t *) &this->SK_O)) {
        error_print(string("secp256k1_ec_pubkey_create failed"));
        // return ERR_KEYPAIR_GEN_FAILED;
    }
}