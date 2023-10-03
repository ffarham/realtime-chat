#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>

#define DEBUG false

int error_handler(std::string msg, int fd){
    std::cout << "[ERROR] " << msg << '\n';
    if ( fd >= 0 ){
        std::cout << "[INFO] Closing socket " << fd << "...\n";
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    
    return 1;
}

int main(int argc, char **argv){

    if (argc < 3){
        std::cerr << "Expecting: <hostname> <port>\n";
        return 1;
    }

    char* hostname = argv[1];
    int port = atoi(argv[2]);

    std::string name;
    std::cout << "Enter name: ";
    std::getline(std::cin, name);
    std::cout << '\n';
    name += ": ";
    unsigned char name_len = name.size();

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0){
        return error_handler("Failed to create socket!", client_fd);
    }

    sockaddr_in client_sock;
    client_sock.sin_family = AF_INET;
    client_sock.sin_port = htons(port);
    // converts internet host address to network byte order (bianry)
    inet_aton(hostname, &client_sock.sin_addr);

    int conn_status = connect(client_fd, (sockaddr *) &client_sock, sizeof(client_sock));
    if (conn_status < 0) return error_handler("Failed to connect to server!", client_fd);
    
    std::cout << "[INFO] Welcome to the chatserver.\n";
    
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
        if ( select_status < 0 ) return error_handler("Failed to listen to multiple file descriptors.", client_fd);

#if DEBUG
        std::cout << "[DEBUG] FD_SET status: (std::cin," << FD_ISSET(0, &rfds) << "), (client_fd," << FD_ISSET(client_fd, &rfds) << ").\n";
#endif

        if (FD_ISSET(0, &rfds)){

            strcpy(BUFFER, name.c_str());
            std::string data;
            std::getline(std::cin, data);
            strcpy(BUFFER + name_len, data.c_str());

            if(data == "exit") break;

            int wr_status = write(client_fd, BUFFER, BUFFERLEN);
            if (wr_status < 0) return error_handler("Failed to write to server!", client_fd);

        } else if ( FD_ISSET(client_fd, &rfds) ){

            int bytes_read = read(client_fd, BUFFER, BUFFERLEN);
            if (bytes_read < 0) return error_handler("Failed to read from server!", client_fd);
            else if (bytes_read == 0) {
                std::cout << "[INFO] Lost connection to the chat server.\n";
                break;
            } else {
                std::cout << BUFFER << '\n';
            }

        } else {
            return error_handler("Unexpected FD_SET state.", client_fd);
        }

    }

    std::cout << "[INFO] Closing down the client...\n"; 

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}
