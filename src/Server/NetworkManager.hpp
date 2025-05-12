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
  char *_payload;

  ~packet() {
    if (_payload) {
      delete[] _payload;
      _payload = nullptr;
    }
  }
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
  void sendPacket(uint16_t type, uint16_t seqn, const std::string &payload);
  packet receivePacket();
  int createAndSetupSocket();
  void acceptConnection();
  void closeSocket();

private:
  int socket_fd;
  bool isPacketValid(const packet &pkt);
  void serializeHeader(const packet &pkt, char *buffer, size_t buffer_size);
  void receivePayload(packet &pkt);
  packet deserializeHeader(const char *header_buffer);
  packet deserializePacket(const char *buffer, size_t buffer_size);
  void serializePacket(const packet &pkt, char *buffer, size_t buffer_size);
  void checkSocketInitialized();
  std::unique_ptr<char[]> receiveHeader();
};
