/* title: RTOS-Scheduler impl

   Implements <RTOS-Scheduler>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: sched.h
    Header file <RTOS-Scheduler>.

   file: sched.c
    Implementation file <RTOS-Scheduler impl>.
*/
#include "konfig.h"
#include "sched.h"
#include "task.h"

int init_sched(uint32_t current/*index into task array*/, uint32_t nrtask, task_t task[nrtask])
{
   if (current >= nrtask) {
      return EINVAL;
   }

   for (uint32_t i = 0; i < nrtask; ++i) {
      task[i].next = &task[(i+1)%nrtask];
   }

   set_current_task(&task[current]);
   setpriority_coreinterrupt(coreinterrupt_PENDSV, interrupt_priority_MIN);

   return 0;
}

__attribute__ ((naked))
void pendsv_interrupt(void)
{
   __asm volatile(
      "movw r0, :lower16:g_task_current\n"  // r0 = (uint16_t) &g_task_current;
      "movt r0, :upper16:g_task_current\n"  // r0 |= &g_task_current << 16;
      "ldr  r1, [r0]\n"          // r1 = g_task_current
      "mrs  r12, psp\n"          // r12 = psp
      "stm  r1, {r4-r12,r14}\n"  // save r4..r11 into s_current_task->regs, s_current_task->sp = r12, s_current_task->lr = lr
      "bl   switchtask_sched\n"  // r0 = switchtask_sched(/*r0*/&g_task_current, /*r1*/g_task_current)
      "ldm  r0, {r4-r12,r14}\n"  // restore r4..r11 from g_task_current->regs, r12 = g_task_current->sp, lr = g_task_current->lr;
      "msr  psp, r12\n"          // psp = g_task_current->sp;
      "bx   lr\n"                // return from interrupt
   );
}

task_t* switchtask_sched(/*out*/task_t** current, task_t* stopped)
{
   task_t* task = stopped;

   if (task->state == task_state_WAITFOR) {
      if (task->wait_for->nrevent > 0 && task->wait_for->last == 0) {
         // no other waiting task and wake-up already occurred ==> consume event and mark task as ready to run
         decrement_atomic(&task->wait_for->nrevent);
         task->state = task_state_ACTIVE;
         task->wait_for = 0;

      } else {
         // append task to waiting task list of task->wait_for
         task_t *last = task->wait_for->last;
         if (last == 0) {
            task->wnext = task;
         } else {
            task->wnext = last->wnext;
            last->wnext = task;
         }
         task->wait_for->last = task;
      }
   }

   // round robin scheduler
   task = task->next;

   while (task->state != task_state_ACTIVE) {
      if (task->state == task_state_WAITFOR && task->wait_for->nrevent) {
         decrement_atomic(&task->wait_for->nrevent);  // one woken-up task therefore decrement wake-up counter by one
         assert(task->wait_for->last);          // at least one task is stored in this list (task)
         task_t *last  = task->wait_for->last;  // remove first task from waiting list
         task_t *first = last->wnext;
         if (first == last) {
            task->wait_for->last = 0;
         } else {
            last->wnext = first->wnext;
         }
         first->wnext = 0;
         first->state = task_state_ACTIVE;      // mark task as runnable
         first->wait_for = 0;
      } else {
         task = task->next;
      }
   }

   *current = task;
   return task;
}

void periodic_sched(uint32_t millisec)
{
   task_t * const first = current_task();
   task_t *       task  = first;

   if (first != 0) {
      do {
         if (task->state == task_state_SLEEP) {
            if (task->sleepms > millisec) {
               task->sleepms -= millisec;
            } else {
               task->sleepms = 0;
               task->state = task_state_ACTIVE;
            }
         }
         task = task->next;
      } while (task != first);
   }
}
