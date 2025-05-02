#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <queue>

class ThreadManager {
public:
    ThreadManager(size_t numThreads);
    ~ThreadManager();

    void start();
    void stop();
    void waitForCompletion(); 

    void addTask(std::function<void()> task); 
    size_t getTaskCount() const;

    void setNumThreads(size_t numThreads);
    size_t getNumThreads() const;

    size_t getActiveThreadCount() const;

    bool isRunning() const;

private:
    void workerThread(size_t threadId); 

    void processNextTask(); 

    std::vector<std::thread> threads;
    std::queue<std::function<void()>> taskQueue;
    mutable std::mutex taskMutex;
    mutable std::mutex completionMutex; 
    std::condition_variable taskCondition;
    std::atomic<bool> running{false};
    std::atomic<size_t> activeThreads{0};
    size_t numThreads;

    std::vector<std::atomic<size_t>> threadLoads; 
};