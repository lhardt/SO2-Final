#include "client.hpp"
#include "../Server/FileManager.hpp"
#include "../Server/NetworkManager.hpp"
#include "../logger.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <regex>
#include <string>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
namespace fs = std::filesystem;

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
void g_handlePushThread(Client *client) {
  log_assert(client != NULL, "Null client!");
  client->handlePushThread();
}

void Client::handleIoThread() {
  log_info("Started IO Thread with ID %d ", std::this_thread::get_id());
  // use std::getline and add to a queue of commands?
  string cmdline;
  smatch cmdarg;
  // this->io_thread = std::thread(g_handleIoThread, this);
  // this->network_thread = std::thread(g_handleNetworkThread, this);

  std::cout << "esperando comando...";
  while (getline(cin, cmdline)) {
    if (regex_match(cmdline, cmdarg, upl)) {
      std::string file_path = cmdarg[1].str();

      // Caminho completo do arquivo a ser copiado
      fs::path arquivo = file_path;

      // Diretório de destino
      fs::path destino_dir = "sync_dir";

      try {
        // Garante que o diretório de destino existe
        fs::create_directories(destino_dir);

        // Cria o caminho final no destino mantendo o mesmo nome do arquivo
        fs::path destino_final = destino_dir / arquivo.filename();

        // Copia o arquivo
        fs::copy(arquivo, destino_final, fs::copy_options::overwrite_existing);

      } catch (const fs::filesystem_error &e) {
        std::cerr << "Erro ao copiar o arquivo: " << e.what() << std::endl;
      }

    } else if (regex_match(cmdline, cmdarg, dow)) { // faz uma copia nao sincronizada do arquivo para o diretorio local(de onde foi chamado o cliente)
      std::string file_name = cmdarg[1].str();
      // std::cout << "DOWNLOAD " + file_name + "\n";
      std::string command = "DOWNLOAD " + file_name;

      command_manager->sendPacket(CMD, 1, vector<char>(command.begin(), command.end()));
      // espera resposta do servidor...
      packet response = command_manager->receivePacket();
      if (std::string(response._payload, response.length) == "FILE_NOT_FOUND") {
        std::cout << "Arquivo não encontrado no servidor." << std::endl;
        continue;
      } else if (std::string(response._payload, response.length) == "FILE_FOUND") {
        std::cout << "Arquivo encontrado no servidor." << std::endl;
      }
      curr_directory_file_manager->createFile(file_name);
      bool stop = false;

      while (!stop) {
        packet pkt_received = command_manager->receivePacket();
        // std::cout << "Recebido do dispositivo: " << pkt_received._payload << std::endl;
        if (std::string(pkt_received._payload, pkt_received.length) ==
            "END_OF_FILE") {
          stop = true;
          break;
        }
        std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
        curr_directory_file_manager->writeFile(file_name, data);
      }

    } else if (regex_match(cmdline, cmdarg, del)) {
      std::string file_name = cmdarg[1].str();
      std::string command = "DELETE " + file_name;
      sync_dir_file_manager->deleteFile(file_name);

    } else if (regex_match(cmdline, cmdarg, lsr)) {
      bool stop = false;
      // std::cout << "MANDANDO LIST SERVER\n";
      std::string command = "LIST";
      command_manager->sendPacket(CMD, 1, vector<char>(command.begin(), command.end()));

      while (!stop) {
        std::cout << std::endl;
        packet pkt_received = command_manager->receivePacket();

        if (std::string(pkt_received._payload, pkt_received.length) == "END_OF_FILE") {
          stop = true;
          break;
        } else {
          std::cout << "Waiting for more Data..." << std::endl;
        }

        std::vector<char> data_filenames(pkt_received._payload, pkt_received._payload + pkt_received.length);
        // vector<char> é um string no formato "teste1.txt teste2.pdf"
        if (data_filenames.size() > 0) {
          std::cout << "Received filenames: ";
        }
        std::cout << std::string(data_filenames.begin(), data_filenames.end());
      }

    } else if (regex_match(cmdline, cmdarg, lcl)) {
      // listClient()
      std::string command = "LIST";
      command_manager->sendPacket(CMD, 1, vector<char>(command.begin(), command.end()));
      // esperar resposta do servidor...
      packet pkt = command_manager->receivePacket();
      std::cout << "pkt payload: ";

    } else if (regex_match(cmdline, cmdarg, gsd)) {
      // getSyncDir();
    } else if (regex_match(cmdline, cmdarg, ext)) {
      exit(0);
    } else {
      log_info("Unrecognized command, please try again.");
    }
  }
}

