#include <stdio.h>
#include <cstring>

#include "utils.h"
#include "app.h"
#include "data_types.h"
#include "enclave.h"

void info_print(const char* str) {
    printf("[INFO] %s\n", str);
}

void warning_print(const char* str) {
    printf("[WARNING] %s\n", str);
}

void error_print(const char* str) {
    printf("[ERROR] %s\n", str);
}

int is_error(int error_code) {
    char err_message[100];

    // check error case
    switch(error_code) {
        case RET_SUCCESS:
            return 0;

        case RET_SUCCESS_INIT_LOADED_STATE:
            info_print("EVM state loaded from sealed file.");
            return 0;

        case RET_SUCCESS_INIT_NEW_STATE:
            info_print("EVM state initialized in enclave.");
            return 0;

        case ERR_RAND_FAILED:
            sprintf(err_message, "Random byte generation failed in enclave.");
            break;

        case ERR_CANNOT_SAVE_EVM_STATE:
            sprintf(err_message, "Fail to save EVM state in a file.");
            break;

        case ERR_FAIL_SEAL_STATE:
            sprintf(err_message, "Fail to seal EVM state.");
            break;

        case ERR_FAIL_UNSEAL:
            sprintf(err_message, "Fail to unseal EVM state.");
            break;

        default:
            sprintf(err_message, "Unknown error.");
    }

    // print error message
    error_print(err_message);
    return 1;
}

