The following questions are answered based on how I ended up designing my system. I know my system is very imperfect so any criticism is desired, no matter how harsh.


1. Describe your design of the page table and frame tables. How and why did you store your data the way you did?

There is one page table, represented by the `PageTable` struct for each thread up to a maximum of 32 threads (arbitrarily chosen).
Each `PageTable` has 2048 page table entries (`PTEntry`) that hold info about whether the page is allocated in memory (i.e. `PTEntry.present`),
and the frame number that it is allocated to (i.e. `PTEntry.frameTblNum`). I also defined a `PageDirectory` which is really just an
array of 32 `PageTables` that I used to easily access any thread's `PageTable`.

There is one frame table for the entire system represented by the `FrameTable` struct. Each entry of the `FrameTable` is organized
in a `FTEntry` struct and supplements a physical frame that pages can be allocated to from any threads' address space. Besides a mutex
used for synchronization, each `FTEntry` stores information about:

1) Whether it was accessed since the eviction algorithm was last run (i.e. `FTEntry.accessed`)
2) The threadId of the thread whose page is allocated to the frame (i.e. `FTEntry.ownerThreadId`)
3) The allocated page's virtual page number (i.e. `FTEntry.virtualPageNum`)
4) The frame number of the physical frame associated with that entry (i.e. `FTEntry.virtualPageNum`)
5) A pointer to the physical address that the frame begins at (i.e. `FTEntry.physAddr`)
6) A pointer to a frame table entry that is next in the free list if there is one (i.e. `FTEntry.next`)

The `FrameTable` has 1792 `FTEntry`s because that is how many physical frames can be allocated by user threads 
(i.e. `ALL_MEM_SIZE - USER_BASE_ADDR = 1792`). I could have had 2048 entries, but having only 1792 entries gave me more kernel space to 
use for other things like the free list (i.e. `FreeList`). However, considering I still have a lot of kernel space left, the increased
difficulty in reasoning about virtual address to physical frame translations may not have been worth it. Each page table has a total of 2048
entries, one for each page of the entire system. This allows for easy conversion from virtual address to the virtual page number (vpn) of its
associated page table entry via bitmasking the address's 11 MSB (addresses range from 0 - 8M which fits in 23 bits). The remaining 12 bits are
used to determine the offset into the physical frame.

All of these structures exist in the kernel space of the given SYSTEM_MEMORY constraints. No call to malloc is used.

2. Describe your algorithm for finding the frame associated with a page.

The following steps occur when a thread accesses its virtual address space. First, (after making a bounds check) the virtual address is
translated into its vpn via bitwise AND with a vpn mask, then shifted 12 bits to the right (i.e. `vpn = (virtualAddr & VPN_MASK) >> VPN_SHIFT`).
Then the page table entry is retrieved by offsetting into the thread's page table entries by `PAGE_TABLE_ENTRY_SIZE * vpn`. Since each entry
stores the frame number that the page is allocated to, fetching the frame table entry is simply a matter of offsetting into the `Frame Table`
by `FRAME_TABLE_ENTRY_SIZE * pageTableEntry.frameTblNum`. After fetching the frame table entry, a pointer to the physical address associated
with the page is one of its fields so can be accessed by offsetting to the start of that field (i.e. `physicalAddr = frameTableEntry.physAddr`).
That is my algorithm for finding the frame associated with a given thread's page.

3. When two user processes both need a new frame at the same time, how are races avoided?

The entries of all frames that are available to be allocated exist as members of the `FreeList` data structure. This data structure is represented
as a linked list where each node is a `FTEntry`. Each `FTEntry` stores a pointer to the next node in the list that is in, whether that be the
`FreeList` or the `AllocatedFramesList` storing the frames that have already been allocated). By giving the `FreeList`  a mutex, the entire 
`FreeList` can be locked whenever an unallocated frame needs to be fetched by a process. This ensures that all other processes must wait for the 
one holding the mutex to find a new frame before they themselves are able to find one for themselves. Thus, races are avoiding by ensuring only
one process can access the `FreeList` at a time.

4. Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

