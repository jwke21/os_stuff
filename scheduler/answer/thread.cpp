#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Thread.h"
#include "thread_lock.h"

const char* readyList = NULL;
const char* sleepList = NULL;

typedef struct SleepingThread {
    int wakeUpTick;
    Thread* thread;
} SleepingThread;

bool compareReadyThreads(void* first, void* second) {
    Thread* firstThread = (Thread*)first;
    Thread* secondThread = (Thread*)second;
    char line[1024];

    sprintf(line, "[compareReadyThreads] Comparing '%s' with '%s'\n",
            firstThread->name, secondThread->name);
    verboseLog(line);

    // Higher priority threads will be placed in front of list
    if (firstThread->priority > secondThread->priority) {
        sprintf(line,
                "[compareReadyThreads] '%s' has a higher priority than '%s'\n",
                firstThread->name, secondThread->name);
        verboseLog(line);
        return true;
    } else if (firstThread->priority < secondThread->priority) {
        sprintf(line,
                "[compareReadyThreads] '%s' has a lower priority than '%s'\n",
                firstThread->name, secondThread->name);
        verboseLog(line);
    }
    // Threads with the same priority will be ordered from older -> younger
    else {
        sprintf(line,
                "[compareReadyThreads] '%s' has the same priority as '%s'\n",
                firstThread->name, secondThread->name);
        verboseLog(line);
    }
    return false;
}

bool compareSleepingThreads(void* first, void* second) {
    SleepingThread* firstThread = (SleepingThread*)first;
    SleepingThread* secondThread = (SleepingThread*)second;
    char line[1024];

    sprintf(line, "[compareSleepingThreads] Comparing '%s' with '%s'\n",
            firstThread->thread->name, secondThread->thread->name);
    verboseLog(line);

    // SleepingThreads with lower wakeUpTick values will be placed in front
    // of list
    if (firstThread->wakeUpTick <= secondThread->wakeUpTick) {
        sprintf(line,
                "[compareSleepingThreads] '%s' will wake up before '%s'\n",
                firstThread->thread->name, secondThread->thread->name);
        verboseLog(line);
        return true;
    }
    sprintf(line, "[compareSleepingThreads] '%s' will wake up after '%s'\n",
            firstThread->thread->name, secondThread->thread->name);
    verboseLog(line);
    return false;
}

/**
 * @brief This function is called whenever a nextThreadToRun is called. It is
 * used to check if any threads in sleepList are ready to be woken up. If a
 * thread is ready to be woken up, then it is added back into the readyList. The
 * SleepingThread struct that it was stored in is removed from the sleepList and
 * free'd. The readyList is sorted to ensure ordering by priority.
 *
 * @param curTick The current CPU tick. Will be compared to the wakeUpTick value
 * of the threads in the sleepList to see if any are ready to be woken up.
 */
void wakeUpSleepingThreads(int curTick) {
    char line[1024];
    SleepingThread* curThread;
    bool threadWokenUp = false;

    sprintf(line, "[wakeUpSleepingThreads] Waking up sleeping threads\n");
    verboseLog(line);
    // Pop all threads that are to be woken up from the sleep list and add
    // them to the ready list
    while (listSize(sleepList) > 0) {
        curThread = (SleepingThread*)listGet(sleepList, 0);
        sprintf(line, "[wakeUpSleepingThreads] Checking thread '%s'\n",
                curThread->thread->name);
        verboseLog(line);
        // If the thread is not scheduled to wake up yet then return because
        // no threads after it will be woken up either
        if (curThread->wakeUpTick > curTick) {
            sprintf(line, "[wakeUpSleepingThreads] Thread '%s' still asleep\n",
                    curThread->thread->name);
            verboseLog(line);
            return;
        }
        sprintf(line, "[wakeUpSleepingThreads] Waking up thread '%s'\n",
                curThread->thread->name);
        verboseLog(line);
        // If the thread is ready to be woken up, add it to the readyList and
        // remove it from the sleeplist
        addToList(readyList, (void*)curThread->thread);
        removeFromList(sleepList, (void*)curThread);
        // Free the SleepingThread struct
        free(curThread);
        threadWokenUp = true;
    }
    // If a thread was woken up re-sort the ready list to restablish the order
    if (threadWokenUp == true) {
        sortList(readyList, compareReadyThreads);
    }
}

