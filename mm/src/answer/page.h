#ifndef VIRTUALMEMFRAMEWORKC_PAGE_H
#define VIRTUALMEMFRAMEWORKC_PAGE_H

#include "thread.h"
#include <stdint.h>

#pragma region Page Macros

/* There are 2048 entries per page table */
#define NUM_PAGE_TABLE_ENTRIES 2048
/* Each page table is 8232 bytes (NUM_PAGES * PTE_SIZE + 40) */
#define PAGE_TABLE_SIZE 8232
/* There are 32 page tables (arbitrarily chosen) */
#define NUM_PAGE_TABLES 32
/* The page directory will be 262144 bytes (PAGE_TABLE_SIZE * NUM_PAGE_TABLES) */
#define DIRECTORY_SIZE 8232 * 32
/* Given a 23 bit address, the 11 MSB are the VPN */
#define VPN_SHIFT 12
/* Given a uint32_t representing the virtual address, will zero all bits
   except for those used to translate into VPN */
#define VPN_MASK 0x7FF000
/* Given a uint32_t representing the virtual addres, will zero all bits 
   except for those used to offset into the physical frame */
#define OFFSET_MASK 0xFFF

#pragma endregion

#pragma region Page Structs

/**
 * This struct defines a page table entry.
*/
typedef struct PTEntry {
    uint16_t frameTblNum;  // The number of the frame table entry associated with the page
    uint8_t valid;         // Whether the entry is valid
    uint8_t present;       // Whether it was swapped to disk
} PTEntry;

/**
 * This struct defines a page table. Used for structuring system memory.
*/
typedef struct PageTable {
    pthread_mutex_t lock;                    // Lock for the thread's page table
    PTEntry entries[NUM_PAGE_TABLE_ENTRIES]; // The entries held by the page table
} PageTable;

/**
 * This struct defines a page directory. Used for structuring system memory.
*/
typedef struct PageDirectory {
    PageTable tables[NUM_PAGE_TABLES]; // The page tables pointed to by the directory
} PageDirectory;

#pragma endregion

#pragma region Page FunctionDeclarations

/**
 * Handles the allocation of the pages for address range starting at and
 * including startAddr and ending at but excluding endAddr. It must be the
 * case that startAddr < endAddr.
*/
void allocatePages(Thread *thread, uint32_t startAddr, uint32_t endAddr);

/**
 * Extracts the page number from a given virtual address.
*/
uint32_t virtualAddressToVPN(uint32_t virtualAddr);

/**
 * Takes a threadId and returns a pointer to that thread's page table in system memory.
*/
PageTable* getThreadPageTable(uint8_t threadId);

#pragma endregion

#endif //VIRTUALMEMFRAMEWORKC_PAGE_H
