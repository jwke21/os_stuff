#include "page.h"
#include "frame.h"
#include "utils.h"
#include <stdio.h>

extern unsigned char *SYSTEM_MEMORY;
extern const int PAGE_SIZE;
extern const int MAX_BUFFER_SIZE;
extern PageDirectory *directory;

#pragma region Page Functions

uint32_t virtualAddressToVPN(uint32_t virtualAddr) {
    // The address space goes up to 8M meaning that at most, 23 bits will be
    // used. Since there are 2048 pages that are 4098 bytes, the virtual page
    // number will be the 11 MSB and the offset will be the 12 LSB.
    return (virtualAddr & VPN_MASK) >> VPN_SHIFT;
}

PageTable* getThreadPageTable(uint8_t threadId) {
    return &(directory->tables[threadId-1]);
}

void allocatePages(Thread *thread, uint32_t startAddr, uint32_t endAddr) {
    char logBuffer[MAX_BUFFER_SIZE];
    sprintf(logBuffer, "Thead %d allocatePages(): Beginning page allocation attempt...\n", thread->threadId);
    logData(logBuffer);
    flushLog();

    // Get the thread's table
    PageTable *pageTable = getThreadPageTable(thread->threadId);
    sprintf(logBuffer, "Thead %d allocatePages(): Retrieved page table...\n", thread->threadId);
    logData(logBuffer);
    flushLog();
    uint32_t vpn;
    uint32_t offset;
    PTEntry *pte;
    while (startAddr < endAddr) {
        // Fetch the page table entry
        vpn = virtualAddressToVPN(startAddr);
        pte = &pageTable->entries[vpn];

        // Check for page fault
        if (pte->valid == 0) {
            // Allocate frame for page
            pte->frameTblNum = allocateFrameForPage(thread, vpn);
            pte->valid = 1;
        }

        // Lock the thread's page table for duration of allocation
        pthread_mutex_lock(&pageTable->lock);
        while (pte->present == 0) {
            pthread_mutex_unlock(&pageTable->lock);

            pte->frameTblNum = allocateFrameForPage(thread, vpn);

            // Lock the thread's page table for duration of allocation
            pthread_mutex_lock(&pageTable->lock);
        }


        sprintf(logBuffer, "Thead %d allocatePages(): Associated vpn %d with fte %d\n", thread->threadId, vpn, pte->frameTblNum);
        logData(logBuffer);
        flushLog();

        offset = startAddr & OFFSET_MASK;
        if (endAddr - startAddr > PAGE_SIZE - offset) {
            startAddr += PAGE_SIZE - offset;
        } else {
            startAddr += endAddr - startAddr;
        }
        // Unlock the thread's page table
        pthread_mutex_unlock(&pageTable->lock);
    }
}

#pragma endregion
