#include "server.h"

mutex mtx, mtx_thread;
map<pthread_t, int> glob_thread_map;

/**
 * @brief      ukonci spojenie a vlakno
 *
 * @param[in]  connectfd  file descriptor soketu
 * @param[in]  lock       bol nastaveny zamok
 */
void kill_thread(int fd, bool lock)
{
    close(fd);
    if (lock) {
        mtx.unlock();
    }
    glob_thread_map.erase(pthread_self());
    pthread_exit((void*)0);
}

// /**
//  * @brief      Odosle spravu
//  *
//  * @param[in]  msg        Sprava
//  * @param[in]  connectfd  Cislo file descriptora pre soket
//  */
// void send_msg(string msg, int connectfd)
// {
//     fd_set set;
//     FD_ZERO(&set);            // vynuluje set
//     FD_SET(connectfd, &set);  // prida do setu sledonavy file descriptor

//     int done = 0;
//     int length;
//     int last_send = 0;

//     // odosiela kym sa neodosle cela sprava
//     do {
//         msg = msg.substr(last_send);
//         length = msg.length();

//         int rv = select(connectfd + 1, NULL, &set, NULL, NULL);
//         // ak sa moze odosielat
//         if (rv > 0) {
//             last_send = write(connectfd, msg.c_str(), length);
//             if (last_send > 0) {  // ak sa nieco odoslalo
//                 done += last_send;
//             }
//         }

//     } while (last_send < length);
// }

/**
 * @brief      Prijme spravu
 *
 * @param[in]  connectfd  Cislo file descriptora pre soket
 * @param[in]  lock       bol nastaveny zamok
 *
 * @return     Prijata sprava
 */
string rcv_msg(int connectfd, bool lock)
{
    int n;
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    string rcv_buf = "";

    fd_set set;
    FD_ZERO(&set);                      // vynuluje set
    FD_SET(connectfd, &set);            // prida do setu sledonavy file descriptor
    struct timeval timeout = {600, 0};  // nastavi casovac

    int rv = select(connectfd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1) {
        // error selektu
        fprintf(stderr, "ERROR select in file descriptor %d\n", connectfd);
    } else if (rv == 0) {
        // casovac na citanie
        kill_thread(connectfd, lock);
    } else {
        while ((n = read(connectfd, buf, BUFSIZE)) > 0) {
            rcv_buf += string(buf, n);
            if (rcv_buf.find("\r\n") != string::npos) {
                break;
            }
        }
        if (n == 0) {
            kill_thread(connectfd, lock);
        }
    }
    return rcv_buf;
}

void* fsm(void* _op)
{
    // zistenie filedescriptoru soketu pre vlakno
    // mutex caka, kym sa pri vytvarani vlakna zapise informacia do globalnej mapy
    mtx_thread.lock();
    int connectfd = glob_thread_map.find(pthread_self())->second;
    mtx_thread.unlock();

    printf("\n");
    debug_print(string("Client's thread created"));

    // thread argument
    aql::Operator* op = (aql::Operator*)_op;

    string recv_msg = rcv_msg(connectfd, false);  // receive first msg from client
    size_t recv_msg_size = recv_msg.size();

    unsigned char* recv_data = (unsigned char*)recv_msg.c_str();

    debug_print(string("Received msg size: ") + to_string(recv_msg_size));
    debug_print(string("Received msg: ") + to_hex_str(recv_data, recv_msg_size));

    // get command from message
    uint8_t cmd;
    std::memcpy(&cmd, recv_data, sizeof(uint8_t));
    debug_print(string("Received cmd: ") + to_hex_str(&cmd, sizeof(uint8_t)));


    // get data from message
    unsigned char data[recv_msg_size - 1];
    std::memcpy(&data, recv_data + 1, recv_msg_size - 1);
    debug_print(string("Received data: ") + to_hex_str((const unsigned char*)&data, recv_msg_size - 1));

    try {
        // Based on cmd, do something
        switch (cmd) {
            case TransferCommand::reg:
                info_print(string("Recieve registration command"));
                registerNewClient(op, data);
                break;
            case TransferCommand::tx:
                info_print(string("Recieve transaction command"));
                transaction(op, data, recv_msg_size - 1);
                break;
            default:
                error_print(string("Invalid command"));
        }
        /* code */
    } catch (const std::exception& e) {
        error_print(e.what());
    }

    glob_thread_map.erase(pthread_self());
    pthread_exit((void*)0);
}

int registerNewClient(aql::Operator* _op, unsigned char* _PK)
{
    auto operAccnt = _op->getAccount(_op->getOperAddr()).acc;  // already deployed  O's account
    auto newAddr = eevm::from_big_endian(_PK, PB_ADDR_SIZE);   // extract address from public key

    // TODO check if is aready registred

    auto* tx = _op->m_ledger.createNewAccountTX(_op->PK_O, _op->SK_O, newAddr, 9, operAccnt.get_nonce());

    if (RET_SUCCESS != _op->dispatcher->addToDispatch(tx)) {
        throw std::logic_error("error when dispatching registration TX");
    }

    return 0;
}

int transaction(aql::Operator* _op, unsigned char* _data, size_t _dataSize)
{
    auto tx = new eevm::PersistantTransaction(_data, _dataSize);

    if (RET_SUCCESS != _op->dispatcher->addToDispatch(tx)) {
        throw std::logic_error("error when dispatching TX");
    }

    return 0;
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
                    if (pthread_create(&thread_id, NULL, fsm, (aql::Operator*)_op) < 0) {
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
