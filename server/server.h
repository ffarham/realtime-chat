#pragma once

#include <unordered_map>
#include <sys/select.h>
#include <vector>
#include <mutex>

#define DEBUG false
#define BUFFERLEN 1024

extern std::mutex print_mutex;
extern std::mutex connected_fds_mutex;
extern std::mutex tracked_fds_mutex;
extern std::mutex fd_set_mutex;
extern std::mutex client_store_mutex;
extern std::mutex queue_mutex;
extern std::mutex self_pipe_mutex;


// store of client meta data
typedef std::pair<std::string, int> client_info_pair;
extern std::unordered_map<int, client_info_pair> client_store;
extern int server_fd;
extern int self_pipe_client_fd;
extern fd_set rfds;
extern std::vector<int> connected_fds;
extern std::vector<int> tracked_fds;
