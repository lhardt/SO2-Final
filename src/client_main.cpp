/**
 * This is the main file for the CLIENT binary.
 *
 * please only define the main() function here.
 */
#include "Client/client.hpp"
#include "Utils/NetworkManager.hpp"
#include "Utils/logger.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_LISTEN_PORT 5000
#define SERVER_PORT 4000
// localhost
#define SERVER_ADDRESS "127.0.0.1"

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
  return 0;
}
