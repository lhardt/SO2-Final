/**
 * This is the main file for the CLIENT binary. 
 * 
 * please only define the main() function here.
 */
#include <iostream>
#include <cstdlib>
#include <chrono>
#include "logger.h"
#include "utils.hpp"
#include "client.hpp"


void g_handleIoThread(Client * client){
	log_assert(client != NULL, "Null client!");
	client->handleIoThread();
}
void g_handleFileThread(Client * client){
	log_assert(client != NULL, "Null client!");
	client->handleFileThread();
}
void g_handleNetworkThread(Client * client){
	log_assert(client != NULL, "Null client!");
	client->handleNetworkThread();
}


void Client::handleIoThread(){
	log_info("Started IO Thread with ID %d ", std::this_thread::get_id());
	while(true){
		std::string user_input;
		std::getline(std::cin, user_input);
		
		int argument_start = 0;
		std::string command = get_first_word(user_input, argument_start);
		std::string argument = trim_string(user_input.substr(argument_start));

		log_info("Received command  <<%s>> with argument <<%s>>\n", command.c_str(), argument.c_str());

		// Do something with the received command.
	}
}
void Client::handleFileThread(){
	log_info("Started File Thread with ID %d ", std::this_thread::get_id());
}
void Client::handleNetworkThread(){
	log_info("Started Network Thread with ID %d ", std::this_thread::get_id());
	// while( can't reach server ){
	//   wait 100ms
	// }
	// check a queue of commands?
}


Client::Client(std::string _client_name, std::string _server_ip, std::string _server_port) :
		client_name(_client_name), server_ip(_server_ip), server_port(_server_port){

	// TODO: log parameters

	this->io_thread = std::thread(g_handleIoThread, this);
	this->network_thread = std::thread(g_handleNetworkThread, this);
	this->file_thread = std::thread(g_handleFileThread, this);

	// If the main thread finishes, all the other threads are prematurely finished.
	while(true){	
		log_info("The main thread, like the Great Dark Beyond, lies asleep");
		std::chrono::milliseconds sleep_time{5000};
		std::this_thread::sleep_for(sleep_time);
	}

	this->io_thread.join();
	this->network_thread.join();
	this->file_thread.join();
}


int main(int argc, char** argv){
	logger_open("logger.log");
	log_info("Hello from CLIENT, SO2-Final!");

	
	if(argc == 4){
		std::string username  = argv[1];
		std::string server_ip  = argv[2];
		std::string server_port  = argv[3];

		log_info("Will start a client with User=[%s] Server=[%s:%s]", username.c_str(), server_ip.c_str(), server_port.c_str());

		Client client("", "", "");
		return 0;
	}
	
	log_info("Did not receive parameters correctly! Expected\n\t %s <username> <server_ip_address> <port>", argv[0]);
	return 1;

}
