#pragma once
#include "ClientManager.hpp"
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#define PORT 4000
using namespace std;

enum ServerState { LEADER, BACKUP };

class Server {
public:
  Server(ServerState state, int running_port = PORT);
  Server(ServerState state, int running_port, std::string connect_ip,
         int connect_port);
  void run();

private:
  int port;
  int main_socket_fd;
  std::mutex clients_mutex;
  std::vector<NetworkManager *> peer_connections;
  NetworkManager *leader_connection;
  ServerState state = LEADER;

  vector<ClientManager *> clients;

  ClientManager *clientExists(std::string client_username);
  void createNewManager(std::string username, int sock_file_descriptor);
  void deliverToManager(ClientManager *manager, int socket);
  void createMainSocket();
};
