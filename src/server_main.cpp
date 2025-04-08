/**
 * This is the main file for the SERVER binary. 
 * 
 * please only define the main() function here.
 */
#include <iostream>
#include <cstdlib>
#include "logger.h"


int main(){
	logger_open("logger.log");
	log_info("Hello from SERVER, SO2-Final!\n");
	return 0;
}
