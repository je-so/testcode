/* title: Task impl

   Implements <Task>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: task.h
    Header file <Task>.

   file: task.c
    Implementation file <Task impl>.
*/
#include "konfig.h"
#include "hw/cm4/iframe.h"
#include "task.h"
#include "scheduler.h"

/* == Globals == */

task_t     *g_task_current;

/* == Locals == */

/* == task_t impl == */

void init_task(task_t *task, uint8_t priority, task_main_f task_main, uintptr_t task_arg)
{
   task->sp = &task->stack[lengthof(task->stack)- iframe_LEN(iframe_len_NOFPU|iframe_len_NOPADDING)];
   task->lr = retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_THREADMODE_PSP);
   task->state = task_state_SUSPEND;
   task->req = 0;
   task->id  = 0;
   task->priority = 0x7&priority;
   task->sleepms = 0;
   task->wait_for = 0;
   task->next = 0;
   task->prev = 0;
   task->wakeup_next = 0;
   task->sp[iframe_R0]  = task_arg;
   task->sp[iframe_LR]  = 0xffffffff;   // invalid LR value
   task->sp[iframe_PC]  = (uintptr_t) task_main;
   task->sp[iframe_PSR] = cpuflag_PSR_THUMB;
}

void sleepms_task(uint32_t millisec)
{
   task_t *task = current_task();
   task->sleepms = millisec;
   task->state = task_state_SLEEP;
   yield_task();
}

void suspend_task(void)
{
   task_t *task = current_task();
   task->state = task_state_SUSPEND;
   yield_task();
}

void end_task(void)
{
   task_t *task = current_task();
   task->state = task_state_SUSPEND;
   task->req = task_req_END;
   yield_task();
}

/* == task_wait_t impl == */

void wait_task(task_wait_t *wait_for)
{
   task_t *task = current_task();
   task->wait_for = wait_for;
   task->state = task_state_WAITFOR;
   yield_task();
}

void signal_task(task_wait_t *wait_for)
{
   increment_atomic(&wait_for->nrevent);

   if (istask_taskwait(wait_for)) {
      preparewakeup_scheduler(wait_for);
   }
}


#ifdef KONFIG_UNITTEST

volatile uint32_t s_pendsvcounter;

static void local_pendsv_interrupt(void)
{
   // simulates freeing of entry in array s_task_wakeup, called with yield_task
   // TODO: add something wakeup ?
   s_pendsvcounter++;
}

static void clear_ram(volatile uint32_t * ram, size_t size)
{
   size /= 4;
   for (size_t i = 0; i < size; ++i) {
      ram[i] = 0x12345678;
   }
}

