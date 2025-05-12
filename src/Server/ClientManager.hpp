#pragma once
#include "NetworkManager.hpp"
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#include "Device.hpp"
#include "FileManager.hpp"

#define MAX_DEVICES 2

using namespace std;
class Device;
class ClientManager {

public:
  ClientManager(string username);
  string getUsername();
  void handle_new_connection(int socket);
  void removeDevice(Device *device);

private:
  vector<Device *> devices;
  FileManager *file_manager;
  NetworkManager *network_manager;
  int max_devices;
  string username;
  std::mutex
      device_mutex; // Mutex para proteger o acesso à lista de dispositivos

  void handle_new_push(string file_path, Device *caller);
};
