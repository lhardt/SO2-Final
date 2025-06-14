#pragma once
#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include "Device.hpp"
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#define MAX_DEVICES 2

using namespace std;
class Device;
class ClientManager {

public:
  ClientManager(string username);
  string getUsername();
  void handle_new_connection(int socket);
  void handle_new_push(string command, Device *caller);
  void removeDevice(Device *device);
  std::string getIp();
  int getPort();

private:
  vector<Device *> devices;
  FileManager *file_manager;
  NetworkManager *network_manager;
  int max_devices;
  string username;
  std::mutex device_mutex; // Mutex para proteger o acesso à lista de dispositivos
};
