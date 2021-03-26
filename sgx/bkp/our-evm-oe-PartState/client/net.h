#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "eEVM/util.h"
#include "secp256k1.h"


typedef enum {
    reg = 1,
    tx
} SendingCommand;

struct SendingObject {
    uint8_t cmd;
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


class Net {
private:
    int sock = 0;
    const char* addr;
    uint16_t port;

    int initConnection();
    int disconnect();

public:
    Net(const char* _addr, uint16_t _port);
    ~Net();
    int sendObj(SendingObject* sendingObj);

    // TODO delete
    // int sendMsg();
    // int sendPK(unsigned char* number);
};
