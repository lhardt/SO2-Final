/**
 * This is the main file for the CLIENT binary. 
 * 
 * please only define the main() function here.
 */
#include "client.hpp"
#include "logger.hpp"

int main(int argc, char** argv){
	logger_open("logger.log");
	log_info("Hello from CLIENT, SO2-Final!\n");

	// TODO: get info from argc/argv
	Client client("", "", "");
	return 0;
}
