#pragma once
#include "ClientManager.hpp"
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

using namespace std;

class Server {
public:
  Server();
  void run();

private:
  int port;
  int main_socket_fd;
  std::mutex clients_mutex;

  vector<ClientManager *> clients;

  ClientManager *clientExists(std::string client_username);
  void createNewManager(std::string username, int sock_file_descriptor);
  void deliverToManager(ClientManager *manager, int socket);
};

class MediatorServer : public Server {
public:
  MediatorServer();

private:
  NetworkManager *push_retransmitter;
  NetworkManager *watcher_retransmitter;
  NetworkManager *command_retransmitter;
};
