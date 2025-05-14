#include "ClientManager.hpp"
#include <algorithm>
#include <unistd.h>



ClientManager::ClientManager(string username): username(username),max_devices(MAX_DEVICES),file_manager(nullptr){

  if (username.empty()){
    throw std::runtime_error("Username cannot be empty");
  }
  file_manager = new FileManager("sync_dir_" + username);
}



void ClientManager::handle_new_connection(int socket){
  try{
    if (devices.size() == max_devices){
      NetworkManager network_manager(socket, "Limite de dispositivos");
      std::string command = "Limite de dispositivos atingido";
      network_manager.sendPacket(CMD, 0, std::vector<char>(command.begin(), command.end()));
      network_manager.closeConnection();
      return;
    }
    std::cout << "CRIANDO NOVO DEVICE\n";
    Device *device = new Device(socket, this, file_manager);
    devices.push_back(device);
    std::cout << "Devices: " << devices.size() << std::endl;
    device->start();
    std::cout << "Devices: " << devices.size() << std::endl;
  }catch (const std::exception &e){
    std::cerr << "Erro ao lidar com nova conexão: " << e.what() << std::endl;
    close(socket);
  }
}



string ClientManager::getUsername(){
  return this->username;
}



void ClientManager::handle_new_push(string file_path, Device *caller){

}



void ClientManager::removeDevice(Device *device){
  std::lock_guard<std::mutex> lock(
      device_mutex); // Protege o acesso à lista de dispositivos
  try{
    std::cout << "Removendo dispositivo: " << device << std::endl;
    device->stop(); // Para o dispositivo
    auto it = std::find(devices.begin(), devices.end(), device);
    if (it != devices.end()){
      devices.erase(it);
      std::cout << "Device removido: " << devices.size() << std::endl;
      delete device; // Libera a memória do dispositivo
    }
  }catch(const std::exception &e){
    std::cerr << "Erro ao remover dispositivo: " << e.what() << std::endl;
  }
}



void ClientManager::sendFileToDevice(Device *device, const string &file_path) {
  std::cout << "Enviando arquivo para o dispositivo: " << file_path
            << std::endl;

  try {

  } catch (const std::exception &e) {
    std::cerr << "Erro ao enviar arquivo: " << e.what() << std::endl;
  }
}
