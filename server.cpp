#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv){

    // validate arguments
    if (argc < 2) {
        std::cerr << "Expecting: <port>" << std::endl;
        return 1;
    }
    int port = atoi(argv[1]);

    // create a socket
    int sock_listener = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock_listener < 0 ){
        std::cerr << "Socket creation failed!" << std::endl;
        return 1;
    }
    std::cout << "[INFO] Socket is successfully created." << std::endl;

    // address info to bind socket
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7007);
    inet_pton(AF_INET, "0.0.0.0", &server_addr.sin_addr);


    // bind the socket to the network via a port
    char buf[INET_ADDRSTRLEN];
    std::memset(buf, 0, INET_ADDRSTRLEN);
    int bind_status = bind(sock_listener, (sockaddr *) &server_addr, sizeof(server_addr));
    if ( bind_status < 0){
        std::cerr << "Failed to bind socket to @"
                  << inet_ntop(AF_INET, &server_addr, buf, INET_ADDRSTRLEN)
                  << ":" << ntohs(server_addr.sin_port) << std::endl;
        return 1;
    }
    std::cout << "[INFO] Socket is binded to @"  
            << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
            << ":" << ntohs(server_addr.sin_port) << std::endl;

    // start listening to incoming connections
    int listen_status = listen(sock_listener, SOMAXCONN);
    if ( listen_status < 0 ){
        std::cerr << "Failed to activate socket to listen mode!" << std::endl;
        return 1;
    }
    std::cout << "[INFO] Socket is now listening to connections..." << std::endl;

    // accept incoming connections
    // TODO: multithread this later
    sockaddr_in client_addr;
    std::memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(sock_listener, (sockaddr*) &client_addr, (socklen_t*) &client_addr_len);
    if( client_fd < 0){
        std::cerr << "Failed to accept incoming connection!" << std::endl;
        return 1;
    }

    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);

    std::cout << "[INFO] Accepted connection from @" << client_ip << ":" << client_port << std::endl;

    int BUFFERLEN = 1024;
    char BUFFER[BUFFERLEN];
    while(true){
        memset(BUFFER, 0, BUFFERLEN);

        // receive from the client
        // important to null terminate buffers (having last char in buffer as null)
        int bytes_received = recv(client_fd, BUFFER, BUFFERLEN-1, 0);
        if(bytes_received < 0){
            std::cerr << "Failed to receive data from client!" << std::endl;
            return 1;
        }
        if (bytes_received == 0){
            std::cout << "[INFO] Connection closed by client." << std::endl;
            break;
        }
        if (BUFFER[bytes_received-1] == '\n') BUFFER[bytes_received-1] = '\0';


        std::cout << "@" << client_ip << ":" << client_port << ": " << BUFFER << std::endl;

        // echo message back to the client
        BUFFER[bytes_received-1] = '\n';
        int bytes_sent = send(client_fd, BUFFER, bytes_received, 0);
        if (bytes_sent < 0){
            std::cerr << "Failed to send message back to client!" << std::endl;
        }
    }

    std::cout << "[INFO] Shutting socker down." << std::endl;
    // shut down both sides of the socket
    shutdown(client_fd, SHUT_RDWR);


    // shutdown(sock_listener, SHUT_RDWR);
}