/* title: Scheduler impl

   Implements <Scheduler>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: scheduler.h
    Header file <Scheduler>.

   file: scheduler.c
    Implementation file <Scheduler impl>.
*/
#include "konfig.h"
#include "scheduler.h"
#include "task.h"

/* == Konfig == */

#define scheduler_PRIORITY       interrupt_priority_MIN
#define scheduler_STACKPROTECT   1  // 1: protection of stack enabled 0: Disabled
#define scheduler_TASKALIGN      sizeof(task_t)

/* == Types == */

struct scheduler_t {
   uint32_t             freeid;
   task_t              *idmap[32];
   uint32_t             sleepmask;     // TODO:
   uint32_t   volatile  wakeupmask;    // TODO:
   uint32_t             priomask;      // TODO:
   task_t              *priotask[32];  // 32 priorities but with only 8 different priority groups:
                                       // for-all 0<=i<=7: all tasks in group priotask[i*4:i*4+3] are scheduled round robin.
};

/* == Locals == */

static task_queue_t *s_queue_last;        // list of updated task_queue_t
static task_wakeup_t*s_wakeup_last;       // list of updated task_wakeup_t
static scheduler_t   s_sched;

/* == scheduler_t Impl == */

static inline int sys_reset_scheduler(void)
{
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   disable_mpu();
   return 0;
}

static inline int sys_init_scheduler(task_t *main_task/*0: No stack protection*/)
{
   int err;
   static_assert(scheduler_PRIORITY != interrupt_priority_MAX);
   static_assert(offsetof(task_t, _protection) == 3*32/*3rd subregion of 256 byte region (begin counting from 0)*/);
   static_assert(scheduler_TASKALIGN >= 256 && "minimum page size of MPU is 256 if subregions are in use");

   if (main_task) {
      mpu_region_t region = mpu_region_INITRAM(main_task, mpu_size_256, (uint8_t)~(1<<3), mpu_access_READ, mpu_access_READ);
      err = config_mpu(1, &region, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE);
      if (err) return err;
   }

   setpriority_coreinterrupt(coreinterrupt_PENDSV, scheduler_PRIORITY);

   return 0;
}

#define PRIOMASK_BIT(pri) (0x80000000 >> (pri))

static inline int is_init_task(const task_t *task)
{
   return   task != 0
            && task->priority < lengthof(s_sched.priotask)
            && task->state == task_state_SUSPEND
            && task->id    == 0
            && task->next  == 0
            && 0 == ((uintptr_t)task & (scheduler_TASKALIGN-1));
}

static inline int is_valid_task(const task_t *task)
{
   return   task != 0
            && task->priority < lengthof(s_sched.priotask)
            && task->id < lengthof(s_sched.idmap)
            && s_sched.idmap[task->id] == task
            && 0 == ((uintptr_t)task & (scheduler_TASKALIGN-1));
}

void init_idmap(scheduler_t *sched, uint8_t nrtask)
{
   s_sched.freeid = (uint8_t) (nrtask+1);
   for (uint32_t i = 0; i < lengthof(sched->idmap); ++i) {
      sched->idmap[i] = 0;
   }
}

int init_scheduler(uint32_t nrtask, task_t task[nrtask])
{
   int err = EINVAL;
   uint32_t priomask = 0;
   task_t *main_task = current_task();

   if (nrtask < lengthof(s_sched.idmap)) {
      for (uint32_t i = 0; i < nrtask; ++i) {
         if (&task[i] == main_task) {
            err = 0;
         }
         if (! is_init_task(&task[i])) {
            return EINVAL;
         }
         uint32_t bit = PRIOMASK_BIT(task[i].priority);
         if (0 != (priomask & bit)) {
            return EINVAL;
         }
         priomask |= bit;
      }
   }
   if (err) return err;

   err = sys_reset_scheduler();
   if (err) return err;

   s_queue_last = 0;
   s_wakeup_last = 0;
   init_idmap(&s_sched, (uint8_t) nrtask);
   s_sched.sleepmask = 0;
   s_sched.wakeupmask = 0;
   s_sched.priomask = priomask;
   for (size_t i = 0; i < lengthof(s_sched.priotask); ++i) {
      s_sched.priotask[i] = 0;
   }

   for (uint32_t i = 0; i < nrtask; ++i) {
      uint8_t  pri = task[i].priority;
      s_sched.idmap[1+i] = &task[i];
      s_sched.priotask[pri] = &task[i];
      task[i].state = task_state_ACTIVE;
      task[i].id   = (uint8_t) (1+i);
   }

   if (main_task->id != 1) {
      s_sched.idmap[main_task->id] = s_sched.idmap[1];
      s_sched.idmap[1]->id = main_task->id;
      s_sched.idmap[1] = main_task;
      main_task->id = 1;
   }

   err = sys_init_scheduler(scheduler_STACKPROTECT ? main_task : 0);
   if (err) return err;

   return 0;
}

