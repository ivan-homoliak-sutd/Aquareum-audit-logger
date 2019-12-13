// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <openenclave/enclave.h>
#include <string>

#include "common.h"

using namespace std;

#define SEAL_KEY_SIZE 16
#define CIPHER_BLOCK_SIZE 16
#define ENCRYPT_OPERATION true
#define DECRYPT_OPERATION false
#define HASH_VALUE_SIZE_IN_BYTES 32

#define STATE_SEAL_MSG "Sealed data of EVM state, i.e., type EvmState_T."
#define STATE_SEAL_MSG_LEN (size_t) strlen(STATE_SEAL_MSG)


class Sealing {
  private:
    mbedtls_entropy_context m_entropy_context;
    mbedtls_ctr_drbg_context m_ctr_drbg_contex;

    unsigned char* m_data; // holds plaintext of data to be sealed
    size_t m_data_size;
    sealed_data_t* m_sealed_data; // holds all info required to sealing/unsealing + encrypted data

  public:
    Sealing();
    ~Sealing();

    // two ecalls
    int seal_data(int seal_policy,
                  unsigned char* opt_mgs,
                  size_t opt_msg_len,
                  unsigned char* data,
                  size_t data_size,
                  sealed_data_t** sealed_data,
                  size_t* sealed_data_size);

    int unseal_data(sealed_data_t* sealed_data,
                    size_t sealed_data_size,
                    unsigned char** data,
                    size_t* data_size);

  private:
    void init_mbedtls(void);
    void cleanup_mbedtls(void);
    int generate_iv(unsigned char* iv, unsigned int ivLen);
    oe_result_t get_seal_key_and_prep_sealed_data(int seal_policy,
                                                  unsigned char* data,
                                                  size_t data_size,
                                                  unsigned char* opt_mgs,
                                                  size_t opt_msg_len,
                                                  uint8_t** seal_key,
                                                  size_t* seal_key_size);
    oe_result_t get_seal_key_by_policy(int policy,
                                       uint8_t** key_buf,
                                       size_t* key_buf_size,
                                       uint8_t** key_info,
                                       size_t* key_info_size);
    oe_result_t get_seal_key_by_keyinfo(uint8_t* key_info,
                                        size_t key_info_size,
                                        uint8_t** key_buf,
                                        size_t* key_buf_size);

    int cipher_data(bool encrypt,
                    unsigned char* input_data,
                    unsigned int input_data_size,
                    unsigned char* key,
                    unsigned int key_size,
                    unsigned char* iv,
                    unsigned char* output_data);
    int sign_sealed_data(sealed_data_t* sealed_data,
                         unsigned char* key,
                         unsigned int key_size,
                         uint8_t* signature);
    void dump_data(const char* name, unsigned char* data, size_t data_size);
};
