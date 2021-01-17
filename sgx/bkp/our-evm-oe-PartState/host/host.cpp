// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cstring>
#include <fstream>
#include <iostream>
#include <openenclave/host.h>
#include <stdio.h>
#include <sys/stat.h>

#include "common.h"
#include "ledger/operator.h"
#include "aqledger_u.h"
#include "utils.h"

#define FILE_SEALED_STORAGE_EVM "./data/sealed-storage-evm.seal"

////////////////////
// OCALL definitions
////////////////////

using namespace std;
using namespace aql;

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

void ocall_host_aqledger() {
    fprintf(stdout, "Enclave called into host to print: Hello World!\n");
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

int parseArgs(int argc, const char* argv[], uint32_t * flags) {
    *flags = OE_ENCLAVE_FLAG_DEBUG;
    if (check_simulate_opt(&argc, argv)) {
        *flags |= OE_ENCLAVE_FLAG_SIMULATE;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: %s enclave_image_path [ --simulate  ]\n", argv[0]);
        return ERR_WRONG_ARGS;
    }
    // debug_print(fmt::format("OPEN ENCLAVE FLAGS = {}", *flags));
    return RET_SUCCESS;
}

///////////////////////// MAIN /////////////////////////

int main(int argc, const char* argv[]) {
    oe_result_t result;
    int ret = 1;
    int ret_e = 0;
    oe_enclave_t* enclave = NULL;
    Operator* op;
    uint32_t flags = 0;

    if (RET_SUCCESS != parseArgs(argc, argv, &flags))
        return ret;

    // Create the enclave
    result = oe_create_aqledger_enclave(argv[1], OE_ENCLAVE_TYPE_SGX, flags, NULL, 0, &enclave); // could be also OE_ENCLAVE_TYPE_SGX
    if (OE_OK != result) {
        ERROR_PRINT("oe_create_aqledger_enclave(): %s", oe_result_str(result));
        goto exit;
    }
    info_print("SGX successfully initialized.");

    // Initialize EVM part of the enclave, generate/lead keys and get E_PK_PB
    secp256k1_pubkey encl_pk;

    result = ecall_initialize_evm(enclave, &ret_e, &encl_pk, sizeof(encl_pk));
    if (OE_OK != result || is_error(ret_e)) {
        error_print("Fail to initialize EVM enclave.");
        goto exit;
    } else {
        info_print("EVM enclave successfully initialized.");
    }

    op = new Operator(&encl_pk);
    op->operatorLoop(enclave); // the main loop of operator

    ret = 0;

exit:
    if (enclave) { // Clean up the enclave if we created one
        if (OE_OK != (result = oe_terminate_enclave(enclave)))
            error_print(string("Enclave was not destryed correctly: ") + string(oe_result_str(result)));
        else
            info_print("Enclave successfully destroyed.");
    }
    if (op)
        free(op);
    return ret;
}
