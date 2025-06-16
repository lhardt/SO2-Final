#include "ClientManager.hpp"
#include "../Utils/logger.hpp"
#include <algorithm>
#include <unistd.h>

ClientManager::ClientManager(State state, string username)
    : file_manager(nullptr), max_devices(MAX_DEVICES),
      username(username), state(state) {
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
      network_manager.sendPacket(t_ERROR, 0, "Limite de dispositivos atingido");
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
      std::string file_name(pkt._payload);
      if (pkt.type == t_WRITE) {
        log_info("Recebendo arquivo: %s", file_name.c_str());
        bool stop = false;
        while (!stop) {
          packet pkt_received = network_manager->receivePacket();
          if ( pkt_received.type == t_END_OF_FILE) {
            stop = true;
            log_info("Recebimento do arquivo %s finalizado", file_name.c_str());
            break;
          }
          std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
          file_manager->writeFile(file_name, data);
        }
      } else if (pkt.type == t_DELETE) {
        log_info("Removendo arquivo: %s", file_name.c_str());
        file_manager->deleteFile(file_name);
      } else {
        log_error("Comando desconhecido recebido: %d ", pkt.type);
      }
    } catch (const std::runtime_error &e) {
      log_warn("Erro ao receber push: %s", e.what());
      break; // Sai do loop se ocorrer um erro
    }
  }
  log_info("Thread de recebimento de push finalizada");
}

string ClientManager::getUsername() { return this->username; }

void ClientManager::handle_new_push(packet pkt, Device *caller) {
  std::string content = pkt._payload;
  for (auto device : this->devices) {
    if (device != caller)
      device->sendPushTo(pkt.type, content);
  }
  // manda o push para os backups
  // TODO paralelizar o envio para os backups e proteger o acesso à lsita de backups
  for (auto backup : backup_peers) {
    // inicia uma thread para enviar o push e o arquivo para os backups
    backup->sendPacket(pkt.type, 0, content);
    if (pkt.type == t_WRITE) {
      log_info("Enviando arquivo: %s para backup", content.c_str());
      backup->sendFileInChunks(content, MAX_PACKET_SIZE, *file_manager);
    } else if (pkt.type == t_DELETE) {
      log_info("Enviando comando de delete para backup");
    } else {
      log_error("Comando desconhecido enviado para backup: %d %s", pkt.type, content.c_str());
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
