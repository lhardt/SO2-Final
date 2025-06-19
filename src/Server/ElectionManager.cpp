#include "ElectionManager.hpp"
#include "../Utils/logger.hpp"
#include <string>

ElectionManager::ElectionManager(Server *server) {
  // Constructor logic if needed
  this->server = server;
  this->election_id = ipToInt(server->getLocalIP());
}

void ElectionManager::startElection() {

  std::string command = "ELECTION " + server->getLocalIP() + " " + std::to_string(server->getPort());
  if (next_peer == this->server->getLocalIP() + ":" + std::to_string(this->server->getPort())) {
    log_info("Já sou o líder, não preciso iniciar uma eleição");
    this->server->turnLeader();
    return;
  }
  NetworkManager *next_peer_manager = server->getPeerConnection(next_peer);
  if (!next_peer_manager) {
    log_error("Não foi possível encontrar o próximo peer: %s", next_peer.c_str());
    return;
  }
  next_peer_manager->sendPacket(CMD, 0, std::vector<char>(command.begin(), command.end()));
}

int ElectionManager::ipToInt(const std::string &ip) {
  std::stringstream ss(ip);
  std::string segment;
  int result = 0;
  int shift = 24; // Começa com o octeto mais significativo

  while (std::getline(ss, segment, '.')) {
    result |= (std::stoi(segment) << shift);
    shift -= 8; // Move para o próximo octeto
  }

  return result;
}
void ElectionManager::setNextPeer(const std::string &peer) {
  this->next_peer = peer;
  log_info("Próximo peer definido: %s", peer.c_str());
}

void ElectionManager::UpdateNextPeer(const std::string peer_list) {
  std::istringstream iss(peer_list);
  std::string peer_info;
  std::vector<std::string> peers;

  while (std::getline(iss, peer_info, ';')) {
    if (!peer_info.empty()) {
      peers.push_back(peer_info);
    }
  }

  if (peers.empty()) {
    log_error("Lista de peers vazia recebida");
    return;
  }

  for (int i = 0; i < peers.size(); i++) {
    if (peers[i] == this->server->getLocalIP() + ":" + std::to_string(this->server->getPort())) {
      if (i + 1 < peers.size()) {
        this->next_peer = peers[i + 1];
      } else {
        this->next_peer = peers[0]; // Volta ao primeiro peer se for o último
      }
      log_info("Próximo peer atualizado para: %s", this->next_peer.c_str());
      return;
    }
  }
}

void ElectionManager::handleCommand(std::string &command) {
  // retira o "ELECTION " do começo
  log_info("Recebendo comando de eleição: %s", command.c_str());
  command = command.substr(9);
  std::istringstream iss(command);
  std::string ip;
  int port;

  iss >> ip >> port;
  if (ip == server->getLocalIP() && port == server->getPort()) {
    log_info("NOVO LIDER ELEITO");
    // manda para todos os peers que ele é o novo lider
    std::string new_leader_msg = "LEADER_IS " + server->getLocalIP() + " " + std::to_string(server->getPort());
    for (NetworkManager *peer_manager : server->getPeers()) {
      peer_manager->sendPacket(CMD, 0, std::vector<char>(new_leader_msg.begin(), new_leader_msg.end()));
    }
  } else {
    if (port > server->getPort()) {
      // reenvio a eleicao para o next_peer
      this->server->getPeerConnection(next_peer)->sendPacket(CMD, 0, "ELECTION " + ip + " " + std::to_string(port));
    }
  }

  return;
}
