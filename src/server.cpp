#include "server.hpp"
#include "logger.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>

Server::Server(int ipv4_port, int ipv6_port){
    this->ipv4_port = ipv4_port;
    this->ipv6_port = ipv6_port;

    log_info("Will try to open IPV4 socket on port %d", ipv4_port);
    ipv4_socket_fd = socket(AF_INET, SOCK_STREAM, 0);   

    if(ipv4_socket_fd < 0){
        int error = errno;
        log_error("Could not create socket! errno=%d ", error);
        std::exit(1);
    }

    // Make our lives easier by not having timed-wait state on sockets when the program is closed.
    int set_opt_to_one = 1;
    setsockopt(ipv4_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &set_opt_to_one, sizeof(set_opt_to_one));


    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( ipv4_port );

    int bind_result = bind(ipv4_socket_fd, (sockaddr*)&address, sizeof(address));

    if( bind_result < 0 ){
        int error = errno;
        log_error("Could not bind socket to port! errno=%d ", error);
        std::exit(1);
    }

    // See https://man7.org/linux/man-pages/man3/listen.3p.html
    int listen_result = listen(ipv4_socket_fd, 10);

    if( listen_result < 0 ){
        int error = errno;
        log_error("Could not listen to port! errno=%d ", error);
        std::exit(1);
    }

    while(true){
        log_info("Will wait to accept a new connection.");

        struct sockaddr_in created_connection;
        socklen_t created_connection_sz = 0;
        int new_socket_info = accept(ipv4_socket_fd, (sockaddr*)&created_connection, &created_connection_sz );

        log_info("Accepted a new connection %s.", inet_ntoa(created_connection.sin_addr));

    }
}