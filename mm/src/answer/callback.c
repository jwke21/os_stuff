#include "callback.h"
#include "utils.h"
#include "memory.h"
#include "thread.h"
#include <stdint.h>
#include <stdio.h>


// Files will be at most 128 chars (arbitrarily chosen but should be
// more than enough given file naming schema)
const int MAX_FILE_NAME_SIZE = 128;
extern uint8_t currentThreadId;
extern int currentlyCheckedFrame;
extern pthread_mutex_t evictionMutex;

void startupCallback() {
    logData("\nstartupCallback(): Initializing system memory...\n");
    flushLog();

    initializeSystemMemory();

    logData("startupCallback(): System memory initialized...\n");
    flushLog();

    // Initialize eviction mutex
    pthread_mutex_init(&evictionMutex, NULL);
    // Since currentThreadId is a global var defined in thread.c,
    // its value needs to be reset to 1 after each test is run
    currentThreadId = 1;
    // Since clock algorithm for swapping modifies a global var
    // defined in frane.c, its value needs to be reset to 0 to
    // ensure previous tests don't affect current one
    currentlyCheckedFrame = 0;
}

void shutdownCallback() {
    logData("shutdownCallback(): De-initializing system memory...\n");
    flushLog();

    deinitializeSystemMemory();

    logData("shutdownCallback(): Deleting remaining swap files...\n");
    flushLog();
    // Clean up all swap files
    extern const int MAX_FILE_NAME_SIZE;
    char fileName[MAX_FILE_NAME_SIZE];
    FILE *file;
    for (int threadId = 0; threadId <= 32; threadId++) {
        for (int vpn = 256; vpn <= NUM_PAGE_TABLE_ENTRIES; vpn++) {
            sprintf(fileName, "%d_%d.swp", threadId, vpn);
            remove(fileName);
        }
    }

    logData("shutdownCallback(): All files cleaned up...\n\n");
    flushLog();
}