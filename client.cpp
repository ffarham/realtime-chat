#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define DEBUG false

int err_handler(std::string msg, int fd){
    std::cout << "[ERROR] " << msg << std::endl;
    shutdown(fd, SHUT_RDWR);
    close(fd);
    
    return 1;
}

int main(int argc, char **argv){

    if (argc < 3){
        std::cerr << "Expecting: <hostname> <port>" << std::endl;
        return 1;
    }

    char* hostname = argv[1];
    int port = atoi(argv[2]);

    std::string name;
    std::cout << "Enter name: ";
    std::getline(std::cin, name);
    std::cout << std::endl;
    name += ": ";
    unsigned char name_len = name.size();

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0){
        return err_handler("Failed to create socket!", client_fd);
    }

    sockaddr_in client_sock;
    client_sock.sin_family = AF_INET;
    client_sock.sin_port = htons(port);
    // converts internet host address to network byte order (bianry)
    inet_aton(hostname, &client_sock.sin_addr);

    int conn_status = connect(client_fd, (sockaddr *) &client_sock, sizeof(client_sock));
    if (conn_status < 0) return err_handler("Failed to connect to server!", client_fd);
    
    std::cout << "[INFO] Welcome to the chatserver." << std::endl;
    
    int BUFFERLEN = 1024;
    char BUFFER[BUFFERLEN];

    fd_set rfds;

    while(true){
        std::memset(BUFFER, '\0', BUFFERLEN);

        // the sets are modified in place and must be reinitialised 
        // std::cin file descriptor is 0
        FD_SET(0, &rfds);
        FD_SET(client_fd, &rfds);

        int select_status = select(client_fd+1, &rfds, NULL, NULL, NULL);
        if ( select_status < 0 ) return err_handler("Failed to listen to multiple file descriptors.", client_fd);

#if DEBUG
        std::cout << "[DEBUG] FD_SET status: (std::cin," << FD_ISSET(0, &rfds) << "), (client_fd," << FD_ISSET(client_fd, &rfds) << ")." << std::endl;
#endif

        if (FD_ISSET(0, &rfds)){

            strcpy(BUFFER, name.c_str());
            std::string data;
            std::getline(std::cin, data);
            strcpy(BUFFER + name_len, data.c_str());

            if(data == "exit") break;

#if DEBUG
            std::cout << "[DEBUG] writing to server..." << std::endl;
#endif

            int wr_status = write(client_fd, BUFFER, BUFFERLEN);
            if (wr_status < 0) return err_handler("Failed to write to server!", client_fd);

        } else if ( FD_ISSET(client_fd, &rfds) ){

            int rd_status = read(client_fd, BUFFER, BUFFERLEN);
            if (rd_status < 0) return err_handler("Failed to read from server!", client_fd);

            std::cout << BUFFER << std::endl;

        } else {
            return err_handler("Unexpected FD_SET state.", client_fd);
        }

    }

    std::cout << "[INFO] Closing connection to the chat server..." << std::endl; 

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}