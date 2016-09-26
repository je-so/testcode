/* title: Test-Semaphore impl

   Implementation of interface <Test-Semaphore>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: sempahore.h
    Header file <Test-Semaphore>.

   file: sempahore.c
    Implementation file <Test-Semaphore impl>.
*/
#include "konfig.h"
#include "semaphore.h"
#include "sched.h"
#include "task.h"

void signal_semaphore(semaphore_t *sem)
{
   int32_t newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"       // 1: newval = sem->value
      "adds    %0, %0, #1\n"        // newval ++
      "strex   r2, %0, [%1]\n"      // try { sem->value = newval; }
      "tst     r2, r2\n"
      "bne     1b\n"                // if (increment not atomic) goto back to label 1:
      : "=&r" (newval) : "r" (sem) : "r2", "cc", "memory"
   );
   if (newval <= 0) {
      signal_taskwait(&sem->taskwait);
   }
}

void wait_semaphore(semaphore_t *sem)
{
   int newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"       // 1: newval = sem->value
      "subs    %0, %0, #1\n"        // newval --;
      "strex   r2, %0, [%1]\n"      // try { sem->value = newval; }
      "tst     r2, r2\n"
      "bne     1b\n"                // if (decrement not atomic) goto back to label 1:
      : "=&r" (newval) : "r" (sem) : "r2", "cc", "memory"
   );
   if (newval < 0) {
      wait_taskwait(&sem->taskwait);
   }
}

int32_t trywait_semaphore(semaphore_t *sem)
{
   int newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"       // 1: newval = sem->value
      "subs    %0, %0, #1\n"        // newval --;
      "blo     2f\n"                // if (newval < 0) goto forward to error label 2:
      "strex   r2, %0, [%1]\n"      // try { sem->value = newval; }
      "tst     r2, r2\n"
      "bne     1b\n"                // if (decrement not atomic) goto back to label 1:
      "2:\n"                        // 2: newval < 0
      : "=&r" (newval) : "r" (sem) : "r2", "cc", "memory"
   );
   return newval; /* newval>=0: trywait was successful. newval<0: Not successfull, sem was not changed. */
}
