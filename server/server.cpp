#include "server.h"
#include "error_handler.h"
#include "threadpool.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <mutex>
#include <algorithm>
#include <cstring>

std::mutex print_mutex;
std::mutex connected_fds_mutex;
std::mutex tracked_fds_mutex;
std::mutex fd_set_mutex;
std::mutex client_store_mutex;
std::mutex queue_mutex;
std::mutex self_pipe_mutex;

std::unordered_map<int, client_info_pair> client_store;
int server_fd;
int self_pipe_client_fd;
fd_set rfds;
// This distinction is required because messages will be broadcasted to all fds currently connected.
// But not all fds currently connected will be in FD_SET (i.e. the fds currently handled or in queue
// to be handled by worker threads won't be in tracked/listened to).
std::vector<int> connected_fds;   // all fds currently connected to the server
std::vector<int> tracked_fds;   // file desciptors currently being tracked


std::vector<int> get_ready_fds(){
    // Method to determine the file descriptors ready for operation by iterating throught he file descriptors
    // currently being tracked.

    std::lock_guard<std::mutex> lock1(fd_set_mutex);
    std::lock_guard<std::mutex> lock2(tracked_fds_mutex);

    std::vector<int> result;
    for ( int& fd : tracked_fds ){
        if (FD_ISSET(fd, &rfds)) result.push_back(fd);
    }
    return result;
}

int initialise_server(int port){
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
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);


    // bind the socket to the network via a port
    char buf[INET_ADDRSTRLEN];
    std::memset(buf, 0, INET_ADDRSTRLEN);
    int bind_status = bind(server_fd, (sockaddr *) &server_addr, sizeof(server_addr));
    if ( bind_status < 0) return handle_error("Failed to bind socket to @"
        + (std::string) inet_ntop(AF_INET, &server_addr, buf, INET_ADDRSTRLEN)
        + ":" + std::to_string(ntohs(server_addr.sin_port)), server_fd); 
    
    std::cout << "[INFO] Socket is binded to @"  
            << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
            << ":" << ntohs(server_addr.sin_port) << std::endl;

    // start listening to incoming connections
    int listen_status = listen(server_fd, SOMAXCONN);
    if ( listen_status < 0 ) return handle_error("Failed to activate socket to listen mode!", server_fd);
    std::cout << "[INFO] Socket is now listening to connections..." << std::endl;

    return server_fd;
}

int initialise_self_pipe(const char* hostname, const int port){
    // Self pipe will be used to inform the main thread that the worker thread has finished work, and updated
    // the list of file descriptors to be tracked.
    self_pipe_client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( self_pipe_client_fd < 0 ){
        std::cerr << "Socket creation failed!" << std::endl;
        return 1;
    }

    sockaddr_in client_sock;
    std::memset(&client_sock, 0, sizeof(client_sock));
    client_sock.sin_family = AF_INET;
    client_sock.sin_port = htons(port);
    inet_aton(hostname, &client_sock.sin_addr);

    int conn_status = connect(self_pipe_client_fd, (sockaddr *) &client_sock, sizeof(client_sock));
    if (conn_status < 0) return handle_error("Failed to connect to server!", self_pipe_client_fd);

    // the server must accept the self-pipe to ensure when a worker thread sends a message,
    //  it doesn't create a new connection for it
    const int self_pipe_server_fd = accept(server_fd, nullptr, nullptr);
    if( self_pipe_server_fd < 0) return handle_error("Failed to accept incoming connection!", self_pipe_server_fd);

    return self_pipe_server_fd;
}

