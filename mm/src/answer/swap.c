#include "swap.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern unsigned char *SYSTEM_MEMORY;
extern FrameTable *frameTable;
extern const int PAGE_SIZE;
extern const int USER_BASE_ADDR;

// Max log buffer size
extern const int MAX_BUFFER_SIZE;
extern const int MAX_FILE_NAME_SIZE;

void swapPageToDisk(Thread *thread, FTEntry *evictedFTE) {
    char logBuffer[MAX_BUFFER_SIZE];
    sprintf(logBuffer, "Thread %d swapPageToDisk(): Beginning swap attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
    // Set the file name
    char fileName[MAX_FILE_NAME_SIZE];
    sprintf(fileName, "%d_%d.swp", evictedFTE->ownerThreadId, evictedFTE->virtualPageNum);

    sprintf(logBuffer, "Thread %d swapPageToDisk(): Swapping frame %d owned by thread %d's vpn %d to file %s\n",
                        thread->threadId, evictedFTE->frameNum, evictedFTE->ownerThreadId, evictedFTE->virtualPageNum, fileName);
    logData(logBuffer);
    flushLog();
    // Open the file that the frame will be written to
    FILE *file = fopen(fileName, "w+");
    // Handle failure to open file with kernelPanic
    if (file == NULL) {
        sprintf(logBuffer, "Thread %d swapPageToDisk(): Error opening file %s\n", thread->threadId, fileName);
        logData(logBuffer);
        flushLog();
        perror("Errno");
        kernelPanic(thread, file);
    }
    // Write the frame to the file
    uint16_t writtenBytes = fwrite(evictedFTE->physAddr, sizeof(uint8_t), PAGE_SIZE, file);
    if (writtenBytes != PAGE_SIZE) {
        sprintf(logBuffer, "Thread %d swapPageToDisk(): Wrote only %d bytes from %d to %s\n",
                            thread->threadId, writtenBytes, evictedFTE->frameNum, fileName);
        logData(logBuffer);
        flushLog();
    }
    // Reset the frame table entry's ownership fields
    evictedFTE->ownerThreadId = 0;
    evictedFTE->virtualPageNum = 0;
    // Close the file to save it
    fclose(file);

    sprintf(logBuffer, "Thread %d swapPageToDisk(): Swap complete...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
}

void swapPageFromDisk(Thread *thread, int virtualPageNumber, uint16_t newFrameNum) {
    char logBuffer[MAX_BUFFER_SIZE];
    // Retrieve the frame table entry
    FTEntry *fte = &frameTable->entries[newFrameNum];
    pthread_mutex_lock(&fte->lock);
    sprintf(logBuffer, "Thread %d swapPageFromDisk(): Swapping data in file %d_%d.swp into memory for page %d at new frame %d...\n",
                        thread->threadId, thread->threadId, virtualPageNumber, virtualPageNumber, newFrameNum);
    logData(logBuffer);
    flushLog();

    // Retreive the file name
    char fileName[MAX_FILE_NAME_SIZE];
    int evictedPageAddress = PAGE_SIZE * virtualPageNumber;
    getCacheFileName(thread, evictedPageAddress, fileName);
    // Open the swap file associated with that page
    FILE *file = fopen(fileName, "r");
    // Handle failure to open file with kernelPanic
    if (file == NULL) {
        sprintf(logBuffer, "Thread %d swapPageFromDisk(): Error opening file %s\n", thread->threadId, fileName);
        logData(logBuffer);
        flushLog();
        perror("Errno");
        kernelPanic(thread, file);
    }
    // Read the contents of the swapped page into the frame
    uint16_t readBytes = fread(fte->physAddr, sizeof(uint8_t), PAGE_SIZE, file);
    if (readBytes != PAGE_SIZE) {
        sprintf(logBuffer, "Thread %d swapPageFromDisk(): Read only %d bytes from %s to %d\n",
                    thread->threadId, readBytes, fileName, fte->frameNum);
        logData(logBuffer);
        flushLog();
    }
    // Update the frame table entry's ownership fields
    fte->ownerThreadId = thread->threadId;
    fte->virtualPageNum = virtualPageNumber;
    pthread_mutex_unlock(&fte->lock);
    fclose(file);
    remove(fileName);

    sprintf(logBuffer, "Thread %d swapPageFromDisk(): Swap complete...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
}
