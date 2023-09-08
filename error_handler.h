#pragma once

#include <string>

int error_handler(std::string msg, int fd);
int error_handler(std::string msg, std::vector<int> fds);