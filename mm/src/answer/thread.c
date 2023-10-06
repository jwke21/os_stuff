#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "thread.h"
#include "utils.h"

extern unsigned char *SYSTEM_MEMORY;
typedef struct PageDirectory PageDirectory;
extern PageDirectory *directory;

uint8_t currentThreadId = 1;

Thread* createThread() {
    Thread* ret = malloc(sizeof(Thread));
    bzero(ret, sizeof(Thread));

    ret->threadId = currentThreadId;
    // Heap grows down in memory, so top of heap is beginning of user space
    ret->heapBottom = 1024 * 1024;
    // Stack grows up in memory, so bottom of stack is end of memory
    ret->stackTop = (8 * 1024 * 1024);
    // Initialize the thread's page table mutex
    // pthread_mutex_init(&ret->ptLock, NULL);

    currentThreadId++;

    return ret;
}

void destroyThread(Thread* thread) {
    // This is line is ABSOLUTELY REQUIRED for the tests to run properly. This allows the thread to finish its work
    // DO NOT REMOVE.
    if (thread->thread) pthread_join(thread->thread, NULL);
    // Destroy the thread's page table mutex
    // pthread_mutex_destroy(&thread->ptLock);

    free(thread);
}
