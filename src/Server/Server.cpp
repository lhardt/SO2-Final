#include "Server.hpp"
#include "../Utils/NetworkManager.hpp"
#include "../Utils/logger.hpp"
#include "ClientManager.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ifaddrs.h>
#include <mutex>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

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
Server::Server(State state, int running_port)
    : port(running_port), state(state) {
  createMainSocket();
  this->leader_connection = NULL; // se inicializou dessa forma , então é o lider
  this->ip = getLocalIP();        // pega o IP local
  this->electionManager = new ElectionManager(this);
}

Server::Server(State state, int running_port, std::string ip, int port) {
  this->state = state;
  this->leader_connection = new NetworkManager();
  this->ip = getLocalIP(); // pega o IP local
  this->port = running_port;
  createMainSocket();

  this->electionManager = new ElectionManager(this);
  leader_connection->connectTo(ip, port);
  PeerInfo *leader_info = new PeerInfo{ip, port, leader_connection};

  std::string peer_msg = "PEER " + this->ip + ":" + std::to_string(this->port);
  leader_connection->sendPacket(CMD, 1, std::vector<char>(peer_msg.begin(), peer_msg.end()));

  leader_connection->sendPacket(CMD, 0, "GET_CLIENTS");
  packet pkt2 = leader_connection->receivePacket();
  log_info("Recebendo lista de clientes do líder");
  NetworkManager::printPacket(pkt2);

  std::string get_peers_msg = "GET_PEERS";
  leader_connection->sendPacket(CMD, 0, std::vector<char>(get_peers_msg.begin(), get_peers_msg.end()));
  packet pkt3 = leader_connection->receivePacket();
  log_info("Recebendo lista de peers do líder");
  NetworkManager::printPacket(pkt3);

  // separa a mensagem recebida em partes (delimitadas por ';')
  //  ignora o "PEERS " no começo
  std::string peers_str(pkt3._payload);
  size_t pos = peers_str.find(' ');
  if (pos != std::string::npos) {
    peers_str = peers_str.substr(pos + 1); // remove o "PEERS "
  }
  std::istringstream iss(peers_str);
  std::string peer_info;
  while (std::getline(iss, peer_info, ';')) {
    if (peer_info.empty())
      continue; // ignora strings vazias
    size_t colon_pos = peer_info.find(':');
    if (colon_pos == std::string::npos) {
      log_error("Peer info inválido: %s", peer_info.c_str());
      continue; // ignora essa entrada
    }
    std::string peer_ip = peer_info.substr(0, colon_pos);
    int peer_port = std::stoi(peer_info.substr(colon_pos + 1));
    if (peer_ip == this->ip && peer_port == this->port) {
      log_info("Ignorando conexão com o próprio servidor");
      continue; // ignora a própria conexão
    }
    NetworkManager *peer_network_manager = new NetworkManager();
    PeerInfo *peer = new PeerInfo{peer_ip, peer_port, peer_network_manager};
    log_info("Conectando ao peer: %s:%d", peer_ip.c_str(), peer_port);
    peer_connections.push_back(peer);
    int res = peer_network_manager->connectTo(peer_ip, peer_port);
    std::string peer_msg = "PEER " + this->ip + ":" + std::to_string(this->port);
    peer_network_manager->sendPacket(CMD, 0, std::vector<char>(peer_msg.begin(), peer_msg.end()));
    if (res) {
      std::thread peer_thread(&Server::handlePeerThread, this, peer);
      peer_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
    } else {
      log_error("Falha ao conectar ao peer: %s:%d", peer_ip.c_str(), peer_port);
    }
  }

  // inicia a thread para lidar com peers
  std::thread peer_thread(&Server::handlePeerThread, this, leader_info);
  peer_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
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
  if (setsockopt(main_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    int error = errno;
    log_error("Não foi possível executar setsockopt, errno=%d ", error);
    close(main_socket_fd);
    exit(EXIT_FAILURE);
  }

  sockaddr_in server_addr;
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
    std::string received_message(pkt._payload);
    // separa a mesagem com base no primeiro espaço
    size_t space_pos = received_message.find(' ');
    if (space_pos == std::string::npos) {
      log_error("Mensagem inválida recebida, não contém espaço");
      close(new_socket_fd);
      continue; // ignora essa conexão
    }
    std::string command = received_message.substr(0, space_pos);

    if (command == "CLIENT") {
      std::string username = received_message.substr(space_pos + 1);
      log_info("Cliente conectado com username: %s", username.c_str());

      // verifica se o cliente já existe
      ClientManager *manager = clientExists(username);
      if (!manager) {
        log_info("Criando novo ClientManager para o cliente %s", username.c_str());
        manager = createNewManager(LEADER, username);
        manager->setNetworkManager(network_manager);
        // envia para os peers a informação de que um novo cliente se conectou
        peer_mutex.lock();
        for (auto &peer : peer_connections) {
          std::string peer_msg = "CLIENT_CONNECTION " + username;

          // para cada um, cria um NetworkManager novo(que vai ser o do ClientManager) e envia a mensagem + porta
          // do novo NetworkManager
          NetworkManager *peer_network_manager = new NetworkManager();
          int listen_port = peer_network_manager->createAndSetupSocket();
          peer_msg += " " + std::to_string(listen_port);
          log_info("Enviando informação de conexão do cliente %s para o peer", username.c_str());
          peer->network_manager->sendPacket(CMD, 0, std::vector<char>(peer_msg.begin(), peer_msg.end()));
          // passa o novo NetworkManager para o ClientManager
          peer_network_manager->acceptConnection();
          manager->add_new_backup(peer_network_manager);
          // printa o vector de ClientManagers
          log_info("Lista de clientes conectados:");
          for (ClientManager *manager : clients) {
            log_info("Cliente: %s", manager->getUsername().c_str());
          }
        }
        peer_mutex.unlock();
      } else {
        log_info("Cliente %s já existe, reutilizando ClientManager", username.c_str());
      }
      deliverToManager(manager, new_socket_fd); // entrega o socket para o manager

    } else if (command == "PEER") {
      log_info("Nova conexão peer recebida");

      // pera o ip e porta do peer
      std::string peer_info_str = received_message.substr(space_pos + 1);
      size_t colon_pos = peer_info_str.find(':');
      if (colon_pos == std::string::npos) {
        log_error("Peer info inválido: %s", peer_info_str.c_str());
        close(new_socket_fd);
        continue; // ignora essa conexão
      }
      std::string peer_ip = peer_info_str.substr(0, colon_pos);
      int peer_port = std::stoi(peer_info_str.substr(colon_pos + 1));

      PeerInfo *peer_info = new PeerInfo{peer_ip, peer_port, network_manager};
      // Adiciona o novo peer à lista de conexões
      peer_mutex.lock();
      peer_connections.push_back(peer_info);
      peer_mutex.unlock();

      // inicia thread para lidar com esse peer
      std::thread peer_thread(&Server::handlePeerThread, this, peer_info);
      peer_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
    }
  }
}

