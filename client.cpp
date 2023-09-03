#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(){

    sockaddr_in server_addr;
    addrinfo *server;

    // create a socket and get the file descripter
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        std::cerr << "Failed to create socket!" << std::endl;
        return -1;
    }
    std::cout << "[INFO] Successfully created socket." << std::endl;

    // get host information
    if (getaddrinfo("0.0.0.1", "7008", NULL, &server) != 0){
        std::cerr << "Failed to get host information!" << std::endl;
        return -2;
    }

    for (int i =0; i < 14; i++){
        std::cout << server->ai_addr->sa_data[i] << std::endl;
    }

    // if (server->ai_addr->sa_data == NULL) std::cout << "[INFO] No address information." << std::endl;

    // std::cout << "[INFO] Retrieved host:"
    //             << *(server->ai_addr) << " yup" << std::endl;

    std::cout << "[INFO] Connecting to server..." << std::endl;
    // fill address fields before attempting to connect to the server
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7007);

    // Check if there is an address of the host
    if (server->ai_addr->sa_len > 0)
        std::memcpy((sockaddr_in *) &server_addr.sin_addr, (addrinfo *) server->ai_addr->sa_data, server->ai_addr->sa_len);
    else {
        std::cerr << "[ERROR] There is no a valid address for that hostname!\n";
        return -3;
    }

    if (connect(client_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection cannot be established!\n";
        return -4;
    }
    std::cout << "[INFO] Connection established.\n";

    // bind the socket to a port
    // sockaddr_in client_addr;
    // client_addr.sin_family = AF_INET;
    // // converts to network byte order, which is the standard in packets
    // client_addr.sin_port = htons(7007); // htons = host to network short
    // inet_pton(AF_INET, "0.0.0.0", &client_addr.sin_addr); // converts internet adddress from text format to numeric binary format

    



}