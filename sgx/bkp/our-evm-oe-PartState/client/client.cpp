#include "client.h"

Client::Client(const char* _addr, uint16_t _port)
  : m_ecc()
{
    // If keys were generated and persisted before, just load them, otherwise generate new keys
    if (this->existsMyKeyFile()) {
        info_print(string("loading client's keys from file."));
        if (RET_SUCCESS != this->loadMyKeysFromFile()) {
            error_print(string("Error when loading client's keys."));
            return;
        }
    } else {
        info_print(string("generating new client's keys."));

        // 1) compute SK of client (under PB)
        int rc = RAND_priv_bytes((unsigned char*)&this->SK, ECC_SK_SIZE);
        if (rc != 1) {
            unsigned long err = ERR_get_error();
            error_print(string("RAND_pseudo_bytes failed, err = ") + std::to_string(err));
            return;
        }

        // 2) compute PK of client (under PB)
        if (1 != secp256k1_ec_pubkey_create(ECC::s_ctx, &this->PK, (const uint8_t*)&this->SK)) {
            error_print(string("secp256k1_ec_pubkey_create failed"));
            return;
        }
        this->persistMyKeys();
    }

    this->addr = eevm::from_big_endian(this->PK.data, PB_ADDR_SIZE);  // save address

    // Create Net object
    this->net = new Net(_addr, _port);

    info_print(string("Address = ") + eevm::address_to_hex_string(this->addr));
    info_print(string("SK = ") + to_hex_str(this->SK, ECC_SK_SIZE));
    info_print(string("PK = ") + to_hex_str((const unsigned char*)&this->PK, ECC_PK_SIZE));
}

Client::~Client()
{
    delete net;
}

/* ----------------------------------------------------------- */
/* -------------------- Public functions --------------------- */
/* ----------------------------------------------------------- */

