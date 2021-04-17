#include "net.h"


Net::Net(const char* _addr, uint16_t _port)
{
    addr = _addr;
    port = _port;
}

Net::~Net()
{
    disconnect();
}

/* ----------------------------------------------------------- */
/* -------------------- Public functions --------------------- */
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
    shutdown(this->sock, SHUT_WR);
    int iResult;
    char recvbuf[32];
    int recvbuflen = 32;
    do {
        iResult = recv(this->sock, recvbuf, recvbuflen, 0);
        // if (iResult > 0)
        //     printf("Bytes received: %d\n", iResult);
        // else if (iResult == 0)
        //     printf("Connection closed\n");
        // else
        //     printf("recv failed\n");

    } while (iResult > 0);

    return close(this->sock);
}

int Net::sendObj(TransferObject* transferObj)
{
    debug_print(string("Size of transferObj: ") + to_string(transferObj->size()));
    debug_print(string("transferObj: ") + eevm::to_hex_string(transferObj->serialize()));

    int ret;

    if ((ssize_t)transferObj->size() != send(this->sock, &(transferObj->serialize())[0], transferObj->size(), 0)) {
        error_print("message not sended");
        ret = ERR_SOCK;
    } else {
        debug_print("Message successfuly sended");
        ret = RET_SUCCESS;
    }
    return ret;
}

int Net::recvData(unsigned char* recvBuf, size_t bufSize)
{
    return recv(this->sock, recvBuf, bufSize, 0);
}
