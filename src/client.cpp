#include <iostream>
#include <cstdlib>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "logger.hpp"
#include "client.hpp"
#include "constants.hpp"

void g_handleIoThread(Client * client){
    log_assert(client != NULL, "Null client!");
    client->handleIoThread();
}
void g_handleFileThread(Client * client){
    log_assert(client != NULL, "Null client!");
    client->handleFileThread();
}
void g_handleNetworkThread(Client * client){
    log_assert(client != NULL, "Null client!");
    client->handleNetworkThread();
}


void Client::handleIoThread(){
    log_info("Started IO Thread with ID %d ", std::this_thread::get_id());
    // use std::getline and add to a queue of commands?
}
void Client::handleFileThread(){
    log_info("Started File Thread with ID %d ", std::this_thread::get_id());
}
void Client::handleNetworkThread(){
    log_info("Started Network Thread with ID %d ", std::this_thread::get_id());
    
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0){
        int error = errno;
        log_error("Could not create socket! errno=%d ", error);
        std::exit(1);
    }


    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(std::atoi(this->server_port.c_str()));

    int conversion_result = inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
    if( conversion_result <= 0 ){
        // https://man7.org/linux/man-pages/man3/inet_pton.3.html
        int error = errno;
        log_error("Could not convert server ip! result=%d errno=%d ", conversion_result, error);
        std::exit(1);
    }

    while(true){
        log_info("Will sleep to give time for server to catch on!");
        std::chrono::milliseconds sleep_time{500};
        std::this_thread::sleep_for(sleep_time);

        int connect_status = connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));
        int error = errno;
        if( connect_status >= 0 ){
            break;
        }
        log_warn("Could not CONNECT! result=%d errno=%d ", connect_status, error);
    }

    log_info("Connected successfully!");

    // check a queue of commands?
}


Client::Client(std::string _client_name, std::string _server_ip, std::string _server_port) :
        client_name(_client_name), server_ip(_server_ip), server_port(_server_port){

    // TODO: log parameters

    this->io_thread = std::thread(g_handleIoThread, this);
    this->network_thread = std::thread(g_handleNetworkThread, this);
    this->file_thread = std::thread(g_handleFileThread, this);

    // If the main thread finishes, all the other threads are prematurely finished.
    while(true){	
        log_info("The main thread, like the Great Dark Beyond, lies asleep");
        std::chrono::milliseconds sleep_time{5000};
        std::this_thread::sleep_for(sleep_time);
    }
}
