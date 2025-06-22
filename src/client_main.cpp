/**
 * This is the main file for the CLIENT binary.
 *
 * please only define the main() function here.
 */
#include "Client/client.hpp"
#include "Utils/logger.hpp"
#include <iostream>
#include <string>

#define DEFAULT_LISTEN_PORT 5000

int main(int argc, char **argv) {
  logger_open("logger.log");
  log_info("Hello from CLIENT, SO2-Final!\n");

  if (argc < 3) {
    std::cerr << "Uso: " << argv[0] << " <username> <ip> <port>" << std::endl;
    return -1;
  }
  std::string username = argv[1];
  std::string server_ip = argv[2];
  std::string server_port = argv[3];

  Client *client = new Client(username, server_ip, server_port, DEFAULT_LISTEN_PORT);
  client->run();
  delete client;
  return 0;
}
