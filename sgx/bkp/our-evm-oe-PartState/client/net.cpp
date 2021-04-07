#include "net.h"


Net::Net(const char* _addr, uint16_t _port)
{
    addr = _addr;
    port = _port;
}

Net::~Net()
{
    // TODO clear communication

    close(sock);
}

/* ----------------------------------------------------------- */
/* -------------------- Public functions --------------------- */
/* ----------------------------------------------------------- */

int Net::sendObj(TransferObject* transferObj)
{
    debug_print(string("Size of transferObj: ") + to_string(transferObj->size()));
    debug_print(string("transferObj: ") + eevm::to_hex_string(transferObj->serialize()));

    if (this->initConnection() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    int ret;

    if ((ssize_t)transferObj->size() != send(this->sock, &(transferObj->serialize())[0], transferObj->size(), 0)) {
        error_print("message not sended");
        ret = ERR_SOCK;
    } else {
        debug_print("Message successfuly sended");
        ret = RET_SUCCESS;
    }

    // TODO
    // this->disconnect();

    return ret;
}

/* ----------------------------------------------------------- */
/* -------------------- Private functions -------------------- */
/* ----------------------------------------------------------- */

int Net::initConnection()
{
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_print("socket creation error");
        return ERR_SOCK;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        error_print("invalid address / address not supported");
        return ERR_SOCK;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error_print("connection failed");
        return ERR_SOCK;
    }
    return RET_SUCCESS;
}

int Net::disconnect()
{
    close(this->sock);
    debug_print("Disconnect");
    return RET_SUCCESS;
}