Thread* createAndSetThreadToRun(const char* name,
                                void* (*func)(void*),
                                void* arg,
                                int pri) {
    Thread* ret = (Thread*)malloc(sizeof(Thread));
    ret->name = strdup(name);
    ret->func = func;
    ret->arg = arg;
    ret->priority = pri;
    ret->originalPriority = pri;

    createThread(ret);
    addToList(readyList, (void*)ret);

    char line[1024];
    sprintf(line, "[createAndSetThreadToRun] Adding thread '%s' to list\n",
            ret->name);
    verboseLog(line);
    // Sort the threads from highest priority to lowest priority
    sprintf(line, "[createAndSetThreadToRun] Sorting list\n");
    verboseLog(line);
    sortList(readyList, compareReadyThreads);
    return ret;
}

void destroyThread(Thread* thread) {
    if (thread != NULL) {
        free(thread->name);
        free(thread);
    }
}

Thread* getHighestPriorityThread() {
    // Will only be called in nextThreadToRun after list size is
    //     verified to have > 0 threads so no size check required
    int numThreads = listSize(readyList);
    Thread* highestPriorityThread = (Thread*)listGet(readyList, 0);
    Thread* curThread = NULL;
    // Get the thread with the highest priority in the list
    for (int i = 1; i < numThreads; i++) {
        curThread = (Thread*)listGet(readyList, i);
        if (curThread->priority > highestPriorityThread->priority) {
            highestPriorityThread = curThread;
        }
    }
    return highestPriorityThread;
}

Thread* nextThreadToRun(int currentTick) {
    char line[1024];
    if (listSize(readyList) == 0 && listSize(sleepList) == 0) {
        return NULL;
    }
    Thread* ret = NULL;
    // Wake up any threads that are scheduled to wake up at this tick
    wakeUpSleepingThreads(currentTick);

    while (listSize(readyList) > 0 && ret == NULL) {
        // Since the list is ordered from higher priority to lower priority
        // the thread at index 0 should have the highest priority
        ret = ((Thread*)listGet(readyList, 0));
        sprintf(line, "[nextThreadToRun] Trying thread '%s'\n", ret->name);
        verboseLog(line);
        if (ret->state == TERMINATED) {
            sprintf(line, "[nextThreadToRun] Thread '%s' was terminated\n",
                    ret->name);
            verboseLog(line);
            removeFromList(readyList, ret);
            ret = NULL;
        }
    }

    return ret;
}

void initializeCallback() {
    readyList = createNewList();
    sleepList = createNewList();
}

void shutdownCallback() {
    destroyList(sleepList);
    destroyList(readyList);
}

int tickSleep(int numTicks) {
    char line[1024];
    int start, stop, curTick;
    Thread* thread;
    SleepingThread* sleepingThread;

    // Get the tick that the thread began sleeping
    start = getCurrentTick();
    // Get the tick that the thread is supposed to wake up
    stop = start + numTicks;
    thread = getCurrentThread();
    // Keep the thread asleep until it is the thread that it was
    // scheduled to wake up
    sprintf(line, "[tickSleep] Thread '%s' is sleeping for %d ticks\n",
            thread->name, numTicks);
    verboseLog(line);
    // Instantiate the the sleepingThread struct for the thread and put it
    // in the sleepList
    sleepingThread = (SleepingThread*)malloc(sizeof(sleepingThread));
    sleepingThread->wakeUpTick = stop;
    sleepingThread->thread = thread;
    addToList(sleepList, (void*)sleepingThread);
    // Remove the now sleeping thread from the readyList
    removeFromList(readyList, thread);
    // Sort the list of sleeping threads
    sortList(sleepList, compareSleepingThreads);
    // Stop the thread until it is ready to be woken up
    stopExecutingThreadForCycle();
    return start;
}

void setMyPriority(int priority) {
    Thread* curThread = getCurrentThread();
    curThread->priority = priority;
}
