#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#include "error_handler.h"

#define DEBUG true

// PROBLEM: when the first client closes, all other clients also close, including the server. Potential cause in the vector
// from which we are removing the fd.

int get_current_fd(std::vector<int>& fds, fd_set& rfds){
    for ( int& fd : fds ){
        if (FD_ISSET(fd, &rfds)) return fd;
    }
    return -1;
}

int main(int argc, char **argv){

    // validate arguments
    if (argc < 2) {
        std::cerr << "Expecting: <port>" << std::endl;
        return 1;
    }
    int port = atoi(argv[1]);

    // create a socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( server_fd < 0 ){
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
    int bind_status = bind(server_fd, (sockaddr *) &server_addr, sizeof(server_addr));
    if ( bind_status < 0) return error_handler("Failed to bind socket to @"
        + (std::string) inet_ntop(AF_INET, &server_addr, buf, INET_ADDRSTRLEN)
        + ":" + std::to_string(ntohs(server_addr.sin_port)), server_fd); 
    
    std::cout << "[INFO] Socket is binded to @"  
            << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
            << ":" << ntohs(server_addr.sin_port) << std::endl;

    // start listening to incoming connections
    int listen_status = listen(server_fd, SOMAXCONN);
    if ( listen_status < 0 ) return error_handler("Failed to activate socket to listen mode!", server_fd);
    std::cout << "[INFO] Socket is now listening to connections..." << std::endl;

    // file descriptor store
    int max_fd = server_fd;
    fd_set rdfs;
    std::vector<int> fds;
    fds.push_back(0);
    fds.push_back(server_fd);

    // client data
    typedef std::pair<std::string, int> client_info_pair;
    std::unordered_map<int, client_info_pair> client_store;

    const int BUFFERLEN = 1024;
    char BUFFER[BUFFERLEN];

    while (true){
        for (int& fd : fds){
            FD_SET(fd, &rdfs);
        }

        int sel_status = select(max_fd+1, &rdfs, NULL, NULL, NULL);
        if ( sel_status < 0 ) return error_handler("Failed to listen from multiple sockets.", server_fd);

#if DEBUG
        std::cout << "File descriptors: ";
        for (int& fd : fds){
            std::cout << fd;
        }
        std::cout << std::endl;
#endif

        int current_fd = get_current_fd(fds, rdfs);
        if (current_fd < 0) return error_handler("Failed to match file descriptor in FD_SET.", server_fd);

#if DEBUG
        std::cout << "[DEBUG] current file director is " << current_fd << "." << std::endl;
#endif

        if (current_fd == 0){
            std::string command;
            std::getline(std::cin, command);
            if (command == "exit"){
                for (int& fd : fds){
                    shutdown(fd, SHUT_RDWR);
                    close(fd);
                }
                break;
            }

        } else if (current_fd == server_fd) {

            // accept incoming connections
            sockaddr_in client_addr;
            std::memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr*) &client_addr, (socklen_t*) &client_addr_len);
            if( client_fd < 0) error_handler("Failed to accept incoming connection!", client_fd);

            // update max file descriptor
            if (client_fd > max_fd) max_fd = client_fd;
            fds.push_back(client_fd);

            // store client ip and port
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);
            std::pair<std::string, int> client_info(client_ip, client_port);
            client_store.insert(std::make_pair(client_fd, client_info));

            std::cout << "[INFO] Accepted connection from @" << client_ip << ":" << client_port << std::endl;

            // TODO: ping to everyone which user has joined the chat

        } else {

            std::memset(BUFFER, 0, BUFFERLEN);
            int bytes_received = recv(current_fd, BUFFER, BUFFERLEN, 0);

#if DEBUG
            std::cout << "[DEBUG] Received " << bytes_received << " bytes." << std::endl;
#endif

            if(bytes_received < 0){
                auto entry = client_store.find(current_fd);
                if (entry == client_store.end()) return error_handler("Missing file descriptor <" + std::to_string(current_fd) + "> in client store.", server_fd);
                error_handler("Failed to received message from @" + entry->second.first + ":" + std::to_string(entry->second.second), -1);

            } else if (bytes_received == 0){
                auto position = std::find(fds.begin(), fds.end(), current_fd);
                if (position == fds.end()) return error_handler("Missing element <" + std::to_string(current_fd) + "> in file desciptor vector.", server_fd);
                fds.erase(position);

                auto entry = client_store.find(current_fd);
                if (entry == client_store.end()) return error_handler("Missing file descriptor <" + std::to_string(current_fd) + "> in client store", server_fd);
                client_store.erase(current_fd);

                int max_fd_checker = *std::max_element(fds.begin(), fds.end());
                if (max_fd_checker != max_fd) max_fd = max_fd_checker;

                std::cout << "[INFO] Connection closed by @." << (std::string) entry->second.first + ":" << std::to_string(entry->second.second) << "." << std::endl;

                // TODO: ping everyone which user has left the chat
            } else {

                std::cout << BUFFER << std::endl;

                // echo message back to the client
                int bytes_sent = send(current_fd, BUFFER, bytes_received, 0);
                if (bytes_sent < 0) error_handler("Failed to send message back to client!", -1);
            }

        }
    }

    std::cout << "[INFO] Shutting server down..." << std::endl;

    // blocking both sending and recieving, stil able to receive pending data the client already sent
    // shutdown(server_fd, SHUT_RDWR);

    // destroy the socket
    close(server_fd);
}
