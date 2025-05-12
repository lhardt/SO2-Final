#include "ClientManager.hpp"
#include <algorithm>
#include <unistd.h>

ClientManager::ClientManager(string username)
    : username(username), max_devices(MAX_DEVICES), file_manager(nullptr) {}

void ClientManager::handle_new_connection(int socket) {
  try {
    if (devices.size() == max_devices) {
      NetworkManager network_manager(socket, "Limite de dispositivos");
      network_manager.sendPacket(CMD, 0, "Limite de dispositivos atingido");
      network_manager.closeConnection();
      return;
    }

    Device *device = new Device(socket, this);
    devices.push_back(device);
    std::cout << "Devices: " << devices.size() << std::endl;
    device->start();
    std::cout << "Devices: " << devices.size() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Erro ao lidar com nova conexão: " << e.what() << std::endl;
    close(socket);
  }
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(string file_path) {}

void ClientManager::removeDevice(Device *device) {
  std::lock_guard<std::mutex> lock(
      device_mutex); // Protege o acesso à lista de dispositivos
  try {
    std::cout << "Removendo dispositivo: " << device << std::endl;
    device->stop(); // Para o dispositivo
    auto it = std::find(devices.begin(), devices.end(), device);
    if (it != devices.end()) {
      devices.erase(it);
      std::cout << "Device removido: " << devices.size() << std::endl;
      delete device; // Libera a memória do dispositivo
    }
  } catch (const std::exception &e) {
    std::cerr << "Erro ao remover dispositivo: " << e.what() << std::endl;
  }
}
