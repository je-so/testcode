/* title: Semaphore

   Implements synchronization or mutual exclusion
   of two or more threads with a simple counting value.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: semaphore.h
    Header file <Semaphore>.

   file: semaphore.c
    Implementation file <Semaphore impl>.
*/
#ifndef JRTOS_SEMAPHORE_HEADER
#define JRTOS_SEMAPHORE_HEADER

/* == Import == */
#include "taskwait.h"

/* == Typen == */
typedef struct semaphore_t semaphore_t;
            // TODO: semaphore_t

/* == Objekte == */
struct semaphore_t {
   volatile int32_t  value;      // value < 0: -value gives number of waiting tasks. vallue >= 0: No tasks ar waiting.
   task_wait_t       taskwait;
};

// semaphore_t: test

#ifdef KONFIG_UNITTEST
int unittest_jrtos_semaphore(void);
#endif

// semaphore_t: lifetime

#define semaphore_INIT(VALUE)    { (VALUE), task_wait_INIT }

// semaphore_t: query

static inline int32_t value_semaphore(const semaphore_t *sem);
            // Returns the value of the semaphore, how many times wait could be called without waiting.
            // returns 0: No waiting tasks. No counted signals. Calling wait_semaphore would block and trywait_semaphore would fail with EAGAIN.
            // returns C>0: No waiting tasks. C counted signals. Calling wait_semaphore would not block and trywait_semaphore would pass and return 0.
            // returns C<0: -C waiting tasks waiting for -C signals. Calling wait_semaphore would block and trywait_semaphore would fail with EAGAIN.

// semaphore_t: signal-or-wait

void signal_semaphore(semaphore_t *sem);
            // Increments the value of sem atomically and wakes-up waiting tasks.
void signalqd_semaphore(semaphore_t *sem);
            // Increments the value of sem atomically ane queues wake-up of waiting tasks.
void wait_semaphore(semaphore_t *sem);
            // Waits until value of sem > 0 and then decrements it atomically.
int trywait_semaphore(semaphore_t *sem);
            // Tries to decrement semaphore only if decremented value >= 0.
            // returns EAGAIN: sem was not changed cause wait would have blocked. 0: sem was changed to a value >= 0 without blocking.


/* == Inline == */

static inline int32_t value_semaphore(const semaphore_t *sem)
{
         return sem->value;
}

#endif
