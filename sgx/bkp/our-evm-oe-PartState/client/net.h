#ifndef NET_H
#define NET_H

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "../host/utils.h"
#include "eEVM/util.h"
#include "secp256k1.h"

typedef enum {
    reg = 1,
    tx,
    getIomcAddresses
} TransferCommand;

struct TransferObject {
    uint8_t cmd;
    std::vector<uint8_t> data;

    std::vector<uint8_t> serialize()
    {
        std::vector<uint8_t> ret = std::vector<uint8_t>(sizeof(uint8_t) + this->data.size());

        memcpy(ret.data(), &(this->cmd), sizeof(uint8_t));
        memcpy(ret.data() + sizeof(uint8_t), this->data.data(), this->data.size());

        return ret;
    };

    size_t size()
    {
        return sizeof(this->cmd) + this->data.size();
    }
};


class Net {
private:
    int sock = 0;
    const char* addr;
    uint16_t port;


public:
    Net(const char* _addr, uint16_t _port);
    ~Net();

    int initConnection();
    int disconnect();
    
    int sendObj(TransferObject* sendingObj);
    int recvData(unsigned char* recvBuf, size_t bufSize);
};

#endif
