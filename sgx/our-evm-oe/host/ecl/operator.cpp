#include <fstream>
#include <iostream>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <string>
#include <sys/stat.h>

#include "common.h"
#include "operator.h"
#include "secp256k1.h"
#include "utils.h"

using namespace ecl;

Operator::Operator(secp256k1_pubkey* _enc_PK) {

    // If keys were generated and persisted before, just load them, otherwise generate new keys
    if (this->existsMyKeyFile()) {
        info_print(string("loading operator's keys from file."));
        if (RET_SUCCESS != this->loadMyKeysFromFile()) {
            error_print(string("Error when loading operator's keys."));
        }
    } else {
        info_print(string("generating new operator's keys."));
        memcpy(this->PK_E_PB.data, _enc_PK->data, ECC_PK_SIZE);

        // 1) compute SK of operator (under PB)
        int rc = RAND_priv_bytes((unsigned char*)&this->SK_O, ECC_SK_SIZE);
        if (rc != 1) {
            unsigned long err = ERR_get_error();
            error_print(string("RAND_pseudo_bytes failed, err = ") + std::to_string(err));
        }

        this->ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY); // ECC context

        // 2) compute PK of operator (under PB)
        if (1 != secp256k1_ec_pubkey_create(Operator::ctx, &this->PK_O, (const uint8_t*)&this->SK_O)) {
            error_print(string("secp256k1_ec_pubkey_create failed"));
        }
    }
    info_print(string("PK_E_PB = ") + to_hex_str(_enc_PK->data, ECC_PK_SIZE));
    info_print(string("SK_O = ") + to_hex_str(this->SK_O, ECC_SK_SIZE));
    info_print(string("PK_O = ") + to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE));
}

int Operator::loadMyKeysFromFile() {
    ifstream file(FILE_OPERATOR_KEYS, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*)this->SK_O, ECC_SK_SIZE);
    file.read((char*)this->PK_O.data, ECC_PK_SIZE);
    file.close();
    return 0;
}

bool Operator::existsMyKeyFile() {
    struct stat buffer;
    if (0 != stat(FILE_OPERATOR_KEYS, &buffer)) {
        return false;
    }
    return true;
}

int Operator::persistMyKeys() {
    ofstream file(FILE_OPERATOR_KEYS, ios::out | ios::binary);
    if (file.fail()) {
        return ERR_SAVING_OPER_KEYS;
    }
    file.write((const char*)this->SK_O, ECC_SK_SIZE);
    file.write((const char*)this->PK_O.data, ECC_PK_SIZE);
    file.close();
    return RET_SUCCESS;
}
