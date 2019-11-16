#ifndef WALLET_H_
#define WALLET_H_

#define MAX_ITEMS 100
#define MAX_ITEM_SIZE 100



// underlying elementary data types

struct ErrTx {
	char * data; // is is dynamic array, so (de)-marshaling  needs to be handled manually
};
typedef struct ErrTx ErrTx_T;

struct ErrTxsCache {
	ErrTx_T ** items; // for performance reasons maybe this could be a pointer denoting a dynamic array
	unsigned int count; // the number of err TXs currnetly cached
};
typedef struct ErrTxsCache ErrTxsCache_T;

struct KeyPairPB {
	unsigned char SK_PB[32] ;
	unsigned char PK_PB[32] ;
};
typedef struct KeyPairPB KeyPairPB_T;


// the sealed storage

struct PublicSealedData {
	unsigned char hdrLast[32]; // the last header created by E
	unsigned char logRootPB[32]; // the last root of L flushed to PB
	unsigned char idCurrent[32]; // the current version of L (not flushed to PB)
	ErrTxsCache_T txsErrCache; // the cache of erroneous Txs
};
typedef struct PublicSealedData PublicSealedData_T;

struct SecretSealedData {
	KeyPairPB_T keypair;
};
typedef struct SecretSealedData SecretSealedData_T;


struct SealedEvmState {
	PublicSealedData_T pub;
	SecretSealedData_T sec;
};
typedef struct SealedEvmState SealedEvmState_T;




/// wallet demo

// item
struct Item {
	char  title[MAX_ITEM_SIZE];
	char  username[MAX_ITEM_SIZE];
	char  password[MAX_ITEM_SIZE];
};
typedef struct Item item_t;

// wallet
struct Wallet {
	item_t items[MAX_ITEMS];
	size_t size;
	char master_password[MAX_ITEM_SIZE];
};
typedef struct Wallet wallet_t;



#endif // WALLET_H_