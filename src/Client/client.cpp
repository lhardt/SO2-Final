#include "client.hpp"
#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/logger.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <regex>
#include <sstream>
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
  log_info("Inicializado IO Thread com ID: %ld ", std::this_thread::get_id());
  try {
    string cmdline;
    smatch cmdarg;

    std::cout << "Esperando comando..." << std::endl;
    while (!stop_requested) {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(STDIN_FILENO, &readfds);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 500000; // 0.5s

      int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
      if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        if (!std::getline(std::cin, cmdline)) {
          break;
        }
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
          command_manager->sendPacket(CMD, 1, "DOWNLOAD " + file_name);
          // espera resposta do servidor...
          packet response = command_manager->receivePacket();
          if (std::string(response._payload, response.length) == "FILE_NOT_FOUND") {
            log_info("Arquivo não encontrado no servidor.");
            continue;
          } else if (std::string(response._payload, response.length) == "FILE_FOUND") {
            log_info("Arquivo encontrado no servidor.");
          }
          curr_directory_file_manager->createFile(file_name);
          bool stop = false;
          log_info("Escrevendo arquivo: %s", file_name.c_str());
          while (!stop && !stop_requested) {
            packet pkt_received = command_manager->receivePacket();
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
          if (sync_dir_file_manager->isFileExists(file_name)) {
            log_info("Deletando arquivo: %s", file_name.c_str());
            sync_dir_file_manager->deleteFile(file_name);
          } else {
            log_info("Arquivo não encontrado.");
            continue;
          }

        } else if (regex_match(cmdline, cmdarg, lsr)) {
          bool stop = false;
          command_manager->sendPacket(CMD, 1, "LIST");

          while (!stop && !stop_requested) {
            packet pkt_received = command_manager->receivePacket();

            if (std::string(pkt_received._payload, pkt_received.length) == "END_OF_FILE") {
              stop = true;
              break;
            }

            std::vector<char> data_filenames(pkt_received._payload, pkt_received._payload + pkt_received.length);
            if (!data_filenames.empty()) {
              std::cout << std::string(data_filenames.begin(), data_filenames.end()) << std::endl;
            }
          }

        } else if (regex_match(cmdline, cmdarg, lcl)) {
          std::string file_info = sync_dir_file_manager->getFiles();
          if (!file_info.empty()) {
            std::cout << "Local files:\n"
                      << file_info;
          } else {
            std::cout << "No files found in the local directory.\n";
          }
        } else if (regex_match(cmdline, cmdarg, gsd)) {
          std::cout << " Sync Dir path is ./sync_dir" << std::endl;
        } else if (regex_match(cmdline, cmdarg, ext)) {
          exit(0);
        } else {
          std::cout << "Comando <<" << cmdline << ">> nao reconhecido, tente novamente." << std::endl;
        }
        // Limpa o buffer de entrada
        cmdline.clear();
      }
    }
  } catch (const std::exception &e) {
    log_error("Erro na IO Thread: %s", e.what());
    stop_requested = true;
    return;
  }
  log_info("IO thread finalizada");
}

