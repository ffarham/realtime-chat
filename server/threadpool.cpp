#include "threadpool.h"
#include "server.h"
#include "client_handler.h"
#include <cstring>

#include <iostream>
#include <sys/socket.h>


void ThreadPool::worker_function(){

    // each thread should have its own buffer to operate with
    char BUFFER[BUFFERLEN];

    while (true) {
        std::memset(BUFFER, 0, BUFFERLEN);

        int fd;
        {
            std::unique_lock<std::mutex> lock_pool(this->pool_mutex);
#if DEBUG
            {
                std::lock_guard<std::mutex> lock_print(print_mutex);
                std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] acquired lock." << std::endl;
            }
#endif
            while (this->work.size() == 0 && this->keep_alive) {
#if DEBUG                
                {
                    std::lock_guard<std::mutex> lock_print(print_mutex);
                    std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] waiting for work..." << std::endl; 
                }
#endif
                this->queue_notifier.wait(lock_pool);
            }
            if (!this->keep_alive) break;

#if DEBUG
            {
                std::lock_guard<std::mutex> lock_print(print_mutex);
                std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] Queue size: " << work.size() << std::endl;
            }
#endif
            {
                std::lock_guard<std::mutex> lock_queue(queue_mutex);
                fd = work.front();
                this->work.pop();
            }
        }
#if DEBUG
            {
                std::lock_guard<std::mutex> lock_print(print_mutex);
                std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] Doing work on " << fd << std::endl;
            }
#endif 

        // read the message sent by the client into the threads own buffer
        int bytes_received = recv(fd, BUFFER, BUFFERLEN, 0);

        handle_client_request(fd, bytes_received, BUFFER);
#if DEBUG
            {
                std::lock_guard<std::mutex> lock_print(print_mutex);
                std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] Completed work!" << std::endl;
            }
#endif
    }
#if DEBUG
        {
            std::lock_guard<std::mutex> lock_print(print_mutex);
            std::cout << "[DEBUG] " << boost::this_thread::get_id() << " is terminating." << std::endl;
        }
#endif
}

ThreadPool::ThreadPool() : keep_alive(true) {

    for (int i = 0; i < NUM_WORKERS; i++){
        // TODO: optimise this
        boost::thread t(&ThreadPool::worker_function, this);
        workers[i] = std::move(t);
#if DEBUG
    {
        std::lock_guard<std::mutex> lock_print(print_mutex);
        std::cout << "[DEBUG] [" << boost::this_thread::get_id() << "] Created thread " << boost::this_thread::get_id() << std::endl;
    }
#endif

    }

}

ThreadPool::~ThreadPool(){
#if DEBUG
    {
        std::lock_guard<std::mutex> lock_print(print_mutex);
        std::cout << "[DEBUG] Destroying threadpool." << std::endl;
    }
#endif
    {
        std::lock_guard<std::mutex> lock(this->pool_mutex);
        this->keep_alive = false;
    }
    this->queue_notifier.notify_all();
    // for (auto& worker : this->workers){
    for (int i = 0; i < NUM_WORKERS; i++)
    workers[i].join();
    // }
}

void ThreadPool::enqueue(int fd){
#if DEBUG
    {
        std::lock_guard<std::mutex> lock_print(print_mutex);
        std::cout << "[DEBUG] [" << bost::this_thread::get_id() << "] Adding " << fd << " to the work queue." << std::endl;
    }
#endif
    std::lock_guard<std::mutex> lock(queue_mutex);
    this->work.push(fd);
    this->queue_notifier.notify_one();
}
