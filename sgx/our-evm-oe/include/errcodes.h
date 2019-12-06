#ifndef ENCLAVEERR_H_
#define ENCLAVEERR_H_


/***************************************************
 * Enclave return codes
 ***************************************************/
#define ERR_PASSWORD_OUT_OF_RANGE 1
#define ERR_WALLET_ALREADY_EXISTS 2
#define ERR_CANNOT_SAVE_WALLET 3
#define ERR_CANNOT_LOAD_WALLET 4
#define ERR_WRONG_MASTER_PASSWORD 5
#define ERR_WALLET_FULL 6
#define ERR_ITEM_DOES_NOT_EXIST 7
#define ERR_ITEM_TOO_LONG 8
#define ERR_FAIL_SEAL 9
#define ERR_FAIL_UNSEAL 10



// EVM enclave return codes
#define RET_SUCCESS 0
#define ERR_RAND_FAILED 100
#define ERR_FAIL_SEAL_STATE 101
#define ERR_CANNOT_SAVE_EVM_STATE 102
#define ERR_STAT_FILE_INIT 103
#define ERR_LOAD_EVM_STATE 104
#define RET_SUCCESS_INIT_NEW_STATE 105
#define RET_SUCCESS_INIT_LOADED_STATE 106
#define ERR_KEYPAIR_GEN_FAILED 107

#endif // ENCLAVEERR_H_