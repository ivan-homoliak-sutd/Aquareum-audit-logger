#include <fstream>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <sys/stat.h>

#include "../host/utils.h"
#include "client.h"
#include "secp256k1.h"
#include "signing.h"

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
        // std::cout << command << std::endl;

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
            info_print(string("CMD: reg"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            this->sendPK(this->PK);

        } else if (0 == strncmp(command, "iomc", 4)) {
            info_print(string("CMD: iomc"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

        } else if (0 == strncmp(command, "t", 1)) {
            info_print(string("CMD: test"));

            uint tokenCnt;
            if (!correct_token_cnt(command_s, {1, 2}, &tokens, &tokenCnt))
                continue;

            this->test();

        } else if (0 == strncmp(command, "exit", 4)) {
            break;
        }
    }
}

/* ----------------------------------------------------------- */
/* -------------------- Private functions -------------------- */
/* ----------------------------------------------------------- */

int Client::sendPK(secp256k1_pubkey _PK)
{
    // TODO prepare data
    // SendingCommand cmd = reg;
    // printf("CMD value = %d", (uint8_t) cmd);
    std::vector<uint8_t> vectorPK = std::vector<uint8_t>(64);

    memcpy(vectorPK.data(), &_PK, 64);
    // info_print(string("vectorPK = ") + eevm::to_hex_string(vectorPK));


    SendingObject data{
        SendingCommand::reg,
        vectorPK};

    this->net->sendObj(&data);  // check return value
    return 0;
}

int Client::test()
{
    debug_print(string("!!!!TEST"));

    // create transaction

    eevm::Address sender = eevm::string_to_uint256("0x7be7d3fffc35b4bc3f0fc9566db2c9f57e225bfd");
    eevm::Address to = eevm::string_to_uint256("0x2cf147f294675cb33cf87c5269978aa51e072186");
    eevm::Code emptyFunc = {0u};
    uint64_t nonce = 6;
    uint64_t value = 2;


    std::cout << "sender: " << std::hex << eevm::address_to_hex_string(sender) << std::endl;


    auto tx = new eevm::PersistantTransaction(sender, to, nonce, value, emptyFunc);
    this->m_ecc.sign_data(tx->asDataForHash(), this->SK, tx->signature);

    SendingObject data{
        SendingCommand::tx,
        tx->asDataForHash()};

    this->net->sendObj(&data);  // check return value
    return 0;
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
    int16_t port = 8080;

    Client client = Client(addr, port);
    // client.connect();
    client.clientLoop();
}
