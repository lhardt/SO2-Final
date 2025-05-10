#include "NetworkManager.hpp"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>

NetworkManager::NetworkManager() : socket_fd(-1) {}
NetworkManager::NetworkManager(int socket_fd) : socket_fd(socket_fd) {}

void NetworkManager::sendPacket(packet *p) {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
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

void NetworkManager::checkSocketInitialized() {
  if (socket_fd == -1) {
    throw std::runtime_error("Socket not initialized");
  }
}

std::unique_ptr<char[]> NetworkManager::receiveHeader() {
  std::unique_ptr<char[]> header_buffer(new char[HEADER_SIZE]);
  size_t total_received = 0;

  while (total_received < HEADER_SIZE) {
    ssize_t received = recv(socket_fd, header_buffer.get() + total_received,
                            HEADER_SIZE - total_received, 0);
    if (received == -1) {
      throw std::runtime_error("Failed to receive packet header");
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
        throw std::runtime_error("Failed to receive packet payload");
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
  std::cout << "Pacote serializado (hex): ";
  for (size_t i = 0; i < buffer_size; ++i) {
    std::cout << std::hex << std::uppercase
              << (unsigned int)(unsigned char)buffer[i] << " ";
  }
  std::cout << std::dec << std::endl; // Voltar para decimal
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
  std::cout << "Pacote antes de ser deserializado (hex): ";
  for (size_t i = 0; i < buffer_size; ++i) {
    std::cout << std::hex << std::uppercase
              << (unsigned int)(unsigned char)buffer[i] << " ";
  }
  std::cout << std::dec << std::endl; // Voltar para decimal

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
