#pragma once

#include "secp256k1.h"
#include <stdint.h>
#include <unordered_map>

#define SIG_SIZE_PB 64

// note that only lower 160 bits are used - but this enables compatibility with eEVM and Ethereum
#define ADDRESS_SIZE_PB 32
#define EVM_WORD_SIZE 32
#define HASH_SIZE 32

// underlying elementary data types

//////////////////////////// ENCLAVE ///////////////////////////////////

typedef struct {
    char* data;  // is is dynamic array, so (de)-marshaling  needs to be handled manually
} ErrTx_T;

typedef struct {
    unsigned char SK_PB[HASH_SIZE];  // private key
    secp256k1_pubkey PK_PB;          // public key (i.e., unsigned char [64])
} KeyPairPB_T;

typedef struct {
    ErrTx_T** items;     // for performance reasons maybe this could be a pointer denoting a dynamic array
    unsigned int count;  // the number of err TXs currently cached
} ErrTxsCache_T;

typedef struct {
    unsigned char hdrLast[HASH_SIZE];    // the last header created by E
    unsigned char logRootPB[HASH_SIZE];  // the last root of L flushed to PB
    unsigned int idCurrent;              // the current version of L (not flushed to PB)
    ErrTxsCache_T txsErrCache;           // the cache of erroneous Txs
    unsigned int diskInits;              // counts the number of how many times was enclave initialized from seald state stored at disk
} PublicSealedData_T;

typedef struct {
    KeyPairPB_T keypair;
} SecretSealedData_T;

typedef struct {  // used as persested object in the sealed storage
    PublicSealedData_T pub;
    SecretSealedData_T sec;
} EvmState_T;

// TX object should be constructed only from elementary C types (this should match TX defined in eEVM)
typedef struct {
    uint8_t origin[ADDRESS_SIZE_PB];  // sender of the TX
    uint8_t to[ADDRESS_SIZE_PB];      // the recepient of the TX
    uint64_t nonce;                   // the number of TXs send by the sender of this TX (i.e., protection against replay attacks)

    uint64_t value;  // call_value

    uint64_t gas_price;
    uint64_t gas_limit;

    uint8_t signature[SIG_SIZE_PB];  // computed over: origin, value, code, gas_price, gas_limit,

    // unsigned char** code; // this will be outside of this struct

} PersistantTxProxy_T;

// typedef struct {

// } FullGlobalStat_T;

//////////////////////////// HOST ///////////////////////////////////