I chose to make the `PageTable`s and the `FrameTable` to be a linear tables because it made for easy mapping from virtual page numbers to
frame number and back. By making them linear tables, fetching the `FTEntry` associated with a page is as simple as indexing into `FrameTable.entries`
by `PTEntry.frameTblNum`. Similarly, fetching the `PTEntry` associated with a frame involves fetching the correct `PageTable` by indexing
into `PageDirectory.tables` by `FrameTable.ownerThreadId` and then indexing into that `PageTable.entries` by `FrameTable.virtualPageNum`.
After retreiving the `FTEntry`, fetching the physical frame simply requires accessing the `FTEntry.physAddr` which is a pointer to the start of
the physical frame in the system memory's user space.

The biggest drawback that first comes to mind is that this implementation cannot scale to a realistic number of threads because the entire
`PageDirectory` containing all the `PageTable`s is stored in kernel memory. Moreover, there is only a maximum of 32 `PageTable`s. A more
scalable solution might have been to load each thread's `PageTable` into memory when it is accessed and swap it out to disk when it has not
been accessed for a while.

5. When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

Each `FTEntry`'s `accessed` field has a value of 1 or 0. A value of 1 means that the frame has been accessed since the clock eviction algorithm
was last run. A value of 0 means that the frame has not been accessed since the clock eviction algorithm was last run. When a new frame is
required by a thread, the eviction algorithm is run. It iterates over the entire `FrameTable` starting at 0 at system intialization, checking
each `FTEntry`'s `accessed` field. If `FTEntry.accessed == 1`, it sets it to 0 and continues iteration. If `FTEntry.accessed == 0`, then
iteration stops, and that `FTEntry` and its associated physical frame is chosen for eviction.

6. When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

When process P obtains a frame that was previously used by process Q, the `FTEntry`'s `ownerThreadId` is set to P's `threadId`, the `virtualPageNum`
is set to P's page is now associated with the frame, and `accessed` is set 1 to reflect that P recently accessed the frame. After the `FTEntry` is
updated, the Q's `PTEntry` that was previously associated with the `FTEntry`'s `present` field is set to 0 to reflect that its associated frame
was swapped to disk, and its `frameTblNum` is set to 0. Although a `frameTblNum` of 0 is technically a real frame, it will never be accessed by
a `PTEntry` whose `present` field is set to 0.

7. A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

When P chooses to evict one of thread Q's pages, Q's `PageTable` is locked so that it is unable to access any of its pages. If Q was in the middle
of accessing some of its pages, then P waits for it to finish before locking Q's `PageTable`. If Q was not in the middle of accessing some of its
pages, then P immediately gets Q's `PageTable` mutex and Q is unable to read/write or allocate any memory until P is done evicting the page that it
has chosen to evict. Since Q's entire `PageTable` is locked, races are avoided because only one thread will be modifying Q's `PTEntry`s at a time.

8. Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

When P is reading one of its entries back into the memory from the file system, it locks the mutex of the `FTEntry` for the physical frame that it
is being read into. If Q chooses to evict chooses to evict the `FTEntry` that P is currently reading into disk, it must first lock that `FTEntry`.
But since P is currently using it, they will be unable to access that entry until P is done with it. Once P finishes swapping the page back in,
they will unlock the mutex and Q will begin eviction. If P attempts to access the page again while Q is evicting it, they will be unable to because
their `PageTable` is locked.

9. A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

I would say that my design falls somewhere in the middle of the coninuum.

Each `PageTable` has a mutex that is locked when any thread accesses any of its entries. Each `FTEntry` has its own mutex that that is locked when 
any thread accesses that entry. The `FrameTable` itself does not have a lock. I chose to have a single lock for an entire thread's `PageTable` to
ensure that a thread will have updated knowledge about its pages, such as whether a page is present in memory and, if so, what is the frame number of
the frame associated with any given page. My reason for having only one lock per `PageTable` is because not many thread's should be accessing 
`PageTable`s other than their own and having only one lock slightly simplifies reasoning about synchronization. On the other hand, I decided that each
`FTEntry` should have its own mutex instead of the entire `FrameTable` because physical memory is accessed fairly frequently by multiple threads at the
same time, so the increased conceptual difficulty is a worthwhile tradeoff for better parallelism.
