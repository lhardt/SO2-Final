#pragma once
#include "../Utils/State.hpp"
#include "ClientManager.hpp"
#include "ElectionManager.hpp"
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#define PORT 4000
using namespace std;
class ClientManager;
class ElectionManager;

class Server {
public:
  Server(State state, int running_port = PORT);
  Server(State state, int running_port, std::string connect_ip, int connect_port);
  void run();
  std::vector<std::string> getBackupPeers();
  std::vector<std::string> getClients();
  std::string getLocalIP();
  int getPort();
  void sendPacketToPeer(std::string peer, std::string command);

private:
  int port;
  std::string ip;
  int main_socket_fd;
  std::mutex clients_mutex;
  std::mutex peer_mutex;
  std::vector<NetworkManager *> peer_connections;
  NetworkManager *leader_connection;
  State state = LEADER;
  ElectionManager *electionManager = nullptr;

  vector<ClientManager *> clients;

  ClientManager *clientExists(std::string client_username);
  ClientManager *createNewManager(State state, std::string username);
  ClientManager *createNewBackupClientManager(std::string username, NetworkManager *push_receiver);
  void deliverToManager(ClientManager *manager, int socket);
  void createMainSocket();
  void handlePeerThread(NetworkManager *peer_manager);
};
