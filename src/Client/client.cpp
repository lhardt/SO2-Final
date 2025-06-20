#include "client.hpp"
#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/logger.hpp"
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
  log_info("Inicializado IO Thread com ID: %ld ", std::this_thread::get_id());

  string cmdline;
  smatch cmdarg;

  std::cout << "Esperando comando..." << std::endl;
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
      command_manager->sendPacket(t_DOWNLOAD, 1, file_name); // espera resposta do servidor...
      packet response = command_manager->receivePacket();
      if (response.type == t_FILE_NOT_FOUND) {
        log_info("Arquivo não encontrado no servidor.");
        continue;
      } else if (std::string(response._payload, response.length) == "FILE_FOUND") {
        log_info("Arquivo encontrado no servidor.");
      }
      curr_directory_file_manager->createFile(file_name);
      bool stop = false;
      log_info("Escrevendo arquivo: %s", file_name.c_str());
      while (!stop) {
        packet pkt_received = command_manager->receivePacket();
        if (pkt_received.type == t_END_OF_FILE) {
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
      command_manager->sendPacket(t_LIST, 1);
      while (!stop) {
        packet pkt_received = command_manager->receivePacket();

        if (pkt_received.type == t_END_OF_FILE) {
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
  }
}

void Client::handleFileThread() {
  log_info("Inicializado File Thread com ID %ld, e porta %d ", std::this_thread::get_id(), this->file_watcher_port);
  static constexpr char *sync_dir = "sync_dir";

  log_info("File thread conectado na porta: %d", this->file_watcher_port);

  NetworkManager file_watcher_manager("FileWatcherManager",this->server_ip, this->file_watcher_port);

  int inotifyFd = inotify_init1(IN_NONBLOCK);
  if (inotifyFd < 0) {
    log_error("Não foi possível inicializar inotify");
    return;
  }
  int watchDescriptor =
      inotify_add_watch(inotifyFd, sync_dir, IN_DELETE | IN_CLOSE_WRITE);
  if (watchDescriptor < 0) {
    log_error("Não foi possível inicializar watchDescriptor");
    return;
  }

  size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
  char buffer[BUF_LEN];

  log_info("Monitorando folder: %s", sync_dir);

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
          watcher_push_lock.lock();
          file_watcher_manager.sendPacket(t_WRITE, 1, file_name);
          file_watcher_manager.sendFileInChunks(file_name, MAX_PACKET_SIZE, *sync_dir_file_manager);
          watcher_push_lock.unlock();

        } else if (event->mask & IN_DELETE) {
          log_info("Arquivo removido: %s", filepath.c_str());
          file_watcher_manager.sendPacket(t_DELETE, 1, event->name);
        }
      }

      i += sizeof(struct inotify_event) + event->len;
    }
  }
}

void Client::handlePushThread() {
  log_info("Inicializado Push Thread com ID %ld ", std::this_thread::get_id());

  int sock = connect_to_socket(this->server_ip, this->push_port);
  if (sock <= 0) {
    log_error("Erro ao criar socket");
    return;
  }

  log_info("Push thread conectado na porta: %d", this->file_watcher_port);

  NetworkManager push_receiver(sock);

  while (true) {
    log_info("Aguardando push do servidor...");
    packet pkt = push_receiver.receivePacket();
    std::istringstream payload_stream(pkt._payload);

    if (pkt.type == t_WRITE) {
      std::string file_name;
      payload_stream >> file_name;

      sync_dir_file_manager->clearFile(file_name);

      bool stop = false;
      log_info("Recebendo arquivo: %s", file_name.c_str());
      watcher_push_lock.lock();
      while (!stop) {
        packet pkt_received = push_receiver.receivePacket();
        std::string pkt_string = std::string(pkt_received._payload, pkt_received.length);

        if (pkt.type == t_END_OF_FILE) {
          stop = true;
          break;
        }

        std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
        sync_dir_file_manager->writeFile(file_name, data);
      }
      watcher_push_lock.unlock();

    } else if (pkt.type == t_DELETE) {

      std::string file_name;
      payload_stream >> file_name;
      sync_dir_file_manager->deleteFile(file_name);

    } else {
      log_error("Comando desconhecido recebido do servidor: %d. ", pkt.type);
    }
  }
}

Client::Client(std::string _client_name, std::string _server_ip,
               std::string _server_port)
    : client_name(_client_name), server_ip(_server_ip),
      server_port(_server_port) {
  int server_port_int = std::stoi(server_port);

  // é o socket de commandos, tem que passar para a thread de IO
  this->command_manager = new NetworkManager("CommandManager", server_ip, server_port_int);
  command_manager->sendPacket(t_CLIENT, 1, client_name);

  // recebe o primeiro pacote do server
  packet pkt = command_manager->receivePacket();
  // payload do pacote está no formato PORT <port>

  if( pkt.type == t_ERROR ){
    log_error("Erro do servidor! %s. ", pkt._payload);
    throw std::runtime_error(pkt._payload);
  }
  log_assert( pkt.type == t_PORT, "Received package of wrong type! %d. ", pkt.type );
  std::string port_str(pkt._payload);
  int port = std::stoi(port_str);
  this->push_port = port;

  // recebe o segundo pacote do server
  packet pkt2 = command_manager->receivePacket();
  std::string port_str2(pkt2._payload);
  int port2 = std::stoi(port_str2);
  this->file_watcher_port = port2;

  this->sync_dir_file_manager = new FileManager("sync_dir");
  this->curr_directory_file_manager = new FileManager("./");

  this->io_thread = std::thread(g_handleIoThread, this);
  this->network_thread = std::thread(g_handlePushThread, this);
  this->file_thread = std::thread(g_handleFileThread, this);

  io_thread.join();
  network_thread.join();
  file_thread.join();
}
