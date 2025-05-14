#include "../Server/FileManager.hpp"
#include "../Server/NetworkManager.hpp"
#include <chrono>
#include <string>
#include <thread>

typedef class Client Client;

void g_handleIoThread(Client *client);
void g_handleFileThread(Client *client);
void g_handleNetworkThread(Client *client);

class Client {
public:
  Client(std::string client_name, std::string server_ip,
         std::string server_port);

  void handleIoThread();
  void handleFileThread();
  void handleNetworkThread();

  std::thread io_thread;
  std::thread file_thread;
  std::thread network_thread;

  std::string client_name;
  std::string server_ip;
  std::string server_port;

  // queue of commands and lock?
  //
private:
  int file_watcher_port;
  int push_port;
  NetworkManager *file_watcher_manager;
  NetworkManager *push_manager;
  NetworkManager *command_manager;
  FileManager *sync_dir_file_manager;
  FileManager *curr_directory_file_manager;
};