int unittest_jrtos_task()
{
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;
   task_t   * const task   = (task_t*) CCMRAM;
   static_assert(512 >= sizeof(uint32_t)*len_interruptTable());
   static_assert(3   <= (CCMRAM_SIZE - 512) / sizeof(task_t));
   size_t     const nrtask = (CCMRAM_SIZE - 512) / sizeof(task_t);
   task_t   * const old_current = g_task_current;
   uint32_t * const itable = (uint32_t*) &task[nrtask];

   // prepare
   setprio0mask_interrupt();
   TEST( 0 == relocate_interruptTable(itable));
   itable[coreinterrupt_PENDSV] = (uintptr_t) &local_pendsv_interrupt;

   // TEST current_task
   for (uintptr_t i = 1; i; i <<= 1) {
      g_task_current = (void*)i;
      TEST( (task_t*)i == current_task());
   }

   // TEST set_current_task
   for (uintptr_t i = 1; i; i <<= 1) {
      set_current_task((void*)i);
      TEST( (task_t*)i == g_task_current);
   }

   // TEST init_main_task
   clear_ram(CCMRAM, nrtask*sizeof(task_t));
   for (uint32_t i = 0; i < nrtask; ++i) {
      for (uint8_t p = 0; p < 16; ++p) {
         init_main_task(&task[i], p);
         TEST( &task[i] == current_task());
         TEST( task[i].regs[0] == 0x12345678);  // unchanged
         TEST( task[i].regs[7] == 0x12345678);  // unchanged
         TEST( task[i].sp      == (uint32_t*)&task[i+1] - 8);
         TEST( task[i].lr      == 0xfffffffd);
         TEST( task[i].state   == task_state_SUSPEND);
         TEST( task[i].req     == 0);
         TEST( task[i].sleepms == 0);
         TEST( task[i].wait_for == 0);
         TEST( task[i].next    == 0);
         TEST( task[i].prev    == 0);
         TEST( task[i].wakeup_next == 0);
         TEST( task[i].priority == (p&~8));
         TEST( task[i]._protection[0] == 0x12345678); // unchanged
         TEST( task[i].stack[0] == 0x12345678); // unchanged
         TEST( task[i].sp[0]   == 0);           // R0 set to parameter
         TEST( task[i].sp[1]   == 0x12345678);  // R1 unchanged
         TEST( task[i].sp[4]   == 0x12345678);  // R12 unchanged
         TEST( task[i].sp[5]   == 0xFFFFFFFF);  // LR invalid
         TEST( task[i].sp[6]   == 0);           // PC invalid
         TEST( task[i].sp[7]   == (1<<24));     // Thumb bit set
      }
   }

   // TEST init_task
   clear_ram(CCMRAM, nrtask*sizeof(task_t));
   set_current_task(old_current);
   for (uintptr_t i = 0; i < nrtask; ++i) {
      for (uint8_t p = 0; p < 16; ++p) {
         init_task(&task[i], p, (task_main_f)(i+1), (i+2));
         TEST( old_current == current_task());
         TEST( task[i].regs[0] == 0x12345678);  // unchanged
         TEST( task[i].regs[7] == 0x12345678);  // unchanged
         TEST( task[i].sp      == (uint32_t*)&task[i+1] - 8);
         TEST( task[i].lr      == 0xfffffffd);
         TEST( task[i].state   == task_state_SUSPEND);
         TEST( task[i].req     == 0);
         TEST( task[i].id      == 0);
         TEST( task[i].priority == (p&~8));
         TEST( task[i].sleepms == 0);
         TEST( task[i].wait_for == 0);
         TEST( task[i].next    == 0);
         TEST( task[i].prev    == 0);
         TEST( task[i].wakeup_next == 0);
         TEST( task[i]._protection[0] == 0x12345678); // unchanged
         TEST( task[i].stack[0] == 0x12345678); // unchanged
         TEST( task[i].sp[0]   == i+2);         // R0 set to parameter
         TEST( task[i].sp[1]   == 0x12345678);  // R1 unchanged
         TEST( task[i].sp[4]   == 0x12345678);  // R12 unchanged
         TEST( task[i].sp[5]   == 0xFFFFFFFF);  // LR invalid
         TEST( task[i].sp[6]   == (i+1));       // PC set to task_main
         TEST( task[i].sp[7]   == (1<<24));     // Thumb bit set
      }
   }

   // TEST yield_task
   // TODO:

   // TEST sleepms_task
   // TODO:

   // TEST wait_task
   // TODO:


   // TEST signal_task: all s_task_wakeup are free
   // TODO: task_wait_t wait_for = task_wait_INIT;
   // TODO: wait_for.last = &task[0];

   // TEST signal_task

   // prepare test signal_task
   TEST( 0 == is_any_coreinterrupt());
   clearprio0mask_interrupt();

   // TEST signal_task: all s_task_wakeup are used ==> yield (dummy pendsv_interrupt simulates freeing)
   for (uint32_t i = 0; i < 10; ++i) {
   }
   // reset
   setprio0mask_interrupt();
   s_pendsvcounter = 0;

   // TODO:

   // TODO:

   // TODO:

   // reset
   TEST( old_current == current_task());
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   reset_interruptTable();
   clearprio0mask_interrupt();

   return 0;
}

#endif
