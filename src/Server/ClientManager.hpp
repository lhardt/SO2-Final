#pragma once
#include "NetworkManager.hpp"
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#include "Device.hpp"
#include "FileManager.hpp"

#define MAX_DEVICES 2

using namespace std;

class ClientManager {

public:
  ClientManager(string username);
  string getUsername();
  void handle_new_connection(int socket);
  void removeDevice(Device *device);

private:
  vector<Device *> devices;
  FileManager *file_manager;
  int max_devices;
  string username;

  void handle_new_push(string file_path);
};
