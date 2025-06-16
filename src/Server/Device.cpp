#include "Device.hpp"
#include "../Utils/FileManager.hpp"
#include "../Utils/logger.hpp"
#include <cstring>
#include <exception>
#include <filesystem>
#include <sstream> // Add this include for std::istringstream
#include <string>
#include <condition_variable>

Device::Device(int command_socket_fd, ClientManager *client_manager, FileManager *file_manager)
    : stop_requested(false), send_push(false), client_manager(client_manager), command_thread(nullptr),
      push_thread(nullptr), file_watcher_thread(nullptr), file_manager(file_manager) {

  command_manager = new NetworkManager(command_socket_fd, "CommandManager");
  push_manager = new NetworkManager("PushManager");
  file_watcher_receiver = new NetworkManager("FileWatcherReceiver");

  int port1 = push_manager->createAndSetupSocket();
  command_manager->sendPacket(t_PORT, 1, std::to_string(port1));
  log_info("Enviado porta do push: %d", port1);

  int port2 = file_watcher_receiver->createAndSetupSocket();
  command_manager->sendPacket(t_PORT, 1, std::to_string(port2));
  log_info("Enviado porta do watcher: %d", port2);

  push_manager->acceptConnection();
  file_watcher_receiver->acceptConnection();
}

Device::~Device() {
  delete command_manager;
  delete push_manager;
  delete file_watcher_receiver;
}



void Device::commandThread() { // thread se comporta recebendo comandos do
                               // cliente e enviando dados para ele
  try {
    log_info("Iniciando thread de comandos IO");
    while (!stop_requested) {
      packet pkt = command_manager->receivePacket();
      
      std::istringstream payload_stream(pkt._payload);

      log_info("Comando recebido: %d", pkt.type);

      if (pkt.type == t_UPLOAD) {
        continue;
      } else if (pkt.type == t_DOWNLOAD) {
        std::string file_name;
        payload_stream >> file_name;
        if (!file_manager->isFileExists(file_name)) {
          command_manager->sendPacket(t_FILE_NOT_FOUND, 1, "");
          continue;
        } else {
          command_manager->sendPacket(t_FILE_FOUND, 1, "");
        }
        command_manager->sendFileInChunks(file_name, MAX_PACKET_SIZE,
                                          *file_manager);

      } else if (pkt.type == t_DELETE) {
        continue;

      } else if (pkt.type == t_LIST) {
        std::string fim = this->file_manager->getFiles();

        // mudar depois para enviar mais pacotes
        // TODO
        if (fim.empty()) {
          fim = "NAO HÁ ARQUIVOS";
        }
        command_manager->sendPacket(t_DATA, 1, vector<char>(fim.begin(), fim.end()));
        command_manager->sendPacket(t_END_OF_FILE, 1, "");
      } else {
        log_error("Comando desconhecido recebido do cliente: %d", pkt.type);
      }
    }
  } catch (const std::runtime_error &e) {
    log_warn("Cliente desconectado: %s", e.what());
    stop_requested = true;
  }
  log_info("Thread de comandos IO finalizada");
  //Liberando PushThread para ser encerrada
  push_cv.notify_one();
}



void Device::pushThread() { 
  try {
    log_info("Iniciando thread de push");
    while (!stop_requested) {

        std::unique_lock<std::mutex> lock(push_mutex);
        push_cv.wait(lock, [this] { return this->send_push.load() || stop_requested; });

        if (stop_requested) break;
        
        log_info("Comando de push recebido", this->push_command.c_str());
        std::istringstream payload_stream(this->push_command);

        if (this->push_type == t_WRITE) {
          std::string file_name;
          payload_stream >> file_name;
          push_manager->sendPacket(t_WRITE, 1, push_command);
          push_manager->sendFileInChunks(file_name, MAX_PACKET_SIZE, *file_manager);
        } else if (this->push_type == t_DELETE) {
          push_manager->sendPacket(t_DELETE, 1, push_command);
        }

        log_info("Push enviado");

        this->send_push = false;
        // TERMINA LOCK
        push_lock.unlock();
      
    }
  } catch (const std::runtime_error &e) {
    stop_requested = true;
    log_warn("Cliente desconectado: %s", e.what());
  }
  log_info("Thread de push finalizada");
}



