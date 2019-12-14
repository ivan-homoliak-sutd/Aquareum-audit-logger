// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cstring>
#include <fstream>
#include <iostream>
#include <openenclave/host.h>
#include <stdio.h>
#include <sys/stat.h>

#include "common.h"
#include "ecl/operator.h"
#include "ecledger_u.h"
#include "utils.h"

#define FILE_SEALED_STORAGE_EVM "./data/sealed-storage-evm.seal"
#define MAX_CMD_LEN 256

////////////////////
// OCALL definitions
////////////////////

using namespace std;
using namespace ecl;

int ocall_save_evm_state(const uint8_t* sealed_data, const size_t sealed_size) {
    ofstream file(FILE_SEALED_STORAGE_EVM, ios::out | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.write((const char*)sealed_data, sealed_size);
    file.close();
    return 0;
}

int ocall_load_evm_state(uint8_t* sealed_data, const size_t sealed_size) {
    ifstream file(FILE_SEALED_STORAGE_EVM, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*)sealed_data, sealed_size);
    file.close();
    return 0;
}

int ocall_does_sealed_state_exist(void) {
    struct stat buffer;
    if (0 != stat(FILE_SEALED_STORAGE_EVM, &buffer)) {
        return 0;
    }
    return 1;
}

void ocall_host_ecledger() {
    fprintf(stdout, "Enclave called into host to print: Hello World!\n");
}

////////////////////////////////////////
// Processing commands from operator  //
////////////////////////////////////////

void operator_loop(oe_enclave_t* enclave) {

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
                error_print("Fail to initialize EVM enclave.");
            }
            cout << "The number of disk inits of enclave is " << pub_evm_state.diskInits << endl;

        } else if (0 == strcmp(command, "q") || 0 == strcmp(command, "quit")) {
            info_print("Syncing sealed state of enclave to disk...");
            ecall_ret = ecall_sync_evm_sealed_state_to_disk(enclave, &ret);
            if (ecall_ret != OE_OK || is_error(ret)) {
                error_print("Error when syncing sealed state.");
            } else {
                info_print("Successfuly synced.");
            }
            cout << "Operator shell quits...\n";
            break;
        } else {
            cout << "Unknown command" << endl;
        }
    }
}

///////////////////////// AUX STUFF /////////////////////////

bool check_simulate_opt(int* argc, const char* argv[]) {
    for (int i = 0; i < *argc; i++) {
        if (strcmp(argv[i], "--simulate") == 0) {
            fprintf(stdout, "Running in simulation mode\n");
            memmove(&argv[i], &argv[i + 1], (*argc - i) * sizeof(char*));
            (*argc)--;
            return true;
        }
    }
    return false;
}

///////////////////////// MAIN /////////////////////////

int main(int argc, const char* argv[]) {
    oe_result_t result;
    int ret = 1;
    int ret_e = 0;
    oe_enclave_t* enclave = NULL;
    Operator* op;

    uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;
    if (check_simulate_opt(&argc, argv)) {
        flags |= OE_ENCLAVE_FLAG_SIMULATE;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: %s enclave_image_path [ --simulate  ]\n", argv[0]);
        goto exit;
    }

    // Create the enclave
    result = oe_create_ecledger_enclave(argv[1], OE_ENCLAVE_TYPE_AUTO, flags, NULL, 0, &enclave); // could be also OE_ENCLAVE_TYPE_SGX
    if (OE_OK != result) {
        error_print(string("oe_create_ecledger_enclave(): ") + string(oe_result_str(result)));
        goto exit;
    }
    info_print("SGX successfully initialized.");

    secp256k1_pubkey encl_pk;

    result = ecall_initialize_evm(enclave, &ret_e, &encl_pk, sizeof(encl_pk));
    if (OE_OK != result || is_error(ret_e)) {
        error_print("Fail to initialize EVM enclave.");
        goto exit;
    } else {
        info_print("EVM enclave successfully initialized.");
    }

    op = new Operator(&encl_pk);
    op->persistMyKeys();

    operator_loop(enclave);

    ret = 0;

exit:
    // Clean up the enclave if we created one
    if (enclave) {
        if (OE_OK != (result = oe_terminate_enclave(enclave))) {
            error_print(string("Enclave was not destryed correctly: ") + string(oe_result_str(result)));
        }
        info_print("Enclave successfully destroyed.");
    }
    if (op)
        free(op);
    return ret;
}
