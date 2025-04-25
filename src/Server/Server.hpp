#pragma once
#include <vector>
#include "../ClientManager/ClientManager.hpp"
#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <string>

using namespace std;

class Server{
    public:
        Server();

    private:
        int port;
        int main_socket_fd;

        vector<ClientManager*> clients;
        
        ClientManager* clientExists(std::string client_username);
        void createNewManager(std::string username,int sock_file_descriptor);
        void deliverToManager(ClientManager* manager,int socket)
        void run();
}