#include "Command.hpp"
#include "NetworkManager.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

void Command::undo() {
  log_warn("Operation Undoable");
}

SendMessageCommand::SendMessageCommand(std::string msg, NetworkManager *networkManager) : msg(msg), networkManager(networkManager) {}

void SendMessageCommand::execute() {
  networkManager->sendPacket(CMD, 1, std::vector<char>(msg.begin(), msg.end()));
}

SendFileCommand::SendFileCommand(std::string &file_path, NetworkManager *networkManager, FileManager *fileManager) : file_path(file_path), networkManager(networkManager), fileManager(fileManager) {};

void SendFileCommand::execute() {
  if (!fileManager->isFileExists(file_path)) {
    SendMessageCommand cmd("FILE_NOT_FOUND", networkManager);
    cmd.execute();
  }
  try {
    // size_t totalSize = fileManager->getFileSize(file_path);
    std::vector<char> fileData = fileManager->readFile(file_path);
    size_t totalSize = fileData.size();
    size_t offset = 0;
    uint16_t sequenceNumber = 0;
    size_t buffer_size = MAX_PACKET_SIZE;

    size_t totalPackets = (totalSize + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    // Divide o arquivo em pedaços e envia cada pedaço
    while (offset < totalSize) {
      size_t chunkSize = std::min(buffer_size, totalSize - offset);

      std::string payload(fileData.begin() + offset,
                          fileData.begin() + offset + chunkSize);

      std::string command = payload;
      networkManager->sendPacket(DATA, sequenceNumber++, std::vector<char>(command.begin(), command.end()));
      log_info("Enviado chunck de tamanho %zu do arquivo: %s, %d/%d", chunkSize,
               file_path.c_str(), sequenceNumber, totalPackets);

      offset += chunkSize;
    }

    SendMessageCommand cmd("END_OF_FILE", networkManager);
    cmd.execute();
    log_info("Arquivo enviado com sucesso: %s", file_path.c_str());
  } catch (const std::exception &e) {
    log_error("Falha ao enviar ou ler arquivo: %s", e.what());
  }
};

ReceiveFileCommand::ReceiveFileCommand(std::string &file_name, NetworkManager *networkManager, FileManager *fileManager) : file_name(file_name), networkManager(networkManager), fileManager(fileManager) {};

void ReceiveFileCommand::execute() {
  bool stop = false;
  log_info("Recebendo arquivo: %s", file_name.c_str());
  while (!stop) {
    packet pkt_received = networkManager->receivePacket();
    log_info("Recebido do dispositivo pacote de SEQ: %d", pkt_received.seqn);

    if (std::string(pkt_received._payload, pkt_received.length) == "END_OF_FILE") {
      stop = true;
      break;
    }

    std::vector<char> data(pkt_received._payload, pkt_received._payload + pkt_received.length);
    fileManager->writeFile(file_name, data);
  }
}

LocalDeleteCommand::LocalDeleteCommand(std::string &file_path, FileManager *fileManager) : file_path(file_path), fileManager(fileManager) {};

void LocalDeleteCommand::execute() {
  fileManager->deleteFile(file_path);
}

NetworkDeleteCommand::NetworkDeleteCommand(std::string &file_path, NetworkManager *networkManager, FileManager *fileManager) : file_path(file_path), networkManager(networkManager), fileManager(fileManager) {}

void NetworkDeleteCommand::execute() {
}

ReceiveMessageCommand::ReceiveMessageCommand(NetworkManager *networkManager) : networkManager(networkManager) {}

void ReceiveMessageCommand::execute() {
  packet pkt = networkManager->receivePacket();
  this->message = pkt._payload;
}
std::string ReceiveMessageCommand::getMessage() {
  return this->message;
}
