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
typedef struct task_t task_t;

/* == Typen == */
typedef struct task_wait_t task_wait_t;
            // Handles list of waiting tasks and stores number of wake-up events in case of race conditions.
            // The race is between the task adding itself to the list and another task which wants to wake-up a waiting task
            // on some external condition. Changing an external condition and adding to the list could not be made atomic
            // without locking on a multi-core CPU, on a single-core CPU disabling interrupts would do the trick.
            // A task_wait_t supports up to 65535 waiting tasks.

typedef struct task_wakeup_t task_wakeup_t;
            // Handles signalling of task_wait_t.
            // TODO: Rename task_wakeup_t into task_wqueue_t or remove task_wakeup_t

/* == Globals == */

/* == Objekte == */

// task_wait_t

struct task_wait_t {
   uint16_t       nrevent;    // Stores number of times wake-up is called. This prevents race condition.
                              // Tasks trying to block could be preempted before they are added themselves to list of waiting task.
   uint8_t        priority;   // not used
   uint8_t        ceilprio;   // not used
   task_t        *last;       // Points to last entry in a list of waiting task_t.
   uint32_t       nreventiq;  // Stores number of times wake-up is called from interrupt context.
   task_wait_t   *nextiq;     // Valid if (nrventiq != 0).
};

// task_wait_t: lifetime

#define task_wait_INIT  { 0, 0, 0, 0, 0, 0 }
            // Initializes task_wait_t with no waiting tasks and no events occurred.

// task_wait_t: query

static inline int istask_taskwait(const task_wait_t *wait_for);


// task_wakeup_t

struct task_wakeup_t {
   uint8_t        qsize;
   uint8_t        size;
   task_wait_t   *queue[4];
};

// task_wakeup_t: lifetime

static inline void init_taskwakeup(task_wakeup_t *fifo);

// task_wakeup_t: query

static inline bool isdata_taskwakeup(const task_wakeup_t *fifo);
            // Returns true: size() > 0. false: size() == 0.

static inline uint32_t size_taskwakeup(const task_wakeup_t *fifo);
            // Returns nr of items written to queue.

// task_wakeup_t: update

static inline void clear_taskwakeup(task_wakeup_t *fifo);

static inline int write_taskwakeup(task_wakeup_t *fifo, task_wait_t *waitfor);
            // Stores waitfor in task_wakeup_t in fifo order.
            // Returns ENOMEM: Queue is full. 0: waitfor stored successfully.

static inline task_wait_t* read_taskwakeup(task_wakeup_t *fifo, uint32_t i/*0..size()-1*/);
            // Unchecked Precondition: isdata_taskwakeup() == true
            // Reads waitfor from task_wakeup_t queue in fifo order.


/* == Inline == */

static inline int istask_taskwait(const task_wait_t *wait_for)
{
         return wait_for->last != 0;
}

static inline void init_taskwakeup(task_wakeup_t *fifo)
{
   fifo->qsize = lengthof(fifo->queue);
   fifo->size = 0;
}

static inline bool isdata_taskwakeup(const task_wakeup_t *fifo)
{
   return (0 != fifo->size);
}

static inline uint32_t size_taskwakeup(const task_wakeup_t *fifo)
{
   return fifo->size;
}

static inline void clear_taskwakeup(task_wakeup_t *fifo)
{           // external mechanism must ensure that clear not interrupted by writeiq
   fifo->size = 0;
}

static inline int write_taskwakeup(task_wakeup_t *fifo, task_wait_t *waitfor)
{           // called from task, could be interrupted by scheduler interrupt calling read/clear.
   if (fifo->size >= fifo->qsize) {
      return ENOMEM;
   }
   do {
      fifo->queue[fifo->size] = waitfor;
      // TODO: test if interrupted here !!
   } while (0 != swap8_atomic(&fifo->size, fifo->size, fifo->size+1));
   return 0;
}

static inline task_wait_t* read_taskwakeup(task_wakeup_t *fifo, uint32_t i/*0..size()-1*/)
{
   return fifo->queue[i];
}

#endif
