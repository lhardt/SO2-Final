#pragma once
#include "ClientManager.hpp"
#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <condition_variable>

class ClientManager;

class Device {
private:
  std::atomic<bool> stop_requested; // Flag para controlar a execução das threads
  std::atomic<bool> send_push;
  NetworkManager *push_manager;          // socket para lidar com push para os outros dispositivos
  NetworkManager *command_manager;       // socket que recebe comando do cliente e arquivos
  NetworkManager *file_watcher_receiver; // socket que recebe quando um arquivo é alterado
  std::mutex push_lock;
  std::mutex push_mutex;
  std::condition_variable push_cv;

  ClientManager *client_manager; // cliente que possui o dispositivo
  std::thread *command_thread;
  std::thread *push_thread;
  std::thread *file_watcher_thread;
  FileManager *file_manager;

  int push_type;
  std::string push_command;

  void commandThread();
  void pushThread();
  void fileWatcherThread();
  void sendFileToClient(std::string file_path, NetworkManager *choosen_manager);

public:
  Device(int command_socket_fd, ClientManager *client_manager,
         FileManager *file_manager);
  void start();
  void stop();
  bool isStopRequested();
  void sendFileTo(std::string file_path);  // no socket de comando
  void sendPushTo(int type, std::string &file_path); // no socket de push
  void buildFile(std::string &file_name);
  ~Device();
};
