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

/* == Locals == */

/* == task_t impl == */

void init_main_task(task_t *task, uint8_t priority/*0..31*/)
{
   priority = (uint8_t) (0x1f & priority);
   task->sp = &task->topstack - iframe_LEN(iframe_len_NOFPU|iframe_len_NOPADDING);
   task->lr = retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_THREADMODE_PSP);
   task->priobit = 0x80000000 >> priority;
   task->sched = 0;
   task->state = task_state_SUSPEND;
   task->id = 0;
   task->priority = priority;
   task->req_stop = 0;
   task->req.waitfor = 0;
   task->qd_task = 0;
   init_taskwakeup(&task->qd_wakeup);
   task->next = 0;
}

void init_task(task_t *task, uint8_t priority/*0..31*/, task_main_f task_main, uintptr_t task_arg)
{
   init_main_task(task, priority);
   task->sp[iframe_R0]  = task_arg;
   task->sp[iframe_LR]  = 0xffffffff;   // invalid LR value
   task->sp[iframe_PC]  = (uintptr_t) task_main;
   task->sp[iframe_PSR] = iframe_flag_PSR_THUMB;
}

void sleepms_task(uint32_t millisec)
{
   if (millisec) {
      task_t *task = current_task();
      task->req.sleepms = millisec;
      sw_msync();
      task->sched->req = task_req_SLEEP;
   }
   yield_task();
}

void suspend_task(void)
{
   task_t *task = current_task();
   task->sched->req = task_req_SUSPEND;
   yield_task();
}

void end_task(void)
{
   task_t *task = current_task();
   task->sched->req = task_req_END;
   yield_task();
}

void resume_task(task_t *task)
{
   task_t *caller = current_task();
   caller->req.task = task;
   sw_msync();
   caller->sched->req = task_req_RESUME;
   yield_task();
}

void resumeqd_task(task_t *task)
{
   task_t *caller = current_task();
   caller->qd_task |= task->priobit;
   sw_msync();
   caller->sched->req_qd_task = 1;
}

void stop_task(task_t *task)
{
   task_t *caller = current_task();
   caller->req.task = task;
   sw_msync();
   caller->sched->req = task_req_STOP;
   yield_task();
}

/* == task_wait_t impl == */

void wakeup_task(task_wait_t *waitfor)
{
   task_t *task = current_task();
   task->req.waitfor = waitfor;
   sw_msync();
   task->sched->req = task_req_WAKEUP;
   yield_task();
}

void wait_task(task_wait_t *waitfor)
{
   task_t *task = current_task();
   task->req.waitfor = waitfor;
   sw_msync();
   task->sched->req = task_req_WAITFOR;
   yield_task();
}

void wakeupqd_task(task_wait_t *waitfor)
{
   task_t *task = current_task();
   while (0 != write_taskwakeup(&task->qd_wakeup, waitfor)) {
      task->sched->req_qd_wakeup = 1;
      yield_task();
   }
   task->sched->req_qd_wakeup = 1;
}



#ifdef KONFIG_UNITTEST

static volatile uint32_t s_pendsvcounter;

