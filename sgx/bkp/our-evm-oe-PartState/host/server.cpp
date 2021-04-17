#include "server.h"

mutex mtx, mtx_thread;
map<pthread_t, int> glob_thread_map;

// /**
//  * @brief      ukonci spojenie a vlakno
//  *
//  * @param[in]  connectfd  file descriptor soketu
//  * @param[in]  lock       bol nastaveny zamok
//  */
// void kill_thread(int fd, bool lock)
// {
//     debug_print("!!!!! killThread");
//     close(fd);
//     if (lock) {
//         mtx.unlock();
//     }
//     glob_thread_map.erase(pthread_self());
//     pthread_exit((void*)0);
// }


// /**
//  * @brief      Prijme spravu
//  *
//  * @param[in]  connectfd  Cislo file descriptora pre soket
//  * @param[in]  lock       bol nastaveny zamok
//  *
//  * @return     Prijata sprava
//  */
// string rcv_msg(int connectfd, bool lock)
// {
//     int n;
//     char buf[BUFSIZE];
//     bzero(buf, BUFSIZE);
//     string rcv_buf = "";

//     fd_set set;
//     FD_ZERO(&set);                      // vynuluje set
//     FD_SET(connectfd, &set);            // prida do setu sledonavy file descriptor
//     struct timeval timeout = {600, 0};  // nastavi casovac

//     int rv = select(connectfd + 1, &set, NULL, NULL, &timeout);
//     if (rv == -1) {
//         // error selektu
//         fprintf(stderr, "ERROR select in file descriptor %d\n", connectfd);
//     } else if (rv == 0) {
//         // casovac na citanie
//         kill_thread(connectfd, lock);
//     } else {
//         while ((n = read(connectfd, buf, BUFSIZE)) > 0) {
//             rcv_buf += string(buf, n);
//             if (rcv_buf.find("\r\n") != string::npos) {
//                 break;
//             }
//         }
//         if (n == 0) {
//             kill_thread(connectfd, lock);
//         }
//     }
//     return rcv_buf;
// }

// size_t recv_msg(int connectfd, unsigned char** recvData)
// {
//     int n;
//     unsigned char buf[BUFSIZE];
//     bzero(buf, BUFSIZE);
//     size_t len = 0;

//     while ((n = read(connectfd, buf, BUFSIZE)) > 0) {
//         std::cout << "rec_msg len: "<< len << std::endl;
//         *recvData = (unsigned char*)realloc(*recvData, len + n);
//         memcpy(*recvData + len, buf, n);
//         len += n;
//     }

//     return len;
// }

size_t recv_msg(int connectfd, unsigned char** recvData)
{
    int n;
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    size_t len = 0;
    fd_set set;
    FD_ZERO(&set);                     // vynuluje set
    FD_SET(connectfd, &set);           // prida do setu sledonavy file descriptor
    struct timeval timeout = {10, 0};  // nastavi casovac

    int rv = select(connectfd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1) {
        error_print("select");
    } else if (rv == 0) {
        info_print("timeout");
    } else {
        while ((n = read(connectfd, buf, BUFSIZE)) > 0) {
            *recvData = (unsigned char*)realloc(*recvData, len + n);
            memcpy(*recvData + len, buf, n);
            len += n;
        }
    }

    return len;
}

void* clientHandling(void* _op)
{
    // check filedescriptor for thread - mutex wait for write to global map of filedescriptors
    mtx_thread.lock();
    int connectfd = glob_thread_map.find(pthread_self())->second;
    mtx_thread.unlock();

    printf("\n");
    debug_print(string("Client's thread created"));

    // thread argument
    aql::Operator* op = (aql::Operator*)_op;

    unsigned char* recv_data = (unsigned char*)malloc(0);
    size_t recv_data_size = recv_msg(connectfd, &recv_data);

    debug_print(string("Received msg size: ") + to_string(recv_data_size));
    debug_print(string("Received msg: ") + to_hex_str(recv_data, recv_data_size));

    // get command from message
    uint8_t cmd;
    std::memcpy(&cmd, recv_data, sizeof(uint8_t));
    debug_print(string("Received cmd: ") + to_hex_str(&cmd, sizeof(uint8_t)));

    unsigned char* data = recv_data + sizeof(uint8_t);

    try {
        // Based on cmd, do something
        switch (cmd) {
            case TransferCommand::reg:
                info_print(string("Recieve registration command"));
                if (recv_data_size != sizeof(uint8_t) + ECC_PK_SIZE) {
                    throw std::length_error("invalid size of receive data");
                }
                registerNewClient(op, data);
                break;

            case TransferCommand::tx:
                info_print(string("Recieve transaction command"));
                transaction(op, data, recv_data_size - sizeof(uint8_t));
                break;

            case TransferCommand::getIomcAddresses: {
                info_print(string("Recieve getIomcAddresses command"));

                std::vector<uint8_t> sendVec = std::vector<uint8_t>(2 * sizeof(Address));

                uint8_t addr[ADDRESS_SIZE];

                intx::be::unsafe::store((uint8_t*)&addr, op->m_ledger.iomc.sendAddr);
                memcpy(sendVec.data(), addr, ADDRESS_SIZE);
                intx::be::unsafe::store((uint8_t*)&addr, op->m_ledger.iomc.recvAddr);
                memcpy(sendVec.data() + sizeof(Address), addr, ADDRESS_SIZE);

                // send response
                auto retVal = send(connectfd, &sendVec[0], sendVec.size(), 0);
                if (retVal < 0 || retVal != (signed)sendVec.size()) {
                    debug_print("Unsuccessfully sended message");
                }
                break;
            }

            default:
                error_print(string("Invalid command"));
        }
    } catch (const std::exception& e) {
        error_print(e.what());
    }

    free(recv_data);
    close(connectfd);

    glob_thread_map.erase(pthread_self());
    pthread_exit((void*)0);
}

