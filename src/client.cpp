#include <iostream>
#include <cstdlib>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <netinet/in.h>
#include <unistd.h>
#include "logger.hpp"
#include "client.hpp"
#include "constants.hpp"

Client::Client(std::string _client_name, std::string _server_ip, std::string _server_port)
    : client_name(_client_name), server_ip(_server_ip), server_port(_server_port)
{
    this->io_thread = std::thread(g_handleIoThread, this);
    this->network_thread = std::thread(g_handleNetworkThread, this);
    this->file_thread = std::thread(g_handleFileThread, this);

}

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
    static constexpr char* sync_dir = "sync_dir";

    //CRIAÇÃO DO SOCKET TCP, CREIO QUE DÁ PRA DEIXAR EM UM MÉTODO PARA SER USADO EM TODAS AS THREADS
    //JÁ QUE CADA UMA VAI TER SEU PRÓPRIO SOCKET, TEM QUE VER O QUE DIFERENCIA UM SOCKET DO OUTRO
    //SÃO PORTAS DIFERENTES? OU CADA THREAD FAZ SEU PRÓPRIO sock = socket() COM OS MESMOS PARAMETROS?
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("Erro ao criar socket");
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(std::stoi(this->server_port));

    if (inet_pton(AF_INET, this->server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        log_error("File thread | Invalid IP address: %s", this->server_ip.c_str());
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("File thread | Failed to connect to server: %s:%d", this->server_ip.c_str(), this->server_port);
        close(sock);
        return;
    }

    int inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd < 0) {
        log_error("Could not initiate inotify");
        return;
    }
    int watchDescriptor = inotify_add_watch(inotifyFd, sync_dir, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (watchDescriptor < 0) {
        log_error("Could not initiate watchDescriptor");
        return;
    }

    size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
    char buffer[BUF_LEN];

    log_info("Monitoring file: %s", sync_dir);

    while (true) {
        int length = read(inotifyFd, buffer, BUF_LEN);
        if (length < 0) {
            std::chrono::milliseconds sleep_time{500};
            std::this_thread::sleep_for(sleep_time); // Evita busy waiting
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];

            if (event->len > 0) {
                std::string filepath = "./" + std::string(sync_dir) + "/" + event->name;

                if (event->mask & IN_CREATE) {
                    log_info("Arquivo adicionado: %s", filepath);
                    //FAZER FUNÇÃO QUE ENVIA ARQUIVO PARA SERVIDOR EM UMA connections.cpp (ou algo assim)
                    //uploadFile(sock, filepath)
                }
                if (event->mask & IN_MODIFY) {
                    log_info("Arquivo modificado: %s", filepath);
                    //FAZER FUNÇÃO QUE ENVIA ARQUIVO PARA SERVIDOR EM UMA connections.cpp (ou algo assim)
                    //uploadFile(sock, filepath)
                }
                if (event->mask & IN_DELETE) {
                    log_info("Arquivo removido: %s", filepath);
                    //FAZER FUNÇÃO QUE ENVIA NOME DO ARQUIVO A SER DELETADO EM UMA connections.cpp (ou algo assim)
                    //deleteFile(sock, event->name)
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }  
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
