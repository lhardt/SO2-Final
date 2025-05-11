/**
 * This is the main file for the CLIENT binary.
 *
 * please only define the main() function here.
 */
#include "Server/NetworkManager.hpp"
#include "client.hpp"
#include "logger.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 4000
// localhost
#define SERVER_ADDRESS "127.0.0.1"

void g_handleIoThread(Client *client) {
  log_assert(client != NULL, "Null client!");
  client->handleIoThread();
}
void g_handleFileThread(Client *client) {
  log_assert(client != NULL, "Null client!");
  client->handleFileThread();
}
void g_handleNetworkThread(Client *client) {
  log_assert(client != NULL, "Null client!");
  client->handleNetworkThread();
}

void Client::handleIoThread() {
  log_info("Started IO Thread with ID %d ", std::this_thread::get_id());
  // use std::getline and add to a queue of commands?
}
void Client::handleFileThread() {
  log_info("Started File Thread with ID %d ", std::this_thread::get_id());
}
void Client::handleNetworkThread() {
  log_info("Started Network Thread with ID %d ", std::this_thread::get_id());
  // while( can't reach server ){
  //   wait 100ms
  // }
  // check a queue of commands?
}

Client::Client(std::string _client_name, std::string _server_ip,
               std::string _server_port)
    : client_name(_client_name), server_ip(_server_ip),
      server_port(_server_port) {

  // TODO: log parameters

  this->io_thread = std::thread(g_handleIoThread, this);
  this->network_thread = std::thread(g_handleNetworkThread, this);
  this->file_thread = std::thread(g_handleFileThread, this);

  // If the main thread finishes, all the other threads are prematurely
  // finished.
  while (true) {
    log_info("The main thread, like the Great Dark Beyond, lies asleep");
    std::chrono::milliseconds sleep_time{5000};
    std::this_thread::sleep_for(sleep_time);
  }
}

int main(int argc, char **argv) {
  logger_open("logger.log");
  log_info("Hello from CLIENT, SO2-Final!\n");

  // TODO: get info from argc/argv

  int sock = 0;
  struct sockaddr_in serv_addr;
  char buffer[1024] = {0};

  // Criar o socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Erro ao criar o socket" << std::endl;
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);

  // Converter o endereço IP para binário
  if (inet_pton(AF_INET, SERVER_ADDRESS, &serv_addr.sin_addr) <= 0) {
    std::cerr << "Endereço inválido ou não suportado" << std::endl;
    return -1;
  }

  // Conectar ao servidor
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Falha ao conectar ao servidor" << std::endl;
    return -1;
  }

  std::cout << "Conectado ao servidor!" << std::endl;

  // Enviar um nome de usuário para o servidor
  std::string username = "test_user";
  send(sock, username.c_str(), username.size(), 0);
  std::cout << "Nome de usuário enviado: " << username << std::endl;

  NetworkManager network_manager(sock);
  packet p;
  p.type = 1;
  p.seqn = 1;
  std::string payload = "Hello from client";
  p._payload = new char[payload.size() + 1]; // +1 for null terminator
  std::strcpy(p._payload, payload.c_str());
  p.length = payload.size();
  p.total_size = HEADER_SIZE + p.length;

  network_manager.sendPacket(&p);
  packet response = network_manager.receivePacket();
  std::cout << "Resposta recebida do servidor: " << response._payload
            << std::endl;

  // resposta é PORT <port>
  // pega o port e se conecta
  std::string port_str(response._payload);
  std::string port_str2 = port_str.substr(5);
  int port = std::stoi(port_str2);
  std::cout << "Porta recebida do servidor: " << port << std::endl;
  // Conectar ao servidor na porta recebida
  int sock2 = socket(AF_INET, SOCK_STREAM, 0);
  if (sock2 < 0) {
    std::cerr << "Erro ao criar o socket" << std::endl;
    return -1;
  }
  serv_addr.sin_port = htons(port);
  // Conectar ao servidor
  if (connect(sock2, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Falha ao conectar ao servidor" << std::endl;
    return -1;
  }
  std::cout << "Conectado ao servidor na porta " << port << "!" << std::endl;

  NetworkManager network_manager2(sock2);

  // enviar algo na nova conexão
  std::string payload2 = "Hello from client on new connection";
  packet p2;
  p2.type = 1;
  p2.seqn = 1;
  p2._payload = new char[payload2.size() + 1]; // +1 for null terminator
  std::strcpy(p2._payload, payload2.c_str());
  p2.length = payload2.size();
  p2.total_size = HEADER_SIZE + p2.length;

  std::cout << "Mandando pacote na nova conexão" << std::endl;
  network_manager2.sendPacket(&p2);
  // Fechar o socket
  close(sock);
  close(sock2);
  return 0;
}
