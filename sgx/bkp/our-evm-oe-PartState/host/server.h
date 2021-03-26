#ifndef SERVER_H
#define SERVER_H

// C POSIX:
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

// C++ standart
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <csignal>
#include <fstream>

// Externy zdroj
#include "ledger/ledger-host.h"


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
} SendingCommand;

struct TransferObject {
    uint8_t cmd;
    unsigned char* data;
};

/* Navratove hodnoty */
enum ret_codes
{
	OK = 0,
	ERROR = 1,
	E_ARG = 2,
	E_SOCK
};

void* fsm(void*);
void* server(void*);

#endif
