/* title: Semaphore impl

   Implements <Semaphore>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: semaphore.h
    Header file <Semaphore>.

   file: semaphore.c
    Implementation file <Semaphore impl>.
*/
#include "konfig.h"
#include "semaphore.h"
#include "scheduler.h"
#include "task.h"

void signal_semaphore(semaphore_t *sem)
{
   int32_t newval = (int32_t) increment_atomic((volatile uint32_t*)&sem->value);
   if (newval <= 0) {
      wakeup_task(&sem->taskwait);
   }
}

void signalqd_semaphore(semaphore_t *sem)
{
   int32_t newval = (int32_t) increment_atomic((volatile uint32_t*)&sem->value);
   if (newval <= 0) {
      wakeupqd_task(&sem->taskwait);
   }
}


void wait_semaphore(semaphore_t *sem)
{
   int newval = (int32_t) decrement_atomic((volatile uint32_t*)&sem->value);
   if (newval < 0) {
      wait_task(&sem->taskwait);
   }
}

int trywait_semaphore(semaphore_t *sem)
{
   int32_t oldval = decrementpositive_atomic(&sem->value);
   return oldval > 0 ? 0 : EAGAIN;
}

#ifdef KONFIG_UNITTEST

// TODO:
volatile uint32_t s_pendsvcounter;

// TODO:
static void local_pendsv_interrupt(void)
{
   // simulates freeing of entry in array s_task_wakeup, called with yield_task
   s_pendsvcounter++;
}

// TODO:
static void clear_ram(volatile uint32_t * ram, size_t size)
{
   size /= 4;
   for (size_t i = 0; i < size; ++i) {
      ram[i] = 0x12345678;
   }
}

int unittest_jrtos_semaphore()
{
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;
   task_t   * const task   = (task_t*) CCMRAM;
   static_assert(512 >= sizeof(uint32_t)*len_interruptTable());
   size_t     const nrtask = (CCMRAM_SIZE - 512) / sizeof(task_t);
   uint32_t * const itable = (uint32_t*) &task[nrtask];
   semaphore_t sem;

   // prepare
   setprio0mask_interrupt();
   TEST( 0 == relocate_interruptTable(itable));
   itable[coreinterrupt_PENDSV] = (uintptr_t) &local_pendsv_interrupt;  // TODO: remove ?

   // TEST semaphore_INIT
   for (uint32_t i = 0; i < 100; ++i) {
      sem = (semaphore_t) semaphore_INIT(i);
      TEST( i == sem.value);
      TEST( 0 == sem.taskwait.nrevent);
      TEST( 0 == sem.taskwait.last);
   }

   // TEST value_semaphore
   for (uint32_t i = -100; i < 100; ++i) {
      sem = (semaphore_t) semaphore_INIT(i);
      TEST( i == value_semaphore(&sem));
   }

   // TEST signal_semaphore
   // TODO:

   // TEST wait_semaphore
   // TODO:

   // TEST trywait_semaphore
   // TODO:


   // reset
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   reset_interruptTable();
   clearprio0mask_interrupt();

   return 0;
}

#endif
