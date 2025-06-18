#pragma once

#include "Server.hpp"

class ElectionManager {
private:
  int election_id;
  Server *server; // Referência ao servidor
  int ipToInt(const std::string &ip);

public:
  ElectionManager(Server *server);
  void startElection();
};
