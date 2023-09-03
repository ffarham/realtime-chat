#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

int main(int argc, char **argv){

    if (argc < 3){
        std::cerr << "Expecting: <hostname> <port>" << std::endl;
        return 1;
    }

    char* hostname = argv[1];
    int port = atoi(argv[2]);

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0){
        std::cerr << "Failed to create socket!" << std::endl;
        return 1;
    }

    sockaddr_in client_sock;
    client_sock.sin_family = AF_INET;
    client_sock.sin_port = htons(port);
    // converts internet host address to network byte order (bianry)
    inet_aton(hostname, &client_sock.sin_addr);

    int conn_status = connect(client_fd, (sockaddr *) &client_sock, sizeof(client_sock));
    if (conn_status < 0){
        std::cerr << "Failed to connect to server!" << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] Connected to server." << std::endl;
    
    int BUFFERLEN = 1024;
    char BUFFER[BUFFERLEN];
    std::memset(BUFFER, 0, BUFFERLEN);

    while(true){
        std::string data;
        std::cout << "send: ";
        getline(std::cin, data);
        std::memset(BUFFER,0,BUFFERLEN);
        strcpy(BUFFER, data.c_str());
        if(data == "exit") break;

        int wr_status = write(client_fd, BUFFER, BUFFERLEN);
        if (wr_status < 0) std::cerr << "Failed to write to server!" << std::endl;

        std::memset(BUFFER,0, BUFFERLEN);

        int rd_status = read(client_fd, BUFFER, BUFFERLEN-1);
        if (rd_status < 0) std::cerr << "Failed to read from server!" << std::endl;

        std::cout << "recv: " << BUFFER << std::endl;

    }

    close(client_fd);
}