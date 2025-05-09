#ifndef SERVER_H
#define SERVER_H

#include <thread>


class Server {
public:
    Server(int ipv4_port, int ipv6_port);

    std::thread socket_accept_thread;
    int ipv4_socket_fd;
    int ipv6_socket_fd;
    int ipv4_port;
    int ipv6_port;
};


#endif /* SERVER_H */