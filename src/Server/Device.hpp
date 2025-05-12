#pragma once
#include "ClientManager.hpp"
#include "NetworkManager.hpp"
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

class ClientManager;

class Device {
private:
  std::atomic<bool>
      stop_requested; // Flag para controlar a execução das threads
  NetworkManager
      *push_manager; // socket para lidar com push para os outros dispositivos
  NetworkManager
      *command_manager; // socket que recebe comando do cliente e arquivos
  NetworkManager
      *file_watcher_receiver; // socket que recebe quando um arquivo é alterado

  ClientManager *client_manager; // cliente que possui o dispositivo
  std::thread *command_thread;
  std::thread *push_thread;
  std::thread *file_watcher_thread;

public:
  Device(int command_socket_fd, ClientManager *client_manager);
  ~Device();
  void commandThread();
  void pushThread();
  void fileWatcherThread();
  void start();
  void stop();
  bool isStopRequested();
};
