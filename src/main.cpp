#include <iostream>
#include <cstdlib>
#include "logger.h"


int main(){
	logger_open("logger.log");
	log_info("Hello, SO2-Final!\n");
	return 0;
}
