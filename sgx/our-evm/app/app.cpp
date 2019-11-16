#include "enclave_u.h"
#include "sgx_urts.h"

#include <cstring>
#include <fstream>
#include <getopt.h>
#include <sys/stat.h>

#include "app.h"
#include "utils.h"
#include "data_types.h"
#include "enclave.h"

using namespace std;

int ocall_save_evm_state(const uint8_t* sealed_data, const size_t sealed_size) {
    ofstream file(SEALED_STORAGE_EVM, ios::out | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.write((const char*) sealed_data, sealed_size);
    file.close();
    return 0;
}


int ocall_load_evm_state(uint8_t* sealed_data, const size_t sealed_size) {
    ifstream file(SEALED_STORAGE_EVM, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*) sealed_data, sealed_size);
    file.close();
    return 0;
}


int main(int argc, char** argv) {

    sgx_enclave_id_t eid = 0;
    sgx_launch_token_t token = {0};
    int updated, ret;
    sgx_status_t ecall_status, enclave_status;

    enclave_status = sgx_create_enclave(ENCLAVE_FILE, SGX_DEBUG_FLAG, &token, &updated, &eid, NULL);
    if(enclave_status != SGX_SUCCESS) {
        error_print("Fail to initialize enclave.");
        return -1;
    }
    info_print("Enclave successfully initilised.");

    // initialize EVM if it was not done before - if sealed storage does not exist
    struct stat buffer;
    char * n_value=NULL;
    if(0 != stat(SEALED_STORAGE_EVM, &buffer)) {
        ecall_status = ecall_initialize_evm(eid, &ret);
        if (ecall_status != SGX_SUCCESS || is_error(ret)) {
            error_print("Fail to initialize EVM enclave.");
        } else {
            info_print("EVM enclave successfully initialized.");
        }
    }

    // destroy enclave
    enclave_status = sgx_destroy_enclave(eid);
    if(enclave_status != SGX_SUCCESS) {
        error_print("Fail to destroy enclave.");
        return -1;
    }
    info_print("Enclave successfully destroyed.");
    return 0;
}
