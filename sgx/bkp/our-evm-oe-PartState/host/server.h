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

// Project's files
#include "common.h"
#include "ledger/ledger-host.h"
#include "ledger/operator.h"
#include "utils.h"

#define BUFSIZE 32
#define QUEUE (2)

extern uint16_t port;

using namespace std;

typedef enum {
    reg = 1,
    tx,
    getIomcAddresses
} TransferCommand;

void* clientHandling(void*);
void* server(void* _op);
void registerNewClient(aql::Operator* _op, unsigned char* _PK);
void transaction(aql::Operator* _op, unsigned char* _data, size_t _dataSize);

#endif
