#include "../Utils/FileManager.hpp"
#include "../Utils/NetworkManager.hpp"
#include <atomic>
#include <mutex>
#include <semaphore.h>
#include <string>
#include <thread>

typedef class Client Client;

void g_handleIoThread(Client *client);
void g_handleFileThread(Client *client);
void g_handleNetworkThread(Client *client);

class Client {
public:
  Client(std::string client_name, std::string server_ip, std::string server_port, int listen_port);

  void handleIoThread();
  void handleFileThread();
  void handlePushThread();

  std::thread io_thread;
  std::thread file_thread;
  std::thread network_thread;

  std::string client_name;
  std::string server_ip;
  std::string server_port;

  // queue of commands and lock?
  //
private:
  int listen_port = 5000; // porta onde vai escutar novas conexoes com eventuais novos lideres
  int file_watcher_port;
  int push_port;
  NetworkManager *file_watcher_manager;
  NetworkManager *push_manager;
  NetworkManager *command_manager;
  FileManager *sync_dir_file_manager;
  FileManager *curr_directory_file_manager;
  std::mutex watcher_push_lock;
  std::atomic<bool> stop_requested{false};
  sem_t cleanup_semaphore;
  void handleListenthread(NetworkManager *listen_network_manager);
  void connectToServer(NetworkManager *command_network_manager);
};
