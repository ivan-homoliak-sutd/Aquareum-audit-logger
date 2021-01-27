#pragma once

#include <iostream>
#include <stddef.h>
#include <string>
#include "mp3_modes.h"

// switch ON or OFF tracing logs or info logs

#define TRACING_ENABLED

#define INFO_LOG_ENABLED

#define DEFAULT_MODE MODE::FullStateMaintained

#define DEFAULT_AS_BUFFER_SIZE 5000000
#define DEFAULT_AS_BUFFER_SIZES_SIZE 20000

// SGX stuff
#define POLICY_UNIQUE 1
#define POLICY_PRODUCT 2

// ECC config
#define MAX_OPT_MESSAGE_LEN 128
#define IV_SIZE 16
#define SIGNATURE_LEN 32
#define ECC_SK_SIZE 32
#define ECC_PK_SIZE 64
#define PB_ADDR_SIZE 20
#define VALID_ECC_SIG_RET 1
#define PRINT_SEP_LEN 120

// default MP3 cleanup settings
#define DEFAULT_MAXSTALEMB_DB_ENC "10"
#define DEFAULT_MAXSTALEMB_DB_HOST "100"

// #define EMPTY_CODE_OBJ eevm::Code(EMPTY_CODE)

typedef struct _sealed_data_t {
    size_t total_size;
    unsigned char signature[SIGNATURE_LEN];
    unsigned char opt_msg[MAX_OPT_MESSAGE_LEN];
    unsigned char iv[IV_SIZE];
    size_t key_info_size;
    size_t original_data_size;
    size_t encrypted_data_len;
    unsigned char encrypted_data[];  // note that key_info is at the end of encrypted_data, while it is unencrypted (based on it, E can reproduce a sealing key)
} sealed_data_t;

enum class EncExec { START,
                     END };

#define NOOP

#ifdef TRACING_ENABLED
#define TRACE_ENCLAVE(fmt, ...)              \
    printf(                                  \
        ">\t[TRACE_ENC]: %s(%d): " fmt "\n", \
        __FILE__,                            \
        __LINE__,                            \
        ##__VA_ARGS__)

#define TRACE_HOST(fmt, ...)                 \
    printf(                                  \
        "\t[TRACE_HOST]: %s(%d): " fmt "\n", \
        __FILE__,                            \
        __LINE__,                            \
        ##__VA_ARGS__)


#else
#define TRACE_ENCLAVE(fmt, ...) NOOP
#define TRACE_HOST(fmt, ...) NOOP
#endif

#define ERROR_PRINT(fmt, ...)       \
    fprintf(stderr,                 \
            "\t[ERROR]: " fmt "\n", \
            ##__VA_ARGS__)

#define INFO_PRINT(fmt, ...)       \
    fprintf(stdout,                \
            "\t[INFO]: " fmt "\n", \
            ##__VA_ARGS__)



inline void print_enc_sep(EncExec e)
{
#ifdef TRACING_ENABLED
    std::string tag = (e == EncExec::START) ? " ECALL START " : "  ECALL END  ";
    char arrow = (e == EncExec::START) ? '>' : '<';
    std::cout << std::string(PRINT_SEP_LEN / 2, arrow) << tag << std::string(PRINT_SEP_LEN / 2, arrow) << "\n";
#else
    NOOP
#endif
}


// errors shared by host and enclaves
#define ERROR_SIGNATURE_VERIFY_FAIL 1
#define ERROR_OUT_OF_MEMORY 2
#define ERROR_GET_SEALKEY 3
#define ERROR_SIGN_SEALED_DATA_FAIL 4
#define ERROR_CIPHER_ERROR 5
#define ERROR_UNSEALED_DATA_FAIL 6

#define ERR_ECC_SIGNING 51


// EVM enclave return codes
#define RET_SUCCESS 0
#define ERR_RAND_FAILED 100
#define ERR_FAIL_SEAL_STATE 101
#define ERR_CANNOT_SAVE_EVM_STATE 102
#define ERR_STAT_FILE_INIT 103
#define ERR_LOAD_EVM_STATE 104
#define RET_SUCCESS_INIT_NEW_STATE 105
#define RET_SUCCESS_INIT_LOADED_STATE 106
#define ERR_PK_GEN_FAILED 107
#define ERR_FAIL_UNSEAL 108
#define ERR_CANNOT_LOAD_SEALED_STATE 109
#define ERR_SAVING_OPER_KEYS 110
#define ERR_WRONG_ARGS 111
#define ERR_KEYPAIR_GEN_FAILED 112
#define ERR_NOT_FSMAINTAINED_MODE 113

#define ERR_EVM_EXEC 201
#define ERR_EVM_WRONG_RET_CODE 202
#define ERR_EVM_WRONG_FULL_STATE 203
#define ERR_EVM_INCONSISTANT_STATE 204
#define ERR_EVM_LOW_BALANCE 205
#define ERR_EVM_WRONG_CONTR_ADDR 206
#define ERR_EVM_SENDER_DOES_NOT_EXIST 207
#define ERR_EVM_WRONG_PARTIAL_STATE 208

#define ERR_EXCEPTION 301