__attribute__ ((naked))
void pendsv_interrupt(void)
{
   __asm volatile(
      "mrs  r3, psp\n"           // r3 = psp
      "lsrs r0, r3, %0\n"        // r0 = psp / scheduler_TASKALIGN
      "lsls r0, r0, %0\n"        // r0 = psp & ~(scheduler_TASKALIGN-1) ==> r0 == current_task()
      "stm  r0, {r3-r11,r14}\n"  // current_task()->sp = r3, save r4..r11 into current_task()->regs, current_task()->lr = lr
      "bl   task_scheduler\n"    // r0 = task_scheduler(/*r0*/current_task())
#ifdef scheduler_STACKPROTECT
      "movw r1, #0xED90\n"       // r1 = 0xE000ED90 (lower-part)
      "movt r1, #0xE000\n"       // r1 = 0xE000ED90 (upper-part)
      "orrs r2, r0, #(1<<4)\n"   // r2 = r0 | HW_BIT(MPU,RBAR,VALID) | 0;
      "str  r2, [r1, #0x0c]\n"   // hMPU->rbar = (uintptr_t)r0 | HW_BIT(MPU,RBAR,VALID) | 0;
#endif
      "ldm  r0, {r3-r11,r14}\n"  // r3 = r0->sp, restore r4..r11 from r0->regs, lr = r0->lr;
      "msr  psp, r3\n"           // psp = r0->sp;  ==> (current_task() == r0)
      "bx   lr\n"                // return from interrupt
      :: "i" (31 - __builtin_clz(scheduler_TASKALIGN))
   );
}

static inline void add_ready_list(task_t *task, uint32_t priobit)
{
   task->state = task_state_ACTIVE;
   s_sched.priomask |= priobit;
}

static inline void wakeup_from_sleep(task_t *task, uint32_t priobit)
{
   s_sched.sleepmask &= ~priobit;
   add_ready_list(task, priobit);
}

static void process_wakeupmask(void)
{
   uint32_t wakeupmask = s_sched.wakeupmask;
   clearbits_atomic(&s_sched.wakeupmask, wakeupmask);

   while (wakeupmask) {
      uint32_t pri = __builtin_clz(wakeupmask);
      uint32_t priobit = PRIOMASK_BIT(pri);
      wakeupmask &= ~ priobit;
      task_t *task = s_sched.priotask[pri];
      if (task && task->state <= task_state_BOUNDARY_COULD_BE_WOKENUP) {
         task->state = task_state_ACTIVE;
         s_sched.priomask |= priobit;
         s_sched.sleepmask &= ~priobit;
      }
   }
}

static inline bool process_taskqueue(task_queue_t *queue)
{
   if (!isdata_taskqueue(queue)) return false;

   do {   // process whole queue
      task_t *task = read_taskqueue(queue);
      uint32_t priobit = task->priobit;
      if (task->state <= task_state_BOUNDARY_COULD_BE_WOKENUP) {
         task->state = task_state_ACTIVE;
         s_sched.priomask |= priobit;
         s_sched.sleepmask &= ~priobit;
      }
   } while (isdata_taskqueue(queue));

   return true;
}

static void process_queuelist(void)
{
   task_queue_t *queue = s_queue_last;

   process_taskqueue(queue);

   task_queue_t *prev = queue;
   queue = queue->next;

   while (queue) {

      task_queue_t *next = queue->next;

      if (queue->keep > 1) {           // decrement keep
         -- queue->keep;
         prev = queue;
      } else {                         // remove queue from update list if keep expires
         prev->next = next;
         queue->keep = 0;
         rw_msync();
      }

      if (process_taskqueue(queue) && queue->keep) {
         queue->keep = 3;
      }

      queue = next;
   }

}