int main(int argc, char **argv){

    if (argc < 2) {
        std::cerr << "Expecting: <port>" << std::endl;
        return 1;
    }
    const int port = atoi(argv[1]);

    ThreadPool threadpool;

    server_fd = initialise_server(port);
    if (server_fd <= 2) return 1;

    const int self_pipe_server_fd = initialise_self_pipe("127.0.0.1", port);
    if (self_pipe_server_fd <= 2) return 1;

    {   // track input, server and self_pipe file descriptors
        std::lock_guard<std::mutex> lock(tracked_fds_mutex);
        tracked_fds.push_back(0);
        tracked_fds.push_back(server_fd);
        tracked_fds.push_back(self_pipe_server_fd);
    }

    while (true){

#if DEBUG
    {
        std::lock_guard<std::mutex> lock_print(print_mutex);
        std::lock_guard<std::mutex> lock(tracked_fds_mutex);
        std::cout << "[DEBUG] [MAIN THREAD] Tracked FDs: [";
        for (auto item : tracked_fds){
            std::cout << item << ", ";
        }
        std::cout << "]" << std::endl;
    }
#endif

        int max_fd;
        {   // Listen to multiple sockets.
            // The select system call examines the file descriptors currently being tracked and determines
            // which ones are ready for reading (or writing). It replaces the given FD_SET with a subset of 
            // file descriptors ready for operation. Finally, it returns the total number of file desciptors
            // that are ready in all given sets.
            
            // NOTE: For FDs active after select, their bits are not set to 0. So, we must manually set them to 0. 
            // For FDs not active after select, their bits are set to 0 for us. Thus we need the tracked_fds to 
            // reset them to 1.
            std::lock_guard<std::mutex> lock1(tracked_fds_mutex);
            std::lock_guard<std::mutex> lock2(fd_set_mutex);
            for (int tracked_fd : tracked_fds){
                FD_SET(tracked_fd, &rfds);
            }
            max_fd = (tracked_fds.size() > 0) ? *std::max_element(tracked_fds.begin(), tracked_fds.end()) : self_pipe_server_fd;
        }
            const int ready_fds_count = select(max_fd+1, &rfds, NULL, NULL, NULL);
            if ( ready_fds_count < 0 ) return handle_error("Failed to listen from multiple sockets.", server_fd);

        // TODO: optimise this, it's a latency bottleneck
        // We know the number of ready file descriptors from ready_fds_count, so can initialise an array instead.
        std::vector<int> ready_fds = get_ready_fds();
        if (ready_fds.size() == 0) return handle_error("Failed to match file descriptor in FD_SET.", server_fd);

        bool exit_flag = false;
        for (int ready_fd : ready_fds){
            if (ready_fd == 0)
            {   // SERVER ADMIN COMMANDS
                std::string command;
                std::getline(std::cin, command);
                if (command == "exit") {
                    exit_flag = true;
                    break;
                }
            }
            else if (ready_fd == self_pipe_server_fd)
            {   // receive update from the worker thread. The buffer must be read to update its state.
                char BUFFER[4];
                int bytes_received = recv(ready_fd, BUFFER, 4, 0);
                continue;
            }
            else if (ready_fd == server_fd) 
            {   // Main thread must handle incoming requests to establish connection.
                // By design, the server file descriptor responsibility is given to the main thread as it should always
                // be tracked in FD_SET.

                sockaddr_in client_addr;
                std::memset(&client_addr, 0, sizeof(client_addr));
                const socklen_t client_addr_len = sizeof(client_addr);
                const int client_fd = accept(server_fd, (sockaddr*) &client_addr, (socklen_t*) &client_addr_len);
                if( client_fd < 0) return handle_error("Failed to accept incoming connection!", client_fd);

                {   // Add the file descriptor to the list of file descriptors connected to the client.
                    std::lock_guard<std::mutex> lock(connected_fds_mutex);
                    connected_fds.push_back(client_fd);   
                }
                {   // Add clients metadata {fd : (IP, PORT)} for logging purposes.
                    std::lock_guard<std::mutex> lock(client_store_mutex);
                    const std::string client_ip = inet_ntoa(client_addr.sin_addr);
                    const int client_port = ntohs(client_addr.sin_port);
                    const std::pair<std::string, int> client_info(client_ip, client_port);
                    client_store.insert(std::make_pair(client_fd, client_info));

                    std::lock_guard<std::mutex> lock_print(print_mutex);
                    std::cout << "[INFO] Accepted connection from @" << client_ip << ":" << client_port << '\n';
                }
                {   // add the new file descriptor in the list of file descriptors being tracked
                    std::lock_guard<std::mutex> lock2(tracked_fds_mutex);
                    tracked_fds.push_back(client_fd);
                }

                // TODO: ping to everyone which user has joined the chat. Might need to get their name first.

            } else {
                // By design, messages from clients are handled by worker threads.
                {   // remove file descriptor from being tracked, once the worker thread has completed the work, it should
                    // add the file descriptor back to the list in order for it to be tracked
                    std::lock_guard<std::mutex> lock(tracked_fds_mutex);
                    tracked_fds.erase(std::remove(tracked_fds.begin(), tracked_fds.end(), ready_fd), tracked_fds.end());
                }
                {   // Remove bit in FD_SET to prevent it from being tracked
                    std::lock_guard<std::mutex> lock(fd_set_mutex);
                    FD_CLR(ready_fd, &rfds);
                }
                threadpool.enqueue(ready_fd);
            }

        }
        if (exit_flag) break;

    }
    {
        std::lock_guard<std::mutex> lock_print(print_mutex);
        std::cout << "[INFO] Shutting server down..." << std::endl;
    }

    // destroy all sockets
    for (int i = 1; i < connected_fds.size(); i++){
        close(connected_fds[i]);
    }
    close(server_fd);
}
