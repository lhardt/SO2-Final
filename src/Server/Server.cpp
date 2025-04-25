#include "Server.hpp"
#include <sys/socket.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread>

# define PORT 4000

struct ThreadArg {
    ClientManager* manager;
    int sock_fd;
};

extern "C"
void* client_thread_entry(void* raw) {
    auto* ta = static_cast<ThreadArg*>(raw);
    ta->manager->handle_new_connection(ta->sock_fd);
    delete ta;  // evitar leak
    return nullptr;
}

Server::Server(){
    //cria socket para aceitar novas conexoes
    port = PORT;
    main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (main_socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(main_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
}



ClientManager* Server::clientExists(string client_username){

    for(ClientManager* manager : clients){
        if(manager->getUsername() == client_username){
            return manager; //retorna o manager se o cliente ja existe
        }
    }
    return nullptr; //retorna null se o cliente nao existe
}

Server::void run(){
    //escuta por novas conexoes
    char buffer[1024] = {0};


    if(listen(main_socket_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while(true){
        socklen_t addrlen = sizeof(client_addr);
        int new_socket_fd = accept(main_socket_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (new_socket_fd < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        //espera receber o username do cliente no socket
        bzero(buffer, sizeof(buffer));
        int valread = read(new_socket_fd, buffer, sizeof(buffer));
        string username(buffer, valread);

        if(ClientManager* manager = clientExists(username)){
            //se o cliente ja existe, entrega o socket para o manager
            deliverToManager(manager, new_socket_fd);
        }
        else{
            //se o cliente nao existe, cria um novo manager e entrega o socket
            createNewManager(username, new_socket_fd);
        }

    }
}



void Server::createNewManager(string username,int sock_file_descriptor){
    //cria um novo manager e entrega o socket para ele
    ClientManager* manager = new ClientManager(username, sock_file_descriptor);
    clients.push_back(manager); //adiciona o manager a lista de clientes
    deliverToManager(manager, sock_file_descriptor); //entrega o socket para o manager

}



void Server::deliverToManager(ClientManager* manager, int sock_fd){
    ThreadArg* ta = new ThreadArg{ manager, sock_fd };
    pthread_t thread;
    if (pthread_create(&thread,
                       nullptr,
                       client_thread_entry,
                       ta) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread);
}
