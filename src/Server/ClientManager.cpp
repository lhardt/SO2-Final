#include "ClientManager.hpp"
#include <algorithm>

ClientManager::ClientManager(string username)
    : username(username), max_devices(MAX_DEVICES), file_manager(nullptr) {}
void ClientManager::handle_new_connection(int socket) {
  // verifica se ja fechou o limite de dispositivos
  if (devices.size() >= max_devices) {
    // fecha a conexao
    return;
  }

  // cria um novo dispositivo
  Device *device = new Device(socket);
  devices.push_back(device);
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(string file_path) {}

void ClientManager::removeDevice(Device *device) {
  // remove o dispositivo da lista de dispositivos
  auto it = std::find(devices.begin(), devices.end(), device);
  if (it != devices.end()) {
    devices.erase(it);
    delete device; // Libera a memória do dispositivo
  }
}
