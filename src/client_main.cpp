/**
 * This is the main file for the CLIENT binary. 
 * 
 * please only define the main() function here.
 */
#include "client.hpp"
#include "logger.hpp"
#include "constants.hpp"

int main(int argc, char** argv){
	logger_open("logger.log");
	log_info("Hello from CLIENT, SO2-Final!\n");

	if( argc != 4 ){
		log_warn("Parameters passed in incorrectly! Usage:\n %s  <username> <server_ip_address> <port> !\n", argv[0]);
		return 1;
	}

	log_info("Will create a client with the following configuration: username=[%s] server=[%s : %s]", argv[1], argv[2], argv[3]);
	Client client(argv[1], argv[2], argv[3]);
	return 0;
}
