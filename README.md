# Realtime Chat Client and Server

Challenge: https://codingchallenges.fyi/challenges/challenge-realtime-chat

The project implements both client and server in C++.

### Client
Simple client that runs on a port and connects to the server using TCP.

### Server
Employs multi-threading via the threadpool. The main thread queue messages for the workers threads to work through. Worker threads are notified of available work using conditional variables. The threads synchronise among themselves using mutex locks.

The server is able to listen to multiple clients using the SELECT system call that monitors file descriptors for reading (i.e. if a client has sent a message). On receiving a message from client X, the file desciptor is enqueued for worker threads. Additionally, client X's file descriptor is removed from the FD_SET in SELECT to prevent the main thread from continuously enqueuing the same work. this is because a worker thread needs to read from client X's file descriptor to prevent SELECT from picking up on the same work. After completing the work regarding client X's file descriptor, the worker thread uses a self-pipe to notify the main thread that clinet X's file descriptor is ready to be monitored again for reading.