static void local_pendsv_interrupt(void)
{
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
   static_assert(3   <= (CCMRAM_SIZE - 512 - sizeof(scheduler_t)) / sizeof(task_t));
   size_t     const nrtask = (CCMRAM_SIZE - 512 - sizeof(scheduler_t)) / sizeof(task_t);
   uint32_t * const itable = (uint32_t*) &task[nrtask];
   scheduler_t     *sched  = (scheduler_t*) ((uint8_t*)itable + 512);
   task_t   * const mtask  = current_task();

   // prepare
   setprio0mask_interrupt();
   TEST( 0 == s_pendsvcounter);
   TEST( 0 == relocate_interruptTable(itable));
   itable[coreinterrupt_PENDSV] = (uintptr_t) &local_pendsv_interrupt;
   TEST( 0 == mtask->sched);
   init_taskwakeup(&mtask->qd_wakeup);
   mtask->sched = sched;
   for (size_t i = 0; i < sizeof(*sched)/sizeof(uint32_t); ++i) {
      ((uint32_t*)sched)[i] = 0;
   }

   // TEST current_task
   for (size_t i = 0; i < nrtask; ++i) {
      uint32_t old_psp;
      __asm volatile (
         "mrs %0, psp\n"                     // save psp value
         : "=r" (old_psp) ::
      );
      __asm volatile (
         "msr psp, %0\n"
         :: "r" (&task[i].stack[lengthof(task[i].stack)]) : // task->topstack excluded from stack size
      );
      TEST( &task[i] == current_task());     // does work
      __asm volatile (
         "msr psp, %0\n"
         :: "r" (&task[i].stack[lengthof(task[i].stack)+1]) : // task->topstack included in stack size
      );
      TEST( &task[i+1] == current_task());   // does not work
      __asm volatile (
         "msr psp, %0\n"                     // restore old psp value
         :: "r" (old_psp) :
      );
   }

   // TEST initialstack_task
   for (size_t i = 0; i < nrtask; ++i) {
      TEST( &task[i].topstack == initialstack_task(&task[i]));
   }

   // TEST init_task
   clear_ram(CCMRAM, nrtask*sizeof(task_t));
   for (uintptr_t i = 0; i < nrtask; ++i) {
      for (uint8_t p = 0; p < 255; ++p) {
         init_task(&task[i], p, (task_main_f)(i+1), (i+2));
         TEST( task[i].sp      == (uint32_t*)&task[i+1] - (8+1));
         TEST( task[i].regs[0] == 0x12345678);  // unchanged
         TEST( task[i].regs[7] == 0x12345678);  // unchanged
         TEST( task[i].lr      == 0xfffffffd);
         TEST( task[i].priobit == (0x80000000>>(p&31)));
         TEST( task[i].sched   == 0);
         TEST( task[i].state   == task_state_SUSPEND);
         TEST( task[i].id      == 0);
         TEST( task[i].priority == (p&31));
         TEST( task[i].req_stop == 0);
         TEST( task[i].req.waitfor == 0);
         TEST( task[i].req.task == 0);
         TEST( task[i].req.sleepms == 0);
         TEST( task[i].qd_task == 0);
         TEST( task[i].qd_wakeup.qsize == 4);
         TEST( task[i].qd_wakeup.size == 0);
         TEST( task[i].qd_wakeup.queue[0] == (void*)0x12345678);
         TEST( task[i].next    == 0);
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
   for (uint32_t i = 0; i < 10; ++i) {
      TEST( 0 == is_coreinterrupt(coreinterrupt_PENDSV));
      yield_task();
      TEST( 0 != is_coreinterrupt(coreinterrupt_PENDSV));
      clear_coreinterrupt(coreinterrupt_PENDSV);
   }

   // TEST sleepms_task: millisec=0
   sleepms_task(0);
   TEST( 0 == sched->req32);
   TEST( 0 != is_coreinterrupt(coreinterrupt_PENDSV));
   clear_coreinterrupt(coreinterrupt_PENDSV);

   // allow interrupts
   clearprio0mask_interrupt();

   // TEST sleepms_task: millisec!=0
   for (uint32_t i = 1, cnt = 1; i; i <<= 1, ++cnt) {
      sleepms_task(i);
      TEST( task_req_SLEEP == sched->req);
      TEST( cnt == s_pendsvcounter);
      // reset
      sched->req = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST suspend_task
   for (uint32_t i = 1; i < 10; ++i) {
      suspend_task();
      TEST( task_req_SUSPEND == sched->req);
      TEST( i == s_pendsvcounter);
      // reset
      sched->req = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST end_task
   for (uint32_t i = 1; i < 10; ++i) {
      end_task();
      TEST( task_req_END == sched->req);
      TEST( i == s_pendsvcounter);
      // reset
      sched->req = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST wait_task
   for (uint32_t i = 1; i < 10; ++i) {
      task_wait_t waitfor = task_wait_INIT;
      wait_task(&waitfor);
      TEST( task_req_WAITFOR == sched->req);
      TEST( &waitfor == mtask->req.waitfor);
      TEST( i == s_pendsvcounter);
      TEST( 0 == waitfor.nrevent);
      TEST( 0 == waitfor.last);
      // reset
      sched->req = 0;
      mtask->req.waitfor = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST resume_task
   for (uint32_t i = 1; i < 3*nrtask; ++i) {
      task_t * const T = &task[i%nrtask];
      resume_task(T);
      TEST( task_req_RESUME == sched->req);
      TEST( T == mtask->req.task);
      TEST( i == s_pendsvcounter);  // called into scheduler
      TEST( task_state_SUSPEND == T->state);
      // reset
      sched->req = 0;
      mtask->req.task = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST resumeqd_task: single task
   for (uint32_t i = 1, cnt = 1; i; i <<= 1, ++cnt) {
      task_t * const T = &task[cnt%nrtask];
      T->priobit = i;
      resumeqd_task(T);
      TEST( 1 == sched->req_qd_task);
      TEST( i == mtask->qd_task);
      TEST( 0 == s_pendsvcounter);  // not called into scheduler
      TEST( task_state_SUSPEND == T->state);
      // reset
      sched->req_qd_task = 0;
      mtask->qd_task = 0;
      TEST( 0 == sched->req32);
   }

   // TEST resumeqd_task: multiple tasks
   for (uint32_t i = 1, cnt = 1, Q = 1; i; i <<= 1, ++cnt, Q |= i) {
      task_t * const T = &task[cnt%nrtask];
      T->priobit = i;
      resumeqd_task(T);
      TEST( 1 == sched->req_qd_task);
      TEST( Q == mtask->qd_task);
      TEST( 0 == s_pendsvcounter);  // not called into scheduler
      TEST( task_state_SUSPEND == T->state);
      // reset
      sched->req_qd_task = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   mtask->qd_task = 0;

   // TEST stop_task
   for (uint32_t i = 1; i < 3*nrtask; ++i) {
      task_t * const T = &task[i%nrtask];
      stop_task(T);
      TEST( task_req_STOP == sched->req);
      TEST( T == mtask->req.task);
      TEST( i == s_pendsvcounter);  // called into scheduler
      TEST( task_state_SUSPEND == T->state);
      // reset
      sched->req = 0;
      mtask->req.task = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST wakeup_task
   for (uint32_t i = 1; i < 3*nrtask; ++i) {
      task_wait_t waitfor = task_wait_INIT;
      task_wait_t*const W = &waitfor;
      wakeup_task(W);
      TEST( task_req_WAKEUP == sched->req);
      TEST( W == mtask->req.waitfor);
      TEST( i == s_pendsvcounter);  // called into scheduler
      TEST( 0 == waitfor.nrevent);
      TEST( 0 == waitfor.last);
      // reset
      sched->req = 0;
      mtask->req.task = 0;
      TEST( 0 == sched->req32);
   }
   // reset
   s_pendsvcounter = 0;

   // TEST wakeupqd_task: single wakeup
   for (uint32_t i = 1; i < 10; ++i) {
      task_wait_t waitfor = task_wait_INIT;
      task_wait_t*const W = &waitfor;
      wakeupqd_task(W);
      TEST( 1 == sched->req_qd_wakeup);
      TEST( 4 == mtask->qd_wakeup.qsize);
      TEST( 1 == mtask->qd_wakeup.size);
      TEST( W == mtask->qd_wakeup.queue[0]);
      TEST( 0 == s_pendsvcounter);  // not called into scheduler
      TEST( 0 == waitfor.nrevent);
      TEST( 0 == waitfor.last);
      // reset
      sched->req_qd_wakeup = 0;
      init_taskwakeup(&mtask->qd_wakeup);
      TEST( 0 == sched->req32);
   }

   // TEST wakeupqd_task: multiple wakeup
   for (uint32_t i = 1; i < 10; ++i) {
      task_wait_t waitfor[4] = { task_wait_INIT, task_wait_INIT, task_wait_INIT, task_wait_INIT };
      for (uint32_t w = 0; w < 4; ++w) {
         task_wait_t*const W = &waitfor[w];
         wakeupqd_task(W);
         TEST( 1 == sched->req_qd_wakeup);
         TEST( 4 == mtask->qd_wakeup.qsize);
         TEST( w+1 == mtask->qd_wakeup.size);
         TEST( W == mtask->qd_wakeup.queue[w]);
         TEST( 0 == s_pendsvcounter);  // not called into scheduler
         TEST( 0 == W->nrevent);
         TEST( 0 == W->last);
         // reset
         sched->req_qd_wakeup = 0;
         TEST( 0 == sched->req32);
      }
      for (uint32_t w = 0; w < 4; ++w) {
         TEST( &waitfor[w] == mtask->qd_wakeup.queue[w]);
      }
      // reset
      init_taskwakeup(&mtask->qd_wakeup);
   }

   // reset
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   mtask->sched = 0;
   reset_interruptTable();
   clearprio0mask_interrupt();

   return 0;
}

#endif
