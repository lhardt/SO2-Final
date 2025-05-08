#include <thread>
#include <chrono>
#include <string>

typedef class Client Client;

void g_handleIoThread(Client * client);
void g_handleFileThread(Client * client);
void g_handleNetworkThread(Client * client);

class Client {
public:
    Client(std::string client_name, std::string server_ip, std::string server_port);

    void handleIoThread();
    void handleFileThread();
    void handleNetworkThread();

    std::thread io_thread;
    std::thread file_thread;
    std::thread network_thread;

    std::string client_name;
    std::string server_ip;
    std::string server_port;

    bool is_exit = false;

    // queue of commands and lock?
};