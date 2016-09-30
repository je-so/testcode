/* title: Task

   Implements managing state of a single task and
   remembers the current running task.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: task.h
    Header file <Task>.

   file: task.c
    Implementation file <Task impl>.
*/
#ifndef JRTOS_TASK_HEADER
#define JRTOS_TASK_HEADER

/* == Import == */
#include "taskwait.h"

/* == Typen == */
typedef uint8_t task_id_t;
            // Task number 0..255 which serves as identifier to reference a task.
typedef void (*task_main_f) (uintptr_t arg);
            // The entry function of a new task.
            // It is not allowed that the function returns, if the task ends it should remove itself from the scheduler.
            // The main task is the initialization task which sets up the whole system and therefore uses the normal main as its entry function.

typedef struct task_t task_t;
            // TODO: task_t

typedef enum task_state_e {
   task_state_ACTIVE,      // task is linked in list of active tasks
   task_state_WAITFOR,     // task is waiting on task_wait_t pointed to by task->wait_for
   task_state_SLEEP,       // task is waiting on sleep list
   task_state_SUSPEND,     // indicates init_task / end_task / suspend_task was called, task completely removed from scheduler
   task_state_END,         // task removed from scheduler
} task_state_e;

typedef enum task_req_e {
   task_req_NONE,
   task_req_END,
} task_req_e;

/* == Globals == */
extern task_t         * g_task_current;
            // Stores the current task and is used to determine the current task.

/* == Objekte == */
struct task_t {
   uint32_t       regs[8]; // saved value of registers r4..r11
   uint32_t      *sp;      // saved value of register psp used as stack pointer in thread mode
   uint32_t       lr;      // saved value of register lr used for return-from-interrupt
   uint8_t        state;   // 0: active. 1: waiting (wait_for is valid). 2: sleeping
   uint8_t        req;     // 0: No change requested. != 0: Request scheduler to change state (end task,...).
   uint8_t        id;      // 0: id of main thread. I!=0: id of other thread
   uint8_t        priority;// 0: highest. ... 7: lowest.
   uint32_t       sleepms; // Number of milliseconds to sleep.
   task_wait_t   *wait_for;// if != 0 then task waits for event on this object
   task_t        *next;    // next task in list of ready (g_task_current, ...) or blocked (task_wait_t/wait_for) tasks
   task_t        *prev;    // prev task in list of ready or blocked tasks
   task_t        *volatile wakeup_next; // next task in wakeup list (scheduler.c: s_wakeup_last)
   uint32_t       _align1[8];
   uint32_t       _protection[8];// mpu uses this to detect stack overflow
   uint32_t       stack[256-32]; // memory used as stack for this task
};

// task_t: test

#ifdef KONFIG_UNITTEST
int unittest_jrtos_task(void);
#endif

// task_t: lifetime

void init_task(task_t *task, uint8_t priority/*0..7*/, task_main_f task_main, uintptr_t task_arg);
            // Initializes task so that task_main is called with task_arg as first argument in register R0.
            // The function task_main is not allowed to return else an exception occurs.

static inline void init_main_task(task_t *task, uint8_t priority/*0..7*/);
            // Same as init_task except that task describes the calling task which is also the main task.
            // This function also sets task as current task.

// task_t: query

static inline task_t* current_task();
            // returns the current task which is running.

static inline uint32_t* initialstack_task(task_t* task);
            // returns the top of stack used to initialize the stack pointer.

// task_t: called-from-current-task

void yield_task();
            // The current task yields the cpu to another task.
            // Internally the scheduler is started to switch to another task.

void sleepms_task(uint32_t millisec);
            // The current task yields the cpu to another task and gets it back only after at least millisec milliseconds have passed.
            // Internally the scheduler is started to switch to another task.

void resume_task(task_t *task);
            // The task is remembered to be added to the list of active tasks the next time the schduler is run.

void suspend_task(void);
            // The current task is stopped from runnning and removed from the list of active tasks.
            // After that it is in a state the same as after initialization.
            // Internally the scheduler is started to remove the task from its list of active tasks and to switch to another task.

void end_task(void);
            // The current task is stopped from runnning and removed from the list of active tasks.
            // Internally the scheduler is started to remove the task from its list of active tasks and to switch to another task.

void wait_task(task_wait_t *wait_for);
            // The current task marks itself as preparing to wait for a condition to occur on wait_for.
            // Internally the scheduler is started to switch to another task and to add this task to wait list in wait_for.
            // The scheduler does not consider this task as runnable until signal_taskwait(wait_for) is called from another thread.

void signal_task(task_wait_t *wait_for);
            // Increments the wake-up event counter of wait_for.
            // The scheduler checks regularly for waiting tasks on wait_for and wakes up as many a number as signal was called.

// task_t: called-from-scheduler

static inline void set_current_task(task_t *current);
            // Remembers current as next current task which is returned by next call to current_task().

/* == Inline == */

static inline void assert_values_task()
{
         static_assert((sizeof(task_t)&(sizeof(task_t)-1)) == 0); // sizeof of structure is power of 2
}

#define yield_task() trigger_scheduler()

#define resume_task(task) reqresumetask_scheduler(task)

static inline task_t* current_task()
{
         return g_task_current;
}

static inline void set_current_task(task_t *current)
{
         g_task_current = current;
}

static inline void init_main_task(task_t *task, uint8_t priority)
{
         init_task(task, priority, 0, 0);
         set_current_task(task);
}

static inline uint32_t* initialstack_task(task_t* task)
{
         return &task->stack[lengthof(task->stack)];
}

#endif
