#include <thread>
#include <chrono>
#include <string>

typedef class Client Client;

void handleIoThread(Client * client);
void handleFileThread(Client * client);
void handleNetworkThread(Client * client);

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
};