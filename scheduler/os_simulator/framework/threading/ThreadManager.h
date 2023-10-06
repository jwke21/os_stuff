#ifndef OS_THREADING_THREADMANAGER_H
#define OS_THREADING_THREADMANAGER_H

#include <cstdlib>
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include "boost/asio/thread_pool.hpp"
#include "InternalThread.h"
#include "LockManager.h"
#include "Thread.h"
#include "ThreadingConstants.h"

using namespace std;

namespace Threading {
class ThreadManager {
    friend class InternalThread;

   private:
    ThreadManager();
    ~ThreadManager();
    vector<InternalThread> waitList;
    int tick;
    std::shared_ptr<InternalThread> runningThread;
    std::shared_ptr<InternalThread> idleThread;
    static ThreadManager* singleton;
    void* idleFunc();
    pthread_mutex_t runningThreadMutex;
    bool keepRunning;
    pthread_mutex_t shutdownMutex;
    pthread_mutex_t threadMappingMutex;
    void setRunningThread(std::shared_ptr<InternalThread> running);
    map<Thread*, std::shared_ptr<InternalThread>> threadMapping;
    bool areAllThreadsTerminated();
    static void signalFunc(int sig);
    static InternalThread* threadToSignal;
    static pthread_mutex_t threadSignalMutex;
    static void* startIdleThread(ThreadManager* threadManager);
    LockManager* lockManager;
    std::mutex pauseMutex;

   public:
    static ThreadManager* getInstance();
    static void shutdown();
    void sleepCurrentThread();
    void waitForFinish();
    static void destroyThreadManager();
    std::shared_ptr<InternalThread> currentThread();
    int currentTick();
    void createThread(Thread* thread);
    void start();
};
}  // namespace Threading

#endif  // OS_THREADING_THREADMANAGER_H
