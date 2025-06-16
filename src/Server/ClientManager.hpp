#pragma once
#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/State.hpp"
#include "Device.hpp"
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#define MAX_DEVICES 2
class Server;

using namespace std;
class Device;
class ClientManager {

public:
  ClientManager(State State, string username);
  string getUsername();
  void handle_new_connection(int socket);
  void handle_new_push(string command, Device *caller);
  void removeDevice(Device *device);
  std::string getIp();
  int getPort();
  void add_new_backup(NetworkManager *peer_manager);
  void receivePushsOn(NetworkManager *network_manager);
  void setNetworkManager(NetworkManager *network_manager);

private:
  vector<Device *> devices;
  FileManager *file_manager;
  NetworkManager *network_manager;
  vector<NetworkManager *> backup_peers; // Lista de backups
  int max_devices;
  Server *server; // Referência ao servidor
  string username;
  std::mutex device_mutex; // Mutex para proteger o acesso à lista de dispositivos
  State state;
};
