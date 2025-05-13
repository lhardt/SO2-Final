/**
 * This is the main file for the SERVER binary.
 *
 * please only define the main() function here.
 */
#include "Server/Server.hpp"
#include "logger.h"
#include <csignal>
#include <cstdlib>
#include <iostream>

int main() {
  signal(SIGPIPE, SIG_IGN);
  logger_open("logger.log");
  log_info("Hello from SERVER, SO2-Final!\n");
  log_info("Starting server...\n");
  Server server;
  server.run();
  return 0;
}
