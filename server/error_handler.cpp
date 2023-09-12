#include "error_handler.h"
#include "server.h"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int handle_error(std::string msg, int fd){
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "[ERROR] " << msg << '\n';
    if ( fd >= 0 ){
        std::cout << "[INFO] Closing socket " << fd << "...\n";
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    
    return 1;
}