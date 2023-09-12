#pragma once

#include <vector>

void broadcast(char* msg, int source_fd);
int handle_client_request(int fd, int bytes_received, char* BUFFER);