static inline void wakeup_taskwait(task_wait_t *waitfor)
{
   if (waitfor->last) {                // wakeup from task_wait_t
      task_t *last  = waitfor->last;   // remove first task from waiting list
      task_t *first = last->next;
      if (first == last) {
         waitfor->last = 0;
      } else {
         last->next = first->next;
      }
      uint32_t bit = first->priobit;
      first->wait_for = 0;
      add_ready_list(first, bit);      // add removed task to ready list
   } else {
      ++ waitfor->nrevent;             // remember event
   }
}

static inline bool process_wakeupqueue(task_wakeup_t *queue)
{
   if (!isdata_taskwakeup(queue)) return false;

   do {   // process whole queue
      wakeup_taskwait(read_taskwakeup(queue));
   } while (isdata_taskwakeup(queue));

   return true;
}

static void process_wakeuplist(void)
{
   task_wakeup_t *queue = s_wakeup_last;

   (void) process_wakeupqueue(queue);

   task_wakeup_t *prev = queue;
   queue = queue->next;

   while (queue) {

      task_wakeup_t *next = queue->next;

      if (queue->keep > 1) {           // decrement keep
         -- queue->keep;
         prev = queue;
      } else {                         // remove queue from update list if keep expires
         prev->next = next;
         queue->keep = 0;
         rw_msync();
      }

      if (process_wakeupqueue(queue) && queue->keep) {
         queue->keep = 3;
      }

      queue = next;
   }
}

static inline void process_wakeup(void)
{
   if (s_wakeup_last) process_wakeuplist();
   if (s_queue_last) process_queuelist();
   if (s_sched.wakeupmask) process_wakeupmask();
}

task_t* task_scheduler(task_t* task)
{
   if (task->state != task_state_ACTIVE) {

RESCHEDULE: ;
      uint32_t bit = task->priobit;
      s_sched.priomask &= ~ bit;

      if (task->state == task_state_SUSPEND) {  // only remove task from list of active tasks
         if (task->req == task_req_END) {
            uint8_t pri = __builtin_clz(bit);
            task->state = task_state_END;       // prev-state: task_state_SUSPEND || task_state_ACTIVE
            s_sched.priotask[pri] = 0;
            s_sched.idmap[task->id] = 0;
         }

      } else if (task->state == task_state_WAITFOR) {
         assert(task->wait_for);
         if (task->wait_for->nrevent) {      // already woken-up ?
            -- task->wait_for->nrevent;
            add_ready_list(task, bit);
         } else {                            // append task to list of waiting tasks as last
            task_t *last = task->wait_for->last;
            if (last == 0) {
               task->next = task;
            } else {
               task->next = last->next;
               last->next = task;
            }
            task->wait_for->last = task;
         }

      } else {
         assert(task->state == task_state_SLEEP);
         s_sched.sleepmask |= bit;
      }
   }

   process_wakeup();
   while (0 == s_sched.priomask) {
      waitevent_core();
      process_wakeup();
   }

   uint32_t pri = __builtin_clz(s_sched.priomask);
   task_t *next = s_sched.priotask[pri];

   assert(next);

   if (next->req == task_req_END) {
      task = next;
      task->state = task_state_SUSPEND;
      goto RESCHEDULE;
   }

   return next;
}

uint32_t periodic_scheduler(uint32_t millisec)
{
   // : caller makes sure priority is higher than scheduler priority : ELSE :
   // core_priority_e oldprio = getpriority_core();
   // setprioritymax_core(scheduler_PRIORITY);

   uint32_t sleepmask = s_sched.sleepmask;
   uint32_t wakeupmask = 0;
   uint32_t wokenup_count = 0;

   while (sleepmask) {
      uint32_t pri = __builtin_clz(sleepmask);
      uint32_t bit = PRIOMASK_BIT(pri);
      sleepmask &= ~ bit;
      task_t *task = s_sched.priotask[pri];
      if (task->sleepms > millisec) {
         task->sleepms -= millisec;
      } else if (task->sleepms) {
         task->sleepms = 0;
         wakeupmask |= bit;
         ++ wokenup_count;
      }
   }

   if (wakeupmask) {
      setbits_atomic(&s_sched.wakeupmask, wakeupmask);
   }
   // setpriority_core(oldprio);
   return wokenup_count;
}