ClientManager *Server::createNewManager(State state, string username) {
  // cria um novo manager e entrega o socket para ele
  ClientManager *manager = new ClientManager(state, username);
  clients_mutex.lock();
  clients.push_back(manager); // adiciona o manager a lista de clientes
  clients_mutex.unlock();
  return manager;
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

vector<std::string> Server::getClients() {
  std::lock_guard<std::mutex> lock(clients_mutex); // Protege o acesso à lista de clientes
  vector<std::string> client_list;
  for (ClientManager *manager : clients) {
    if (manager == nullptr) {
      log_warn("Encontrado ClientManager nulo na lista de clientes, pulando...");
      continue; // pula se o manager for nulo
    }
    log_info("atualmente no cliente: %s", manager->getUsername().c_str());
    client_list.push_back(manager->getUsername() + " " + manager->getIp() + ":" + std::to_string(manager->getPort()));
  }
  return client_list;
}

std::vector<std::string> Server::getBackupPeers() {
  std::lock_guard<std::mutex> lock(peer_mutex); // Protege o acesso à lista de peers
  std::vector<std::string> backup_peers_list;
  for (PeerInfo *peer : peer_connections) {
    if (peer == nullptr) {
      log_warn("Encontrado NetworkManager nulo na lista de peers, pulando...");
      continue; // pula se o peer for nulo
    }
    backup_peers_list.push_back(peer->ip + ":" + std::to_string(peer->listen_port));
  }
  return backup_peers_list;
}

void Server::handlePeerThread(PeerInfo *peer_info) {
  log_info("Iniciando thread para lidar com peer");
  NetworkManager *peer_manager = peer_info->network_manager;
  while (true) {
    try {
      packet pkt = peer_manager->receivePacket();
      NetworkManager::printPacket(pkt);

      std::string received_message(pkt._payload);
      std::string command = received_message.substr(0, received_message.find(' '));
      if (command == "WHO_IS_LEADER") {
        if (state == LEADER) {
          log_info("Respondendo ao peer com o IP do líder");
          // responde com o proprio IP e porta
          peer_manager->sendPacket(CMD, 0, "LEADER_IS " + ip);
        } else {
          log_info("Peer solicitou o líder, mas este servidor é um backup");
        }
      } else if (command == "GET_CLIENTS") {
        log_info("Peer solicitou a lista de clientes");
        std::vector<std::string> clients = getClients();
        std::string response = "CLIENTS ";
        clients_mutex.lock();
        for (const auto &client : clients) {
          response += client + ";";
        }
        clients_mutex.unlock();
        peer_manager->sendPacket(CMD, 0, std::vector<char>(response.begin(), response.end()));
        log_info("Lista de clientes enviada para o peer");
      } else if (command == "CLIENT_CONNECTION") {
        std::string client_info = received_message.substr(received_message.find(' ') + 1);
        std::istringstream iss(client_info);
        std::string username;
        int porta;
        iss >> username >> porta;
        log_info("Peer enviou informação de conexão de cliente: %s", client_info.c_str());
        if (!clientExists(username)) {
          // cria um novo ClientManager para o client que se conectou
          NetworkManager *push_receiver = new NetworkManager();
          ClientManager *manager = createNewBackupClientManager(username, push_receiver);
          push_receiver->connectTo(peer_manager->getPeerIP(), porta);
          std::thread push_receiver_thread(&ClientManager::receivePushsOn, manager, push_receiver);
          push_receiver_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
        }
      } else if (command == "GET_PEERS") {
        log_info("Peer solicitou a lista de peers");
        std::vector<std::string> backup_peers = getBackupPeers();
        std::string response = "PEERS ";
        peer_mutex.lock();
        for (const auto &peer : backup_peers) {
          response += peer + ";";
        }
        peer_mutex.unlock();
        peer_manager->sendPacket(CMD, 0, std::vector<char>(response.begin(), response.end()));
        log_info("Lista de peers enviada para o peer");
      } else if (command == "PEERS") {

      } else {
        log_warn("Comando desconhecido recebido do peer: %s", command.c_str());
      }
    } catch (const std::exception &e) {
      log_error("Erro ao receber pacote do peer: %s", e.what());
      // se quem desconectou foi o lider, inicia uma nova eleição
      if (state != LEADER && peer_info->network_manager == leader_connection) {
        electionManager->startElection();
      }
      // se ocorrer um erro, tira o peer da lista de conexões
      peer_mutex.lock();
      auto it = std::find(peer_connections.begin(), peer_connections.end(), peer_info);
      if (it != peer_connections.end()) {
        delete *it; // libera o PeerInfo associado
        peer_connections.erase(it);
        log_info("Peer desconectado e removido da lista de conexões");
      } else {
        log_warn("Peer não encontrado na lista de conexões");
      }
      peer_mutex.unlock();
      // inicia uma nova eleicao
      break; // sai do loop se houver erro
    }
  }
}
std::string Server::getLocalIP() {
  struct ifaddrs *ifaddr, *ifa;
  char ip[INET_ADDRSTRLEN] = {0};
  if (getifaddrs(&ifaddr) == -1) {
    return "127.0.0.1";
  }
  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
        !(ifa->ifa_flags & IFF_LOOPBACK)) {
      void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      inet_ntop(AF_INET, addr, ip, INET_ADDRSTRLEN);
      break;
    }
  }
  freeifaddrs(ifaddr);
  return std::string(ip);
}

