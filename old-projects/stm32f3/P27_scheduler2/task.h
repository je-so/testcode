/* title: RTOS-Task

   Implements managing state of a single task and
   remembers the current running task.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: task.h
    Header file <RTOS-Task>.

   file: task.c
    Implementation file <RTOS-Task impl>.
*/
#ifndef CORTEXM4_TASK_HEADER
#define CORTEXM4_TASK_HEADER

// == exported types
struct task_t;
struct task_waitfor_t;

typedef enum task_state_e {
   task_state_ACTIVE,
   task_state_WAITFOR,
   task_state_SLEEP,
} task_state_e;

// == global variables
extern struct task_t* g_task_current;

// == exported objects
typedef struct task_wait_t {
   volatile uint32_t nrevent;
   struct task_t    *last;       // last locked if (1 == ((uintptr_t)last & 1)) else unlocked (currently no locking needed)
} task_wait_t;

typedef struct task_t {
   uint32_t       regs[8]; // saved value of registers r4..r11
   uint32_t      *sp;      // saved value of register psp used as stack pointer in thread mode
   uint32_t       lr;      // saved value of register lr used for return-from-interrupt
   uint32_t       state;   // 0: active. 1: waiting (wait_for is valid). 2: sleeping
   uint32_t       sleepms; // Number of milliseconds to sleep.
   task_wait_t   *wait_for;// if != 0 then task waits for event on this object
   struct task_t *next;    // next task in task list
   struct task_t *wnext;   // next task in task_wait_t list
   uint32_t       stack[256-15]; // memory used as stack for this task
} task_t;

// == lifetime

#define task_wait_INIT  { 0, 0 }
            // Initializes task_wait_t with no waiting tasks and no events occurred.

void init_task(task_t *task, void (*task_main)(uintptr_t/*task_arg*/), uintptr_t task_arg);
            // Initializes task so that task_main is called with task_arg as first argument in register R0.
            // The function task_main is not allowed to return else an exception occurs.

// == query

static inline task_t* current_task();
            // returns the current task which is running or active

// == called-from-current-task

void yield_task();
            // The current task yields the cpu to another task.
            // Internally the scheduler is started to switch to another task.

void sleepms_task(uint32_t millisec);
            // The current task yields the cpu to another task and gets it back only after at least millisec milliseconds have passed.
            // Internally the scheduler is started to switch to another task.

void wait_taskwait(task_wait_t *wait_for);
            // The current task marks itself as preparing to wait for a condition to occur on wait_for.
            // Internally the scheduler is started to switch to another task and to add this task to wait list in wait_for.
            // The scheduler does not consider this task as runnable until signal_taskwait(wait_for) is called from another thread.

void signal_taskwait(task_wait_t *wait_for);
            // Increments the wake-up event counter of wait_for.
            // The scheduler checks regularly for waiting tasks on wait_for and wakes up as many a number as signal was called.

// == called-from-scheduler

static inline void set_current_task(task_t *current);
            // Remembers current as next current task which is returned by next call to current_task().


// == Inline Implementation

static inline void assert_values_task()
{
         static_assert((sizeof(task_t)&(sizeof(task_t)-1)) == 0); // sizeof of structure is power of 2
}

#define yield_task() \
         yield_sched()

static inline task_t* current_task()
{
         return g_task_current;
}

static inline void set_current_task(task_t *current)
{
         g_task_current = current;
}

#endif