int addtask_scheduler(task_t *task)
{
   if (! is_init_task(task)) {
      return EINVAL;
   }

   for (uint32_t rep = 0, id = s_sched.freeid; rep < 2; ++rep) {
      for (; id < lengthof(s_sched.idmap); ++id) {
         static_assert(sizeof(s_sched.idmap[0]) == sizeof(void*));
         if (0 == s_sched.idmap[id] && 0 == swap_atomic((void**)&s_sched.idmap[id], 0, task)) {
            static_assert(sizeof(s_sched.priotask[0]) == sizeof(void*));
            if (0 != swap_atomic((void**)&s_sched.priotask[task->priority], 0, task)) {
               s_sched.idmap[id] = 0;
               s_sched.freeid = id;
               return EALREADY;
            }
            task->id = (uint8_t) id;
            s_sched.freeid = id + 1;
            uint32_t bit = PRIOMASK_BIT(task->priority);
            setbits_atomic(&s_sched.wakeupmask, bit);
            return 0;
         }
      }
      s_sched.freeid = 2;
      id = 2;
   }

   return ENOMEM; // no more free id value
}

int reqendtask_scheduler(task_t *task, task_queue_t *queue)
{
   task->req = task_req_END;
   return resumetask_scheduler(task, queue);
}

int resumetask_scheduler(struct task_t *task, task_queue_t *queue)
{
   // Design decision: Every task must own its own task_queue_t queue
   // therefore no protection is necessary and blocking another task is also not possible
   if (0 == write_taskqueue(queue, task)) {  // use queue
      rw_msync();
      if (queue->keep == 0) {
         queue->keep = 3;
         task_queue_t *old;
         do {
            old = s_queue_last;
            queue->next = old;
         } while (0 != swap_atomic((void **)&s_queue_last, old, queue));
      }
   } else {                                  // use priomask
      setbits_atomic(&s_sched.wakeupmask, task->priobit);
   }

   return 0;
}

int unblocktask_scheduler(task_wait_t *waitfor, task_wakeup_t *wakeup)
{
   // Design decision: Every task must own its own task_wakeup_t queue
   // therefore no protection is necessary and blocking another task is also not possible
   while (0 != write_taskwakeup(wakeup, waitfor)) {      // use queue
      yield_task();
   }
   rw_msync();
   if (wakeup->keep == 0) {
      wakeup->keep = 3;
      task_wakeup_t *old;
      do {
         old = s_wakeup_last;
         wakeup->next = old;
      } while (0 != swap_atomic((void **)&s_wakeup_last, old, wakeup));
   }

   return 0;
}


#ifdef KONFIG_UNITTEST

static void clear_ram(volatile uint32_t * ram, size_t size)
{
   size /= 4;
   for (size_t i = 0; i < size; ++i) {
      ram[i] = 0x12345678;
   }
}

#define INIT_VARIABLE \
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START; \
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE; \
   task_t   * const task   = (task_t*) CCMRAM; \
   size_t     const nrtask = CCMRAM_SIZE / sizeof(task_t)


