#pragma once

#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_PACKET_SIZE 2048
#define DATA 0
#define CMD 1

typedef struct packet {
  uint16_t type;
  uint16_t seqn;
  uint32_t total_size;
  uint16_t length;
  const char *_payload;
} packet;

static constexpr size_t HEADER_SIZE = sizeof(uint16_t)   /* type */
                                      + sizeof(uint16_t) /* seqn */
                                      + sizeof(uint32_t) /* total_size */
                                      + sizeof(uint16_t) /* length */;

class NetworkManager {
public:
  NetworkManager();
  NetworkManager(int socket_fd);

  void sendPacket(packet *p);
  packet receivePacket();

  void serializePacket(const packet &pkt, char *buffer, size_t buffer_size);
  packet deserializePacket(const char *buffer, size_t buffer_size);
  void checkSocketInitialized();
  std::unique_ptr<char[]> receiveHeader();
  packet deserializeHeader(const char *header_buffer);
  void receivePayload(packet &pkt);
  void serializeHeader(const packet &pkt, char *buffer, size_t buffer_size);

private:
  int socket_fd;
};
