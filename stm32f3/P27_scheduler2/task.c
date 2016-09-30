/* title: RTOS-Task impl

   Implements <RTOS-Task>

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: task.h
    Header file <RTOS-Task>.

   file: task.c
    Implementation file <RTOS-Task impl>.
*/
#include "konfig.h"
#include "task.h"
#include "sched.h"

// == global variables

task_t* g_task_current;

// == internal types

typedef enum task_iframe_e {
   task_iframe_R0 = 0,
   task_iframe_R1,
   task_iframe_R2,
   task_iframe_R3,
   task_iframe_R12,
   task_iframe_R14,
   task_iframe_LR = task_iframe_R14,
   task_iframe_PC,
   task_iframe_PSR,  // Bit[9] remembers if stack frame has additional alignment
} task_iframe_e;

#define task_iframe_LEN_NOFPU 8
            // Length (number of registers) of interrupt stack frame without used FPU.


// == task_t implementation

void init_task(task_t *task, void (*task_main)(uintptr_t/*task_arg*/), uintptr_t task_arg)
{
      task->sp = &task->stack[lengthof(task->stack)-task_iframe_LEN_NOFPU];
      task->lr = retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_THREADMODE_PSP);
      task->state = task_state_ACTIVE;
      task->sleepms = 0;
      task->wait_for = 0;
      task->next  = 0;
      task->wnext = 0;
      static_assert(task_iframe_R0 == 0);
      static_assert(task_iframe_LR == 5);
      static_assert(task_iframe_PC == 6);
      static_assert(task_iframe_PSR == 7);
      task->sp[task_iframe_R0]  = task_arg;
      task->sp[task_iframe_LR]  = 0xffffffff;   // invalid LR value
      task->sp[task_iframe_PC]  = (uintptr_t) task_main;
      task->sp[task_iframe_PSR] = (1 << 24);    // set thumb bit
}

void sleepms_task(uint32_t millisec)
{
   task_t *task = current_task();
   task->sleepms = millisec;
   __asm volatile ("dmb");
   task->state = task_state_SLEEP;
   yield_task();
}

void wait_taskwait(task_wait_t *wait_for)
{
   task_t *task = current_task();
   task->wait_for = wait_for;
   __asm volatile ("dmb");
   task->state = task_state_WAITFOR;
   yield_task();
}

void signal_taskwait(task_wait_t *wait_for)
{
   increment_atomic(&wait_for->nrevent);
}
