#include "Device.hpp"
#include "FileManager.hpp"
#include <cstring>
#include <exception>
#include <filesystem>
#include <sstream> // Add this include for std::istringstream
#include <string>

Device::Device(int command_socket_fd, ClientManager *client_manager, FileManager *file_manager)
    : stop_requested(false), send_push(false),command_thread(nullptr), push_thread(nullptr),
      file_watcher_thread(nullptr), client_manager(client_manager),
      file_manager(file_manager) {

  // Inicializa os sockets
  command_manager = new NetworkManager(command_socket_fd, "CommandManager");

  push_manager = new NetworkManager("PushManager");
  file_watcher_receiver = new NetworkManager("FileWatcherReceiver");

  int port1 = push_manager->createAndSetupSocket();
  std::string command = "PORT " + std::to_string(port1);
  command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end())); // Envia o comando para o cliente com a porta do push_manager
  std::cout<<"porta do push: " + std::to_string(port1);

  int port2 = file_watcher_receiver->createAndSetupSocket();
  std::string command2 = "PORT " + std::to_string(port2);
  command_manager->sendPacket(CMD, 1, std::vector<char>(command2.begin(), command2.end())); // Envia o comando para o cliente com a
  std::cout<<"porta do watcher: " + std::to_string(port2);

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
    std::cout << "Iniciando thread de comandos..." << std::endl;
    while (!stop_requested) {
      packet pkt = command_manager->receivePacket();
      // interpretar os comandos
      std::istringstream payload_stream(pkt._payload);
      std::string command_keyword;
      payload_stream >> command_keyword;

      std::cout << "COMANDO RECEBIDO: " << command_keyword << std::endl;

      if (command_keyword == "UPLOAD") {

        continue;

      } else if (command_keyword == "DOWNLOAD") {
        std::string file_name;
        payload_stream >> file_name;
        if (!file_manager->isFileExists(file_name)) {
          std::string command = "FILE_NOT_FOUND";
          command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
          continue;
        } else {
          std::string command = "FILE_FOUND";
          command_manager->sendPacket(CMD, 1, std::vector<char>(command.begin(), command.end()));
        }
        command_manager->sendFileInChunks(file_name, MAX_PACKET_SIZE,
                                          *file_manager);

      } else if (command_keyword == "DELETE") {
        std::string file_name;
        payload_stream >> file_name;
        file_manager->deleteFile(file_name);

      }
       else if (command_keyword == "LIST") {
        namespace fs = std::filesystem;
        fs::path diretorio = "sync_dir_" + client_manager->getUsername();
        std::ostringstream oss;

        try {
          for (const auto &entry : fs::directory_iterator(diretorio)) {
            if (entry.is_regular_file()) {
              oss << entry.path().filename().string() << " ";
            }
          }
        } catch (const fs::filesystem_error &e) {
          std::cerr << "Erro: " << e.what() << std::endl;
        }
        std::string fim = oss.str();

        // mudar depois para enviar mais pacotes
        // TODO
        if (fim.empty()) {
          fim = "NAO HÁ ARQUIVOS";
        }
        command_manager->sendPacket(DATA, 1, vector<char>(fim.begin(), fim.end()));
        std::string end_of_file = "END_OF_FILE";
        command_manager->sendPacket(CMD, 1, vector<char>(end_of_file.begin(), end_of_file.end()));
        std::cout << "Terminando o LIST\n";
      } else {
        cout << "TA ERRADO!\n";
      }
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
      
      if(this->send_push){
        
        std::cout<<"COMANDO DO PUSH: "+this->push_command<<std::endl;
        std::istringstream payload_stream(this->push_command);
        // primeira palavra do push_command (PUSH ou DELETE)
        std::string command_keyword;
        payload_stream >> command_keyword;
        
        if (command_keyword == "WRITE") {

          std::string file_name;
          payload_stream >> file_name;
          std::cout<<"MANDANDO PUSH PARA DEVICE...\n";
          push_manager->sendPacket(CMD,1,vector<char>(push_command.begin(),push_command.end()));
          push_manager->sendFileInChunks(file_name,MAX_PACKET_SIZE,*file_manager);

        }else if (command_keyword == "DELETE"){

          push_manager->sendPacket(CMD,1,vector<char>(push_command.begin(),push_command.end()));
          
        }

        std::cout<<"TERMINOU O PUSH...\n";

        this->send_push=false;
        //TERMINA LOCK
        push_lock.unlock();
      }
  
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

      // Extrai a primeira palavra do PAYLOAD (QUE SEMPRE SERÁ CREATED, WRITE,
      // DELETED)
      std::istringstream payload_stream(pkt._payload);
      std::string first_word;
      payload_stream >> first_word;

      std::cout << "Primeira palavra do payload: " << first_word << std::endl;

      // Interpret the packet based on the first word
      if (first_word == "CREATED") { 

        std::string file_name;
        payload_stream >> file_name;
        file_manager->createFile(file_name);

      } else if (first_word == "WRITE") {
        
        // VERIFICAR SE O ARQUIVO FOI ATUALIZADO

        
        std::string file_name;
        payload_stream >> file_name;


        if (!file_manager->isFileExists(file_name)) {
          file_manager->createFile(file_name);
          buildFile(file_name); // FUNCAO QUE RECEBE PACOTES E VAI ESCREVENDO EM ARQUIVO
        }else{
          //recebe uma copia do arquivo e verifica se mudou o hash
          std::cout<<"verificando HASH..."<<std::endl;
          std::string copy_name="copy_"+file_name;
          file_manager->createFile(copy_name);
          buildFile(copy_name);
          std::cout<<"TERMINOU DE RECEBER A COPIA"<<std::endl;
          
          std::string path_original="./sync_dir_"+this->client_manager->getUsername()+"/"+file_name;
          std::string path_copy="./sync_dir_"+this->client_manager->getUsername()+"/"+copy_name;
          if(file_manager->checkFileHashchanged(path_original,path_copy)){
            //sobrescreve o arquivo original pela copia
            std::cout<<"Mudou o HASH..."<<std::endl;
            file_manager->deleteFile(file_name);
            file_manager->renameFile(path_copy,path_original);

            std::cout<<"mandando handle new push com payload: "<<pkt._payload<<std::endl;
            this->client_manager->handle_new_push(pkt._payload,this);
          }
          else{
            //deleta a copia
            std::cout<<"Não mudou o HASH..."<<std::endl;
            file_manager->deleteFile(copy_name);
          }
        }
        //file_manager->clearFile(file_name);

        std::cout << "escrito no arquivo: " << file_name << std::endl;

      } else if (first_word == "DELETE") {
        std::string file_name;
        payload_stream >> file_name;
        // verifica se o arquivo existe antes de deletar
        if (file_manager->isFileExists(file_name)) {
          std::cout << "deletando arquivo: " << file_name << std::endl;
          file_manager->deleteFile(file_name);
        }
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

  std::cout << "Parando dispositivo...stop_requested: " << stop_requested << std::endl;
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



//void sendFileTo(std::string &file_path);



void Device::sendPushTo(std::string &command){
  // // vai criar uma nova thread executando a funcao sendFileInChunks do push_manager
  // std::thread send_thread([this, file_path]() {
  //   push_manager->sendFileInChunks(file_path, MAX_PACKET_SIZE, *file_manager);
  // })
  push_lock.lock();
  this->push_command=command;
  this->send_push=true;
  // push_thread = new thread(&Device::pushThread, this);
  
}

void Device::buildFile(std::string &file_name){
  bool stop = false;
          std::cout << "recebendo arquivo: " << file_name << std::endl;
          while (!stop) {
            packet pkt_received = file_watcher_receiver->receivePacket();
            std::cout << "Recebido do dispositivo: " << pkt_received._payload
                      << std::endl;
            if (std::string(pkt_received._payload, pkt_received.length) ==
                "END_OF_FILE") {
              stop = true;
              break;
            } else {
              std::cout << "Waiting for more Data..." << std::endl;
            }

            std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
            file_manager->writeFile(file_name, data);
          }
}
