#pragma once

#include <vector>
#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <string>

#include "Device.hpp"
#include "FileManager.hpp"

using namespace std;

class ClientManager{

    public:
        ClientManager();

    private:

        vector<Device*> devices;
        FileManager* file_manager;
        int max_devices;
        string username;

        
        void handle_new_connection(int socket);
        string getUsername();
        void handle_new_push(string file_path);
        

}