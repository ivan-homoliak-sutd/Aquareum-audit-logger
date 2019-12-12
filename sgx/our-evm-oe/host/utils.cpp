#include <stdio.h>
#include <cstring>

#include "utils.h"
#include "common.h"

#include <string>
#include <iostream>

using namespace std;

void info_print(const string &str) {
    std::cout << "[INFO]" << str << std::endl;
}

void warning_print(const string &str) {
    std::cerr << "[WARNING]" << str << std::endl;
}

void error_print(const string &str) {
    std::cerr << "[ERROR]" << str << std::endl;
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

        case ERR_CANNOT_LOAD_SEALED_STATE:
            sprintf(err_message, "Failed to load sealed state of EVM from a file.");
            break;

        case ERR_CANNOT_SAVE_EVM_STATE:
            sprintf(err_message, "Failed to save EVM state in a file.");
            break;

        case ERR_FAIL_SEAL_STATE:
            sprintf(err_message, "Failed to seal EVM state.");
            break;

        case ERR_FAIL_UNSEAL:
            sprintf(err_message, "Failed to unseal EVM state.");
            break;

        default:
            sprintf(err_message, "Unknown error.");
    }

    error_print(string(std::move(err_message))); // print error message
    return 1;
}

