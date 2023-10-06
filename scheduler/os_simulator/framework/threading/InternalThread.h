#ifndef OS_THREADING_THREAD_H
#define OS_THREADING_THREAD_H

#include <cstdlib>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "boost/thread/thread.hpp"
#include "Thread.h"
#include "ThreadingConstants.h"

using namespace std;
namespace Threading {
class InternalThread {
   public:
    InternalThread(void* (*func)(void*), void* arg);
    InternalThread(void* (*func)(void*), void* arg, Thread* externalThread);
    ~InternalThread();
    void terminated();
    void pause();
    void resume();
    void start();
    State getState();
    bool joinWithTimeout();
    bool join();
    Thread* getExternalThread();
    void sendSignal(int sig, State waitForState);
    void runningSigFunc(int sig);
    void stopExecution();

   private:
    boost::thread *currentThread;
    std::mutex stateMutex;
    std::mutex stopExecutionMutex;
    std::condition_variable stopExecutionCond;

    State currentState;
    void* (*func)(void*);

    void setState(State newState);
    static void* startThread(void* thread);
    void* arg;
    Thread* externalThread;
};
}  // namespace Threading

#endif  // OS_THREADING_THREAD_H
