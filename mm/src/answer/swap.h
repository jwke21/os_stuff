#ifndef VIRTUALMEMFRAMEWORKC_SWAP_H
#define VIRTUALMEMFRAMEWORKC_SWAP_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "thread.h"
#include "page.h"
#include "frame.h"

/**
 * Given a frame table entry, will swap the frame associated with it to disk
*/
void swapPageToDisk(Thread *thread, FTEntry *frameTableEntry);

/** 
 * Given a thread, it's evicted virtual page number, will swap frame associated
 * with that vpn from disk back into memory.
*/
void swapPageFromDisk(Thread *thread, int virtualPageNumber, uint16_t frameTableNum);

#endif // VIRTUALMEMFRAMEWORKC_SWAP_H