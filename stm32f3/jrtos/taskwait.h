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
            // Handles signalling of task_wait_t.
            // TODO: Rename task_wakeup_t into task_wqueue_t or remove task_wakeup_t

typedef struct task_queue_t task_queue_t;
            // Handles wakeup of task_t.
            // TODO: Combine with task_wakeup_t and remove task_wakeup_t

/* == Globals == */

/* == Objekte == */

// task_wait_t

struct task_wait_t {
   uint16_t       nrevent;    // Stores number of times wake-up is called. This prevents race condition. Tasks trying to block could be preempted before they are added themselves to list of waiting task.
   uint8_t        priority;   // not used
   uint8_t        ceilprio;   // not used
   task_t        *last;       // Points to last entry in a list of waiting task_t.
};

// task_wait_t: lifetime

#define task_wait_INIT  { 0, 0, 0, 0 }
            // Initializes task_wait_t with no waiting tasks and no events occurred.

// task_wait_t: query

static inline int istask_taskwait(const task_wait_t *wait_for);


// task_wakeup_t

struct task_wakeup_t {
   task_wait_t *queue[2];
   task_wakeup_t *next; // list of updated task_wakeup_t
   uint8_t  keep;       // number of rounds object will be kept in update list.
                        // ==0: Not stored in list, next not in use.
                        // !=0: Stored in listm next in use, even if value is 0.
   uint8_t  szm1;
   uint8_t  rpos;
   uint8_t  wpos;
};

// task_wakeup_t: lifetime

static inline void init_taskwakeup(task_wakeup_t *fifo);

// task_wakeup_t: query

static inline bool isdata_taskwakeup(const task_wakeup_t *fifo);

// task_wakeup_t: update

static inline int write_taskwakeup(task_wakeup_t *fifo, task_wait_t *waitfor);
            // Stores waitfor in task_wakeup_t in fifo order.
            // Returns ENOMEM: Queue is full. 0: waitfor stored successfully.

static inline task_wait_t* read_taskwakeup(task_wakeup_t *fifo);
            // Unchecked Precondition: isdata_taskwakeup() == true
            // Reads waitfor from task_wakeup_t queue in fifo order.


// task_queue_t

struct task_queue_t {
   task_t        *queue[2];
   task_queue_t  *next; // list of updated task_queue_t
   uint8_t  keep;       // number of rounds object will be kept in update list.
                        // ==0: Not stored in list, next not in use.
                        // !=0: Stored in listm next in use, even if value is 0.
   uint8_t  szm1;
   uint8_t  rpos;
   uint8_t  wpos;
};

// task_queue_t: lifetime

static inline void init_taskqueue(task_queue_t *fifo);

// task_queue_t: query

static inline bool isdata_taskqueue(const task_queue_t *fifo);

// task_queue_t: update

static inline int write_taskqueue(task_queue_t *fifo, task_t* task);
            // Stores task in task_queue_t in fifo order.
            // Returns ENOMEM: Queue is full. 0: task stored successfully.

static inline task_t* read_taskqueue(task_queue_t *fifo);
            // Unchecked Precondition: isdata_taskqueue() == true
            // Reads task from task_queue_t in fifo order.


/* == Inline == */

static inline int istask_taskwait(const task_wait_t *wait_for)
{
         return wait_for->last != 0;
}

static inline void init_taskwakeup(task_wakeup_t *fifo)
{
   fifo->next = 0;
   fifo->keep = 0;
   fifo->szm1 = lengthof(fifo->queue)-1;
   fifo->rpos = 0;
   fifo->wpos = 0;
   assert( 0 == (fifo->szm1&(fifo->szm1+1)));
}

static inline bool isdata_taskwakeup(const task_wakeup_t *fifo)
{
   uint8_t rpos = fifo->rpos;
   uint8_t wpos = fifo->wpos;
   return (rpos != wpos);
}

static inline int write_taskwakeup(task_wakeup_t *fifo, task_wait_t *waitfor)
{
   uint8_t rpos = fifo->rpos;
   uint8_t wpos = fifo->wpos;
   if ((uint8_t)(wpos - rpos) > fifo->szm1) { // TODO: test
      return ENOMEM;
   }
   fifo->queue[wpos & fifo->szm1] = waitfor;
   rw_msync();
   fifo->wpos = (uint8_t) (wpos+1);
   return 0;
}

static inline task_wait_t* read_taskwakeup(task_wakeup_t *fifo)
{
   uint8_t rpos = fifo->rpos;
   task_wait_t *waitfor = fifo->queue[rpos & fifo->szm1];
   rw_msync();
   fifo->rpos = (uint8_t) (rpos+1);
   return waitfor;
}

static inline void init_taskqueue(task_queue_t *fifo)
{
   fifo->next = 0;
   fifo->keep = 0;
   fifo->szm1 = lengthof(fifo->queue)-1;
   fifo->rpos = 0;
   fifo->wpos = 0;
   assert( 0 == (fifo->szm1&(fifo->szm1+1)));
}

static inline bool isdata_taskqueue(const task_queue_t *fifo)
{
   uint8_t rpos = fifo->rpos;
   uint8_t wpos = fifo->wpos;
   return (rpos != wpos);
}

static inline int write_taskqueue(task_queue_t *fifo, task_t *task)
{
   uint8_t rpos = fifo->rpos;
   uint8_t wpos = fifo->wpos;
   if ((uint8_t)(wpos - rpos) > fifo->szm1) {	// TODO: test
      return ENOMEM;
   }
   fifo->queue[wpos & fifo->szm1] = task;
   rw_msync();
   fifo->wpos = (uint8_t) (wpos+1);
   return 0;
}

static inline task_t* read_taskqueue(task_queue_t *fifo)
{
   uint8_t rpos = fifo->rpos;
   task_t *task = fifo->queue[rpos & fifo->szm1];
   rw_msync();
   fifo->rpos = (uint8_t) (rpos+1);
   return task;
}


#endif
