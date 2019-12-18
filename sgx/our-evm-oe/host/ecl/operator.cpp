#include "operator.h"
#include "common.h"
#include "secp256k1.h"
#include "utils.h"

#include <boost/tokenizer.hpp>
#include <fmt/format_header_only.h>
#include <fstream>
#include <iostream>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

using namespace ecl;

Operator::Operator(secp256k1_pubkey* _enc_PK) : ecl() {

    // If keys were generated and persisted before, just load them, otherwise generate new keys
    if (this->existsMyKeyFile()) {
        info_print(string("loading operator's keys from file."));
        if (RET_SUCCESS != this->loadMyKeysFromFile()) {
            error_print(string("Error when loading operator's keys."));
            return;
        }
    } else {
        info_print(string("generating new operator's keys."));

        // 1) compute SK of operator (under PB)
        int rc = RAND_priv_bytes((unsigned char*)&this->SK_O, ECC_SK_SIZE);
        if (rc != 1) {
            unsigned long err = ERR_get_error();
            error_print(string("RAND_pseudo_bytes failed, err = ") + std::to_string(err));
            return;
        }

        this->ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY); // ECC context

        // 2) compute PK of operator (under PB)
        if (1 != secp256k1_ec_pubkey_create(this->ctx, &this->PK_O, (const uint8_t*)&this->SK_O)) {
            error_print(string("secp256k1_ec_pubkey_create failed"));
            return;
        }
        this->persistMyKeys();
    }
    memcpy(this->PK_E_PB.data, _enc_PK->data, ECC_PK_SIZE);

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

//////////////////// AUX ////////////////////

void Operator::print_evm_state(PublicSealedData_T& es) {
    cout << "\t PK_E_PB = " << to_hex_str(this->PK_E_PB.data, ECC_PK_SIZE) << "\n"
         << "\t PK_O = " << to_hex_str((const unsigned char*)&this->PK_O, ECC_PK_SIZE) << "\n"
         << "\t SK_O = " << to_hex_str((const unsigned char*)&this->SK_O, ECC_SK_SIZE) << "\n";

    cout << fmt::format("\t hdrLast[{}] = ", es.idCurrent) << to_hex_str(es.hdrLast, HASH_SIZE) << "\t(the last header created by E)\n"
         << "\t logRootPB  = " << to_hex_str(es.logRootPB, HASH_SIZE) << "\t(the last root of L flushed to PB)\n"
         << "\t |txsErrCache| = " << es.txsErrCache.count << "\n"
         << "\t diskInits = " << es.diskInits << "\n";
}

////////////////////////////////////////
// Processing commands from operator  //
////////////////////////////////////////

void Operator::operatorLoop(oe_enclave_t* enclave) {

    int ret;               // internal return value
    oe_result_t ecall_ret; // return value of general enclave call
    char command[MAX_CMD_LEN];

    while (true) {

        cout << "$>";
        cin.getline(command, MAX_CMD_LEN);
        if (0 == strcmp(command, "show") || 0 == strcmp(command, "s")) {
            PublicSealedData_T pub_evm_state;
            ecall_ret = ecall_read_pub_state(enclave, &ret, &pub_evm_state, sizeof(pub_evm_state));
            if (ecall_ret != OE_OK && is_error(ret)) {
                error_print("Failed to initialize EVM enclave.");
            }
            this->print_evm_state(pub_evm_state);
        } else if (0 == strcmp(command, "test")) {
            info_print("Invoking internally generated TXs in enclave...");

            ecall_ret = ecall_enclave_ecledger(enclave);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when invoking internal TX generation.");
            }
            info_print("...done");
        } else if (0 == strncmp(command, "tx add", 6)) {

            const std::string& delims = " ";
            typedef boost::char_separator<char> separator;
            boost::tokenizer<separator> tokens(string(std::move(command)), separator(delims.c_str()));
            if (std::distance(tokens.begin(), tokens.end()) != 4) {
                std::cerr << "wrong token count: " << std::distance(tokens.begin(), tokens.end()) << std::endl;
                continue;
            }
            int a, b;
            try {
                auto it = tokens.begin();
                std::advance(it, 2);
                a = std::stoi(*it);
                b = std::stoi(*std::next(it));
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }
            INFO_PRINT( "Creating TX that sums %d + %d ...", a, b);

            // create TX using eEVM
            eevm::PersistantTransaction* tx = this->ecl.createSumTx(a, b, this->PK_O, this->SK_O, *(this->ctx));
            ecall_ret = ecall_run_single_tx(enclave, &ret,
                                            (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                            (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing sum TX in Enclave.");
            }
        } else if (0 == strcmp(command, "tx")) {
            info_print("Creating hello world TX ...");

            // create and sign TX
            eevm::PersistantTransaction* tx = this->ecl.createHelloWorldTX(this->PK_O, this->SK_O, *(this->ctx));

            ecall_ret = ecall_run_single_tx(enclave, &ret,
                                            (PersistantTxProxy_T*)tx, sizeof(PersistantTxProxy_T),
                                            (const uint8_t*)tx->code.data(), tx->code.size());
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when processing print hello world TX in Enclave.");
            }
        } else if (0 == strcmp(command, "q") || 0 == strcmp(command, "quit")) {
            info_print("Syncing sealed state of enclave to disk...");
            ecall_ret = ecall_sync_evm_sealed_state_to_disk(enclave, &ret);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when syncing sealed state.");
            }
            cout << "Operator shell quits...\n";
            break;
        } else {
            cout << "Unknown command\n";
        }
    }
}
