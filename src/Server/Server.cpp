#include "Server.hpp"
#include "NetworkManager.hpp"
#include <arpa/inet.h>
#include <cstring>
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
  cout << "Entrando na thread do cliente!" << endl;
  ta->manager->handle_new_connection(ta->sock_fd);
  delete ta; // evitar leak
  return nullptr;
}

Server::Server() {
  // cria socket para aceitar novas conexoes
  port = PORT;
  main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (main_socket_fd == -1) {
    perror("socket");
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(main_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    perror("setsockopt");
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }

  sockaddr_in server_addr, client_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(main_socket_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  cout << "Server started on port " << PORT << endl;
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
    perror("listen");
    exit(EXIT_FAILURE);
  }

  sockaddr_in client_addr; // Declaração de client_addr
  while (true) {
    socklen_t addrlen = sizeof(client_addr);
    cout << "Listening for new connections..." << endl;
    int new_socket_fd =
        accept(main_socket_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (new_socket_fd < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }
    cout << "Nova conexão recebida" << endl;

    // Espera receber o username do cliente no socket
    memset(buffer, 0, sizeof(buffer));
    int valread = read(new_socket_fd, buffer, sizeof(buffer));
    buffer[valread] = '\0'; // Adiciona o terminador nulo
    cout << "Username recebido: " << buffer << endl;
    if (valread <= 0) {
      close(new_socket_fd);
      continue;
    }
    string username(buffer, valread);

    std::cout << "Criando um novo network_manager" << std::endl;

    // cria um networkManager
    NetworkManager *network_manager = new NetworkManager(new_socket_fd);

    packet p = network_manager->receivePacket();
    std::cout << "Pacote recebido: " << std::endl;
    std::cout << "Type: " << p.type << std::endl;
    std::cout << "Seqn: " << p.seqn << std::endl;
    std::cout << "Total size: " << p.total_size << std::endl;
    std::cout << "Length: " << p.length << std::endl;
    std::cout << "Payload: " << p._payload << std::endl;

    // packet *p = new packet;
    // const char *payload = "Hello from server\0";
    // p->type = 0;
    // p->seqn = 0;
    // p->_payload = payload;
    // p->length = strlen(payload);
    // p->total_size = HEADER_SIZE + p->length;
    //
    // std::cout << "Pacote a ser enviado: " << std::endl;
    // std::cout << "Type: " << p->type << std::endl;
    // std::cout << "Seqn: " << p->seqn << std::endl;
    // std::cout << "Total size: " << p->total_size << std::endl;
    // std::cout << "Length: " << p->length << std::endl;
    // std::cout << "Payload: " << p->_payload << std::endl;
    //
    // network_manager->serializePacket(*p, buffer, p->total_size);
    // packet received_packet =
    //     network_manager->deserializePacket(buffer, p->total_size);
    //
    // std::cout << "Pacote recebido: " << std::endl;
    // std::cout << "Type: " << received_packet.type << std::endl;
    // std::cout << "Seqn: " << received_packet.seqn << std::endl;
    // std::cout << "Total size: " << received_packet.total_size << std::endl;
    // std::cout << "Length: " << received_packet.length << std::endl;
    // std::cout << "Payload: " << received_packet._payload << std::endl;

    // if (ClientManager *manager = clientExists(username)) {
    //   // Se o cliente já existe, entrega o socket para o manager
    //   cout << "Cliente já existe, entregando socket para o manager..." <<
    //   endl; deliverToManager(manager, new_socket_fd);
    // } else {
    //   // Se o cliente não existe, cria um novo manager e entrega o socket
    //   cout << "Criando novo client manager..." << endl;
    //   createNewManager(username, new_socket_fd);
    // }
  }
}

void Server::createNewManager(string username, int sock_file_descriptor) {
  // cria um novo manager e entrega o socket para ele
  ClientManager *manager = new ClientManager(username);
  clients.push_back(manager); // adiciona o manager a lista de clientes
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
