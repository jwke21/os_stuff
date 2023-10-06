#include "memory.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>


#pragma region Memory Globals

// 4k is the size of a page (12 bits)
const int PAGE_SIZE = 4*1024;
// A total of 8M exists in memory (23 bits)
const int ALL_MEM_SIZE = 8*1024*1024;
// USER Space starts at 1M
const int USER_BASE_ADDR = 1024*1024;
// Stack starts at 8M and goes down to 6M
const int STACK_END_ADDR = 6*1024*1024;
// There are a total of 2048 pages
const int NUM_PAGES = 2*1024;

/* System memory (8388608 bytes) */
unsigned char SYSTEM_MEMORY[8*1024*1024];
/* Pointer to the page directory located in kernel space (263424 bytes) */
PageDirectory *directory;
/* Pointer to the free list located in kernel space (64 bytes) */
FreeList *freeList;
/* Pointer to the frame table containing the frame table entries (114688 bytes) */
FrameTable *frameTable;
/* Pointer to the beginning of user space */
uint8_t *userSpace;

// Max log buffer size
extern const int MAX_BUFFER_SIZE;

#pragma endregion

#pragma region API

int allocateHeapMem(Thread *thread, int size) {
    char logBuffer[MAX_BUFFER_SIZE];
    uint32_t memoryBeginsAt = thread->heapBottom;
    // Check that there is enough heap space
    if (memoryBeginsAt >= thread->stackTop) {
        sprintf(logBuffer, "Thread %d allocateHeapMem(): Maximum heap space has allocated\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        return -1;
    }
    sprintf(logBuffer, "Thread %d allocateHeapMem(): Beginning heap allocation attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
    // Allocate the required pages
    allocatePages(thread, thread->heapBottom, thread->heapBottom + size);
    // Move the thread's heap pointer
    thread->heapBottom += size;
    return memoryBeginsAt;
}

int allocateStackMem(Thread *thread, int size) {
    char logBuffer[MAX_BUFFER_SIZE];
    uint32_t memoryBeginsAt = thread->stackTop - size;
    // Check that there is enough memory remaining on the stack
    if (memoryBeginsAt < STACK_END_ADDR) {
        sprintf(logBuffer, "Thread %d allocateStackMem(): Maximum stack space has been allocated\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        return -1;
    }
    sprintf(logBuffer, "Thread %d allocateStackMem(): Beginning stack allocation attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
    // Allocate the required pages
    allocatePages(thread, thread->stackTop - size, thread->stackTop);
    // Move the thread's stack pointer
    thread->stackTop -= size;
    return memoryBeginsAt;
}

void writeToAddr(const Thread *thread, int addr, int size, const void* data) {
    char logBuffer[MAX_BUFFER_SIZE];
    // Check that the write attempt is within bounds
    if (addr < USER_BASE_ADDR || addr > ALL_MEM_SIZE || addr + size > ALL_MEM_SIZE) {
        sprintf(logBuffer, "Thread %d writeToAddr(): Write out of bounds error\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        kernelPanic(thread, addr);
        return;
    }

    sprintf(logBuffer, "Thread %d writeToAddr(): Beginning write attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();

    // Get the thread's page table
    PageTable *pageTable = getThreadPageTable(thread->threadId);
    sprintf(logBuffer, "Thread %d writeToAddr(): Writing frames associated with address %d\n", thread->threadId, addr);
    logData(logBuffer);
    flushLog();
    // Find the frames associated with the virtual address
    FTEntry *fte;
    int vpn;
    uint32_t currentAddr = addr;
    uint32_t dataOffset = 0;
    size_t bytesToWrite;
    uint16_t frameOffset;
    uint8_t *physicalAddr;
    PTEntry *pte;
    int leftToWrite = size;
    while (leftToWrite > 0) {
        // Translate the vpn and fetch the page table entry
        vpn = virtualAddressToVPN(currentAddr);
        // Access the page table entry for the address
        pte = &pageTable->entries[vpn];
        sprintf(logBuffer, "Thread %d writeToAddr(): Fetched vpn %d for addr %d\n", thread->threadId, vpn, currentAddr);
        logData(logBuffer);
        flushLog();

        // Lock the thread's page table while in use
        pthread_mutex_lock(&pageTable->lock);
        // If the frame is not present in memory swap it back
        while (pte->present == 0) {
            sprintf(logBuffer, "Thread %d writeToAddr(): Page fault for vpn %d\n", thread->threadId, vpn);
            logData(logBuffer);
            flushLog();

            // unlock the thread's page table
            pthread_mutex_unlock(&pageTable->lock);

            // Allocate a frame for the page
            pte->frameTblNum = allocateFrameForPage(thread, vpn);
            // Swap the frame back into memory
            swapPageFromDisk(thread, vpn, pte->frameTblNum);

            // Lock the thread's page table while in use
            pthread_mutex_lock(&pageTable->lock);
        }
        // Fetch the frame table entry
        fte = &frameTable->entries[pte->frameTblNum];
        // Lock the frame
        pthread_mutex_lock(&fte->lock);
        sprintf(logBuffer, "Thread %d writeToAddr(): Fetching frame %d at addr %p for vpn %d\n", thread->threadId, fte->frameNum, fte->physAddr, vpn);
        logData(logBuffer);
        flushLog();

        // Get offset
        frameOffset = currentAddr & OFFSET_MASK;
        // Get the amount of bytes that will be written to this frame
        if (leftToWrite > PAGE_SIZE - frameOffset) {
            bytesToWrite = PAGE_SIZE - frameOffset;    
        } else {
            bytesToWrite = leftToWrite;
        }

        sprintf(logBuffer, "Thread %d writeToAddr(): Virtual addr %d offsets %d bytes into frame %d\n", thread->threadId, currentAddr, frameOffset, fte->frameNum);
        logData(logBuffer);
        flushLog();

        // Get the physical address
        physicalAddr = fte->physAddr + frameOffset;
        sprintf(logBuffer, "Thread %d writeToAddr(): Writing %ld bytes from data into %p\n", thread->threadId, bytesToWrite, physicalAddr);
        logData(logBuffer);
        flushLog();
        // Write to the frame
        memcpy(physicalAddr, data + dataOffset, bytesToWrite);
        leftToWrite -= bytesToWrite;
        dataOffset += bytesToWrite;
        currentAddr += bytesToWrite;
        // Unlock the frame
        pthread_mutex_unlock(&fte->lock);
        // unlock the thread's page table
        pthread_mutex_unlock(&pageTable->lock);
    }
}

void readFromAddr(Thread *thread, int addr, int size, void* outData) {
    char logBuffer[MAX_BUFFER_SIZE];
    // Check that the read attempt is within bounds
    if (addr < USER_BASE_ADDR || addr > ALL_MEM_SIZE || addr + size > ALL_MEM_SIZE) {
        sprintf(logBuffer, "Thread %d readFromAddr(): Read out of bounds error\n", thread->threadId);
        logData(logBuffer);
        flushLog();
        kernelPanic(thread, addr);
        return;
    }
    sprintf(logBuffer, "Thread %d readFromAddr(): Beginning read attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
    // Get the thread's page table
    PageTable *pageTable = getThreadPageTable(thread->threadId);
    sprintf(logBuffer, "Thread %d readFromAddr(): Reading frames starting at addr %d\n", thread->threadId, addr);
    logData(logBuffer);
    flushLog();
    // Find the frames associated with the virtual address
    FTEntry *fte;
    int vpn;
    uint32_t currentAddr = addr;
    uint32_t frameOffset;
    uint32_t dataOffset = 0;
    size_t bytesToRead;
    uint8_t *physicalAddr;
    PTEntry *pte;
    int leftToRead = size;
    while (leftToRead > 0) {
        // Translate the vpn and fetch the page table entry
        vpn = virtualAddressToVPN(currentAddr);
        // Access the page table entry for the address
        pte = &pageTable->entries[vpn];
        sprintf(logBuffer, "Thread %d readFromAddr(): Fetched vpn %d for addr %d\n", thread->threadId, vpn, currentAddr);
        logData(logBuffer);
        flushLog();

        // Lock the thread's page table while in use
        pthread_mutex_lock(&pageTable->lock);
        // If the frame is not present in memory swap it back
        if (pte->present == 0) {
            sprintf(logBuffer, "Thread %d writeToAddr(): Page fault for vpn %d\n", thread->threadId, vpn);
            logData(logBuffer);
            flushLog();

            // Unlock the thread's page table
            pthread_mutex_unlock(&pageTable->lock);

            // Allocate a frame for the page
            pte->frameTblNum = allocateFrameForPage(thread, vpn);
            // Swap the frame back into memory
            swapPageFromDisk(thread, vpn, pte->frameTblNum);

            // Lock the thread's page table while in use
            pthread_mutex_lock(&pageTable->lock);
        }
        // Fetch the frame table entry
        fte = &frameTable->entries[pte->frameTblNum];        
        // Lock the frame
        pthread_mutex_lock(&fte->lock);
        sprintf(logBuffer, "Thread %d readFromAddr(): Fetched frame %d at addr %p for vpn %d\n", thread->threadId, fte->frameNum, fte->physAddr, vpn);
        logData(logBuffer);
        flushLog();
        // Get offset
        frameOffset = currentAddr & OFFSET_MASK;
        // Get the amount of bytes that will be read from the fra
        if (leftToRead > PAGE_SIZE - frameOffset) {
            bytesToRead = PAGE_SIZE - frameOffset;
        } else {
            bytesToRead = leftToRead;
        }

        sprintf(logBuffer, "Thread %d readFromAddr(): Virtual addr %d offsets %d bytes into frame %d\n", thread->threadId, vpn, frameOffset, fte->frameNum);
        logData(logBuffer);
        flushLog();

        // Get the physical address
        physicalAddr = fte->physAddr + frameOffset;
        sprintf(logBuffer, "Thread %d readFromAddr(): Reading %ld bytes from %p into outData\n", thread->threadId, bytesToRead, physicalAddr);
        logData(logBuffer);
        flushLog();
        // Read from the frames into outData
        memcpy(outData + dataOffset, physicalAddr, bytesToRead);
        leftToRead -= bytesToRead;
        dataOffset += bytesToRead;
        currentAddr += bytesToRead;        
        // Unlock the frame
        pthread_mutex_unlock(&fte->lock);
        // Unlock the thread's page table
        pthread_mutex_unlock(&pageTable->lock);
    }
}

char* getCacheFileName(Thread *thread, int addr, char *fileNameBuf) {
    char logBuffer[MAX_BUFFER_SIZE];
    sprintf(logBuffer, "Thread %d getCacheFileName(): Retreiving cache file for addr %d\n", thread->threadId, addr);
    logData(logBuffer);
    flushLog();

    extern const int MAX_FILE_NAME_SIZE;
    memset(fileNameBuf, 0, MAX_FILE_NAME_SIZE);
    // The file name is in the format of "{threadId}_{vpn}.swp"
    uint32_t vpn = virtualAddressToVPN(addr);
    sprintf(fileNameBuf, "%d_%d.swp", thread->threadId, vpn);

    return fileNameBuf;
}

#pragma endregion

#pragma region Memory Callback

void initializeSystemMemory() {
    char logBuffer[MAX_BUFFER_SIZE];
    // Initialize all system memory
    memset(SYSTEM_MEMORY, 0, ALL_MEM_SIZE);
    sprintf(logBuffer, "System memory initialized at %p\n", SYSTEM_MEMORY);
    logData(logBuffer);
    flushLog();
    // Initialize the page directory
    logData("Initializing page table directory...\n");
    flushLog();
    directory = &SYSTEM_MEMORY[PAGE_DIRECTORY_OFFSET];
    for (int i = 0; i < 32; i++) {
        pthread_mutex_init(&(directory->tables[i].lock), NULL);
    }
    sprintf(logBuffer, "Page table directory initialized at: %p\n", directory);
    logData(logBuffer);
    flushLog();
    // Initialize the frame table (starts after page directory)
    logData("Initializing frame table..\n");
    flushLog();
    frameTable = &(SYSTEM_MEMORY[FRAME_TABLE_OFFSET]);
    for (uint16_t i = 0; i < NUM_FRAME_TABLE_ENTRIES; i++) {
        // Initialize the frame's lock
        pthread_mutex_init(&(frameTable->entries[i].lock), NULL);
        // Set the frame number
        frameTable->entries[i].frameNum = i;
        // Point each frame table entry to its associated frame in memory
        frameTable->entries[i].physAddr = &SYSTEM_MEMORY[USER_BASE_ADDR + (PAGE_SIZE * i)];
        // Point the entry to the next one
        frameTable->entries[i].next = &(frameTable->entries[i + 1]);
    }
    frameTable->entries[NUM_FRAME_TABLE_ENTRIES].physAddr = &SYSTEM_MEMORY[ALL_MEM_SIZE - PAGE_SIZE];
    frameTable->entries[NUM_FRAME_TABLE_ENTRIES].next = NULL;
    sprintf(logBuffer, "Frame table initialized at: %p\n", frameTable);
    logData(logBuffer);
    flushLog();

    logData("Initializing free list...\n");
    flushLog();
    freeList = &(SYSTEM_MEMORY[FREE_LIST_OFFSET]);
    pthread_mutex_init(&freeList->lock, NULL);
    freeList->first = &frameTable->entries[0];
    freeList->last = &frameTable->entries[NUM_FRAME_TABLE_ENTRIES-1];
    freeList->numFreeFrames = NUM_FRAME_TABLE_ENTRIES;
    // Make the free list circular
    freeList->last->next = freeList->first;
    sprintf(logBuffer, "Free list initialized at: %p\n", freeList);
    logData(logBuffer);
    flushLog();
}

void deinitializeSystemMemory() {
    // Destroy free list lock
    pthread_mutex_destroy(&freeList->lock);

    // Destroy the page table locks
    for (int i = 0; i < 32; i++) {
        pthread_mutex_consistent(&(directory->tables[i]));
    }

    memset(SYSTEM_MEMORY, 0, ALL_MEM_SIZE);
}

#pragma endregion
