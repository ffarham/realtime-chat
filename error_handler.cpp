#include "error_handler.h"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int error_handler(std::string msg, int fd){
    std::cout << "[ERROR] " << msg << std::endl;
    if ( fd >= 0 ){
        std::cout << "[INFO] Closing socket " << fd << "..." << std::endl;
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    
    return 1;
}

int error_handler(std::string msg, std::vector<int> fds){
    std::cout << "[ERROR] " << msg << std::endl;
    for (int& fd : fds){
        if (fd <= 0) continue;
        std::cout << "[INFO] Closing socket " << fd << "..." << std::endl;
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    return 1;
}