void Client::handleFileThread() {
  log_info("Inicializado File Thread com ID %ld, e porta %d ", std::this_thread::get_id(), this->file_watcher_port);
  static constexpr char *sync_dir = "sync_dir";
  while (!stop_requested) {
    log_info("file thread stop_requested: %d", stop_requested.load());
    try {
      log_info("File thread conectado na porta: %d", this->file_watcher_port);

      this->file_watcher_manager = new NetworkManager("FileWatcherManager", this->server_ip, this->file_watcher_port);

      int inotifyFd = inotify_init1(IN_NONBLOCK);
      if (inotifyFd < 0) {
        log_error("Não foi possível inicializar inotify");
        throw std::runtime_error("inotify_init1 failed");
      }
      int watchDescriptor =
          inotify_add_watch(inotifyFd, sync_dir, IN_DELETE | IN_CLOSE_WRITE);
      if (watchDescriptor < 0) {
        log_error("Não foi possível inicializar watchDescriptor");
        throw std::runtime_error("inotify_add_watch failed");
      }

      size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
      char buffer[BUF_LEN];

      log_info("Monitorando folder: %s", sync_dir);

      while (true && !stop_requested) {
        int length = read(inotifyFd, buffer, BUF_LEN);
        if (length < 0) {
          std::chrono::milliseconds sleep_time{500};
          std::this_thread::sleep_for(sleep_time);
          continue;
        }

        int i = 0;
        while (i < length && !stop_requested) {
          struct inotify_event *event = (struct inotify_event *)&buffer[i];

          if (event->len > 0) {
            std::string filepath =
                "./" + std::string(sync_dir) + "/" + std::string(event->name);

            if (event->mask & IN_CLOSE_WRITE) {
              std::string file_name = event->name;
              log_info("Arquivo modificado: %s", file_name.c_str());
              std::string command = "WRITE " + file_name;
              watcher_push_lock.lock();
              file_watcher_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
              file_watcher_manager->sendFileInChunks(file_name, MAX_PACKET_SIZE, *sync_dir_file_manager);
              watcher_push_lock.unlock();

            } else if (event->mask & IN_DELETE) {
              log_info("Arquivo removido: %s", filepath.c_str());
              std::string command = "DELETE " + std::string(event->name);
              file_watcher_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
            }
          }

          i += sizeof(struct inotify_event) + event->len;
        }
      }
    } catch (const std::exception &e) {
      log_error("Erro na File Thread: %s", e.what());
      stop_requested = true;
      return;
    }
  }
  log_info("File thread finalizada");
}

void Client::handlePushThread() {
  log_info("Inicializado Push Thread com ID %ld ", std::this_thread::get_id());

  while (!stop_requested) {
    log_info("push thread stop_requested: %d", stop_requested.load());
    try {
      int sock = connect_to_socket(this->server_ip, this->push_port);
      if (sock <= 0) {
        throw std::runtime_error("Erro ao criar socket");
      }
      log_info("Push thread conectado na porta: %d", this->file_watcher_port);

      this->push_manager = new NetworkManager(sock);

      while (true && !stop_requested) {
        log_info("Aguardando push do servidor...");
        packet pkt = push_manager->receivePacket();
        std::istringstream payload_stream(pkt._payload);
        std::string command;
        payload_stream >> command;

        if (command == "WRITE") {
          std::string file_name;
          payload_stream >> file_name;

          sync_dir_file_manager->clearFile(file_name);

          bool stop = false;
          log_info("Recebendo arquivo: %s", file_name.c_str());
          watcher_push_lock.lock();
          while (!stop && !stop_requested) {
            packet pkt_received = push_manager->receivePacket();
            std::string pkt_string = std::string(pkt_received._payload, pkt_received.length);

            if (pkt_string == "END_OF_FILE") {
              stop = true;
              break;
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
          log_error("Comando desconhecido recebido do servidor: %s", command.c_str());
        }
      }
    } catch (const std::exception &e) {
      log_error("Conexão com o servidor perdida: %s", e.what());
      stop_requested = true;
      log_info("stop_requested set to true, encerrando push thread");
      return;
    }
  }
  log_info("Push thread finalizada");
}

void Client::handleListenthread() {
  this->listen_thread_network_manager->acceptConnection();
  while (true) {
    try {
      std::cout << "AEHOOO\n";
      packet pkt = listen_thread_network_manager->receivePacket();
      NetworkManager::printPacket(pkt);
      std::string payload_msg(pkt._payload, pkt.length);
      istringstream iss(payload_msg);
      std::string command;
      iss >> command;
      if (command == "LEADER_IS") {

        std::string leader_ip;
        std::string leader_port;
        iss >> leader_ip >> leader_port;

        this->server_ip = leader_ip;
        this->server_port = leader_port;

        sem_post(&this->cleanup_semaphore); // libera o semaforo
        log_info("terminei");
      }
    } catch (const std::runtime_error &e) {
      return;
    }
  }
}

void Client::connectToServer() {

  this->command_manager = new NetworkManager("CommandManager", this->server_ip, std::stoi(this->server_port));
  std::string command = "CLIENT " + client_name + " " + std::to_string(this->listen_port);

  command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
  log_info("Enviando comando de conexão para o servidor: %s", command.c_str());

  // recebe o primeiro pacote do server
  packet pkt = command_manager->receivePacket();
  // payload do pacote está no formato PORT <port>
  std::string payload(pkt._payload);
  std::string port_str = payload.substr(payload.find(" ") + 1);
  int port = std::stoi(port_str);
  this->push_port = port;

  // recebe o segundo pacote do server
  packet pkt2 = command_manager->receivePacket();
  std::string payload2(pkt2._payload);
  std::string port_str2 = payload2.substr(payload2.find(" ") + 1);
  int port2 = std::stoi(port_str2);
  this->file_watcher_port = port2;
  log_info("Conectado ao servidor com push_port: %d e file_watcher_port: %d", this->push_port, this->file_watcher_port);
}

Client::Client(std::string _client_name, std::string _server_ip, std::string _server_port, int listen_port) : client_name(_client_name), server_ip(_server_ip), server_port(_server_port), listen_port(listen_port) {
  int server_port_int = std::stoi(server_port);

  this->listen_thread_network_manager = new NetworkManager("listen_thread_network_manager"); // problema aqui talvez?
  this->listen_port = listen_thread_network_manager->createAndSetupSocket();

  // é o socket de commandos, tem que passar para a thread de IO
  {
    std::lock_guard<std::mutex> lock(command_manager_lock);

    this->command_manager = new NetworkManager("CommandManager", server_ip, server_port_int);
  }
  std::string command = "CLIENT " + client_name + " " + std::to_string(this->listen_port);

  command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));

  // recebe o primeiro pacote do server
  packet pkt = command_manager->receivePacket();
  // payload do pacote está no formato PORT <port>
  std::string payload(pkt._payload);
  std::string port_str = payload.substr(payload.find(" ") + 1);
  int port = std::stoi(port_str);
  this->push_port = port;

  // recebe o segundo pacote do server
  packet pkt2 = command_manager->receivePacket();
  std::string payload2(pkt2._payload);
  std::string port_str2 = payload2.substr(payload2.find(" ") + 1);
  int port2 = std::stoi(port_str2);
  this->file_watcher_port = port2;

  this->sync_dir_file_manager = new FileManager("sync_dir");
  this->curr_directory_file_manager = new FileManager("./");

  sem_init(&this->cleanup_semaphore, 0, 1);
}

