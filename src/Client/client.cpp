#include "client.hpp"
#include "../Server/NetworkManager.hpp"
#include "../constants.hpp"
#include "../logger.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <regex>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

std::regex upl("upload ([a-zA-Z0-9_/\\.]+)"),
    dow("download ([a-zA-Z0-9_/\\.]+)"), del("delete ([a-zA-Z0-9_/\\.]+)"),
    lsr("list_server"), lcl("list_client"), gsd("get_sync_dir"), ext("exit");

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
  string cmdline;
  smatch cmdarg;
  while (getline(cin, cmdline)) {
    if (regex_match(cmdline, cmdarg, upl)) {
      // FAZER FUNÇÃO QUE ENVIA ARQUIVO PARA SERVIDOR EM UMA connections.cpp (ou
      // algo assim) uploadFile(sock, cmdarg[1].str())
    } else if (regex_match(cmdline, cmdarg, dow)) {
      // FAZER FUNÇÃO QUE ENVIA NOME DE ARQUIVO PARA SERVIDOR E O BAIXA EM UMA
      // connections.cpp (ou algo assim) downloadFile(sock, cmdarg[1].str());
    } else if (regex_match(cmdline, cmdarg, del)) {
      // FAZER FUNÇÃO QUE ENVIA NOME DE ARQUIVO PARA SERVIDOR E ENVIA ALGUMA
      // FLAG PARA DELETAR EM UMA connections.cpp (ou algo assim)
      // deleteFile(sock, cmdarg[1].str());
    } else if (regex_match(cmdline, cmdarg, lsr)) {
      // FAZER FUNÇÃO QUE ENVIA COMANDO DE LISTAR ARQUIVOS EM UMA
      // connections.cpp (ou algo assim) listServer(sock);
    } else if (regex_match(cmdline, cmdarg, lcl)) {
      // listClient()
    } else if (regex_match(cmdline, cmdarg, gsd)) {
      // getSyncDir();
    } else if (regex_match(cmdline, cmdarg, ext))
      exit(0);
    else
      log_info("Unrecognized command, please try again.");
  }
}
void Client::handleFileThread() {
  log_info("Started File Thread with ID %d ", std::this_thread::get_id());
  static constexpr char *sync_dir = "sync_dir";

  // // CRIAÇÃO DO SOCKET TCP, CREIO QUE DÁ PRA DEIXAR EM UM MÉTODO PARA SER
  // USADO
  // // EM TODAS AS THREADS JÁ QUE CADA UMA VAI TER SEU PRÓPRIO SOCKET, TEM QUE
  // VER
  // // O QUE DIFERENCIA UM SOCKET DO OUTRO SÃO PORTAS DIFERENTES? OU CADA
  // THREAD
  // // FAZ SEU PRÓPRIO sock = socket() COM OS MESMOS PARAMETROS?
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    log_error("Erro ao criar socket");
    return;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(this->file_watcher_port);

  if (inet_pton(AF_INET, this->server_ip.c_str(), &server_addr.sin_addr) <= 0) {
    log_error("File thread | Invalid IP address: %s", this->server_ip.c_str());
    close(sock);
    return;
  }

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    log_error("File thread | Failed to connect to server: %s:%d",
              this->server_ip.c_str(), this->server_port.c_str());
    close(sock);
  }

  // recebi uma porta de arugmento na thread
  // crio um socket e me conecto nessa porta

  NetworkManager file_watcher_manager(sock, "FileWatcherManager");
  log_info("File thread | Connected to server: %s:%d", this->server_ip.c_str(),
           this->file_watcher_port);

  int inotifyFd = inotify_init1(IN_NONBLOCK);
  if (inotifyFd < 0) {
    log_error("Could not initiate inotify");
    return;
  }
  int watchDescriptor =
      inotify_add_watch(inotifyFd, sync_dir, IN_DELETE | IN_CLOSE_WRITE);
  if (watchDescriptor < 0) {
    log_error("Could not initiate watchDescriptor");
    return;
  }

  size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
  char buffer[BUF_LEN];

  log_info("Monitoring file: %s", sync_dir);

  while (true) {
    int length = read(inotifyFd, buffer, BUF_LEN);
    if (length < 0) {
      std::chrono::milliseconds sleep_time{500};
      std::this_thread::sleep_for(sleep_time); // Evita busy waiting
      continue;
    }

    int i = 0;
    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];

      if (event->len > 0) {
        std::string filepath =
            "./" + std::string(sync_dir) + "/" + std::string(event->name);

        if (event->mask & IN_CLOSE_WRITE) {
          log_info("Arquivo modificado: %s", filepath.c_str());
          // FAZER FUNÇÃO QUE ENVIA ARQUIVO PARA SERVIDOR EM UMA
          // connections.cpp
          // (ou algo assim) uploadFile(sock, filepath)
        } else if (event->mask & IN_DELETE) {
          log_info("Arquivo removido: %s", filepath.c_str());
          // FAZER FUNÇÃO QUE ENVIA NOME DO ARQUIVO A SER DELETADO EM UMA
          // connections.cpp (ou algo assim) deleteFile(sock, event->name)
        }
      }

      i += sizeof(struct inotify_event) + event->len;
    }
  }
}

