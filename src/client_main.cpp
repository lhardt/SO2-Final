/**
 * This is the main file for the CLIENT binary.
 *
 * please only define the main() function here.
 */
#include "Client/client.hpp"
#include "Server/NetworkManager.hpp"
#include "logger.hpp"
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

#define SERVER_PORT 4000
// localhost
#define SERVER_ADDRESS "127.0.0.1"

int main(int argc, char **argv) {
  logger_open("logger.log");
  log_info("Hello from CLIENT, SO2-Final!\n");

  // TODO: get info from argc/argv
  // primeiro argumento username
  // segundo argumento ip do servidor
  // terceiro argumento porta do servidor

  if (argc < 3) {
    std::cerr << "Uso: " << argv[0] << " <username> <ip> <port>" << std::endl;
    return -1;
  }
  std::string username = argv[1];
  std::string server_ip = argv[2];
  std::string server_port = argv[3];

  // send(sock, username.c_str(), username.size(), 0);
  // std::cout << "Nome de usuário enviado: " << username << std::endl;

  // criar sync_dir
  Client client(username, server_ip, server_port);
  return 0;
}
