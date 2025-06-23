#pragma once

#include "Server.hpp"
#include <string>

class ElectionManager {
private:
  int election_id;
  Server *server; // Referência ao servidor
  int ipToInt(const std::string &ip);
  std::string next_peer;
  bool participating_in_election = false;

public:
  ElectionManager(Server *server);
  void startElection();
  void resolveElection();
  void setNextPeer(const std::string &peer);
  void UpdateNextPeer(const std::string peer_list);
  void handleCommand(std::string &command);
};
