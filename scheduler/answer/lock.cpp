#include <Logger.h>
#include <stddef.h>
#include <string.h>

#include "List.h"
#include "Lock.h"
#include "Map.h"
#include "thread_lock.h"

const char* mapOfLockToThread = CREATE_MAP(const char*);

#pragma region Lock Callbacks

void lockCreated(const char* lockId) {
    char line[1024];
    // Put the new lockId in the map and initialize it to NULL
    sprintf(line,
            "[lockCreated] Putting key '%s' into map and initializing "
            "to NULL\n",
            lockId);
    verboseLog(line);
    PUT_IN_MAP(const char*, mapOfLockToThread, lockId, NULL);
}

void lockAttempted(const char* lockId, Thread* thread) {
    char line[1024];

    // Inform the debug console of the attempt to acquire the lock
    sprintf(line,
            "[lockAttempted] Thread '%s' attempting to acquire lock '%s'\n",
            thread->name, lockId);
    verboseLog(line);

    // Check if the lock is already controlled by a thread
    sprintf(line, "[lockAttempted] Checking if lock '%s' already held\n",
            lockId);
    verboseLog(line);
    Thread* threadHoldingLock =
        (Thread*)GET_FROM_MAP(const char*, mapOfLockToThread, lockId);
    if (threadHoldingLock == NULL) {
        sprintf(line, "[lockAttempted] Lock '%s' not already held\n", lockId);
        verboseLog(line);
        return;
    }
    sprintf(line, "[lockAttempted] Lock '%s' already held by thread '%s'\n",
            lockId, threadHoldingLock->name);
    verboseLog(line);

    // Check if the thread holding the lock has a lower priority than the
    // current thread
    if (threadHoldingLock->priority < thread->priority) {
        sprintf(
            line,
            "[lockAttempted] Thread '%s' is lower priority than thread '%s'\n",
            threadHoldingLock->name, thread->name);
        verboseLog(line);
        // Donate current thread's priority to thread holding the lock to avoid
        // infinite loop
        int temp = threadHoldingLock->priority;
        threadHoldingLock->priority = thread->priority;
        thread->priority = temp;
        sprintf(line,
                "[lockAttempted] Thread '%s' donated priority to thread '%s'\n",
                thread->name, threadHoldingLock->name);
        verboseLog(line);
        sortList(readyList, compareReadyThreads);
    }
}

void lockAcquired(const char* lockId, Thread* thread) {
    char line[1024];

    sprintf(line, "[lockAcquired] Giving lock '%s' to thread '%s'\n", lockId,
            thread->name);
    verboseLog(line);
    // Put the thread in the map with the lock that it holds as its key
    PUT_IN_MAP(const char*, mapOfLockToThread, lockId, (void*)thread);
    // Inform the debug console the successful lock acquisition
    sprintf(line,
            "[lockAcquired] Lock '%s' successfully acquired by thread '%s'\n",
            lockId, thread->name);
    verboseLog(line);
}

void lockFailed(const char* lockId, Thread* thread) {
    char line[1024];

    // Inform the debug console of the unsuccessful attempt to acquire the lock
    sprintf(line,
            "[lockFailed] Lock '%s' unable to be acquired by thread '%s'\n",
            lockId, thread->name);
    verboseLog(line);
}

void lockReleased(const char* lockId, Thread* thread) {
    char line[1024];

    // Remove the thread that released the lock from the map, and reinitialize
    // the value of the key in the map to NULL
    REMOVE_FROM_MAP(const char*, mapOfLockToThread, lockId);
    PUT_IN_MAP(const char*, mapOfLockToThread, lockId, NULL);
    sprintf(line,
            "[lockReleased] Lock '%s' successfully released by thread '%s'\n",
            lockId, thread->name);
    verboseLog(line);
    // Check if the thread received a priority donation
    if (thread->priority != thread->originalPriority) {
        sprintf(line,
                "[lockReleased] Thread '%s' received a priority donation\n",
                thread->name);
        verboseLog(line);
        // Restore the priorities of all threads to ensure that the highest
        // priority thread waiting for the lock will receive it next
        Thread* curThread = NULL;
        for (int i = 0; i < listSize(readyList); i++) {
            curThread = (Thread*)listGet(readyList, i);
            // If the thread's priority does not match its original priority,
            // then restore its original priority
            if (curThread->priority != curThread->originalPriority) {
                sprintf(line,
                        "[lockReleased] Restoring thread '%s's priority\n",
                        curThread->name);
                verboseLog(line);
                curThread->priority = curThread->originalPriority;
                thread->priority = thread->originalPriority;
            }
        }
        sortList(readyList, compareReadyThreads);
    }
}

#pragma endregion

#pragma region Lock Functions

Thread* getThreadHoldingLock(const char* lockId) {
    char line[1024];

    sprintf(line, "[getThreadHoldingLock] Checking that key '%s' is in map\n",
            lockId);
    verboseLog(line);

    Thread* ret = (Thread*)GET_FROM_MAP(const char*, mapOfLockToThread, lockId);
    sprintf(line,
            "[getThreadHoldingLock] Getting thread '%s' holding "
            "lock '%s'\n",
            ret->name, lockId);
    verboseLog(line);
    return ret;
}

#pragma endregion