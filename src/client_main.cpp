/**
 * This is the main file for the CLIENT binary. 
 * 
 * please only define the main() function here.
 */
#include <iostream>
#include <cstdlib>
#include <chrono>
#include "logger.h"
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
	// use std::getline and add to a queue of commands?
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
}


int main(int argc, char** argv){
	logger_open("logger.log");
	log_info("Hello from CLIENT, SO2-Final!\n");

	// TODO: get info from argc/argv
	Client client("", "", "");
	return 0;
}
