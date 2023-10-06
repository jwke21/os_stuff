#ifndef _THREAD_LOCK_H
#define _THREAD_LOCK_H

// You can use this header to share data and/or definitions between your
// lock.cpp and thread.cpp.

// For example, if you wanted to share a map between the two...

// extern const char *sharedMap;

// Then in ONE OF lock.cpp or thread.cpp you would write

// const char *sharedMap = CREATE_MAP(const char*);

// Now both lock.cpp and thread.cpp may refer to sharedMap.

// Mapping of locks to threads. Specifically, the lockId is the key
// whose value is a pointer to the thread that holds the lock with
// that id. Implementation depends on the API in "Map.h". It is
// defined in "lock.cpp".
extern const char* mapOfLockToThreads;

// List of threads that are ready to be executed. Specifically, these
// are threads whose state is PAUSED and that are not asleep. It is
// ordered according to the order in which the threads should be executed.
// Implementation depends on the API in "List.h". It is defined in "thread.cpp".
extern const char* readyList;

// List of threads that are currently sleeping.
extern const char* sleepList;

/**
 * @brief Compares the priority of the given two threads. Returns true if
 * the first thread's priority is greater than the second thread.
 * Returns false if the first thread's priority is less than or equal to
 * the second thread.
 *
 * Meant to be used as the comparator function that is a parameter of
 * the sortList function. However, the definition given to this function
 * differs from that stipulated by the sortList documentaion. If this function
 * was defined as sortList stipulated it would place threads with higher
 * priority at the end of the list. Instead, I have definied this function
 * such that it will place the threads that have higher priority at the
 * beginning of the list.
 *
 * Moreover, since older threads with the same priority are to be scheduled
 * first, as stipulated by the round-robin policy, this function will cause
 * sortList to order the list from high-priority to low-priority and from
 * older threads to younger threads.
 *
 * @param first the first thread being compared, i.e. the one that was added
 * to the readyList.
 * @param second the second thread being compared, i.e. the one that was already
 * in the readyList.
 * @return true if the priority of first is greater than the priority of second
 * @return false if the priority of first is less than or equal to the
 * priority of second.
 */
extern bool compareReadyThreads(void* first, void* second);

/**
 * @brief Compares the wakeUpTick of the two given SleepingThread structs.
 * Returns true if the first SleepingThread's wakeUpTick is less than or
 * equal to the second SleepingThread's. Returns false if the first
 * SleepingThread's wakeUpTick is greater than the second thread.
 *
 * Meant to be used as the comparator function that is a parameter of the
 * sortList function. Enables sorting of the sleepList list.
 *
 * @param first the first thread being compared, i.e. the one that was added
 * to the sleepList.
 * @param second the second thread being compared, i.e. the one that was already
 * in the sleepList.
 * @return true if the wakeUpTick of first is less than or equal to the
 * wakeUpTick of second.
 * @return false if the wakeUpTick of first is greater than the wakeUpTick of
 * second.
 */
extern bool compareSleepingThreads(void* first, void* second);

#endif