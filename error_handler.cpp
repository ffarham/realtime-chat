#include "error_handler.h"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int error_handler(std::string msg, int fd){
    std::cout << "[ERROR] " << msg << std::endl;
    if ( fd >= 0 ){
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    
    return 1;
}