#include "net.h"
#include "../host/utils.h"
#include "eEVM/util.h"


#include <fmt/format_header_only.h>
#include <string>
#include <vector>

Net::Net(const char* _addr, uint16_t _port)
{
    addr = _addr;
    port = _port;
}

Net::~Net()
{
    // TODO clear communication

    close(sock);
    debug_print("NET desdtructor");
}

/* ----------------------------------------------------------- */
/* -------------------- Public functions --------------------- */
/* ----------------------------------------------------------- */

int Net::sendObj(SendingObject* sendingObj)
{
    // printf("CMD: %d, ", sendingObj->cmd);
    info_print(string("serialize = ") + eevm::to_hex_string(sendingObj->serialize()));
    std::cout << "size " << sendingObj->size() << std::endl;


    this->initConnection();

    // TODO return value
    // send(this->sock, (unsigned char*) sendingObj, sizeof(*sendingObj), 0);
    send(this->sock, &(sendingObj->serialize())[0], sendingObj->size(), 0);

    debug_print("Msg sended");

    // this->disconnect();

    return 0;
}

// // TODO delete
// int Net::sendMsg()
// {
//     char buffer[1024] = {0};
//     char const* hello = "Hello from client";
//     send(sock, hello, strlen(hello), 0);
//     printf("Hello message sent\n");
//     read(sock, buffer, 1024);
//     printf("%s\n", buffer);
//     return 0;
// }

// // TODO delete
// int Net::sendPK(unsigned char* PK)
// {
//     send(sock, PK, ECC_PK_SIZE, 0);

//     debug_print("after send()");
//     return 0;
// }

/* ----------------------------------------------------------- */
/* -------------------- Private functions -------------------- */
/* ----------------------------------------------------------- */

int Net::initConnection()
{
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        fprintf(stderr, "Error: socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: invalid address / address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error: connection failed\n");
        return -1;
    }
    return 0;
}

int Net::disconnect()
{
    close(this->sock);
    debug_print("Disconnect");
    return 0;
}
