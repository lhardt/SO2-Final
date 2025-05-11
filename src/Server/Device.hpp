#pragma once
#include "NetworkManager.hpp"
#include <iostream>
#include <string>
#include <vector>

class Device {
private:
  NetworkManager
      *push_manager; // socket para lidar com push para os outros dispositivos
  NetworkManager
      *command_manager; // socket que recebe comando do cliente e arquivos
  NetworkManager
      *file_watcher_receiver; // socket que recebe quando um arquivo é alterado

public:
  Device(int command_socket_fd);
  ~Device();
  void commandThread();
  void pushThread();
  void fileWatcherThread();
};
