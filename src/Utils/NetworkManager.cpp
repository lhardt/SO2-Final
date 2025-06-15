#include "NetworkManager.hpp"
#include "FileManager.hpp"
#include "logger.hpp"
#include <arpa/inet.h>
#include <thread>
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

NetworkManager::NetworkManager(std::string name, std::string ip, int port){
  int sock = connect_to_socket(ip, port);
  this->socket_fd = sock;
  this->name = name;
  
  if( sock <= 0) throw std::runtime_error("Socket wasn't created succesfully!");

  log_info("Network Manager [%s] conectou a [%s : %d]!", name.c_str(), ip.c_str(), port);
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
      throw std::runtime_error("Failed to send packet on " + this->name);
    }
    total_sent += sent;
  }
};

void NetworkManager::sendPacket(uint16_t type, uint16_t seqn,
                                const std::vector<char> &payload) {
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
      sendPacket(DATA, sequenceNumber++,
                 std::vector<char>(command.begin(), command.end()));
      log_info("Enviado chunck de tamanho %zu do arquivo: %s, %d/%d", chunkSize,
               filepath.c_str(), sequenceNumber, totalPackets);

      offset += chunkSize;
    }

    // Envia um pacote final indicando o término do envio
    std::string command = "END_OF_FILE";
    sendPacket(CMD, sequenceNumber,
               std::vector<char>(command.begin(), command.end()));
    log_info("Arquivo enviado com sucesso: %s", filepath.c_str());
  } catch (const std::exception &e) {
    log_error("Falha ao enviar ou ler arquivo: %s", e.what());
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
  log_info("Socket de escuta criado na porta: %d", port);

  // Armazena o socket de escuta no atributo do objeto
  this->socket_fd = listen_socket_fd;

  // Retorna a porta para o chamador
  return port;
}

void NetworkManager::acceptConnection() {
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

  log_info("Esperando conexão na porta: %d", port);

  sockaddr_in client_addr;
  socklen_t client_addrlen = sizeof(client_addr);
  int client_socket_fd =
      accept(socket_fd, (struct sockaddr *)&client_addr, &client_addrlen);
  if (client_socket_fd == -1) {
    throw std::runtime_error("Falha ao aceitar conexão do cliente");
  }

  log_info("Conexão aceita de cliente!");

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
    log_info("%s Fechou conexão", name.c_str());
  }
}

std::string NetworkManager::getIP() {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
  }

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (getsockname(socket_fd, (struct sockaddr *)&addr, &addrlen) == -1) {
    throw std::runtime_error("Failed to get socket name");
  }

  char ip_str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str)) == nullptr) {
    throw std::runtime_error("Failed to convert IP address to string");
  }

  return std::string(ip_str);
}

int NetworkManager::getPort() {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
  }

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (getsockname(socket_fd, (struct sockaddr *)&addr, &addrlen) == -1) {
    throw std::runtime_error("Failed to get socket name");
  }

  return ntohs(addr.sin_port);
}

void NetworkManager::connectTo(const std::string &ip, int port) {
  if (socket_fd != -1) {
    throw std::runtime_error("Socket already initialized");
  }

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    throw std::runtime_error("Failed to create socket");
  }

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
    close(socket_fd);
    throw std::runtime_error("Invalid IP address");
  }

  if (connect(socket_fd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) == -1) {
    close(socket_fd);
    throw std::runtime_error("Failed to connect to server");
  }

  log_info("Connected to server at %s:%d", ip.c_str(), port);
}

void NetworkManager::printPacket(packet &pkt) {
  std::cout << "Packet Type: " << pkt.type << "\n"
            << "Sequence Number: " << pkt.seqn << "\n"
            << "Total Size: " << pkt.total_size << "\n"
            << "Length: " << pkt.length << "\n"
            << "Payload: ";
  if (pkt._payload) {
    std::cout.write(pkt._payload, pkt.length);
  } else {
    std::cout << "(null)";
  }
  std::cout << "\n";
}

int connect_to_socket(std::string ip, int port) {
  int sock = 0;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    int err = errno;
    log_warn("Falha (errno=%d) ao criar um socket para [%s:%d].", err, ip.c_str(), port);
  }

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
    std::cerr << "Endereço (" << ip << ") inválido ou não suportado" << std::endl;
    close(sock);
    return -1;
  }

  int retries = 0, retry_time_ms = 1000, max_retries = 150;; // 1  segundo cada, 2.5 min.
  while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    int err = errno;
    log_warn("Falha (errno=%d) ao conectar com [%s:%d]. Cliente irá esperar 500ms", err, ip.c_str(), port);
    std::chrono::milliseconds sleep_time{retry_time_ms};
    std::this_thread::sleep_for(sleep_time);
    retries++;

    if(retries > max_retries){
      log_warn("Excedeu o máximo de tentativas para conectar a [%s:port].", ip.c_str(), port);
      close(sock);
      return -1;
    }
  }
  return sock;
}