void Client::clientLoop()
{
    char command[MAX_CMD_LEN];
    boost::tokenizer<separator>* tokens = NULL;  // tokens object for parsing command line

    // shell variables
    std::unordered_map<std::string, std::string> sh_vars;
    sh_vars["$?"] = "NULL";  // the last deployed contract

    while (true) {
        if (tokens)
            free(tokens);
        tokens = NULL;

        std::cin.getline(command, MAX_CMD_LEN);
        std::string command_s(expand_vars(command, sh_vars));

        if (0 == strncmp(command, "$", 1)) {
            boost::char_separator<char> sep("=");
            auto tmp = std::string(command);
            auto tokens = boost::tokenizer<separator>{tmp, sep};
            auto cnt = std::distance(tokens.begin(), tokens.end());
            auto it = tokens.begin();

            if (cnt == 1) {  // just display
                std::cout << fmt::format("\t {} = {} \n", *it, sh_vars[*it]);
                continue;
            } else if (cnt != 2) {
                std::cerr << "\t Incorrect arguments.\n";
                continue;
            }

            auto key = *it;
            it++;
            sh_vars[key] = expand_var(*it, sh_vars);  // do expansion also here
            std::cout << fmt::format("\t Setting  {} <= {} \n", key, sh_vars[key]);

        } else if (0 == strncmp(command, "reg", 3)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1}, &tokens, &tokenCnt))
                continue;

            this->registration(this->PK);

        } else if (0 == strncmp(command, "pay", 1)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {3}, &tokens, &tokenCnt))
                continue;

            // parse amount
            uint64_t amount;
            auto it = tokens->begin();
            try {
                std::advance(it, 1);
                amount = std::stoul(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // adjust destination
            eevm::Address dest;
            try {
                std::advance(it, 1);
                dest = eevm::string_to_uint256(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->pay(dest, amount);

        } else if (0 == strncmp(command, "call", 4)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {4, 5, 6, 7, 8, 9, 10}, &tokens, &tokenCnt))
                continue;

            // parse amount
            uint64_t amount;
            auto it = tokens->begin();
            try {
                std::advance(it, 1);
                amount = std::stoul(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // adjust destination
            eevm::Address dest;
            try {
                std::advance(it, 1);
                dest = eevm::string_to_uint256(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // parse function_hex_ptr
            Bytes function_hex_ptr;
            try {
                std::advance(it, 1);
                // if start with 0x
                if ((*it).compare(0, 2, "0x") == 0) {
                    function_hex_ptr = eevm::hex_str_to_bytes((*it).substr(2));
                } else {
                    function_hex_ptr = eevm::hex_str_to_bytes(*it);
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (++it; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->call(dest, amount, function_hex_ptr, parsedParams);

        }
        /* -------------------- IOMC -------------------- */
        else if (0 == strncmp(command, "iomc addr", 9)) {
            uint tokenCnt;
            if (!correct_token_cnt(command_s, {2}, &tokens, &tokenCnt))
                continue;

            this->getIomcAddresses();

        } else if (0 == strncmp(command, "iomc send-init", 14)) {
            info_print(string("CMD: iomc send-init"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {6}, &tokens, &tokenCnt))
                continue;
            auto it = tokens->begin();
            std::advance(it, 2);

            // parse amount
            uint64_t amount;
            try {
                amount = std::stoul(*it);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (++it; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->call(this->iomc.sendAddr, amount, this->iomc.endpoints[this->iomc.sendInit].second, parsedParams);

        } else if (0 == strncmp(command, "iomc send-commit", 16)) {
            info_print(string("CMD: iomc send-commit"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {4}, &tokens, &tokenCnt))
                continue;
            auto it = tokens->begin();
            std::advance(it, 2);

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->call(this->iomc.sendAddr, 0, this->iomc.endpoints[this->iomc.sendCommit].second, parsedParams);

        } else if (0 == strncmp(command, "iomc send-revert", 16)) {
            info_print(string("CMD: iomc send-revert"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {3}, &tokens, &tokenCnt))
                continue;
            auto it = tokens->begin();
            std::advance(it, 2);

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->call(this->iomc.sendAddr, 0, this->iomc.endpoints[this->iomc.sendRevert].second, parsedParams);

        } else if (0 == strncmp(command, "iomc recv-init", 14)) {
            info_print(string("CMD: iomc recv-init"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {6}, &tokens, &tokenCnt))
                continue;
            auto it = tokens->begin();
            std::advance(it, 2);

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->call(this->iomc.recvAddr, 0, this->iomc.endpoints[this->iomc.receiveInit].second, parsedParams);

        } else if (0 == strncmp(command, "iomc recv-claim", 15)) {
            info_print(string("CMD: iomc recv-claim"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {4, 5}, &tokens, &tokenCnt))
                continue;
            auto it = tokens->begin();
            std::advance(it, 2);

            // parse arguments
            std::vector<u256> parsedParams;
            try {
                for (; it != tokens->end(); ++it) {
                    parsedParams.push_back(string_to_uint256(*it));
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument\n";
                continue;
            }

            this->callAndNotSignAllParams(this->iomc.recvAddr, 0, this->iomc.endpoints[this->iomc.receiveClaim].second, parsedParams, 2);

        } else if (0 == strncmp(command, "exit", 4)) {
            break;
        } else {
            error_print("Unknown command");
        }
    }
}

/* ----------------------------------------------------------- */
/* -------------------- Private functions -------------------- */
/* ----------------------------------------------------------- */

int Client::registration(secp256k1_pubkey _PK)
{
    std::vector<uint8_t> vectorPK = std::vector<uint8_t>(ECC_PK_SIZE);

    memcpy(vectorPK.data(), &_PK, ECC_PK_SIZE);

    TransferObject data{
        TransferCommand::reg,
        vectorPK};

    if (this->net->initConnection() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    if (this->net->sendObj(&data) != RET_SUCCESS) {
        error_print("sendObj() != RET_SUCCESS");
        return ERR_SOCK;
    }

    this->net->disconnect();

    return RET_SUCCESS;
}

int Client::pay(eevm::Address _dest, uint64_t _amount)
{
    eevm::Code emptyFunc = {0u};

    auto tx = new eevm::PersistantTransaction(this->addr, _dest, ++(this->nonce), _amount, emptyFunc);

    this->m_ecc.sign_data(tx->asDataForHash(), this->SK, tx->signature);

    return this->sendTX(tx);
}

int Client::call(eevm::Address _dest, uint64_t _amount, Bytes function_hex_ptr, std::vector<u256> params)
{
    auto function_call = function_hex_ptr;  // copy vector

    // append all passed arguments to function call pointer
    for (auto& p : params) {
        append_arg(function_call, p);
    }

    auto tx = new eevm::PersistantTransaction(this->addr, _dest, ++(this->nonce), _amount, function_call);

    this->m_ecc.sign_data(tx->asDataForHash(), this->SK, tx->signature);

    return this->sendTX(tx);
}

int Client::callAndNotSignAllParams(eevm::Address _dest, uint64_t _amount, Bytes function_hex_ptr, std::vector<u256> params, uint8_t numberOfSignParams)
{
    // sign tx with onle first 2 params
    auto function_call = function_hex_ptr;  // copy vector

    // append only first 2 params to function call pointer
    for (uint8_t i = 0; i < numberOfSignParams && i < params.size(); i++) {
        auto& p = params[i];
        append_arg(function_call, p);
    }

    auto tx = new eevm::PersistantTransaction(this->addr, _dest, ++(this->nonce), _amount, function_call);

    this->m_ecc.sign_data(tx->asDataForHash(), this->SK, tx->signature);

    // add rest of params
    for (uint8_t i = numberOfSignParams; i < params.size(); i++) {
        auto& p = params[i];
        append_arg(tx->code, p);
    }

    return this->sendTX(tx);
}

int Client::sendTX(eevm::PersistantTransaction* tx)
{
    auto packedTx = tx->asDataForNetTransfer();

    TransferObject data{
        TransferCommand::tx,
        packedTx};

    if (this->net->initConnection() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    if (this->net->sendObj(&data) != RET_SUCCESS) {
        return ERR_SOCK;
    }

    if (this->net->disconnect() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    return RET_SUCCESS;
}

int Client::getIomcAddresses()
{
    std::vector<uint8_t> empty;

    TransferObject data{
        TransferCommand::getIomcAddresses,
        empty};

    if (this->net->initConnection() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    if (this->net->sendObj(&data) != RET_SUCCESS) {
        return ERR_SOCK;
    }

    // wait for response
    unsigned char recvBuf[2 * ADDRESS_SIZE];
    if (this->net->recvData(recvBuf, sizeof(recvBuf)) != 2 * ADDRESS_SIZE) {
        error_print("Invalid response");
        return ERR_SOCK;
    }

    // save response
    this->iomc.sendAddr = intx::be::unsafe::load<eevm::Address>((const uint8_t*)recvBuf);
    this->iomc.recvAddr = intx::be::unsafe::load<eevm::Address>((const uint8_t*)recvBuf + sizeof(uint256_t));

    info_print(string("sendAddr = ") + eevm::address_to_hex_string(this->iomc.sendAddr));
    info_print(string("recvAddr = ") + eevm::address_to_hex_string(this->iomc.recvAddr));

    if (this->net->disconnect() != RET_SUCCESS) {
        return ERR_SOCK;
    }

    return RET_SUCCESS;
}

/* ----------------------------------------------------------- */
/* --------------------- Copied functions -------------------- */
/* ----------------------------------------------------------- */
// ledger-host.cpp
void Client::append_arg(std::vector<uint8_t>& code, const uint256_t& arg)
{
    // ABI encode a function call with a uint256_t (or Address) argument.
    // ABI-encoding for more complicated types is more complicated.
    const auto pre_size = code.size();
    code.resize(pre_size + 32u);
    eevm::to_big_endian(arg, code.data() + pre_size);
}

int Client::loadMyKeysFromFile()
{
    ifstream file(FILE_CLIENTS_KEYS, ios::in | ios::binary);
    if (file.fail()) {
        return 1;
    }
    file.read((char*)this->SK, ECC_SK_SIZE);
    file.read((char*)this->PK.data, ECC_PK_SIZE);
    file.close();
    return 0;
}

bool Client::existsMyKeyFile()
{
    struct stat buffer;
    if (0 != stat(FILE_CLIENTS_KEYS, &buffer)) {
        return false;
    }
    return true;
}

int Client::persistMyKeys()
{
    ofstream file(FILE_CLIENTS_KEYS, ios::out | ios::binary);
    if (file.fail()) {
        return ERR_SAVING_OPER_KEYS;
    }
    file.write((const char*)this->SK, ECC_SK_SIZE);
    file.write((const char*)this->PK.data, ECC_PK_SIZE);
    file.close();
    return RET_SUCCESS;
}

/* ----------------------------------------------------------- */
/* -------------------------- Main --------------------------- */
/* ----------------------------------------------------------- */

int main(int argc, char* argv[])
{
    // TODO parse arg IP and port
    const char* addr = "127.0.0.1";
    int16_t port = 63290;

    Client client = Client(addr, port);
    client.clientLoop();
}
