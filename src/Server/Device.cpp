#include "Device.hpp"
#include <cstring>
#include <exception>
#include <string>

Device::Device(int command_socket_fd, ClientManager *client_manager)
    : stop_requested(false), command_thread(nullptr), push_thread(nullptr),
      file_watcher_thread(nullptr), client_manager(client_manager) {

  // Inicializa os sockets
  command_manager = new NetworkManager(command_socket_fd, "CommandManager");

  push_manager = new NetworkManager("PushManager");
  file_watcher_receiver = new NetworkManager("FileWatcherReceiver");

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
    std::cout << "Iniciando thread de comandos..." << std::endl;
    while (!stop_requested) {
      command_manager->receivePacket();
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Cliente desconectado: " << e.what() << std::endl;
    stop_requested = true;
  }
  std::cout << "Command thread finished" << std::endl;
}

void Device::pushThread() {
  try {
    std::cout << "Iniciando thread de push..." << std::endl;
    while (!stop_requested) {
      push_manager->receivePacket();
    }
  } catch (const std::runtime_error &e) {
    stop_requested = true;
    std::cout << "Cliente desconectado: " << e.what()
              << "stop_requested: " << stop_requested << std::endl;
  }
  std::cout << "Push thread finished" << std::endl;
}

void Device::fileWatcherThread() {
  try {
    std::cout << "Iniciando thread de file watcher..." << std::endl;
    while (!stop_requested) {
      file_watcher_receiver->receivePacket();
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Cliente desconectado: " << e.what() << std::endl;
    stop_requested = true;
  }
  std::cout << "File watcher thread finished" << std::endl;
}

void Device::start() {
  command_thread = new thread(&Device::commandThread, this);
  push_thread = new thread(&Device::pushThread, this);
  file_watcher_thread = new thread(&Device::fileWatcherThread, this);

  // espera o join
  if (command_thread->joinable()) {
    command_thread->join();
  }
  if (push_thread->joinable()) {
    push_thread->join();
  }
  if (file_watcher_thread->joinable()) {
    file_watcher_thread->join();
  }
  client_manager->removeDevice(this);
}

void Device::stop() {
  stop_requested = true;

  std::cout << "Parando dispositivo...stop_requested: " << stop_requested
            << std::endl;
  // Fechar conexões de socket
  command_manager->closeConnection();
  push_manager->closeConnection();
  file_watcher_receiver->closeConnection();

  // espera as threads terminarem
  if (command_thread->joinable() &&
      command_thread->get_id() != std::this_thread::get_id()) {
    command_thread->join();
  }
  if (push_thread->joinable() &&
      push_thread->get_id() != std::this_thread::get_id()) {
    push_thread->join();
  }
  if (file_watcher_thread->joinable() &&
      file_watcher_thread->get_id() != std::this_thread::get_id()) {
    file_watcher_thread->join();
  }

  std::cout << "Device Parado" << std::endl;

  delete command_thread;
  delete push_thread;
  delete file_watcher_thread;

  command_thread = nullptr;
  push_thread = nullptr;
  file_watcher_thread = nullptr;
}

bool Device::isStopRequested() { return stop_requested; }
