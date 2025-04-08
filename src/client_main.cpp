/**
 * This is the main file for the CLIENT binary. 
 * 
 * please only define the main() function here.
 */
#include <iostream>
#include <cstdlib>
#include "logger.h"


int main(){
	logger_open("logger.log");
	log_info("Hello from CLIENT, SO2-Final!\n");
	return 0;
}
