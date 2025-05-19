#include "ClientManager.hpp"
#include "../Utils/Command.hpp"
#include "../Utils/logger.hpp"
#include <algorithm>
#include <unistd.h>

ClientManager::ClientManager(string username) : username(username), max_devices(MAX_DEVICES), file_manager(nullptr) {

  if (username.empty()) {
    throw std::runtime_error("Username cannot be empty");
  }
  file_manager = new FileManager("sync_dir_" + username);
}

void ClientManager::handle_new_connection(int socket) {
  std::lock_guard<std::mutex> lock(device_mutex);
  try {
    if (devices.size() == max_devices) {
      log_warn("Limite de dispositivos atingido.");
      NetworkManager network_manager(socket, "Limite de dispositivos");
      SendMessageCommand cmd("Limite de dispositivos atingido", &network_manager);
      cmd.execute();
      network_manager.closeConnection();
      return;
    }
    log_info("Criando novo device");
    Device *device = new Device(socket, this, file_manager);
    devices.push_back(device);
    device_mutex.unlock();
    log_info("Dispositivos conectados: %d", devices.size());
    device->start();
    log_info("Dispositivos conectados: %d", devices.size());
  } catch (const std::exception &e) {
    log_error("Erro ao lidar com nova conexão: %s", e.what());
    close(socket);
    device_mutex.unlock();
  }
}

string ClientManager::getUsername() {
  return this->username;
}

void ClientManager::handle_new_push(string command, Device *caller) {
  for (auto device : this->devices) {
    if (device != caller)
      device->sendPushTo(command);
  }
}

void ClientManager::removeDevice(Device *device) {
  std::lock_guard<std::mutex> lock(
      device_mutex); // Protege o acesso à lista de dispositivos
  try {
    log_info("Removendo dispositivo");
    auto it = std::find(devices.begin(), devices.end(), device);
    if (it != devices.end()) {
      devices.erase(it);
      log_info("Dispositivo removido com sucesso");
      delete device; // Libera a memória do dispositivo
    }
  } catch (const std::exception &e) {
    log_error("Erro ao remover dispositivo: %s", e.what());
  }
}
