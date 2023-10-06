#ifndef VIRTUALMEMFRAMEWORKC_FRAME_H
#define VIRTUALMEMFRAMEWORKC_FRAME_H

#include <pthread.h>
#include <stdint.h>
#include "thread.h"
#include "utils.h"

#pragma region Frame Macros

/* There are 1792 frame table entries, one for each physical frame able to be
   allocated */
#define NUM_FRAME_TABLE_ENTRIES 1792

#pragma endregion

#pragma region Frame Structs

typedef struct FTEntry FTEntry;

/**
 * Defines a frame table entry implemented as a linked list node. Contains
 * the ThreadId of the thread whose page is written to the frame, that
 * thread's virtual page number, and the physical frame that the
 * virtual page is written to.
*/
struct FTEntry {
    pthread_mutex_t lock;      // Lock for the frame
    uint8_t accessed;          // Accessed bit to indicate if the frame was accessed since last clock run
    uint8_t ownerThreadId;     // The threadId of the owner thread
    uint16_t virtualPageNum;   // The virtual page number associated with the frame
    uint16_t frameNum;         // Number of the frame (its index in the frame table)
    uint8_t *physAddr;         // Pointer to the physical frame
    FTEntry *next;             // Next frame in the list
};

/**
 * Defines the frame table. Used for structuring system memory.
*/
typedef struct FrameTable {
    FTEntry entries[NUM_FRAME_TABLE_ENTRIES]; // All frame table entries
} FrameTable;

/**
 * Defines the free list implemented as a linked list. Used to manage
 * unallocated frames. Contains a pointer to the first and last elements,
 * and the number of free frames remaining.
*/
typedef struct FreeList {
    pthread_mutex_t lock;   // Lock for the free list
    FTEntry *first;      // First entry in the free list (used to index into frame table)
    FTEntry *last;       // Last entry in the free list (used to index into frame table)
    uint16_t numFreeFrames; // Number of nodes in the free list
} FreeList;

#pragma endregion

#pragma region Frame FunctionDeclarations

/**
 * Finds and evicts a frame from the allocatedFramesList. Will swap the frame
 * data to disk. Returns the frame number of the frame that was evicted.
*/
uint16_t evictAFrame(Thread *thread);

/**
 * Allocates a frame for the page and returns that frame's frame number.
*/
uint16_t allocateFrameForPage(Thread *thread, uint32_t vpn);

#pragma endregion

#endif //VIRTUALMEMFRAMEWORKC_FRAME_H