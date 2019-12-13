#pragma once

#include "secp256k1.h"
#include <stdint.h>

#define SIG_SIZE_PB 64

// note that only lower 160 bits are used - but this enables compatibility with eEVM and Ethereum
#define ADDRESS_SIZE_PB 32
#define EVM_WORD_SIZE 32
#define HASH_SIZE 32

// underlying elementary data types

//////////////////////////// ENCLAVE ///////////////////////////////////

typedef struct {
    char* data; // is is dynamic array, so (de)-marshaling  needs to be handled manually
} ErrTx_T;

typedef struct {
    unsigned char SK_PB[HASH_SIZE]; // private key
    secp256k1_pubkey PK_PB;         // public key (i.e., unsigned char [64])
} KeyPairPB_T;

typedef struct {
    ErrTx_T** items;    // for performance reasons maybe this could be a pointer denoting a dynamic array
    unsigned int count; // the number of err TXs currently cached
} ErrTxsCache_T;

// the sealed storage

typedef struct {
    unsigned char hdrLast[HASH_SIZE];   // the last header created by E
    unsigned char logRootPB[HASH_SIZE]; // the last root of L flushed to PB
    unsigned int idCurrent;             // the current version of L (not flushed to PB)
    ErrTxsCache_T txsErrCache;          // the cache of erroneous Txs
    unsigned int diskInits;             // counts the number of how many times was enclave initialized from seald state stored at disk
} PublicSealedData_T;

typedef struct {
    KeyPairPB_T keypair;
} SecretSealedData_T;

typedef struct {
    PublicSealedData_T pub;
    SecretSealedData_T sec;
} EvmState_T;

// TX object should be constructed only from elementary C types
typedef struct {
    const char origin[ADDRESS_SIZE_PB];

    const uint64_t value; // call_value
    const unsigned char** code;

    const uint64_t gas_price;
    const uint64_t gas_limit;

    unsigned char signature[SIG_SIZE_PB]; // computed over: origin, value, code, gas_price, gas_limit,
} PersistantTransaction_T;

//////////////////////////// HOST ///////////////////////////////////
