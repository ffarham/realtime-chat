#include "client_handler.h"
#include "server.h"
#include "error_handler.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <thread>


void broadcast(char* msg, int source_fd){
    // Relay message from source  file descriptor to all connected file descriptors
    // TODO: multicast message
    std::lock_guard<std::mutex> lock_fds(connected_fds_mutex);
    for (int fd : connected_fds){
        if (fd == source_fd) continue;
        int bytes_sent = send(fd, msg, BUFFERLEN, 0);
        if (bytes_sent < 0) handle_error("Failed to send message.", fd);
    }
}


int handle_client_request(int fd, int bytes_received, char *BUFFER){
    // Method to handle client request. Note that the fd is already removed from the 
    // tracked file descriptors vector before hand. Depending on the success, we will add the 
    // file descriptor back to the tracker list.

    std::unordered_map<int, client_info_pair>::iterator entry;
    {   // get the entry for where the client meta data is stored
        // TODO: improve this
        std::lock_guard<std::mutex> lock_client_store(client_store_mutex);
        entry = client_store.find(fd);
        if (entry == client_store.end()) {
            handle_error("Missing file descriptor <" + std::to_string(fd) + "> in client store", fd);
            {   // remove client from the list of connected users
                std::lock_guard<std::mutex> lock_fds(connected_fds_mutex);
                connected_fds.erase(std::remove(connected_fds.begin(), connected_fds.end(), fd), connected_fds.end());
            }        
            return 1;
        }
    }

    if (bytes_received <= 0)
    {   
        // Either the client closed their connected or an error occured. In both scenarios, handle closing
        // down the clients file descriptor.

        if (bytes_received < 0) handle_error("Failed to receive bytes from @" + entry->second.first + ":" + std::to_string(entry->second.second), -1);
        else {
            // TODO: notify everyone connected who has left the chat
        }
        {   // remove clients meta data
            std::lock_guard<std::mutex> lock_client_store(client_store_mutex);
            client_store.erase(fd);
        }
        {   // remove client from the list of connected users
            std::lock_guard<std::mutex> lock_fds(connected_fds_mutex);
            connected_fds.erase(std::remove(connected_fds.begin(), connected_fds.end(), fd), connected_fds.end());
        }   

        close(fd);
        
    } else {
        {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << BUFFER << std::endl;
        }
        broadcast(BUFFER, fd);

        {   // add the file descriptor back to the tracking list
            std::lock_guard<std::mutex> lock(tracked_fds_mutex);
            tracked_fds.push_back(fd);
        }
        {   // notify the main thread of completed work.
            // required because the main thread will only track the file descriptor if the select 
            // system call has detected a file descriptor ready to be operated on.
            // TODO: OPTIMISE: dont need to send anything to the server
            std::lock_guard<std::mutex> lock(self_pipe_mutex);
            int wr_status = write(self_pipe_client_fd, "DONE", 4);
            if (wr_status < 0) handle_error("Failed to notify server of completed work!", -1);
        }
    }
    return 0;
}
