#include "frame.h"
#include "page.h"
#include "swap.h"
#include <stdlib.h>
#include <stdio.h>

extern unsigned char *SYSTEM_MEMORY;
extern const int USER_BASE_ADDR;
extern FreeList *freeList;
extern FrameTable *frameTable;
extern PageDirectory *directory;

extern const int PAGE_SIZE;
// Max log buffer size
extern const int MAX_BUFFER_SIZE;

int currentlyCheckedFrame = 0;
pthread_mutex_t evictionMutex;

#pragma region Frame Functions

uint16_t evictAFrame(Thread *thread) {
    pthread_mutex_lock(&evictionMutex);

    char logBuffer[MAX_BUFFER_SIZE];
    sprintf(logBuffer, "Thread %d evictAFrame(): Finding frame to evict...\n", thread->threadId);
    logData(logBuffer);
    flushLog();

    // Use a clock algorithm to evict the frame that was accessed the longest period of time ago
    FTEntry *evictedFrameTE = NULL;
    for (currentlyCheckedFrame; evictedFrameTE == NULL; currentlyCheckedFrame++) {
        // Prevent accessing outside frame table bounds
        if (currentlyCheckedFrame == NUM_FRAME_TABLE_ENTRIES) {
            currentlyCheckedFrame = 0;
        }
        if (frameTable->entries[currentlyCheckedFrame].ownerThreadId == 0) {
            continue;
        }
        if (frameTable->entries[currentlyCheckedFrame].accessed == 1) {
            frameTable->entries[currentlyCheckedFrame].accessed = 0;
            continue;
        }
        evictedFrameTE = &frameTable->entries[currentlyCheckedFrame];
        // Lock the entry that will be evicted
        pthread_mutex_lock(&evictedFrameTE->lock);
    }
    // Get the page table and entry of the evicted frames owner
    int evictedOwnerId = evictedFrameTE->ownerThreadId;
    PageTable *evictedFrameOwnersPageTable = getThreadPageTable(evictedOwnerId);
    PTEntry *evictedPageTE = &evictedFrameOwnersPageTable->entries[evictedFrameTE->virtualPageNum];

    sprintf(logBuffer, "Thread %d evictAFrame(): Evicting thread %d's frame %d associated with vpn %d...\n",
                            thread->threadId, evictedFrameTE->ownerThreadId, evictedFrameTE->frameNum, evictedFrameTE->virtualPageNum);
    logData(logBuffer);
    flushLog();


    // Lock the evicted page's owner's page table
    pthread_mutex_lock(&getThreadPageTable(evictedOwnerId)->lock);

    // Swap the first entry in the allocatedFramesList to disk
    swapPageToDisk(thread, evictedFrameTE);

    // Mark the previous frame owner's page table entry as not present
    evictedPageTE->present = 0;
    evictedPageTE->frameTblNum = 0;

    // Unlock evicted thread's page table
    pthread_mutex_unlock(&getThreadPageTable(evictedOwnerId)->lock);

    // Put the frame table entry back into free list
    if (freeList->numFreeFrames == 0) {
        freeList->first = evictedFrameTE;
        freeList->last = evictedFrameTE;
    } else {
        freeList->last->next = evictedFrameTE;
        freeList->last = evictedFrameTE;
    }
    evictedFrameTE->next = NULL;
    freeList->numFreeFrames++;

    sprintf(logBuffer, "Thread %d evictAFrame(): Frame %d placed back in free list...\n", thread->threadId, evictedFrameTE->frameNum);
    logData(logBuffer);
    flushLog();

    pthread_mutex_unlock(&evictionMutex);

    return evictedFrameTE->frameNum;
}

uint16_t allocateFrameForPage(Thread *thread, uint32_t vpn) {
    char logBuffer[MAX_BUFFER_SIZE];

    sprintf(logBuffer, "Thread %d allocateFrameForPage(): Allocating frame for page %d\n", thread->threadId, vpn);
    logData(logBuffer);
    flushLog();

    // Lock the free list for allocation
    pthread_mutex_lock(&freeList->lock);

    sprintf(logBuffer, "Thread %d allocateFrameForPage(): There are %d frames available\n", thread->threadId, freeList->numFreeFrames);
    logData(logBuffer);
    flushLog();

    FTEntry *entry;
    // Evict a frame from the allocated frames list if there are none available
    if (freeList->numFreeFrames == 0) {
        sprintf(logBuffer, "Thread %d allocateFrameForPage(): Beginning frame eviction attempt...\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        entry = &frameTable->entries[evictAFrame(thread)];
    } else {
        sprintf(logBuffer, "Thread %d allocateFrameForPage(): Fetching free frame...\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        // Get the frame table entry for the first available free frame
        entry = freeList->first;
        // Lock the frame table entry and update its values
        pthread_mutex_lock(&entry->lock);
    }

    sprintf(logBuffer, "Thread %d allocateFrameForPage(): Found free frame %d at physical addr %d (%p)\n", thread->threadId, entry->frameNum, USER_BASE_ADDR + (PAGE_SIZE * entry->frameNum), entry->physAddr);
    logData(logBuffer);
    flushLog();

    // Remove the frame table entry from the freeList
    if (freeList->numFreeFrames == 1) {
        freeList->first = NULL;
        freeList->last = NULL;
    } else {
        freeList->first = entry->next;
    }
    freeList->numFreeFrames--;

    sprintf(logBuffer, "Thread %d allocateFrameForPage(): Frame %d removed from free list\n", thread->threadId, entry->frameNum);
    logData(logBuffer);
    flushLog();

    // Update the frame's accessed bit
    entry->accessed = 1;
    entry->next = NULL;
    entry->ownerThreadId = thread->threadId;
    entry->virtualPageNum = vpn;     // unused 16 MSB will be trucated

    sprintf(logBuffer, "Thread %d allocateFrameForPage(): There are %d frames remaining in free list\n", thread->threadId, freeList->numFreeFrames);
    logData(logBuffer);
    flushLog();

    // Mark the thread's page table entry as present
    getThreadPageTable(thread->threadId)->entries[vpn].present = 1;

    // Unlock the entry
    pthread_mutex_unlock(&entry->lock);
    // Allocation complete so free list can be unlocked
    pthread_mutex_unlock(&freeList->lock);

    // Return that entry's frame number (i.e. its index into the frame table)
    return entry->frameNum;
}

#pragma endregion
