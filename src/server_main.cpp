/**
 * This is the main file for the SERVER binary.
 *
 * please only define the main() function here.
 */
#include "Server/Server.hpp"
#include "Utils/logger.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>

// server <porta> -l
// server <porta> <ip> <porta> -b
int main(int argc, char *argv[]) {

  Server *server;
  if (argc == 3) {
    server = new Server(LEADER, std::stoi(argv[1]));
  } else if (argc == 5) {
    server =
        new Server(BACKUP, std::stoi(argv[1]), argv[2], std::stoi(argv[3]));
  } else {
    std::cerr << "Usage: " << argv[0] << " [<ip> <port>] [-b]\n";
    return EXIT_FAILURE;
  }
  signal(SIGPIPE, SIG_IGN);
  logger_open("logger.log");
  log_info("Hello from SERVER, SO2-Final!\n");
  log_info("Inicializando servidor...\n");
  server->run();
  return 0;
}
