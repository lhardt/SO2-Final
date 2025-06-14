#include "ClientManager.hpp"
#include <algorithm>
#include <unistd.h>
#include "../Utils/logger.hpp"

ClientManager::ClientManager(string _username) : file_manager(nullptr), max_devices(MAX_DEVICES), username(username) {

  if (username.empty()) {
    throw std::runtime_error("Username cannot be empty");
  }
  file_manager = new FileManager("sync_dir_" + username);
}

void ClientManager::handle_new_connection(int socket) {
  try {
    if (devices.size() == max_devices) {
      log_warn("Limite de dispositivos atingido.");
      NetworkManager network_manager(socket, "Limite de dispositivos");
      std::string command = "Limite de dispositivos atingido";
      network_manager.sendPacket(CMD, 0, std::vector<char>(command.begin(), command.end()));
      network_manager.closeConnection();
      return;
    }
    log_info("Criando novo device");
    Device *device = new Device(socket, this, file_manager);
    std::lock_guard<std::mutex> lock(device_mutex);
    devices.push_back(device);
    device_mutex.unlock();
    log_info("Dispositivos conectados: %d", devices.size());
    device->start();
    log_info("Dispositivos conectados: %d", devices.size());
  } catch (const std::exception &e) {
    log_error("Erro ao lidar com nova conexão: %s", e.what());
    close(socket);
  }
}

string ClientManager::getUsername() {
  return this->username;
}

void ClientManager::handle_new_push(string command, Device *caller) {
  for(auto device:this->devices){
    if(device != caller) 
    device->sendPushTo(command);
  }
  
}

void ClientManager::removeDevice(Device *device) {
  std::lock_guard<std::mutex> lock(
      device_mutex); // Protege o acesso à lista de dispositivos
  try {
    log_info("Removendo dispositivo");
    device->stop(); // Para o dispositivo
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