void registerNewClient(aql::Operator* _op, unsigned char* _PK)
{
    auto operAccnt = _op->getAccount(_op->getOperAddr()).acc;  // already deployed  O's account
    auto newAddr = eevm::from_big_endian(_PK, PB_ADDR_SIZE);   // extract address from public key

    // check if is aready registred
    auto it = _op->m_clients_accounts.find(newAddr);
    if (it != _op->m_clients_accounts.end()) {
        error_print("Client already registred");
    } else {
        auto* tx = _op->m_ledger.createNewAccountTX(_op->PK_O, _op->SK_O, newAddr, 9, operAccnt.get_nonce());

        if (RET_SUCCESS != _op->dispatcher->addToDispatch(tx)) {
            throw std::logic_error("error when dispatching registration TX");
        }

        _op->m_clients_accounts[newAddr] = _PK;
    }
}

void transaction(aql::Operator* _op, unsigned char* _data, size_t _dataSize)
{
    auto tx = new eevm::PersistantTransaction(_data, _dataSize);

    if (RET_SUCCESS != _op->dispatcher->addToDispatch(tx)) {
        throw std::logic_error("error when dispatching TX");
    }
}

void* server(void* _op)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error_print("socket");
        pthread_exit((void*)ERR_SOCK);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        error_print("setsockopt(SO_REUSEADDR) failed");
        pthread_exit((void*)ERR_SOCK);
    }

    if (fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        error_print("fcntl(O_NONBLOCK) failed");
        pthread_exit((void*)ERR_SOCK);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address)) < 0) {
        error_print("port is not available");
        pthread_exit((void*)ERR_SOCK);
    }

    if (listen(server_fd, QUEUE) < 0) {
        error_print("listen");
        pthread_exit((void*)ERR_SOCK);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    fd_set readset, tempset;
    int maxfd;
    int result, len, connect_fd;
    sockaddr_in addr;

    FD_ZERO(&readset);
    FD_SET(server_fd, &readset);
    maxfd = server_fd;

    pthread_t thread_id;
    string tmp_string;

    do {
        memcpy(&tempset, &readset, sizeof(tempset));

        // Select pre neblokujuci port
        result = select(maxfd + 1, &tempset, NULL, NULL, NULL);

        if (result < 0 && errno != EINTR) {
            error_print(fmt::format("Error in select(): {}", strerror(errno)));
        } else if (result > 0) {
            if (FD_ISSET(server_fd, &tempset)) {
                len = sizeof(addr);
                connect_fd = accept(server_fd, (struct sockaddr*)&addr, (unsigned*)&len);
                if (connect_fd < 0) {
                    error_print(fmt::format("Error in accept(): {}", strerror(errno)));
                } else {
                    // nastavenie neblokujuceho soketu
                    int status = fcntl(connect_fd, F_SETFL, fcntl(connect_fd, F_GETFL, 0) | O_NONBLOCK);
                    if (status == -1) {
                        error_print("setsockopt(SO_REUSEADDR) failed");
                        close(connect_fd);
                        continue;
                    }

                    // vytvorenie vlakna + naplnenie struktury s informaciami o vlakne a deskriptorom soketu
                    mtx_thread.lock();
                    if (pthread_create(&thread_id, NULL, clientHandling, (aql::Operator*)_op) < 0) {
                        error_print(fmt::format("could not create thread: {}", strerror(errno)));
                        mtx_thread.unlock();
                    } else {
                        glob_thread_map.insert(pair<pthread_t, int>(thread_id, connect_fd));
                        mtx_thread.unlock();
                    }
                }
                FD_CLR(server_fd, &tempset);
            }
        }

    } while (1);

    pthread_exit((void*)RET_SUCCESS);
}
