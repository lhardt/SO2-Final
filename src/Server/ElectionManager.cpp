#include "ElectionManager.hpp"
#include "../Utils/logger.hpp"
#include <string>

ElectionManager::ElectionManager(Server *server) {
  // Constructor logic if needed
  this->server = server;
  this->election_id = ipToInt(server->getLocalIP());
}

void ElectionManager::startElection() {
  std::vector<std::string> backup_peers = server->getBackupPeers();
  if (backup_peers.empty()) {
    log_info("Nenhum backup disponível para iniciar a eleição");
    return;
  }
  log_info("Iniciando eleição com %zu backups", backup_peers.size());

  std::string command = "ELECTION " + server->getLocalIP() + " " + std::to_string(server->getPort()) + " " + std::to_string(election_id);

  std::vector<std::string> peers = server->getBackupPeers();
  std::string next_peer;
  for (int i = 0; i < peers.size(); ++i) {
    // acha a si mesmo na lista de peers
    if (peers[i] == server->getLocalIP() + ":" + std::to_string(server->getPort())) {
      // manda para o proximo peer da lista

      if (i + 1 < peers.size()) {
        next_peer = peers[i + 1];
      } else {
        next_peer = peers[0]; // volta para o primeiro peer
      }
      server->sendPacketToPeer(next_peer, command);
      break;
    }
  }
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
