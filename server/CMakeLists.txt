cmake_minimum_required(VERSION 3.9.1)
project(realtime-chat-server)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

add_executable(server server.cpp error_handler.cpp threadpool.cpp client_handler.cpp)