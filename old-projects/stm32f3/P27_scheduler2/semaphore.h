/* title: Test-Semaphore

   Implements simple data structure for synchronization
   or mutual exclusion of two or more threads.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: sempahore.h
    Header file <Test-Semaphore>.

   file: sempahore.c
    Implementation file <Test-Semaphore impl>.
*/
#ifndef CORTEXM4_SEMAPHORE_HEADER
#define CORTEXM4_SEMAPHORE_HEADER

#include "task.h"

// == exported objects

typedef struct semaphore_t {
   volatile int32_t  value;      // value < 0 ==> -value is number of waiting tasks
   task_wait_t       taskwait;
} semaphore_t;

// == lifetime
#define semaphore_INIT(VALUE) { (VALUE), task_wait_INIT }

// == query
static inline int32_t value_semaphore(const semaphore_t *sem);
            // Returns the value of the semaphore, how many times wait could be called without waiting.

// == signal / wait
void signal_semaphore(semaphore_t *sem);
            // Increments the value of sem atomically.
void wait_semaphore(semaphore_t *sem);
            // Waits until value of sem > 0 and then decrements it atomically.
int32_t trywait_semaphore(semaphore_t *sem);
            // Tries to decrement sempahore only if dcremented value >= 0.
            // Returns new decremented value.
            // returns <0: sem was not changed cause wait would have blocked. >=0: sem was changed to returned value without blocking.

// == inline implementations

static inline int32_t value_semaphore(const semaphore_t *sem)
{
         return sem->value;
}

#endif
