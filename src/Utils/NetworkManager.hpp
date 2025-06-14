#pragma once
#include "FileManager.hpp"
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

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
  NetworkManager(const std::string &name = "");
  NetworkManager(int socket_fd, const std::string &name = "");
  
  /** Creates a socket and tries to connect automatically to a remote. */
  NetworkManager(std::string name, std::string ip, int port);

  void sendPacket(packet *p);
  void sendPacket(uint16_t type, uint16_t seqn, const std::vector<char> &payload);
  packet receivePacket();
  void sendFileInChunks(const std::string &filepath, const size_t bufferSize,
                        FileManager &fileManager);
  void sendBufferInChunks(const std::vector<char> &buffer);
  void closeConnection();
  int createAndSetupSocket();
  void acceptConnection();
  void closeSocket();

private:
  int socket_fd;
  std::string name;
  bool isPacketValid(const packet &pkt);
  packet deserializeHeader(const char *header_buffer);
  void receivePayload(packet &pkt);
  void serializeHeader(const packet &pkt, char *buffer, size_t buffer_size);
  void serializePacket(const packet &pkt, char *buffer, size_t buffer_size);
  packet deserializePacket(const char *buffer, size_t buffer_size);
  void checkSocketInitialized();
  std::unique_ptr<char[]> receiveHeader();
};

int connect_to_socket(std::string server_name, int server_port);
