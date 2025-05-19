#include "Server.hpp"
#include "../Utils/Command.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/logger.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 4000

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

// See https://man7.org/linux/man-pages/man3/listen.3p.html
Server::Server() {
  // cria socket para aceitar novas conexoes
  port = PORT;
  main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (main_socket_fd == -1) {
    int error = errno;
    log_error("Falha ao criar socket, errno:%d ", error);
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(main_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    int error = errno;
    log_error("Não foi possível executar setsockopt, errno=%d ", error);
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }

  sockaddr_in server_addr, client_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(main_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    int error = errno;
    log_error("Não foi possível bindar socket, errno=%d ", error);
    exit(EXIT_FAILURE);
  }
  log_info("Server inicializado na porta: %d ", PORT);
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
    socklen_t addrlen = sizeof(client_addr);
    log_info("Esperando novas conexões");
    int new_socket_fd = accept(main_socket_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (new_socket_fd < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }
    log_info("Nova conexão recebida");
    NetworkManager network_manager(new_socket_fd, "server");
    // espera um pacote com payload sendo o username
    ReceiveMessageCommand cmd(&network_manager);
    cmd.execute();
    std::string username(cmd.getMessage());
    log_info("Recebido username: %s", username.c_str());

    if (ClientManager *manager = clientExists(username)) {
      // Se o cliente já existe, entrega o socket para o manager
      log_info("Cliente já existe, entregando socket para o manager");
      deliverToManager(manager, new_socket_fd);
    } else {
      // Se o cliente não existe, cria um novo manager e entrega o socket
      log_info("Cliente não existe, criando novo client manager");
      createNewManager(username, new_socket_fd);
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
