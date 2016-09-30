/* title: Task-Waiting-Support

   Manages tasks in a list waiting for events.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: taskwait.h
    Header file <Task-Waiting-Support>.

   file: taskwait.c
    Implementation file <Task-Waiting-Support impl>.
*/
#ifndef JRTOS_TASKWAIT_HEADER
#define JRTOS_TASKWAIT_HEADER

/* == Import == */
typedef uint8_t task_id_t;
typedef struct task_t task_t;

/* == Typen == */
typedef struct task_wait_t task_wait_t;
            // Handles list of waiting tasks and stores number of wake-up events in case of race conditions.
            // The race is between the task adding itself to the list and another task which wants to wake-up a waiting task
            // on some external condition. Changing an external condition and adding to the list could not be made atomic
            // without locking on a multi-core CPU, on a single-core CPU disabling interrupts would do the trick.
            // A task_wait_t supports up to 65535 waiting tasks.

typedef struct task_wakeup_t task_wakeup_t;
            // Handles list of task_wait_t with at least one task waiting for wakeup (task_wait_t.nrevent > 0 and task_wait_t.last).

/* == Globals == */

/* == Objekte == */
struct task_wait_t {
   volatile uint16_t nrevent;    // Stores number of times wake-up is called. This prevents race condition. Tasks trying to block could be preempted before they are added themselves to list of waiting task.
   uint8_t           priority;   // not used
   uint8_t           ceilprio;   // not used
   task_t           *last;       // Points to last in a list of waiting task_t.
};

// task_wait_t: lifetime

#define task_wait_INIT  { 0, 0, 0, 0 }
            // Initializes task_wait_t with no waiting tasks and no events occurred.

// task_wait_t: query

static inline int istask_taskwait(const task_wait_t *wait_for);


/* == Inline == */

static inline int istask_taskwait(const task_wait_t *wait_for)
{
         return wait_for->last != 0;
}

#endif
