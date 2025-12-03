#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <functional> 
#include "threadsafe_queue.h"
#include "join_threads.h"
#include <atomic>
class thread_pool
{   
private:
    std::atomic_bool done;
    ThreadSafeQueue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;

    void worker_thread()
    {
        while(!done)
        {
            std::function<void()> task;
            // if(work_queue.try_pop(task))
            // {
            //     task();
            // }
            // else
            // {
            //     std::this_thread::yield();
            // }

            work_queue.wait_and_pop(task);
            task();
        }
    }

public:
    thread_pool()
        :done(false),joiner(threads)
    {
        unsigned const thread_count = std::thread::hardware_concurrency();
        //unsigned const thread_count = 40;
        try
        {
            for (unsigned i = 0; i < thread_count; ++i)
            {
                threads.push_back(std::thread(&thread_pool::worker_thread,this));
            }
        }
        catch(...)
        {
            done = true;
            throw;
        }
    }
    ~thread_pool()
    {
        done = true;
    }
    template<typename FunctionType>
    void submit(FunctionType f)
    {
        work_queue.push(std::function<void()>(f));
    }

};
#endif