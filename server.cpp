#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#include "error_handler.h"

#define DEBUG false
#define BUFFERLEN 1024

// store of client meta data
typedef std::pair<std::string, int> client_info_pair;
std::unordered_map<int, client_info_pair> client_store;
int max_fd;
fd_set rdfs;
std::vector<int> fds;


// LOCKS:
// max_fd
// std::cout
// rfds + fds


void broadcast(char* msg, std::vector<int>& target_fds, int source_fd){
    for (int i = 2; i < target_fds.size(); i++){
        int fd = target_fds[i];
        if (fd == source_fd) continue;
        int bytes_sent = send(fd, msg, BUFFERLEN, 0);
        if (bytes_sent < 0) error_handler("Failed to send message.", fd);
    }
}

int get_current_fd(std::vector<int>& fds, fd_set& rfds){
    for ( int& fd : fds ){
        if (FD_ISSET(fd, &rfds)) return fd;
    }
    return -1;
}

void handle_incoming_connections(int server_fd){
    // accept incoming connections
    sockaddr_in client_addr;
    std::memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*) &client_addr, (socklen_t*) &client_addr_len);
    if( client_fd < 0) error_handler("Failed to accept incoming connection!", client_fd);

    // update max file descriptor
    if (client_fd > max_fd) max_fd = client_fd;
    fds.push_back(client_fd);   // TODO: lock this

    // store client ip and port
    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    std::pair<std::string, int> client_info(client_ip, client_port);
    client_store.insert(std::make_pair(client_fd, client_info));

    std::cout << "[INFO] Accepted connection from @" << client_ip << ":" << client_port << std::endl;

    // TODO: ping to everyone which user has joined the chat
}

void handle_client_request(int current_fd){
    char BUFFER[BUFFERLEN];
    std::memset(BUFFER, 0, BUFFERLEN);

    int bytes_received = recv(current_fd, BUFFER, BUFFERLEN, 0);

    auto entry = client_store.find(current_fd);
    if (entry == client_store.end()) {
        error_handler("Missing file descriptor <" + std::to_string(current_fd) + "> in client store", current_fd);
        return;
    }


    if (bytes_received < 0 ) {

        error_handler("Failed to receive bytes from @" + entry->second.first + ":" + std::to_string(entry->second.second), -1);
        auto position = std::find(fds.begin(), fds.end(), current_fd);
        if (position == fds.end()) error_handler("Missing file descriptor <" + std::to_string(current_fd) + "> from vector.", -1);
        else fds.erase(position);
        
    } else if (bytes_received == 0){

        client_store.erase(current_fd);
        
        // TODO: lock this
        auto position = std::find(fds.begin(), fds.end(), current_fd);
        if (position == fds.end()) error_handler("Missing file descriptor <" + std::to_string(current_fd) + "> from vector.", -1);
        else fds.erase(position);

        int max_fd_checker = *std::max_element(fds.begin(), fds.end());
        // TODO: lock this
        if (max_fd_checker != max_fd) max_fd = max_fd_checker;

        // Need to close down the server's respective socket for the client. 
        shutdown(current_fd, SHUT_RDWR);
        close(current_fd);
        
    } else {
        broadcast(BUFFER, fds, current_fd);
    }
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

    max_fd = server_fd;
    fds.push_back(0);
    fds.push_back(server_fd);

    while (true){

#if DEBUG
        std::cout << "[DEBUG] Client store: ";
        for (auto item = client_store.begin(); item != client_store.end(); item++){
            std::cout << item->first << " => (" << item->second.first << "," <<  item->second.second << ")" << std::endl;
        }
        std::cout << "[DEBUG] File descriptors: [";
        for (auto fd : fds){
            std::cout << fd << ",";
        }
        std::cout << "]" << std::endl;
#endif
        
        for (int& fd : fds){
            FD_SET(fd, &rdfs);
        }

        int sel_status = select(max_fd+1, &rdfs, NULL, NULL, NULL);
        if ( sel_status < 0 ) return error_handler("Failed to listen from multiple sockets.", server_fd);

        int current_fd = get_current_fd(fds, rdfs);
        if (current_fd < 0) return error_handler("Failed to match file descriptor in FD_SET.", server_fd);

#if DEBUG
        std::cout << "[DEBUG] current file director is " << current_fd << "." << std::endl;
#endif

        if (current_fd == 0){
            std::string command;
            std::getline(std::cin, command);
            if (command == "exit") break;
        } 
        else if (current_fd == server_fd) handle_incoming_connections(server_fd);
        else handle_client_request(current_fd);
    }

    std::cout << "[INFO] Shutting server down..." << std::endl;

    // destroy all sockets
    for (int i = 1; i < fds.size(); i++){
        close(fds[i]);
    }
}
