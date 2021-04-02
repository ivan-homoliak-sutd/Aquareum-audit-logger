#ifndef SERVER_H
#define SERVER_H

// C POSIX:
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// C++ standart
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

// Externy zdroj
#include "ledger/ledger-host.h"
#include "ledger/operator.h"


#define DIRECTORY 2
#define MYFILE 1
#define WRONG_PATH -1

#define AUX_FILE "reset.txt"
#define SIZE_FILE "size_file.txt"

#define BUFSIZE 1
#define QUEUE (2)

using namespace std;

typedef enum {
    reg = 1,
    tx
} TransferCommand;

struct TransferObject {
    TransferCommand cmd;
    std::vector<uint8_t> data;

    std::vector<uint8_t> serialize()
    {
        std::vector<uint8_t> ret = std::vector<uint8_t>(sizeof(uint8_t) + 64);

        memcpy(ret.data(), &(this->cmd), sizeof(uint8_t));
        memcpy(ret.data() + sizeof(uint8_t), this->data.data(), 64);

        return ret;
    };

    size_t size()
    {
        return sizeof(this->cmd) + this->data.size();
    }
};

/* Navratove hodnoty */
enum ret_codes {
    OK = 0,
    ERROR = 1,
    E_ARG = 2,
    E_SOCK
};

void* fsm(void*);
void* server(void* _op);
int registerNewClient(aql::Operator* _op, unsigned char* _PK);
int transaction(aql::Operator* _op, unsigned char* _data, size_t _dataSize);



#endif
