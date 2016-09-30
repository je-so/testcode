/* title: RTOS-Scheduler

   Implements managing running of multiple tasks
   on a single CPU.

   Implements strategy for choosing next task to run.

   Tasks share all resources (like Threads in other OS).

   TODO: Support of stack protection!

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: sched.h
    Header file <RTOS-Scheduler>.

   file: sched.c
    Implementation file <RTOS-Scheduler impl>.
*/
#ifndef CORTEXM4_SCHEDULER_HEADER
#define CORTEXM4_SCHEDULER_HEADER

// imported types
struct task_t;

// == Exported Types


// == exported objects

struct sched_t;

// group: lifetime

int init_sched(uint32_t current/*index into task array*/, uint32_t nrtask, struct task_t *task/*[nrtask]*/);
            // Takes list of task and current is an index into task array to identify the current running task which calls init.
            // The priority of coreinterrupt_PENDSV is set to lowest so that it does not preempt any other running interrupt handler.

// group: internal

void pendsv_interrupt(void);
            // Interrupt used for task switching.

struct task_t* switchtask_sched(/*out*/struct task_t** current, struct task_t* stopped);
            // Called from pendsv_interrupt to determine next current thread.
            // Sets *current to next current task and returns *current.

// group: called-from-task

static inline void yield_sched();
            // The current task yields the cpu to another task.
            // Internally the scheduler is started to switch to another task.

void unblock_sched(void *wait_for_object);
            // Searches a task waiting for wait_for_object in a blocked state and unblocks it.

// group: called-from-timer

static inline void trigger_sched();
            // Called from timer-interrupt to trigger task switching interrupt after leaving timer interrupt.

void periodic_sched(uint32_t millisec);
            // Called from timer-interrupt to allow scheduler handle sleeping tasks.
            // Parameter millisec should hold the time passed since the last call, typically the value 1.


// == Inline Implementation

static inline void yield_sched()
{
         generate_coreinterrupt(coreinterrupt_PENDSV);
}

static inline void trigger_sched()
{
         generate_coreinterrupt(coreinterrupt_PENDSV);
}

#endif
