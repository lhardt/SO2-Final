/**
 * This is the main file for the SERVER binary. 
 * 
 * please only define the main() function here.
 */
#include <iostream>
#include <cstdlib>
#include "logger.hpp"
#include "constants.hpp"
#include "server.hpp"

int main(){
    logger_open("logger.log");
    log_info("Hello from SERVER, SO2-Final!\n");
    
    Server server(SERVER_PORT_IPV4, SERVER_PORT_IPV6);
    return 0;
}
