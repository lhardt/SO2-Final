#include "Device.hpp"
#include <cstring>
#include <exception>
#include <string>

Device::Device(int command_socket_fd, ClientManager *client_manager) {
  // Inicializa os sockets
  command_manager = new NetworkManager(command_socket_fd);
  this->client_manager = client_manager;

  push_manager = new NetworkManager();
  file_watcher_receiver = new NetworkManager();

  int port1 = push_manager->createAndSetupSocket();
  std::string command1 = "PORT " + std::to_string(port1);
  command_manager->sendPacket(
      CMD, 1,
      command1); // Envia o comando para o cliente com a porta do push_manager

  int port2 = file_watcher_receiver->createAndSetupSocket();
  std::string command2 = "PORT " + std::to_string(port2);
  command_manager->sendPacket(CMD, 1,
                              command2); // Envia o comando para o cliente com a

  push_manager->acceptConnection();
  file_watcher_receiver->acceptConnection();

  std::cout << "AAAAAA" << std::endl;

  packet pkt = push_manager->receivePacket();
  packet pkt2 = file_watcher_receiver->receivePacket();
  std::cout << "Recebido do dispositivo: " << pkt._payload << std::endl;
  std::cout << "Recebido do dispositivo: " << pkt2._payload << std::endl;
}

Device::~Device() {
  delete command_manager;
  delete push_manager;
  delete file_watcher_receiver;
}

void Device::commandThread() {
  try {
    // recebe e trata os comandos do cliente

  } catch (const std::exception &e) {
    // Desconexão detectada

    std::cerr << "Cliente desconectado: " << e.what() << std::endl;

    client_manager->removeDevice(this);
  }
}

void Device::pushThread() {
  try {
    // lida com pushs para os outros dispositivos

  } catch (const std::exception &e) {
    // Desconexão detectada

    std::cerr << "Cliente desconectado: " << e.what() << std::endl;

    client_manager->removeDevice(this);
  }
}

void Device::fileWatcherThread() {
  try {
    // recebe e trata mudancas nos arquivos causadas pelos dispositivos

  } catch (const std::exception &e) {
    // Desconexão detectada

    std::cerr << "Cliente desconectado: " << e.what() << std::endl;

    client_manager->removeDevice(this);
  }
}
