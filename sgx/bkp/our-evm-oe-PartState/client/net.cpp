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

int Net::sendObj(TransferObject* transferObj)
{
    debug_print(string("Size of transferObj: ") + to_string(transferObj->size()));
    debug_print(string("transferObj: ") + eevm::to_hex_string(transferObj->serialize()));

    this->initConnection();

    // TODO return value
    send(this->sock, &(transferObj->serialize())[0], transferObj->size(), 0);

    debug_print("Msg sended");

    // TODO
    // this->disconnect();

    return 0;
}

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
