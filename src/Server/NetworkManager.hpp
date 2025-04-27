#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct packet {
  uint16_t type;
  uint16_t seqn;
  uint32_t total_size;
  uint16_t length;
  const char *_payload;
} packet;

class NetworkManager {
public:
  NetworkManager();
  NetworkManager(int socket_fd);

  void sendPacket(packet *p);
  packet receivePacket();

private:
  int socket_fd;

  void serializePacket(const packet &pkt, char *buffer, size_t buffer_size);
  packet deserializePacket(const char *buffer, size_t buffer_size);
};
