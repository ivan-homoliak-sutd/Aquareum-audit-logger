#pragma once

#include <boost/tokenizer.hpp>
#include <fmt/format_header_only.h>
#include <iostream>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_map>

#include "../host/utils.h"
#include "data_types.h"
#include "eEVM/transaction.h"
#include "eEVM/util.h"
#include "helper.h"
#include "net.h"
#include "signing.h"


#define FILE_CLIENTS_KEYS "./client/data/clients-keys.txt"

// #include "../host/ledger/ledger-host.h"
// #include "common.h"

// #include "../common/signing-PB/signing.h"

#define MAX_CMD_LEN 256

#define ECC_SK_SIZE 32
#define ECC_PK_SIZE 64

class Client {
private:
    Net* net;

    secp256k1_pubkey PK;      // PK of client (under Sigma_PB)
    uint8_t SK[ECC_SK_SIZE];  // SK of client (under Sigma_PB)
    eevm::Address addr;

    int sendPK(secp256k1_pubkey PK);
    int test();

    int persistMyKeys();
    bool existsMyKeyFile();
    int loadMyKeysFromFile();

public:
    ECC m_ecc;
    
    Client(const char* _addr, uint16_t _port);
    ~Client();
    void clientLoop();
};