void Client::handleNetworkThread() {
  log_info("Started Network Thread with ID %d ", std::this_thread::get_id());

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    int error = errno;
    log_error("Could not create socket! errno=%d ", error);
    std::exit(1);
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(std::atoi(this->server_port.c_str()));

  int conversion_result =
      inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
  if (conversion_result <= 0) {
    // https://man7.org/linux/man-pages/man3/inet_pton.3.html
    int error = errno;
    log_error("Could not convert server ip! result=%d errno=%d ",
              conversion_result, error);
    std::exit(1);
  }

  while (true) {
    log_info("Will sleep to give time for server to catch on!");
    std::chrono::milliseconds sleep_time{500};
    std::this_thread::sleep_for(sleep_time);

    int connect_status = connect(socket_fd, (struct sockaddr *)&server_addr,
                                 sizeof(server_addr));
    int error = errno;
    if (connect_status >= 0) {
      break;
    }
    log_warn("Could not CONNECT! result=%d errno=%d ", connect_status, error);
  }

  log_info("Connected successfully!");

  // check a queue of commands?
}

Client::Client(std::string _client_name, std::string _server_ip,
               std::string _server_port)
    : client_name(_client_name), server_ip(_server_ip),
      server_port(_server_port) {

  // TODO: log parameters
  // faz conexao inicial com o servidor

  int server_port_int = std::stoi(server_port);

  int sock = 0;
  struct sockaddr_in serv_addr;
  char buffer[1024] = {0};

  // Criar o socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Erro ao criar o socket" << std::endl;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(server_port_int);

  // Converter o endereço IP para binário
  if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
    std::cerr << "Endereço inválido ou não suportado" << std::endl;
  }

  // Conectar ao servidor
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Falha ao conectar ao servidor" << std::endl;
  }

  std::cout << "Conectado ao servidor!" << std::endl;

  this->command_manager = new NetworkManager(sock, "CommandManager");
  command_manager->sendPacket(CMD, 1, client_name);

  // recebe o primeiro pacote do server
  packet pkt = command_manager->receivePacket();
  // payload do pacote está no formato PORT <port>
  std::string payload(pkt._payload);
  std::string port_str = payload.substr(payload.find(" ") + 1);
  int port = std::stoi(port_str);
  std::cout << "Porta recebida do servidor: " << port << std::endl;
  this->file_watcher_port = port;

  // recebe o segundo pacote do server
  packet pkt2 = command_manager->receivePacket();
  std::string payload2(pkt2._payload);
  std::cout << "Recebido do servidor: " << pkt2._payload << std::endl;
  std::string port_str2 = payload2.substr(payload2.find(" ") + 1);
  int port2 = std::stoi(port_str2);
  std::cout << "Porta recebida do servidor: " << port2 << std::endl;
  this->push_port = port2;

  std::string sync_dir = "sync_dir";
  if (mkdir(sync_dir.c_str(), 0777) == -1) {
    std::cerr << "Erro ao criar o diretório: " << sync_dir << std::endl;
  } else {
    std::cout << "Diretório criado: " << sync_dir << std::endl;
  }

  // this->io_thread = std::thread(g_handleIoThread, this);
  // this->network_thread = std::thread(g_handleNetworkThread, this);
  this->file_thread = std::thread(g_handleFileThread, this);

  // If the main thread finishes, all the other threads are prematurely
  // finished.
  while (true) {
    // log_info("The main thread, like the Great Dark Beyond, lies asleep");
    std::chrono::milliseconds sleep_time{5000};
    std::this_thread::sleep_for(sleep_time);
  }
}
