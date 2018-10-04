#include "common/concurrency/WorkerPool.h"

using namespace std;

WorkerPool::WorkerPool(int size, const shared_ptr<spd::logger> &logger) : size(size), logger(logger) {
    logger->debug("Creating {} worker threads", size);
    threadQueues.reserve(size);
    for (int i = 0; i < size; i++) {
        auto &last = threadQueues.emplace_back(make_unique<Queue>());
        auto *ptr = last.get();
        auto threadIdleName = "idle" + to_string(i);
        threads.emplace_back(runInAThread(threadIdleName, [ptr, logger, threadIdleName]() {
            bool repeat = true;
            while (repeat) {
                Task_ task;
                setCurrentThreadName(threadIdleName);
                ptr->wait_dequeue(task);
                logger->debug("Worker got task");
                repeat = task();
            }
        }));
    }
    logger->debug("Worker threads created");
}

WorkerPool::~WorkerPool() {
    auto logger = this->logger;
    multiplexJob_([logger]() {
        logger->debug("Killing worker thread");
        return false;
    });
    // join will be called when destructing joinable;
}

void WorkerPool::multiplexJob(const string &taskName, WorkerPool::Task t) {
    multiplexJob_([t{move(t)}, taskName] {
        setCurrentThreadName(taskName);
        t();
        return true;
    });
}

void WorkerPool::multiplexJob_(WorkerPool::Task_ t) {
    logger->debug("Multiplexing job");
    for (int i = 0; i < size; i++) {
        threadQueues[i]->enqueue(t);
    }
}
