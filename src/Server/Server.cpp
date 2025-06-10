#include "Server.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/logger.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

struct ThreadArg {
  ClientManager *manager;
  int sock_fd;
};

extern "C" void *client_thread_entry(void *raw) {
  auto *ta = static_cast<ThreadArg *>(raw);
  log_info("Entrando na thread do cliente com ID");
  ta->manager->handle_new_connection(ta->sock_fd);
  delete ta; // evitar leak
  return nullptr;
}

void Server::createMainSocket() {
  // cria socket para aceitar novas conexoes
  main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (main_socket_fd == -1) {
    int error = errno;
    log_error("Falha ao criar socket, errno:%d ", error);
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(main_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    int error = errno;
    log_error("Não foi possível executar setsockopt, errno=%d ", error);
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }

  sockaddr_in server_addr, client_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(main_socket_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    int error = errno;
    log_error("Não foi possível bindar socket, errno=%d ", error);
    exit(EXIT_FAILURE);
  }
  log_info("Server inicializado na porta: %d ", port);
}

// See https://man7.org/linux/man-pages/man3/listen.3p.html
Server::Server(ServerState state, int running_port)
    : port(running_port), state(state) {
  createMainSocket();
}

Server::Server(ServerState state, int running_port, std::string ip, int port) {
  this->state = state;
  this->leader_connection = new NetworkManager();
  createMainSocket();
  leader_connection->connectTo(ip, port);
  std::string peer_msg = "PEER 0";
  std::string running_port_str = std::to_string(running_port);
  leader_connection->sendPacket(CMD, 1, std::vector<char>(peer_msg.begin(), peer_msg.end()));
  leader_connection->sendPacket(CMD, 1, std::vector<char>(running_port_str.begin(), running_port_str.end()));
}

ClientManager *Server::clientExists(string client_username) {

  for (ClientManager *manager : clients) {
    if (manager->getUsername() == client_username) {
      return manager; // retorna o manager se o cliente ja existe
    }
  }
  return nullptr; // retorna null se o cliente nao existe
}

void Server::run() {
  // Escuta por novas conexões
  char buffer[MAX_PACKET_SIZE] = {0};

  if (listen(main_socket_fd, 5) == -1) {
    int error = errno;
    log_error("Não foi possível dar listen no socket, errno=%d ", error);
    exit(EXIT_FAILURE);
  }

  sockaddr_in client_addr; // Declaração de client_addr
  while (true) {
    if (state == BACKUP) {
      // conecta com o lider
    }
    socklen_t addrlen = sizeof(client_addr);
    log_info("Esperando novas conexões");
    int new_socket_fd =
        accept(main_socket_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (new_socket_fd < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }
    log_info("Nova conexão recebida");
    NetworkManager *network_manager = new NetworkManager(new_socket_fd, "server");

    packet pkt = network_manager->receivePacket();
    NetworkManager::printPacket(pkt);
    std::string first_message(pkt._payload);
    // separa a mesagem com base no primeiro espaço
    size_t space_pos = first_message.find(' ');
    if (space_pos == std::string::npos) {
      log_error("Mensagem inválida recebida, não contém espaço");
      close(new_socket_fd);
      continue; // ignora essa conexão
    }
    std::string command = first_message.substr(0, space_pos);

    if (command == "CLIENT") {
      std::string username = first_message.substr(space_pos + 1);
      log_info("Cliente conectado com username: %s", username.c_str());
      delete network_manager; // nao precisa mais do NetworkManager, pois o socket vai ser entregue ao manager
      if (ClientManager *manager = clientExists(username)) {
        log_info("Cliente já existe, entregando socket para o manager");
        deliverToManager(manager, new_socket_fd);
      } else {
        log_info("Cliente não existe, criando novo client manager");
        createNewManager(username, new_socket_fd);
      }
    } else if (command == "PEER") {
      log_info("Nova conexão peer recebida");
      // Adiciona o novo peer à lista de conexões
      peer_connections.push_back(network_manager); // nao precisa de mutex, pois aqui ainda é single-threaded
      // inicia thread para lidar com esse peer
      packet pkt = network_manager->receivePacket();
      NetworkManager::printPacket(pkt);
    }
  }
}

void Server::createNewManager(string username, int sock_file_descriptor) {
  // cria um novo manager e entrega o socket para ele
  ClientManager *manager = new ClientManager(username);
  clients_mutex.lock();
  clients.push_back(manager); // adiciona o manager a lista de clientes
  clients_mutex.unlock();
  deliverToManager(manager,
                   sock_file_descriptor); // entrega o socket para o manager
}

void Server::deliverToManager(ClientManager *manager, int sock_fd) {
  ThreadArg *ta = new ThreadArg{manager, sock_fd};
  pthread_t thread;
  if (pthread_create(&thread, nullptr, client_thread_entry, ta) != 0) {
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }
  pthread_detach(thread);
}
