# Project 2 Questions

## Directions

Answer each question to the best of your ability; feel free to copy sections of
your code or link to it if it helps explain but answers such as "this line
guarantees that the highest priority thread is woken first" are not sufficient.

Your answers can be relatively short - around 100 words, and no more than 200,
should suffice per subsection.

To submit, edit this file and replace every `ANSWER_BEGIN` to `ANSWER_END` with
your answer. Your submission should not include **any** instances of
`ANSWER_BEGIN` or `ANSWER_END`; the text provided is meant to give you some tips
on how to begin answering.

## Scheduling

**Two tests have infinite loops with the scheduler you were provided. Explain
why `Running.PriorityOrder` will never terminate with the basic scheduler.**

ANSWER_BEGIN

The reason that the `Running.PriorityOrder` test will never terminate with the
basic scheduler is because `nextThreadToRun` just blindly chooses the thread at index
0 of the list of threads that are ready to be executed (i.e. `readyList`). 
It does not consider the relative priority of the threads when selecting 
the next one to execute. So even after higher priority threads get added to 
`readyList`, the Low Priority thread keeps getting chosen because it is at
index 0. Moreover, the `Running.PriorityOrder` test is set up such that the threads
terminate from High Priority thread -> Medium Priority thread -> Low Priority
thread. Therefore, none of the threads terminate because neither the Medium nor
the High priority thread ever get executed.

ANSWER_END

## Sleep

**Briefly describe what happens in your `sleep(...)` function.**

ANSWER_BEGIN

1. When the sleep function is called on a thread, a struct of type `SleepingThread`
containing the pointer to the thread and the tick that it is scheduled to wake up 
is allocated to the heap.
2. The `SleepingThread` struct that was created for the thread that was put to sleep
is added to the list containing all sleeping threads called `sleepList`.
3. The thread that was put to sleep is removed from the `readyList`.
4. The `sleepList` is sorted by the value of wake up tick in ascending order.
5. The thread is paused by calling the `stopExecutingThreadForCycle()` and will not
be run again until its wake up tick is reached.

ANSWER_END

**How does your `sleep(...)` function guarantee duration?**

ANSWER_BEGIN

What prevents a thread from waking up too early?

The thread is stored in `sleepList` in a struct that also contains the tick that it is
scheduled to wake up after. When checking a thread's wake up tick value, the scheduler will
never run the thread if the current tick is less than the scheduled wake up tick. This
prevents a sleeping thread from waking up too early.

What prevents a thread from _never_ waking up?

The scheduler checks the list of sleeping threads each tick before running any threads that
are in the `readyList`. This can be seen on line 172 of `thread.cpp`. Since no thread can be
run before the sleeping threads are checked, the scheduler will never "lock out" the sleeping
threads by continually running threads in the ready state.

ANSWER_END

**Do you have to check every sleeping thread every tick? Why not?**

ANSWER_BEGIN

No, the scheduler does not need to check every sleeping thread every tick.
Since the `sleepList` is sorted by the wake up tick values in ascending order,
if a thread at lower index is not ready to be woken up, then all threads at
larger indices will not be woken up either. Therefore, once a thread that does
not need to be woken up is reached, the function will exit without checking any
other threads.

ANSWER_END

## Locking

**How do you ensure that the highest priority thread waiting for a lock gets it
first?**

ANSWER_BEGIN

This is similar to how you should be handling `sleep(...)`ing threads.

The scheduler is designed such that the threads in `readyList` are sorted in ascending
order relative to priority. This means that `nextThreadToRun()` will choose to run
threads with higher priority before those with lower priority. Therefore, if two threads
with different priorities are waiting for the same lock, the thread with the higher
priority will be run first meaning that it will obtain the lock before the thread with
the lower priority.

ANSWER_END

## Priority Donation

**Describe the sequence of events before and during a priority donation.**

ANSWER_BEGIN

The invariant for the locking function is that the thread holding a lock has a higher
priority than all other threads waiting for the lock. If a thread chosen by `nextThreadToRun()`
attempts to acquire a lock that is held by another thread, that means that the invariant
is violated because `nextThreadToRun()` runs threads from higher priority to lower priority.
This violation is detected in `lockAttempted()` after which a priority donation occurs where
the thread that was running swaps priorities with the thread holding the lock and the `readyList`
is sorted ensuring that the thread holding the lock has the highest priority. After the donation
occurs in `lockAttempted()` and the tick is advanced, `nextThreadToRun()` will choose the thread
that received the priority donation since it now has the highest priority and is at the front
of the `readyList`. Once it runs and releases the lock, the `lockReleased()` function will be
called. `lockReleased()` will detect that a priority donation occured because the thread's
original priority will differ from its current priority. It will then iterate over the `readyList`
and restore the priorities of all the threads in it. It will then re-sort the `readyList`. This
priority restoration that occurs in `lockReleased()` ensures that the thread with the highest
priority waiting for the lock will acquire it next.

ANSWER_END

**How is nested donation handled?**

ANSWER_BEGIN

Describe a system of three threads (A, B, C) and three locks (1, 2, 3).

Thread A holds lock 1, thread B holds lock 2, and thread C holds lock 3. Thread A has 'High'
priority, thread B has 'Medium' priority, and thread 'C' has low priority. Thread A is waiting
on lock 2, thread B is waiting on lock 3, and thread C is waiting on lock 1. Since, thread A
has the highest priority `nextThreadToRun()` will run it first. Thread A will release lock 1
and then attempt to acquire lock 2. It will then see that thread B has a lower priority
and swap priorities with it. Next tick `nextThreadToRun()` will schedule thread B since it
now has the highest priority. Thread B will then release lock 2, restoring the original
priorities in `lockReleased()`. It will then attempt to acquire lock 3, and see that thread
C has a lower priority and swap priorities with it. Next tick `nextThreadToRun()` will schedule
thread A. Thread A will acquire lock 2, then release it and restore all original priorities.
Thread A will run till termination, then `nextThreadToRun()` will schedule B which will swap
priorities with thread C again after attempting to acquire lock 3. Next tick `nextThreadToRun()`
will schedule thread C which will release lock 3, restore priorities and then acquire lock 1.
Thread B will then acquire lock 3, release it, then run to completion. Then thread C will
run to completion releasing lock 1 in the process.

ANSWER_END

**How do you ensure that the highest priority thread waiting is called when the
lock is released?**

ANSWER_BEGIN

When a lock is released the `lockReleased()` function is called. If the thread that released
the lock received a priority donation, then this will be detected on line 101 of `lock.cpp` which
compares its current priority to its original priority. It will then iterate over `readyList` restoring
the priorities of all threads whose current priorities differ from their original priorities. This
ensures that `nextThreadToRun()` will always schedule the highest priority threads waiting for the lock
first. If the thread that released the lock did not receive a priority donation, then it was the highest
priority thread and no priorities need to be restored before scheduling the next thread to run.

ANSWER_END

## Bonus points

1) In the documentation for `stopExecutingThreadForCycle()` on line 68-69 of `os_simulatior/includes/Thread.h`: 
"It can also be used to increment your score by half of two if you refer to this comment."

2) Typo in documentation of `getCurrentTick()` on line 82 of `os_simulator/includes/Thread.h`: "THere" should
be "There".

3) In the documentation of `destroyThread()` On line 47-48 of `os_simulator/includes/Thread.student.h`: https://i.ytimg.com/vi/m7-rPzYxtws/maxresdefault.jpg. Link shows thread being burned to answer "Attach an image of thread being destroyed for a small present."
