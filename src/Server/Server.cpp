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
  packet pkt = leader_connection->receivePacket();
  NetworkManager::printPacket(pkt);

  leader_connection->sendPacket(CMD, 0, "GET_CLIENTS");
  packet pkt2 = leader_connection->receivePacket();
  log_info("Recebendo lista de clientes do líder");
  NetworkManager::printPacket(pkt2);

  // std::string get_peers_msg = "GET_PEERS";
  // leader_connection->sendPacket(CMD, 0, std::vector<char>(get_peers_msg.begin(), get_peers_msg.end()));
  // packet pkt3 = leader_connection->receivePacket();
  // log_info("Recebendo lista de peers do líder");
  // NetworkManager::printPacket(pkt3);

  // separa a mensagem recebida em partes (delimitadas por ';')
  //  ignora o "PEERS " no começo
  std::string peers_str(pkt._payload);
  size_t pos = peers_str.find(' ');
  if (pos != std::string::npos) {
    peers_str = peers_str.substr(pos + 1); // remove o "PEERS "
  }
  std::istringstream iss(peers_str);
  std::string peer_info;
  vector<std::string> peer_info_list;
  log_info("Valor de peers_str: %s", peers_str.c_str());
  while (std::getline(iss, peer_info, ';')) {
    log_info("aaaaa");
    if (peer_info.empty()) {
      log_warn("Encontrado peer_info vazio, pulando...");
      continue; // ignora strings vazias
    }
    peer_info_list.push_back(peer_info);
    size_t colon_pos = peer_info.find(':');
    if (colon_pos == std::string::npos) {
      log_error("Peer info inválido: %s", peer_info.c_str());
      continue; // ignora essa entrada
    }
    std::string peer_ip = peer_info.substr(0, colon_pos);
    int peer_port = std::stoi(peer_info.substr(colon_pos + 1));
    if (peer_ip == this->ip && peer_port == this->port) {
      log_info("Ignorando conexão com o próprio servidor");
      continue;
    }
    NetworkManager *peer_network_manager = new NetworkManager();
    PeerInfo *peer = new PeerInfo{peer_ip, peer_port, peer_network_manager};
    log_info("Conectando ao peer: %s:%d", peer_ip.c_str(), peer_port);
    peer_connections.push_back(peer);
    int res = peer_network_manager->connectTo(peer_ip, peer_port);
    std::string peer_msg = "PEER " + this->ip + ":" + std::to_string(this->port);
    peer_network_manager->sendPacket(CMD, 0, std::vector<char>(peer_msg.begin(), peer_msg.end()));
    if (res) {
      log_info("Conexão com o peer %s:%d estabelecida", peer_ip.c_str(), peer_port);
      std::thread peer_thread(&Server::handlePeerThread, this, peer);
      peer_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
    } else {
      log_error("Falha ao conectar ao peer: %s:%d", peer_ip.c_str(), peer_port);
    }
  }
  for (int i = 0; i < peer_info_list.size(); i++) {
    if (peer_info_list[i] == this->ip + ":" + std::to_string(this->port)) {
      std::string next_peer = i + 1 < peer_info_list.size() ? peer_info_list[i + 1] : peer_info_list[0];
      this->electionManager->setNextPeer(next_peer);
      break;
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
    int new_socket_fd = accept(main_socket_fd, (struct sockaddr *)&client_addr, &addrlen);
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
      std::istringstream iss(received_message.substr(space_pos + 1));
      std::string username;
      int client_listen_port;
      iss >> username >> client_listen_port;
      log_info("Cliente conectado com username: %s", username.c_str());

      // verifica se o cliente já existe
      ClientManager *manager = clientExists(username);
      if (!manager) {
        log_info("Criando novo ClientManager para o cliente %s", username.c_str());
        std::string listen_adress = network_manager->getPeerIP() + ":" + std::to_string(client_listen_port);
        manager = createNewManager(LEADER, username);
        manager->setNetworkManager(network_manager);
        // envia para os peers a informação de que um novo cliente se conectou
        peer_mutex.lock();
        for (auto &peer : peer_connections) {
          std::string peer_msg = "CLIENT_CONNECTION " + username; // formato vai ser CLIENT_CONNECTION <username> <listen_port> <client_listen_adress(ip:port)>

          // para cada um, cria um NetworkManager novo(que vai ser o do ClientManager) e envia a mensagem + porta
          // do novo NetworkManager
          NetworkManager *peer_network_manager = new NetworkManager();
          int listen_port = peer_network_manager->createAndSetupSocket();
          std::string client_listen_adress = network_manager->getPeerIP() + ":" + std::to_string(client_listen_port);
          peer_msg += " " + std::to_string(listen_port) + " " + client_listen_adress;
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
        // coloca o listen_port do novo device
        std::string listen_adress = network_manager->getPeerIP() + ":" + std::to_string(client_listen_port);
        manager->add_listen_adress(listen_adress);
        std::string peer_msg = "DEVICE_CONNECTION " + username + " " + listen_adress;
        peer_mutex.lock();
        for (auto &peer : peer_connections) {
          log_info("Enviando informação de conexão do dispositivo %s para o peer", username.c_str());
          peer->network_manager->sendPacket(CMD, 0, std::vector<char>(peer_msg.begin(), peer_msg.end()));
        }
        peer_mutex.unlock();
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

      if (this->state == LEADER) {
        std::string new_peer_list_msg = "PEERS ";
        for (const auto &peer : peer_connections) {
          new_peer_list_msg += peer->ip + ":" + std::to_string(peer->listen_port) + ";";
        }
        // envia a lista atualizada de peers para todos os peers conectados
        for (PeerInfo *peer : peer_connections) {
          log_info("Enviando lista de peers atualizada para o peer: %s:%d", peer->ip.c_str(), peer->listen_port);
          peer->network_manager->sendPacket(CMD, 0, std::vector<char>(new_peer_list_msg.begin(), new_peer_list_msg.end()));
        }
      }

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
  log_info("Entregando socket %d para o ClientManager %s", sock_fd, manager->getUsername().c_str());
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
        std::string client_listen_adress;
        iss >> username >> porta >> client_listen_adress;
        log_info("Peer enviou informação de conexão de cliente: %s", client_info.c_str());
        if (ClientManager *client = clientExists(username)) {
          // deleta o clientManager antigo se ele já existir
          log_info("Cliente %s já existe, removendo o antigo ClientManager", username.c_str());
          clients_mutex.lock();
          auto it = std::remove_if(clients.begin(), clients.end(),
                                   [&username](ClientManager *manager) {
                                     return manager->getUsername() == username;
                                   });
          if (it != clients.end()) {
            delete *it;                       // libera o ClientManager antigo
            clients.erase(it, clients.end()); // remove o ClientManager da lista
            log_info("Antigo ClientManager removido com sucesso");
          } else {
            log_warn("ClientManager para o cliente %s não encontrado na lista", username.c_str());
          }
          clients_mutex.unlock();
        }
        if (!clientExists(username)) {
          // cria um novo ClientManager para o client que se conectou
          NetworkManager *push_receiver = new NetworkManager();
          ClientManager *manager = createNewBackupClientManager(username, push_receiver);
          manager->add_listen_adress(client_listen_adress);
          push_receiver->connectTo(peer_manager->getPeerIP(), porta);
          manager->setNetworkManager(push_receiver);
          std::thread push_receiver_thread(&ClientManager::receivePushsOn, manager, push_receiver);
          push_receiver_thread.detach(); // desanexa a thread para que ela possa rodar em paralelo
        }
      } else if (command == "DEVICE_CONNECTION") {
        std::string device_info = received_message.substr(received_message.find(' ') + 1);
        std::istringstream iss(device_info);
        std::string username;
        std::string device_listen_adress;
        iss >> username >> device_listen_adress;
        log_info("Peer enviou informação de conexão de dispositivo: %s", device_info.c_str());
        if (ClientManager *client = clientExists(username)) {
          client->add_listen_adress(device_listen_adress); // adiciona o novo dispositivo ao ClientManager
          log_info("Dispositivo adicionado ao ClientManager do cliente %s", username.c_str());
        } else {
          log_warn("Cliente %s não encontrado, não foi possível adicionar o dispositivo", username.c_str());
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
        this->electionManager->UpdateNextPeer(received_message.substr(received_message.find(' ') + 1));

      } else if (command == "ELECTION") {
        this->electionManager->handleCommand(received_message);

      } else if (command == "LEADER_IS") { // LEADER_IS <IP> <PORT>
        // tira o LEADER IS do começo
        std::string leader_info = received_message.substr(received_message.find(' ') + 1);
        std::string ip;
        int port;
        std::istringstream iss(leader_info);
        iss >> ip >> port;
        log_info("Novo líder recebido: %s:%d", ip.c_str(), port);
        if (state == BACKUP) {
          if (leader_connection != nullptr) {
            delete leader_connection; // libera o antigo líder
          }

          // acha na lista de peer_connections o peer que é o lider
          for (PeerInfo *peer : peer_connections) {
            if (peer->ip == ip && peer->listen_port == port) {
              leader_connection = peer->network_manager; // atualiza o líder
              log_info("Líder atualizado para: %s:%d", ip.c_str(), port);
              std::string get_peers_msg = "GET_PEERS";
              leader_connection->sendPacket(CMD, 0, std::vector<char>(get_peers_msg.begin(), get_peers_msg.end()));
              packet pkt = leader_connection->receivePacket();
              NetworkManager::printPacket(pkt);
              // remove o "PEERS " do começo
              std::string peers_str(pkt._payload);
              size_t pos = peers_str.find(' ');
              if (pos != std::string::npos) {
                peers_str = peers_str.substr(pos + 1); // remove o "PEERS "
              }
              this->electionManager->UpdateNextPeer(peers_str);
              log_info("Lista de peers atualizada do líder: %s", peers_str.c_str());
              break;
            }
          }
        }
      } else {
        log_warn("Comando desconhecido recebido do peer: %s", command.c_str());
      }
    } catch (const std::exception &e) {
      log_info("AAAAAAAAAAAAAAA");
      log_error("Erro ao receber pacote do peer: %s", e.what());
      // se quem desconectou foi o lider, inicia uma nova eleição
      if (state != LEADER && peer_info->network_manager == leader_connection) {
        log_info("Líder caiu, iniciando nova eleicao");
        electionManager->startElection();
      } else {
        log_info("Nao foi o Lider que caiu");
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

NetworkManager *Server::getPeerConnection(std::string peer_ip_port) {
  std::lock_guard<std::mutex> lock(peer_mutex); // Protege o acesso à lista de peers
  for (PeerInfo *peer_info : peer_connections) {
    log_info("Verificando peer: %s:%d", peer_info->ip.c_str(), peer_info->listen_port);
    if (peer_info->ip + ":" + std::to_string(peer_info->listen_port) == peer_ip_port) {
      log_info("Conexão com o peer %s encontrada", peer_ip_port.c_str());
      return peer_info->network_manager; // Retorna o NetworkManager do peer
    }
  }
  log_error("Peer %s não encontrado na lista de conexões", peer_ip_port.c_str());
  return nullptr; // Retorna nullptr se o peer não for encontrado
}

vector<NetworkManager *> Server::getPeers() {
  std::lock_guard<std::mutex> lock(peer_mutex);
  std::vector<NetworkManager *> peers;
  for (PeerInfo *peer_info : peer_connections) {
    peers.push_back(peer_info->network_manager);
  }
  return peers;
}

void Server::turnLeader() {
  this->state = LEADER;
  log_info("Servidor agora é o líder");
  // envia para todos os peers que ele é o novo lider
  std::string new_leader_msg = "LEADER_IS " + this->ip + " " + std::to_string(this->port);
  for (NetworkManager *peer_manager : getPeers()) {
    peer_manager->sendPacket(CMD, 0, std::vector<char>(new_leader_msg.begin(), new_leader_msg.end()));
  }

  // TODO
  //  avisar o cliente que é novo lider e mandar ele se conectar comigo de novo
  clients_mutex.lock();
  for (int i = 0; i < clients.size(); i++) {
    ClientManager *manager = clients[i];
    log_info("Enviando mensagem de novo líder para o cliente: %s", manager->getUsername().c_str());
    manager->notify(new_leader_msg);
    // tira da lista de clientes
    clients.erase(clients.begin() + i);
    delete manager; // libera a memória do ClientManager
  }
  clients_mutex.unlock();
}
