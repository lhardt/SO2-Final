#include "ClientManager.hpp"
#include "../Utils/logger.hpp"
#include "Server.hpp"
#include <algorithm>
#include <unistd.h>

ClientManager::ClientManager(State state, string username) : file_manager(nullptr), max_devices(MAX_DEVICES), username(username), state(state) {
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

void ClientManager::receivePushsOn(NetworkManager *network_manager) {
  while (true) {
    try {
      packet pkt = network_manager->receivePacket();
      log_info("Recebendo push no clientManagerBackup\n");
      NetworkManager::printPacket(pkt);
      std::string received_message(pkt._payload);
      std::string command = received_message.substr(0, received_message.find(' '));
      if (command == "WRITE") {
        std::string file_name = received_message.substr(received_message.find(' ') + 1);
        log_info("Recebendo arquivo: %s", file_name.c_str());
        bool stop = false;
        while (!stop) {
          packet pkt_received = network_manager->receivePacket();
          if (std::string(pkt_received._payload, pkt_received.length) == "END_OF_FILE") {
            stop = true;
            log_info("Recebimento do arquivo %s finalizado", file_name.c_str());
            break;
          }
          std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
          file_manager->writeFile(file_name, data);
        }
      } else if (command == "DELETE") {
        std::string file_name = received_message.substr(received_message.find(' ') + 1);
        log_info("Removendo arquivo: %s", file_name.c_str());
        file_manager->deleteFile(file_name);
      } else {
        log_error("Comando desconhecido recebido: %s", command.c_str());
        log_info("Thread de recebimento de push finalizada");
      }
    } catch (const std::runtime_error &e) {
      log_warn("Erro ao receber push: %s", e.what());
      log_info("Thread de recebimento de push finalizada");
      break; // Sai do loop se ocorrer um erro
    }
  }
  log_info("Thread de recebimento de push finalizada");
}

void ClientManager::notify(std::string msg) {
  log_info("listen_adresses:");
  for (const auto &address : listen_adreesses) {
    log_info(" - %s", address.c_str());
  }
  // notifica todos os dipositivos conectados
  std::string ip = network_manager->getIP();
  for (int i = 0; i < listen_adreesses.size(); i++) {
    NetworkManager *notify_manager = new NetworkManager();
    log_info("Enviando notificação para o %s", listen_adreesses[i].c_str());
    std::string ip = listen_adreesses[i].substr(0, listen_adreesses[i].find(':'));
    int port = std::stoi(listen_adreesses[i].substr(listen_adreesses[i].find(':') + 1));
    notify_manager->connectTo(ip, port);
    notify_manager->sendPacket(CMD, 0, std::vector<char>(msg.begin(), msg.end()));
    notify_manager->closeConnection();
    delete notify_manager; // Libera a memória do NetworkManager
  }
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(string command, Device *caller) {
  for (auto device : this->devices) {
    if (device != caller)
      device->sendPushTo(command);
  }
  std::string command_upper = command.substr(0, command.find(' '));
  // manda o push para os backups
  // TODO paralelizar o envio para os backups e proteger o acesso à lsita de backups
  for (auto backup : backup_peers) {
    // inicia uma thread para enviar o push e o arquivo para os backups
    backup->sendPacket(CMD, 0, std::vector<char>(command.begin(), command.end()));
    if (command_upper == "WRITE") {
      std::string file_name = command.substr(command.find(' ') + 1);
      log_info("Enviando arquivo: %s para backup", file_name.c_str());
      backup->sendFileInChunks(file_name, MAX_PACKET_SIZE, *file_manager);
    } else if (command_upper == "DELETE") {
      log_info("Enviando comando de delete para backup");
    } else {
      log_error("Comando desconhecido enviado para backup: %s", command.c_str());
    }
  }
}

void ClientManager::removeDevice(Device *device) {
  std::lock_guard<std::mutex> lock(device_mutex); // Protege o acesso à lista de dispositivos
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
std::string ClientManager::getIp() { return network_manager->getIP(); }
int ClientManager::getPort() { return network_manager->getPort(); }

void ClientManager::add_new_backup(NetworkManager *peer_manager) {
  std::lock_guard<std::mutex> lock(device_mutex);
  backup_peers.push_back(peer_manager);
  log_info("Novo backup adicionado. Total de backups: %d", backup_peers.size());
}

void ClientManager::setNetworkManager(NetworkManager *network_manager) {
  this->network_manager = network_manager;
  log_info("NetworkManager configurado com IP: %s e porta: %d",
           network_manager->getIP().c_str(), network_manager->getPort());
}

void ClientManager::add_listen_adress(std::string listen_adress) {
  std::lock_guard<std::mutex> lock(device_mutex);
  listen_adreesses.push_back(listen_adress);
  log_info("Novo endereço de escuta adicionado: %s", listen_adress.c_str());
}
