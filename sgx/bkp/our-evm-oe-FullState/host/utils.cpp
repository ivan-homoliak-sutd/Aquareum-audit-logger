#include "utils.h"
#include "common.h"

#include <cstring>
#include <fmt/format_header_only.h>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

using namespace std;

void info_print(const string& str)
{
    std::cout << "\t[INFO] " << str << "\n";
}

void debug_print(const string& str, bool endline)
{
    debug_print(str.c_str(), endline);
}

void debug_print(const char* str, bool endline)
{
    string a = (endline) ? "\n" : "";
    string b = (endline) ? "\t[DEBUG] " : "";
    std::cout << b << str << a;
}

void warning_print(const string& str)
{
    std::cerr << "\t[WARNING] " << str << "\n";
}

void error_print(const string& str)
{
    std::cerr << "\t[ERROR] " << str << "\n";
}

int is_error(int error_code)
{
    char err_message[100];

    // check error case
    switch (error_code) {
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

    error_print(string(std::move(err_message)));  // print error message
    return 1;
}

string to_hex_str(const unsigned char* _bytes, size_t cnt)
{
    auto hex_buf = (char*)malloc(2 * cnt + 1);
    for (size_t i = 0; i < cnt; i++) {
        std::sprintf(hex_buf + 2 * i, "%02X", _bytes[i]);
    }
    hex_buf[2 * cnt] = '\0';
    string ret(hex_buf);
    free(hex_buf);
    return ret;
}

template <class T>
T sumVect(std::vector<T>& v)
{
    T sum = 0;
    for (auto item : v) {
        sum += item;
    }
    return sum;
    // return std::accumulate(v->begin(), v->end(), 0);
}

size_t sumVectST(std::vector<size_t>& v)
{
    return sumVect(v);
}