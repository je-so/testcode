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
   task_state_SLEEP,       // task is waiting on sleep list
   task_state_SUSPEND,     // indicates init_task / end_task / suspend_task was called, task completely removed from scheduler
   task_state_WAITFOR,     // task is waiting on task_wait_t pointed to by task->wait_for
   task_state_END,         // task removed from scheduler

#define task_state_RESUMABLE    task_state_SUSPEND
} task_state_e;


typedef enum task_req_e {
   task_req_NONE,
   task_req_END,
   task_req_SUSPEND,
   task_req_RESUME,
   task_req_SLEEP,
   task_req_WAITFOR,
   task_req_WAKEUP,
} task_req_e;

/* == Globals == */

/* == Objekte == */
struct task_t {
   uint32_t      *sp;      // saved value of register psp used as stack pointer in thread mode
   uint32_t       regs[8]; // saved value of registers r4..r11
   uint32_t       lr;      // saved value of register lr used for return-from-interrupt
   uint8_t        state;   // 0: active. 1: waiting (wait_for is valid). 2: sleeping
   uint8_t        req;     // 0: No change requested. != 0: Request scheduler to change state (end task,...).
   uint8_t        id;      // 0: id of main thread. I!=0: id of other thread
   uint8_t        priority;// 0: highest. ... 31: lowest.
   uint32_t       priobit; // 0x80000000 >> priority
   union {
   task_wait_t   *wait_for;// (state == task_state_WAITFOR) ==> task waits for event on this object
   task_t        *req_task;
   uint32_t       sleepms; // Number of milliseconds to sleep.
   };
   task_t        *next;    // next task in a list of tasks (task_wait_t)
   task_wakeup_t  wakeup;           // TODO: Describe, remove or rename into wqueue ?
   task_queue_t   queue;            // TODO: Describe, rename into queue ?
   uint32_t       _align1[2];
   uint32_t       _protection[8];   // mpu uses this to detect stack overflow
   uint32_t       stack[256-32-1];  // memory used as stack for this task
   uint32_t       topstack;         // not used for stack
};

// task_t: test

#ifdef KONFIG_UNITTEST
int unittest_jrtos_task(void);
#endif

// task_t: lifetime

void init_task(task_t *task, uint8_t priority/*0..7*/, task_main_f task_main, uintptr_t task_arg);
            // Initializes task so that task_main is called with task_arg as first argument in register R0.
            // The function task_main is not allowed to return else an exception occurs.

void init_main_task(task_t *task, uint8_t priority/*0..7*/);
            // Initializes calling task which is also the main task.

// task_t: query

static inline task_t* current_task();
            // returns the current task which is running.

static inline uint32_t* initialstack_task(task_t* task);
            // returns the top of stack used to initialize the stack pointer.

// task_t: update-current-task

void yield_task();
            // The current task yields the cpu to another task.
            // Internally the scheduler is started to switch to another task.

void sleepms_task(uint32_t millisec);
            // The current task yields the cpu to another task and gets it back only after at least millisec milliseconds have passed.
            // Internally the scheduler is started to switch to another task.

void suspend_task(void);
            // The current task is stopped from runnning and removed from the list of active tasks.
            // After that it is in a state the same as after initialization.
            // Internally the scheduler is started to remove the task from its list of active tasks and to switch to another task.

void end_task(void);
            // The current task is stopped from runnning and removed from the list of active tasks.
            // Internally the scheduler is started to remove the task from its list of active tasks and to switch to another task.

void wait_task(task_wait_t *waitfor);
            // The current task marks itself as preparing to wait for a condition to occur on wait_for.
            // Internally the scheduler is started to switch to another task and to add this task to wait list in wait_for.
            // The scheduler does not consider this task as runnable until signal_taskwait(wait_for) is called from another thread.

// task_t: update-other-task

void resume_task(task_t *task);
            // The current task yields the cpu and task is resumed if in a sleeping or suspended state.

void queueresume_task(task_t *task);
            // The task is remembered to be resumed the next time the schduler is run.

void wakeup_task(task_wait_t *waitfor);
            // The current task yields the cpu and task is woken-up if in a blocking (waitfor) state.

void queuewakeup_task(task_wait_t *waitfor);
            // The blocking condition waitfor is remembered to be signaled the next time the schduler is run.
            // The scheduler wakes up a waiting task on waitfor or else increments the wake-up event counter of waitfor.
            // Internally a queue is used to store waitfor into.
            // This queue is added to an internal update-list of the scheduler.


/* == Inline == */

static inline void assert_values_task(void)
{
         static_assert(0 == (sizeof(task_t)&(sizeof(task_t)-1)) && "sizeof of structure is power of 2");
}

#define yield_task()          trigger_scheduler()

#define queueresume_task(task) queueresume_scheduler(task, &current_task()->queue)

#define queuewakeup_task(waitfor)  queuewakeup_scheduler(waitfor, &current_task()->wakeup)


static inline task_t* current_task(void)
{
         task_t *task;
         __asm volatile(
            "mrs %0, psp\n"      // task = psp
            "bfc %0, #0, %1\n"   // task = psp & ~(sizeof(task_t)-1)
            : "=&r" (task) : "i" (31 - __builtin_clz(sizeof(task_t)))
         );
         return task;
}

static inline uint32_t* initialstack_task(task_t* task)
{
         return &task->topstack;
}

#endif
