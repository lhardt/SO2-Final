#include "ClientManager.hpp"

ClientManager::ClientManager(string username)
    : username(username), max_devices(MAX_DEVICES), file_manager(nullptr) {}
void ClientManager::handle_new_connection(int socket) {
  // verifica se ja fechou o limite de dispositivos
  if (devices.size() >= max_devices) {
    // fecha a conexao
    return;
  }
  cout << "Handling new connection!" << endl;

  // Rotina de criar novos sockets
  // instanciar um novo device
  // Device* device = new Device(socket, file_manager);
  // dentro do device, criar novos 2 sockets
  // device.create_socket();
  // device.create_socket();
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(string file_path) {}
