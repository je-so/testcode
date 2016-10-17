/* title: Scheduler

   Implements managing running of multiple tasks on a single CPU.

   Implements strategy for choosing next task to run.

   Tasks share all resources (like Threads in other OS).

   TODO: Support of stack protection!

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: scheduler.h
    Header file <Scheduler>.

   file: scheduler.c
    Implementation file <Scheduler impl>.
*/
#ifndef JRTOS_SCHEDULER_HEADER
#define JRTOS_SCHEDULER_HEADER

/* == Import == */
#include "taskwait.h"
struct task_t;

/* == Typen == */
typedef struct scheduler_t scheduler_t;

/* == Globals == */

/* == Objekte == */
struct scheduler_t;

// scheduler_t: test

#ifdef KONFIG_UNITTEST
int unittest_jrtos_scheduler(void);
#endif

// scheduler_t: lifetime

int init_scheduler(uint32_t nrtask, struct task_t *task/*[nrtask]*/);
            // Takes list of initialized task including the main task.
            // The scheduling is done with coreinterrupt_PENDSV.
            // Its priority is set to lowest so that it does not preempt any other running interrupt handler.

// scheduler_t: task-management

int addtask_scheduler(struct task_t *task);
            // TODO: Replace implementation with version which could not block higher priority tasks.

int reqendtask_scheduler(struct task_t *task, task_queue_t *queue);
            // TODO: Replace implementation with version which could not block higher priority tasks.

int resumetask_scheduler(struct task_t *task, task_queue_t *queue);
            // TODO: Replace implementation with version which could not block higher priority tasks.

int unblocktask_scheduler(task_wait_t *waitfor, task_wakeup_t *wakeup);
            // TODO: Describe signaltask_scheduler

// scheduler_t: internal

void pendsv_interrupt(void);
            // Interrupt used for task switching.

struct task_t* task_scheduler(struct task_t* task/*stopped task*/);
            // Called from pendsv_interrupt to switch to next task.
            // Returns next current task.

// scheduler_t: called-from-task-or-interrupt

static inline void trigger_scheduler();
            // Called from timer-interrupt to trigger task switching interrupt after leaving timer interrupt.

// scheduler_t: called-from-timer

uint32_t periodic_scheduler(uint32_t millisec);
            // Called from timer-interrupt to allow scheduler handle sleeping tasks.
            // Parameter millisec should hold the time passed since the last call, typically value 1 is passed.
            // Must be called with priority higher than scheduler interrupt.
            // returns 0: No task woken up. N>0: N is number of tasks woken up.


/* == Inline == */

static inline void trigger_scheduler()
{
         generate_coreinterrupt(coreinterrupt_PENDSV);
}



#endif