static void test_switched_main(void)
{
   INIT_VARIABLE ;
   volatile uint32_t mi;

   for (mi = 0; mi < nrtask; ++mi) {
      if (&task[mi] == current_task()) break;
   }
   TEST(mi < nrtask);

   // TEST init_scheduler: task array contains current_task
   for (volatile uint8_t p = 0; p < (uint8_t)lengthof(s_sched.priotask); ++p) {
      for (volatile uint32_t nr = 1; nr <= nrtask; ++nr) {
         if (mi >= nr) continue;

         // prepare
         for (uint32_t i = 0; i < nr; ++i) {
            if (i == mi) {
               init_main_task(&task[mi], p);
            } else {
               init_task(&task[i], p, 0, 0);
            }
         }
         // test
         TEST( 0 == init_scheduler(nr, task));
         TEST( nr == s_sched.freeid-1);
         TEST( (0x80000000>>p) == s_sched.priomask);
         for (size_t i = 0; i < lengthof(s_sched.priotask); ++i) {
            TEST( s_sched.priotask[i] == (i == p ? task[mi].next : 0));
         }
         for (size_t i = 0; i < lengthof(s_sched.idmap); ++i) {
            if (1 <= i && i <= nrtask) continue;
            TEST( 0 == s_sched.idmap[i]);
         }
         for (uint32_t i = 0; i < nr; ++i) {
            TEST( task[i].state == task_state_ACTIVE);
            TEST( task[i].id   == (i == mi ? 1 : i ? i+1 : mi+1));
            TEST( task[i].next == &task[i+1 < nr ? i+1 : 0]);
            TEST( &task[i]     == s_sched.idmap[task[i].id]);
         }
         TEST( &task[mi] == current_task());
         TEST( scheduler_PRIORITY == getpriority_coreinterrupt(coreinterrupt_PENDSV));

         // reset
         setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
         disable_mpu();
      }
   }

   // TEST pendsv_interrupt
   trigger_scheduler();
   init_main_task(&task[mi], 0);
   TEST( 0 == init_scheduler(1, &task[mi]));
   uint32_t _pc;
   uint32_t _sp;
   __asm volatile (
      "push    {r0-r12, lr}\n"
      "movs    r0, #1\n"
      "movs    r1, #2\n"
      "movs    r2, #3\n"
      "movs    r3, #4\n"
      "movs    r12, #5\n"
      "movs    lr, #6\n"
      "movs    r4, #0xf8000000\n"
      "msr     apsr_nzcvq, r4\n"
      "mov     r4, #7\n"
      "mov     r5, #8\n"
      "mov     r6, #9\n"
      "mov     r7, #10\n"
      "mov     r8, #11\n"
      "mov     r9, #12\n"
      "mov     r10, #13\n"
      "mov     r11, #14\n"
      "cpsie I\n"          // allow interrupts
      "nop\n"              // scheduler interrupt is run
      "1: cpsid I\n"       // disable interrupts
      "pop     {r0-r12, lr}\n"
      "mrs     %1, psp\n"
      "sub     %1, #(14+8)*4\n"  // r0-r12,lr + additional iframe 8 reg (+ possible alignment)
      "adr     %0, 1b\n"
      : "=r" (_pc), "=r" (_sp) :: "cc"
   );
   TEST( _sp == (uint32_t) task[mi].sp || _sp - 4 == (uint32_t) task[mi].sp);
   TEST( 1 == task[mi].sp[0]); // r0
   TEST( 2 == task[mi].sp[1]); // r1
   TEST( 3 == task[mi].sp[2]); // r2
   TEST( 4 == task[mi].sp[3]); // r3
   TEST( 5 == task[mi].sp[4]); // r12
   TEST( 6 == task[mi].sp[5]); // lr
   TEST( _pc + 4 >= task[mi].sp[6]);    // pc
   TEST( _pc - 4 <= task[mi].sp[6]);    // pc
   TEST( 0xf9000000 == (task[mi].sp[7] & ~iframe_flag_PSR_PADDING)); // xpsr + thumb-bit
   // reset
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   disable_mpu();
}

int unittest_jrtos_scheduler()
{
   INIT_VARIABLE ;
   task_t   * const oldmain = current_task();

   // prepare
   static_assert(scheduler_PRIORITY != 0);
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   setprio0mask_interrupt();
   clear_ram(CCMRAM, CCMRAM_SIZE);

   // TEST trigger_scheduler
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   trigger_scheduler();
   TEST( 1 == is_coreinterrupt(coreinterrupt_PENDSV));
   clear_coreinterrupt(coreinterrupt_PENDSV);
   wait_msync();
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());

   // TEST init_scheduler: EINVAL
   TEST( EINVAL   == init_scheduler(nrtask, task)); // check everything unchanged
   for (uint32_t i = 0; i < nrtask; ++i) {
      TEST( (void*) 0x12345678 == task[i].next);
   }
   TEST( oldmain == current_task());
   TEST( 0 == getpriority_coreinterrupt(coreinterrupt_PENDSV));

#define SWITCH_MAIN_TASK(task, test_fct) \
      __asm volatile(               \
         "mrs  r0, psp\n"           \
         "msr  psp, %0\n"           \
         "push {r0-r3,r12,lr}\n"    \
         "blx  %1\n"                \
         "pop  {r0-r3,r12,lr}\n"    \
         "msr  psp, r0\n"           \
         :: "r" (initialstack_task(task)), "r" (test_fct) : "r0" \
      )

   // test_switched_main: &task[mi] == current_task()
   for (size_t mi = 0; mi < nrtask; ++mi) {
      clear_ram(CCMRAM, CCMRAM_SIZE);
      SWITCH_MAIN_TASK(&task[mi], &test_switched_main);
   }

   // TEST task_scheduler
   // TODO:

   // TEST addwakeup_scheduler
   // TODO:

   // TEST periodic_scheduler
   // TODO:

   // TEST pendsv_scheduler

   // reset
   TEST(oldmain == current_task());
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   clearprio0mask_interrupt();

   return 0;
}

#endif
