#include "Device.hpp"
#include <cstring>
#include <exception>
#include <sstream> // Add this include for std::istringstream
#include <string>

Device::Device(int command_socket_fd, ClientManager *client_manager)
    : stop_requested(false), command_thread(nullptr), push_thread(nullptr),
      file_watcher_thread(nullptr), client_manager(client_manager),
      file_manager(file_manager) {

  // Inicializa os sockets
  command_manager = new NetworkManager(command_socket_fd, "CommandManager");

  push_manager = new NetworkManager("PushManager");
  file_watcher_receiver = new NetworkManager("FileWatcherReceiver");

  int port1 = push_manager->createAndSetupSocket();
  std::string command1 = "PORT " + std::to_string(port1);
  command_manager->sendPacket(
      CMD, 1,
      command1); // Envia o comando para o cliente com a porta do push_manager

  int port2 = file_watcher_receiver->createAndSetupSocket();
  std::string command2 = "PORT " + std::to_string(port2);
  command_manager->sendPacket(CMD, 1,
                              command2); // Envia o comando para o cliente com a

  // push_manager->acceptConnection();
  std::cout << "AAAAAA" << std::endl;
  file_watcher_receiver->acceptConnection();

  // std::cout << "Recebido do dispositivo: " << pkt._payload << std::endl;
  // std::cout << "Recebido do dispositivo: " << pkt2._payload << std::endl;
}

Device::~Device() {
  delete command_manager;
  delete push_manager;
  delete file_watcher_receiver;
}

void Device::commandThread() { // thread se comporta recebendo comandos do
                               // cliente e enviando dados para ele
  try {
    std::cout << "Iniciando thread de comandos..." << std::endl;
    while (!stop_requested) {
      command_manager->receivePacket();
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Cliente desconectado: " << e.what() << std::endl;
    stop_requested = true;
  }
  std::cout << "Command thread finished" << std::endl;
}

void Device::pushThread() { // thread se comporta somente enviando dados ao
                            // cliente, nao recebe
  try {
    std::cout << "Iniciando thread de push..." << std::endl;
    while (!stop_requested) {
      // push_manager->receivePacket();
    }
  } catch (const std::runtime_error &e) {
    stop_requested = true;
    std::cout << "Cliente desconectado: " << e.what()
              << "stop_requested: " << stop_requested << std::endl;
  }
  std::cout << "Push thread finished" << std::endl;
}

void Device::fileWatcherThread() { // thread se comporta somente recebendo dados
                                   // do cliente,nao envia
  // tipos de pacotes que essa thread pode receber:
  //  CMD(WRITE/CREATED DELETED)
  //  DATA
  try {
    std::cout << "Iniciando thread de file watcher..." << std::endl;
    while (!stop_requested) {
      // interpretar o pacote aqui
      // tem que fazer uma funcao para interpretar o pacote
      //  se for CREATED/WRITE
      //  alocar o buffer
      //  receber o nome do arquivo
      //  recebe o vector de char para escrever
      // formato de pacote: "DOWNLOAD nomeDOarquivo" (payload)
      // se for DELETED
      // deletar o arquivo

      packet pkt = file_watcher_receiver->receivePacket();
      std::cout << "Recebido do dispositivo: " << pkt._payload << std::endl;

      // Extract the first word from the payload
      std::istringstream payload_stream(pkt._payload);
      std::string first_word;
      payload_stream >> first_word;

      std::cout << "Primeira palavra do payload: " << first_word << std::endl;

      // Interpret the packet based on the first word
      if (first_word == "CREATED") {
        // Handle DOWNLOAD command

      } else if (first_word == "WRITE") {
        // Handle DELETED command
        std::string second_word;
        payload_stream >> second_word;

        file_manager->clearFile(second_word);
        bool stop = false;
        while (!stop) {
          packet pkt_received = file_watcher_receiver->receivePacket();
          // recebe o pacote de dados e vai escrevendo
          // quando receber o end of file stop = true
        }

      } else if (first_word == "DELETED") {
        // Handle WRITE command
      }
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Cliente desconectado: " << e.what() << std::endl;
    stop_requested = true;
  }
  std::cout << "File watcher thread finished" << std::endl;
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

  std::cout << "Parando dispositivo...stop_requested: " << stop_requested
            << std::endl;
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

  std::cout << "Device Parado" << std::endl;

  delete command_thread;
  delete push_thread;
  delete file_watcher_thread;

  command_thread = nullptr;
  push_thread = nullptr;
  file_watcher_thread = nullptr;
}

bool Device::isStopRequested() { return stop_requested; }