ClientManager *Server::createNewBackupClientManager(std::string username, NetworkManager *push_receiver) {
  // cria um novo socket para o manager e manda a porta
  // quando um cliente se conectar, o lider vai avisar os backups com o nome do
  // cleinte e a porta em que o ClientManagerBackup que eles vao criar tem que
  // se conectar
  ClientManager *manager = new ClientManager(BACKUP, username);
  clients_mutex.lock();
  clients.push_back(manager); // adiciona o manager a lista de clientes
  clients_mutex.unlock();

  log_info("Criando novo ClientManager para backup: %s", username.c_str());
  return manager; // retorna o manager criado
}

int Server::getPort() {
  return port;
}

void Server::sendPacketToPeer(std::string peer, std::string command) {
  // std::lock_guard<std::mutex> lock(peer_mutex); // Protege o acesso à lista de peers
  // for (NetworkManager *peer_manager : peer_connections) {
  //   if (peer_manager->getIP() + ":" + std::to_string(peer_manager->getPort()) == peer) {
  //     log_info("Enviando comando para o peer: %s", peer.c_str());
  //     peer_manager->sendPacket(CMD, 0, std::vector<char>(command.begin(), command.end()));
  //     return; // Enviado com sucesso
  //   }
  // }
  log_error("Peer %s não encontrado na lista de conexões", peer.c_str());
}