void Device::fileWatcherThread() {
  log_info("Iniciando thread de file watcher");
  try {
    while (!stop_requested) {
      packet pkt = file_watcher_receiver->receivePacket();
      log_info("Recebido do dispositivo: %d %s", pkt.type, pkt._payload);

      std::string file_name(pkt._payload);

      // NOTE: Never used?
      //   if (first_word == "CREATED") file_manager->createFile(file_name);
      if (pkt.type == t_WRITE) {

        if (!file_manager->isFileExists(file_name)) {
          file_manager->createFile(file_name);
        }
        // recebe uma copia do arquivo e verifica se mudou o hash
        log_info("Verificando hash do arquivo");
        std::string copy_name = "copy_" + file_name;
        file_manager->createFile(copy_name);
        buildFile(copy_name);

        std::string path_original = "./sync_dir_" + this->client_manager->getUsername() + "/" + file_name;
        std::string path_copy = "./sync_dir_" + this->client_manager->getUsername() + "/" + copy_name;
        if (file_manager->checkFileHashchanged(path_original, path_copy)) {
          // sobrescreve o arquivo original pela copia
          log_info("Mudou o HASH, sobrescrevendo o arquivo original");
          file_manager->deleteFile(file_name);
          file_manager->renameFile(path_copy, path_original);

          log_info("Propagando mudança para outros dispositivos");
          this->client_manager->handle_new_push(pkt, this);
        } else {
          // deleta a copia
          log_info("Não mudou o HASH, não será propagado");
          file_manager->deleteFile(copy_name);
        }

      } else if (pkt.type == t_DELETE) {
        if (file_manager->isFileExists(file_name)) {
          log_info("Deletando arquivo: %s", file_name.c_str());
          file_manager->deleteFile(file_name);
          this->client_manager->handle_new_push(pkt, this);
        } else {
          log_info("Arquivo não existe, não será deletado: %s", file_name.c_str());
        }
      }
    }
  } catch (const std::runtime_error &e) {
    log_warn("Cliente desconectado: %s", e.what());
    stop_requested = true;
  }
  log_info("Thread de File watcher finalizada");
}

void Device::start() {
  command_thread = new thread(&Device::commandThread, this);
  push_thread = new thread(&Device::pushThread, this);
  file_watcher_thread = new thread(&Device::fileWatcherThread, this);

  // espera o join
  if (command_thread->joinable()) {
    command_thread->join();
  }
  if (push_thread->joinable()) {
    push_thread->join();
  }
  if (file_watcher_thread->joinable()) {
    file_watcher_thread->join();
  }
  client_manager->removeDevice(this);
}



void Device::stop() {
  stop_requested = true;

  log_info("Parando dispositivo...stop_requested");
  // Fechar conexões de socket
  command_manager->closeConnection();
  push_manager->closeConnection();
  file_watcher_receiver->closeConnection();

  // espera as threads terminarem
  if (command_thread->joinable() &&
      command_thread->get_id() != std::this_thread::get_id()) {
    command_thread->join();
  }
  if (push_thread->joinable() &&
      push_thread->get_id() != std::this_thread::get_id()) {
    push_thread->join();
  }
  if (file_watcher_thread->joinable() &&
      file_watcher_thread->get_id() != std::this_thread::get_id()) {
    file_watcher_thread->join();
  }

  log_info("Dispositivo parado");

  delete command_thread;
  delete push_thread;
  delete file_watcher_thread;

  command_thread = nullptr;
  push_thread = nullptr;
  file_watcher_thread = nullptr;
}

bool Device::isStopRequested() { return stop_requested; }

// void sendFileTo(std::string &file_path);

void Device::sendPushTo(int type, std::string &command) {
  // // vai criar uma nova thread executando a funcao sendFileInChunks do push_manager
  // std::thread send_thread([this, file_path]() {
  //   push_manager->sendFileInChunks(file_path, MAX_PACKET_SIZE, *file_manager);
  // })
  push_lock.lock();
  this->push_command = command;
  this->push_type = type;
  this->send_push = true;
  push_cv.notify_one();
  // push_thread = new thread(&Device::pushThread, this);
}

void Device::buildFile(std::string &file_name) {
  bool stop = false;
  log_info("Recebendo arquivo: %s", file_name.c_str());
  while (!stop) {
    packet pkt_received = file_watcher_receiver->receivePacket();
    log_info("Recebido do dispositivo pacote de SEQ: %d", pkt_received.seqn);

    if (std::string(pkt_received._payload, pkt_received.length) == "END_OF_FILE") {
      stop = true;
      break;
    }

    std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
    file_manager->writeFile(file_name, data);
  }
}
