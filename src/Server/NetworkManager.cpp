#include "NetworkManager.hpp"
#include "../logger.hpp"
#include "FileManager.hpp"
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

NetworkManager::NetworkManager(const std::string &name)
    : socket_fd(-1), name(name) {}
NetworkManager::NetworkManager(int socket_fd, const std::string &name)
    : socket_fd(socket_fd), name(name) {
  if (socket_fd == -1) {
    throw std::runtime_error("Invalid socket file descriptor");
  }
}

void NetworkManager::sendPacket(packet *p) {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
  }
  if (!isPacketValid(*p)) {
    throw std::runtime_error("Invalid packet");
  }
  size_t payload_length = p->length;
  size_t buffer_size = HEADER_SIZE + payload_length;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);

  serializePacket(*p, buffer.get(), buffer_size);

  size_t total_sent = 0;

  while (total_sent < buffer_size) {
    ssize_t sent =
        send(socket_fd, buffer.get() + total_sent, buffer_size - total_sent, 0);
    if (sent == -1) {
      throw std::runtime_error("Failed to send packet");
    }
    total_sent += sent;
  }
};

void NetworkManager::sendPacket(uint16_t type, uint16_t seqn, const std::vector<char> &payload) {
  checkSocketInitialized();

  packet pkt;
  pkt.type = type;
  pkt.seqn = seqn;
  pkt.length = payload.size();
  pkt.total_size = HEADER_SIZE + pkt.length;

  pkt._payload = new char[payload.size()];
  if (pkt._payload != nullptr) {
    std::memcpy(pkt._payload, payload.data(), payload.size());
  }

  sendPacket(&pkt);

  delete[] pkt._payload;
  pkt._payload = nullptr;
}

void NetworkManager::closeSocket() {
  if (socket_fd != -1) {
    close(socket_fd);
    socket_fd = -1;
  }
}

void NetworkManager::checkSocketInitialized() {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
  }
}

bool NetworkManager::isPacketValid(const packet &pkt) {
  // Verifica se o tamanho total do pacote é válido
  if (pkt.total_size < HEADER_SIZE || pkt.length > MAX_PACKET_SIZE) {
    return false;
  }

  // Verifica se o payload é nulo quando o comprimento é zero
  if (pkt.length == 0 && pkt._payload != nullptr) {
    return false;
  }

  return true;
}

std::unique_ptr<char[]> NetworkManager::receiveHeader() {
  std::unique_ptr<char[]> header_buffer(new char[HEADER_SIZE]);
  size_t total_received = 0;

  while (total_received < HEADER_SIZE) {
    ssize_t received = recv(socket_fd, header_buffer.get() + total_received,
                            HEADER_SIZE - total_received, 0);
    if (received == -1) {
      perror("recv"); // Log the error
      throw std::runtime_error("Failed to receive packet header\n");
    } else if (received == 0) {
      // Connection closed by the client
      std::cerr << "Connection closed by peer during header reception"
                << std::endl;
      throw std::runtime_error("Connection closed by peer\n");
    }
    total_received += received;
  }

  return header_buffer;
}

packet NetworkManager::deserializeHeader(const char *header_buffer) {
  packet pkt;
  size_t offset = 0;

  u_int16_t net_type;
  u_int16_t net_seqn;
  u_int32_t net_total_size;
  u_int16_t net_length;

  std::memcpy(&net_type, header_buffer + offset, sizeof(net_type));
  offset += sizeof(net_type);
  std::memcpy(&net_seqn, header_buffer + offset, sizeof(net_seqn));
  offset += sizeof(net_seqn);
  std::memcpy(&net_total_size, header_buffer + offset, sizeof(net_total_size));
  offset += sizeof(net_total_size);
  std::memcpy(&net_length, header_buffer + offset, sizeof(net_length));
  offset += sizeof(net_length);

  pkt.type = ntohs(net_type);
  pkt.seqn = ntohs(net_seqn);
  pkt.total_size = ntohl(net_total_size);
  pkt.length = ntohs(net_length);

  return pkt;
}