void Client::handleFileThread() {
  log_info("Started File Thread with ID %d ", std::this_thread::get_id());
  static constexpr char *sync_dir = "sync_dir";

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

  std::cout << "conectado na porta " << this->file_watcher_port << std::endl;

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
          std::string file_name = event->name;
          log_info("Arquivo modificado: %s", file_name.c_str());
          std::string command = "WRITE " + file_name;
          watcher_push_lock.lock();
          file_watcher_manager.sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
          file_watcher_manager.sendFileInChunks(file_name, MAX_PACKET_SIZE, *sync_dir_file_manager);
          watcher_push_lock.unlock();

        } else if (event->mask & IN_DELETE) {
          log_info("Arquivo removido: %s", filepath.c_str());
          std::string command = "DELETE " + std::string(event->name);
          file_watcher_manager.sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
        }
      }

      i += sizeof(struct inotify_event) + event->len;
    }
  }
}

void Client::handlePushThread() {
  // OBJETIVO: RECEBE OS DADOS DO SERVIDOR E TRATA DE ACORDO
  log_info("INICIANDO THREAD PUSH");

  // I) Cria socket para tratar os dados enviados pelo servidor

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    log_error("Erro ao criar socket");
    return;
  }
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(this->push_port);
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
  std::cout << "conectado na porta " << this->push_port << std::endl;

  NetworkManager push_receiver(sock);

  while (true) {
    std::cout << "ESPERANDO PUSHS...\n";
    packet pkt = push_receiver.receivePacket();
    std::istringstream payload_stream(pkt._payload);
    std::string command;
    payload_stream >> command;

    if (command == "WRITE") {
      std::string file_name;
      payload_stream >> file_name;

      sync_dir_file_manager->clearFile(file_name);

      bool stop = false;
      std::cout << "recebendo arquivo: " << file_name << std::endl;
      watcher_push_lock.lock();
      while (!stop) {
        packet pkt_received = push_receiver.receivePacket();
        std::string pkt_string = std::string(pkt_received._payload, pkt_received.length);
        // std::cout << "Recebido do dispositivo: " << pkt_received._payload << std::endl;

        if (pkt_string == "END_OF_FILE") {
          stop = true;
          break;
        } else {
          std::cout << "Waiting for more Data..." << std::endl;
        }

        std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
        sync_dir_file_manager->writeFile(file_name, data);
      }
      watcher_push_lock.unlock();

    } else if (command == "DELETE") {

      std::string file_name;
      payload_stream >> file_name;
      sync_dir_file_manager->deleteFile(file_name);

    } else {
      std::cout << "ERRO";
    }
  }
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
    std::cerr << "Endereço (" << server_ip << ") inválido ou não suportado" << std::endl;
  }

  // Conectar ao servidor
  while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    int err = errno;
    log_warn("Falha (errno=%d) ao conectar ao servidor. Cliente irá esperar 500ms", errno);
    std::chrono::milliseconds sleep_time{500};
    std::this_thread::sleep_for(sleep_time);
  }

  std::cout << "Conectado ao servidor!" << std::endl;

  this->command_manager = new NetworkManager(sock, "CommandManager"); // é o socket de commandos, tem que passar para a thread de IO
  std::string command = client_name;
  command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));

  // recebe o primeiro pacote do server
  packet pkt = command_manager->receivePacket();
  // payload do pacote está no formato PORT <port>
  std::string payload(pkt._payload);
  std::string port_str = payload.substr(payload.find(" ") + 1);
  int port = std::stoi(port_str);
  std::cout << "Porta recebida do servidor: " << port << std::endl;
  this->push_port = port;

  // recebe o segundo pacote do server
  packet pkt2 = command_manager->receivePacket();
  std::string payload2(pkt2._payload);
  std::cout << "Recebido do servidor: " << pkt2._payload << std::endl;
  std::string port_str2 = payload2.substr(payload2.find(" ") + 1);
  int port2 = std::stoi(port_str2);
  std::cout << "Porta recebida do servidor: " << port2 << std::endl;
  this->file_watcher_port = port2;

  this->sync_dir_file_manager = new FileManager("sync_dir");
  this->curr_directory_file_manager = new FileManager("./");

  this->io_thread = std::thread(g_handleIoThread, this);
  this->network_thread = std::thread(g_handlePushThread, this);
  this->file_thread = std::thread(g_handleFileThread, this);

  // If the main thread finishes, all the other threads are prematurely finished.
  io_thread.join();
  network_thread.join();
  file_thread.join();
}
