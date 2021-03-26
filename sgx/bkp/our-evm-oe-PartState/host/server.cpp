#include "server.h"
#include "utils.h"

#define PORT 8080

mutex mtx, mtx_thread;
map<int, string> glob_fd_map;
map<pthread_t, int> glob_thread_map;
ifstream infile;
FILE* out;
string path_to_binary;

/**
 * @brief      ukonci spojenie a vlakno
 *
 * @param[in]  connectfd  file descriptor soketu
 * @param[in]  lock       bol nastaveny zamok
 */
void kill_thread(int fd, bool lock)
{
    close(fd);
    glob_fd_map.erase(fd);
    if (lock) {
        mtx.unlock();
    }
    glob_thread_map.erase(pthread_self());
    pthread_exit((void*)0);
}

/**
 * @brief      Odosle spravu
 *
 * @param[in]  msg        Sprava
 * @param[in]  connectfd  Cislo file descriptora pre soket
 */
void send_msg(string msg, int connectfd)
{
    fd_set set;
    FD_ZERO(&set);            // vynuluje set
    FD_SET(connectfd, &set);  // prida do setu sledonavy file descriptor

    int done = 0;
    int length;
    int last_send = 0;

    // odosiela kym sa neodosle cela sprava
    do {
        msg = msg.substr(last_send);
        length = msg.length();

        int rv = select(connectfd + 1, NULL, &set, NULL, NULL);
        // ak sa moze odosielat
        if (rv > 0) {
            last_send = write(connectfd, msg.c_str(), length);
            if (last_send > 0) {  // ak sa nieco odoslalo
                done += last_send;
            }
        }

    } while (last_send < length);
}

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

void* fsm(void*)
{
    // zistenie filedescriptoru soketu pre vlakno
    // mutex caka, kym sa pri vytvarani vlakna zapise informacia do globalnej mapy
    mtx_thread.lock();
    int connectfd = glob_thread_map.find(pthread_self())->second;
    mtx_thread.unlock();

    info_print(string("Vlakno s klientom vytvorene"));

    string recv_msg = rcv_msg(connectfd, false);  // prijatie prvej spravy od klienta
    size_t recv_msg_size = recv_msg.size();
    cout << "size: " << recv_msg_size << endl;


    // unsigned char clientPK[ECC_PK_SIZE + 1];
    unsigned char* test = (unsigned char*)recv_msg.c_str();
    // TransferObject obj = (unsigned char *) (recv_msg.c_str());;

    // todo read first msg - PK and print it
    // read(connectfd, clientPK, sizeof(clientPK));

    info_print(string("buff = ") + to_hex_str(test, recv_msg_size));


    // string recv_msg = rcv_msg(connectfd, false);  // prijatie prvej spravy od klienta

    // while (true) {
    //     send_msg(recv_msg, connectfd);
    //     recv_msg = rcv_msg(connectfd, false);
    // }

    glob_thread_map.erase(pthread_self());
    pthread_exit((void*)0);
}

void* server(void*)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    // int addrlen = sizeof(address);
    // char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        fprintf(stderr, "ERROR: socket\n");
        // return E_SOCK;
        pthread_exit((void*)E_SOCK);
    }

    // pridanie fd do mapy vsetkych filedescriptorov
    glob_fd_map.insert(pair<int, string>(server_fd, ""));

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        fprintf(stderr, "ERROR: setsockopt(SO_REUSEADDR) failed\n");
        // return E_SOCK;
        pthread_exit((void*)E_SOCK);
    }

    if (fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        fprintf(stderr, "ERROR: fcntl(O_NONBLOCK) failed\n");
        // return E_SOCK;
        pthread_exit((void*)E_SOCK);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address)) < 0) {
        fprintf(stderr, "ERROR: port is not available\n");
        // return E_SOCK;
        pthread_exit((void*)E_SOCK);
    }

    if (listen(server_fd, QUEUE) < 0) {
        fprintf(stderr, "ERROR: listen\n");
        // return E_SOCK;
        pthread_exit((void*)E_SOCK);
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
            fprintf(stderr, "Error in select(): %s\n", strerror(errno));
        } else if (result > 0) {
            if (FD_ISSET(server_fd, &tempset)) {
                len = sizeof(addr);
                connect_fd = accept(server_fd, (struct sockaddr*)&addr, (unsigned*)&len);
                if (connect_fd < 0) {
                    fprintf(stderr, "Error in accept(): %s\n", strerror(errno));
                } else {
                    // nastavenie neblokujuceho soketu
                    int status = fcntl(connect_fd, F_SETFL, fcntl(connect_fd, F_GETFL, 0) | O_NONBLOCK);
                    if (status == -1) {
                        fprintf(stderr, "ERROR: setsockopt(SO_REUSEADDR) failed\n");
                        close(connect_fd);
                        continue;
                    }

                    // Generovanie a odoslanie uvodnej spravy
                    // tmp_string = generate_timestamp();
                    // glob_fd_map.insert(pair<int, string>(connect_fd, tmp_string));

                    // send_msg("+OK server ready" + tmp_string + "\r\n", connect_fd);

                    // vytvorenie vlakna + naplnenie struktury s informaciami o vlakne a deskriptorom soketu
                    mtx_thread.lock();
                    if (pthread_create(&thread_id, NULL, fsm, NULL) < 0) {
                        fprintf(stderr, "Error: could not create thread: %s\n", strerror(errno));
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

    // return OK;
    pthread_exit((void*)OK);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}