void Client::run() {
  // inicia a listen_thread
  this->listen_thread = new std::thread(&Client::handleListenthread, this);
  while (true) {
    sem_wait(&this->cleanup_semaphore); // espera o semáforo ser liberado
    connectToServer();
    this->io_thread = new std::thread(g_handleIoThread, this);
    this->network_thread = new std::thread(g_handlePushThread, this);
    this->file_thread = new std::thread(g_handleFileThread, this);

    log_info("Client iniciado com sucesso, aguardando comandos...");
    io_thread->join();
    network_thread->join();
    file_thread->join();
    stop_requested = false;

    delete this->command_manager;
    this->command_manager = nullptr;
    delete this->file_watcher_manager;
    this->file_watcher_manager = nullptr;
    delete this->push_manager;
    this->push_manager = nullptr;

    delete this->io_thread;
    this->io_thread = nullptr;
    delete this->network_thread;
    this->network_thread = nullptr;
    delete this->file_thread;
    this->file_thread = nullptr;
    log_info("Client finalizado");
  }
}

Client::~Client() {
  if (listen_thread) {
    std::cout << "Deletando listen_thread" << std::endl;
    listen_thread->join();
    delete listen_thread;
    listen_thread = nullptr;
  } else {
    std::cout << "listen_thread já foi deletado" << std::endl;
  }
  delete listen_thread_network_manager;
  listen_thread_network_manager = nullptr;
  delete sync_dir_file_manager;
  sync_dir_file_manager = nullptr;
  delete curr_directory_file_manager;
  curr_directory_file_manager = nullptr;
}
