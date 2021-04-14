#pragma once

#include "eEVM/address.h"
#include "eEVM/util.h"

#include <string>
#include <utility>
#include <vector>

struct Iomc {
    eevm::Address sendAddr;
    eevm::Address recvAddr;

    const std::vector<std::pair<std::string, std::vector<uint8_t>>> endpoints{
        std::pair<std::string, std::vector<uint8_t>>(std::string("sendInitialize"), eevm::to_bytes("8400f826")),
        std::pair<std::string, std::vector<uint8_t>>(std::string("sendCommit"), eevm::to_bytes("635c97a8")),
        std::pair<std::string, std::vector<uint8_t>>(std::string("sendRevert"), eevm::to_bytes("a505f77a")),
        std::pair<std::string, std::vector<uint8_t>>(std::string("receiveInitialize"), eevm::to_bytes("3e7c913a")),
        std::pair<std::string, std::vector<uint8_t>>(std::string("receiveClaim"), eevm::to_bytes("2f514577")),
        std::pair<std::string, std::vector<uint8_t>>(std::string("fund"), eevm::to_bytes("b60d4288"))};

    enum functionsId {
        sendInit,
        sendCommit,
        sendRevert,
        receiveInit,
        receiveClaim,
        receiveFund,
    };
};