void NetworkManager::receivePayload(packet &pkt) {
  if (pkt.length > 0) {
    pkt._payload = new char[pkt.length + 1];
    if (pkt._payload == nullptr) {
      throw std::runtime_error("Failed to allocate memory for payload");
    }

    size_t total_payload_received = 0;
    while (total_payload_received < pkt.length) {
      ssize_t received = recv(
          socket_fd, const_cast<char *>(pkt._payload) + total_payload_received,
          pkt.length - total_payload_received, 0);
      if (received == -1) {
        delete[] pkt._payload; // Clean up allocated memory
        throw std::runtime_error("Failed to receive packet payload\n");
      }
      total_payload_received += received;
    }
    const_cast<char *>(pkt._payload)[pkt.length] = '\0'; // Null-terminate
  } else {
    pkt._payload = nullptr;
  }
}

packet NetworkManager::receivePacket() {
  checkSocketInitialized();
  auto header_buffer = receiveHeader();
  packet pkt = deserializeHeader(header_buffer.get());

  receivePayload(pkt);

  return pkt;
}

void NetworkManager::sendFileInChunks(const std::string &filepath, const size_t bufferSize, FileManager &fileManager) {

  try {
    // Lê o arquivo inteiro como um vetor de bytes
    std::vector<char> fileData = fileManager.readFile(filepath);

    size_t totalSize = fileData.size();
    size_t offset = 0;
    uint16_t sequenceNumber = 0;

    size_t totalPackets = (totalSize + bufferSize - 1) / bufferSize;
    // Divide o arquivo em pedaços e envia cada pedaço
    while (offset < totalSize) {
      size_t chunkSize = std::min(bufferSize, totalSize - offset);

      std::string payload(fileData.begin() + offset,
                          fileData.begin() + offset + chunkSize);

      std::string command = payload;
      sendPacket(DATA, sequenceNumber++, std::vector<char>(command.begin(), command.end()));
      log_info("Sent chunk of size %zu from file: %s, %d/%d", chunkSize,
               filepath.c_str(), sequenceNumber, totalPackets);

      offset += chunkSize;
    }

    // Envia um pacote final indicando o término do envio
    std::string command = "END_OF_FILE";
    sendPacket(CMD, sequenceNumber, std::vector<char>(command.begin(), command.end()));
    log_info("Finished sending file: %s", filepath.c_str());
  } catch (const std::exception &e) {
    log_error("Error reading or sending file: %s", e.what());
  }
}

void NetworkManager::serializePacket(const packet &pkt, char *buffer,
                                     size_t buffer_size) {
  if (buffer_size < HEADER_SIZE + pkt.length) {
    throw std::runtime_error("Buffer size is too small for serialization");
  }

  u_int16_t net_type = htons(pkt.type);
  u_int16_t net_seqn = htons(pkt.seqn);
  u_int32_t net_total_size = htonl(pkt.total_size);
  u_int16_t net_length = htons(pkt.length);

  size_t offset = 0;
  std::memcpy(buffer + offset, &net_type, sizeof(net_type));
  offset += sizeof(net_type);
  std::memcpy(buffer + offset, &net_seqn, sizeof(net_seqn));
  offset += sizeof(net_seqn);
  std::memcpy(buffer + offset, &net_total_size, sizeof(net_total_size));
  offset += sizeof(net_total_size);
  std::memcpy(buffer + offset, &net_length, sizeof(net_length));
  offset += sizeof(net_length);

  if (pkt._payload != nullptr && pkt.length > 0) {
    std::memcpy(buffer + offset, pkt._payload, pkt.length);
  }

  // Printar o buffer serializado em formato hexadecimal
  // std::cout << "Pacote serializado (hex): ";
  // for (size_t i = 0; i < buffer_size; ++i) {
  //   std::cout << std::hex << std::uppercase
  //             << (unsigned int)(unsigned char)buffer[i] << " ";
  // }
  // std::cout << std::dec << std::endl; // Voltar para decimal
}

