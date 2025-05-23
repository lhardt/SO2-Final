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

int main() {
  signal(SIGPIPE, SIG_IGN);
  logger_open("logger.log");
  log_info("Hello from SERVER, SO2-Final!\n");
  log_info("Inicializando servidor...\n");
  Server *server = new Server();
  server->run();
  return 0;
}
