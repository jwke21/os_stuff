#include "InternalThread.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include "io/InternalLogger.h"

using namespace Threading;
using namespace std;

InternalThread::InternalThread(void* (*func)(void*), void* arg) {
    signal(SIGUSR1, ThreadManager::signalFunc);
    signal(SIGUSR2, ThreadManager::signalFunc);
    this->func = func;
    this->arg = arg;
    externalThread = NULL;
    currentState = CREATED;
}

InternalThread::InternalThread(void* (*func)(void*),
                               void* arg,
                               Thread* externalThread)
    : InternalThread(func, arg) {
    this->externalThread = externalThread;
    this->externalThread->state = CREATED;
}

InternalThread::~InternalThread() {}

State InternalThread::getState() {
    stateMutex.lock();
    State ret = currentState;
    stateMutex.unlock();
    return ret;
}

void InternalThread::setState(State newState) {
    stateMutex.lock();
    currentState = newState;
    if (externalThread != NULL)
        externalThread->state = newState;
    stateMutex.unlock();
}

void InternalThread::runningSigFunc(int sig) {
    if (InternalLogger::getLogger().isVerbose()) {
        InternalLogger::eventSink()
            << "[InternalThread] "
            << "Got signal " << strsignal(sig) << " on thread "
            << externalThread->name << "\n";
        InternalLogger::getLogger().flush();
    }
    if (sig == SIGUSR1) {
        if (InternalLogger::getLogger().isVerbose()) {
            InternalLogger::eventSink()
                << "[InternalThread] " << strsignal(sig)
                << " so pausing thread " << externalThread->name << "\n";
            InternalLogger::getLogger().flush();
        }
        setState(PAUSED);
        sigset_t sigSet;
        sigemptyset(&sigSet);
        sigaddset(&sigSet, SIGUSR2);

        bool cont = true;
        while (cont) {
            int sig;
            int ret = sigwait(&sigSet, &sig);
            if (ret == 0 && sig == SIGUSR2) {
                setState(RUNNING);
                stopExecutionMutex.lock();
                stopExecutionCond.notify_one();
                stopExecutionMutex.unlock();
                cont = false;
            }
        }
    }
}

bool InternalThread::joinWithTimeout() {
    try {
        return currentThread->try_join_for(boost::chrono::microseconds(MICROSECONDS_TICK));
    } catch (boost::thread_interrupted e) {
        return false;
    }
}

bool InternalThread::join() {
    try {
        currentThread->join();
    } catch (boost::thread_interrupted e) {
        return false;
    }
    return true;
}

void InternalThread::terminated() {
    setState(TERMINATED);
}

void InternalThread::sendSignal(int sig, State waitForState) {
    pthread_mutex_lock(&ThreadManager::threadSignalMutex);
    if (InternalLogger::getLogger().isVerbose()) {
        InternalLogger::eventSink()
            << "[InternalThread] "
            << "Sending signal '" << strsignal(sig) << "' to thread "
            << externalThread->name << "\n";
        InternalLogger::getLogger().flush();
    }
    ThreadManager::threadToSignal = this;
    pthread_kill(currentThread->native_handle(), sig);
    State state = getState();
    while (state != waitForState) {
        usleep(MICROSECONDS_TICK);
        if (InternalLogger::getLogger().isVerbose()) {
            InternalLogger::eventSink()
                << "[InternalThread] "
                << "Goal state (" << waitForState << ") != current state ("
                << state << ") for thread " << externalThread->name << "\n";
            InternalLogger::getLogger().flush();
        }
        state = getState();
    }
    ThreadManager::threadToSignal = NULL;
    if (InternalLogger::getLogger().isVerbose()) {
        InternalLogger::eventSink() << "[InternalThread] "
                                    << "Thread " << externalThread->name
                                    << " in state " << waitForState << "\n";
        InternalLogger::getLogger().flush();
    }
    pthread_mutex_unlock(&ThreadManager::threadSignalMutex);
}

void InternalThread::pause() {
    sendSignal(SIGUSR1, PAUSED);
}

void InternalThread::start() {
    setState(RUNNING);
    currentThread = new boost::thread(&InternalThread::startThread, this);
}

void InternalThread::resume() {
    sendSignal(SIGUSR2, RUNNING);
}

void* InternalThread::startThread(void* thread) {
    InternalThread* actualThread = ((InternalThread*)thread);
    return actualThread->func(actualThread->arg);
}

void InternalThread::stopExecution() {
    std::unique_lock<std::mutex> lock(stopExecutionMutex);
    stopExecutionCond.wait(lock);
}

Thread* InternalThread::getExternalThread() {
    return externalThread;
}