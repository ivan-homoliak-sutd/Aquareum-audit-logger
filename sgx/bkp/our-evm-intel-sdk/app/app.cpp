#include "enclave_u.h"
#include "sgx_urts.h"

#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <sys/stat.h>

#include "app.h"
#include "data_types.h"
#include "enclave.h"
#include "utils.h"

using namespace std;

#define MAX_CMD_LEN 256

////////////////////
// OCALL definitions
////////////////////

int ocall_save_evm_state(const uint8_t* sealed_data, const size_t sealed_size) {
    ofstream file(SEALED_STORAGE_EVM, ios::out | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.write((const char*)sealed_data, sealed_size);
    file.close();
    return 0;
}

int ocall_load_evm_state(uint8_t* sealed_data, const size_t sealed_size) {
    ifstream file(SEALED_STORAGE_EVM, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*)sealed_data, sealed_size);
    file.close();
    return 0;
}

int ocall_does_sealed_state_exist(void) {
    struct stat buffer;
    if (0 != stat(SEALED_STORAGE_EVM, &buffer)) {
        return 1;
    }
    return 0;
}

////////////////////////////////////////
// Processing commands from admin user
////////////////////////////////////////

void main_loop(sgx_enclave_id_t eid) {

    int ret;
    sgx_status_t ecall_status;
    char command[MAX_CMD_LEN];

    while (true) {

        cout << "$>";
        cin.getline(command, MAX_CMD_LEN);
        if (0 == strcmp(command, "show") || 0 == strcmp(command, "s")) {
            PublicSealedData pub_evm_state;
            ecall_status = ecall_read_pub_state(eid, &ret, &pub_evm_state, sizeof(pub_evm_state));
            if (ecall_status != SGX_SUCCESS || is_error(ret)) {
                error_print("Fail to initialize EVM enclave.");
            }
            cout << "The number of disk inits of enclace is " << pub_evm_state.diskInits << endl;

        } else if (0 == strcmp(command, "q") || 0 == strcmp(command, "quit")) {
            info_print("Syncing sealed state of enclave to disk...");
            ecall_status = ecall_sync_evm_sealed_state_to_disk(eid, &ret);
            if (ecall_status != SGX_SUCCESS || is_error(ret)) {
                error_print("Error when syncing sealed state.");
            } else {
                info_print("Successfuly synced.");
            }
            cout << "Shell quits..\n";
            break;
        } else {
            cout << "Unknown command" << endl;
        }
    }
}

int main(int argc, char** argv) {

    sgx_enclave_id_t eid = 0;
    sgx_launch_token_t token = {0};
    int updated, ret;
    sgx_status_t ecall_status, enclave_status;

    enclave_status = sgx_create_enclave(ENCLAVE_FILE, SGX_DEBUG_FLAG, &token, &updated, &eid, NULL);
    if (enclave_status != SGX_SUCCESS) {
        error_print("Fail to initialize enclave.");
        return -1;
    }
    info_print("SGX successfully initilised.");

    // initialize EVM
    ecall_status = ecall_initialize_evm(eid, &ret);
    if (ecall_status != SGX_SUCCESS || is_error(ret)) {
        error_print("Fail to initialize EVM enclave.");
    } else {
        info_print("EVM enclave successfully initialized.");
    }

    // process requests from user using an interactive shell
    main_loop(eid);

    // destroy enclave
    enclave_status = sgx_destroy_enclave(eid);
    if (enclave_status != SGX_SUCCESS) {
        error_print("Fail to destroy enclave.");
        return -1;
    }
    info_print("Enclave successfully destroyed.");
    return 0;
}
