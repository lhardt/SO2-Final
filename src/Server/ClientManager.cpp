#include "ClientManager.hpp"
#include <algorithm>

ClientManager::ClientManager(string username)
    : username(username), max_devices(MAX_DEVICES), file_manager(nullptr) {
  if (username.empty()) {
    throw std::runtime_error("Username cannot be empty");
  }
  file_manager = new FileManager(username);
}

void ClientManager::handle_new_connection(
    int socket) { // verifica se ja fechou o limite de dispositivos
  if (devices.size() >= max_devices) {
    // fecha a conexao
    network_manager = new NetworkManager(socket);
    network_manager->sendPacket(CMD, 0, "Limite de dispositivos atingido");
    network_manager->closeSocket();
    delete network_manager;
    return;
  }

  Device *device = new Device(socket, this);
  devices.push_back(device);
  // device->start();
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(string file_path, Device *caller) {}

void ClientManager::removeDevice(Device *device) {
  // remove o dispositivo da lista de dispositivos
  auto it = std::find(devices.begin(), devices.end(), device);
  if (it != devices.end()) {
    devices.erase(it);
    delete device; // Libera a memória do dispositivo
  }
}
