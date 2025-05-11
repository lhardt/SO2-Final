#include "Device.hpp"
#include <cstring>
#include <string>

Device::Device(int command_socket_fd) {
  // Inicializa os sockets
  command_manager = new NetworkManager(command_socket_fd);

  push_manager = new NetworkManager();
  file_watcher_receiver = new NetworkManager();

  int port1 = push_manager->createAndSetupSocket();

  std::string command1 = "PORT " + std::to_string(port1);

  command_manager->sendPacket(
      CMD, 1,
      command1); // Envia o comando para o cliente com a porta do push_manager
  push_manager->acceptConnection();

  packet pkt = push_manager->receivePacket();
  std::cout << "Recebido do dispositivo: " << pkt._payload << std::endl;
}

Device::~Device() {
  delete command_manager;
  delete push_manager;
  delete file_watcher_receiver;
}

void Device::commandThread() {}

void Device::pushThread() {}

void Device::fileWatcherThread() {}
