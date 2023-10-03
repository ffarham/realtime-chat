#pragma once

#include <vector>
#include <queue>
//#include <thread>
#include <boost/thread.hpp>
#include <mutex>
#include <condition_variable>

#define NUM_WORKERS 4

class ThreadPool {
private:
    std::queue<int> work;
    std::mutex pool_mutex;
    std::condition_variable queue_notifier;
    boost::thread workers[NUM_WORKERS];
    bool keep_alive;
    void worker_function();

// std::thread::hardware_concurrency()
public:
    ThreadPool();
    ~ThreadPool();

    void enqueue(int fd);

};
