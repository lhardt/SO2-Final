#pragma once
#include "ClientManager.hpp"
#include "NetworkManager.hpp"
#include <iostream>
#include <string>
#include <vector>

class ClientManager;

class Device {
private:
  NetworkManager
      *push_manager; // socket para lidar com push para os outros dispositivos
  NetworkManager
      *command_manager; // socket que recebe comando do cliente e arquivos
  NetworkManager
      *file_watcher_receiver; // socket que recebe quando um arquivo é alterado
                              //
  ClientManager *client_manager; // cliente que possui o dispositivo

public:
  Device(int command_socket_fd, ClientManager *client_manager);
  ~Device();
  void commandThread();
  void pushThread();
  void fileWatcherThread();
};