packet NetworkManager::deserializePacket(const char *buffer,
                                         size_t buffer_size) {
  if (buffer_size < HEADER_SIZE) {
    throw std::runtime_error("Buffer size is too small");
  }

  packet pkt;
  size_t offset = 0;

  u_int16_t net_type;
  u_int16_t net_seqn;
  u_int32_t net_total_size;
  u_int16_t net_length;

  // Printar o buffer serializado em formato hexadecimal
  // std::cout << "Pacote antes de ser deserializado (hex): ";
  // for (size_t i = 0; i < buffer_size; ++i) {
  //   std::cout << std::hex << std::uppercase
  //             << (unsigned int)(unsigned char)buffer[i] << " ";
  // }
  // std::cout << std::dec << std::endl; // Voltar para decimal

  std::memcpy(&net_type, buffer + offset, sizeof(net_type));
  offset += sizeof(net_type);
  std::memcpy(&net_seqn, buffer + offset, sizeof(net_seqn));
  offset += sizeof(net_seqn);
  std::memcpy(&net_total_size, buffer + offset, sizeof(net_total_size));
  offset += sizeof(net_total_size);
  std::memcpy(&net_length, buffer + offset, sizeof(net_length));
  offset += sizeof(net_length);

  pkt.type = ntohs(net_type);
  pkt.seqn = ntohs(net_seqn);
  pkt.total_size = ntohl(net_total_size);
  pkt.length = ntohs(net_length);

  if (pkt.length > buffer_size - HEADER_SIZE || pkt.length < 0) {
    throw std::runtime_error("Invalid packet length");
  }

  pkt._payload = new char[pkt.length + 1];
  if (pkt._payload == nullptr) {
    throw std::runtime_error("Failed to allocate memory for payload");
  }

  std::memcpy(const_cast<char *>(pkt._payload), buffer + offset, pkt.length);
  const_cast<char *>(pkt._payload)[pkt.length] = '\0';

  return pkt;
}

int NetworkManager::createAndSetupSocket() {
  // Cria o socket de escuta
  int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket_fd == -1) {
    throw std::runtime_error("Falha ao criar socket de escuta");
  }

  // Configura o endereço e porta
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0; // Porta será atribuída automaticamente pelo SO

  // Faz o bind do socket
  if (bind(listen_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    close(listen_socket_fd);
    throw std::runtime_error("Falha ao fazer bind no socket");
  }

  // Coloca o socket em modo de escuta
  if (listen(listen_socket_fd, 1) == -1) {
    close(listen_socket_fd);
    throw std::runtime_error("Falha ao fazer listen no socket");
  }

  // Obtém a porta atribuída automaticamente
  socklen_t addrlen = sizeof(addr);
  if (getsockname(listen_socket_fd, (struct sockaddr *)&addr, &addrlen) == -1) {
    close(listen_socket_fd);
    throw std::runtime_error("Falha ao obter informações do socket");
  }

  int port = ntohs(addr.sin_port);
  std::cout << "Socket de escuta criado na porta: " << port << std::endl;

  // Armazena o socket de escuta no atributo do objeto
  this->socket_fd = listen_socket_fd;

  // Retorna a porta para o chamador
  return port;
}

void NetworkManager::acceptConnection() {
  // std::cout << "esperando conexao na porta: " << socket_fd << std::endl;
  if (socket_fd == -1) {
    throw std::runtime_error("Socket não inicializado para aceitar conexões");
  }
  // Obtém a porta do socket
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (getsockname(socket_fd, (struct sockaddr *)&addr, &addrlen) == -1) {
    throw std::runtime_error("Falha ao obter informações do socket");
  }
  int port = ntohs(addr.sin_port);

  std::cout << "Esperando conexão na porta: " << port << std::endl;

  sockaddr_in client_addr;
  socklen_t client_addrlen = sizeof(client_addr);
  int client_socket_fd =
      accept(socket_fd, (struct sockaddr *)&client_addr, &client_addrlen);
  if (client_socket_fd == -1) {
    throw std::runtime_error("Falha ao aceitar conexão do cliente");
  }

  std::cout << "Conexão aceita de cliente!" << std::endl;

  // Fecha o socket de escuta
  close(socket_fd);

  // Armazena o socket do cliente no atributo do objeto
  this->socket_fd = client_socket_fd;
}

void NetworkManager::closeConnection() {
  if (socket_fd != -1) {
    shutdown(socket_fd, SHUT_RDWR); // garante despertar o recv()
    close(socket_fd);
    socket_fd = -1;
    std::cout << name << " Fechou conexão!" << std::endl;
  }
}
