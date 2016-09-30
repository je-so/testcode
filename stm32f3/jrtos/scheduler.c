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
#define scheduler_STACKPROTECT_ALIGN 256

/* == Types == */

struct scheduler_t {
   uint32_t    freeid;
   task_t     *idmap[256];
};

/* == Locals == */

static task_t  *s_wakeup_last;
static task_t  *s_sleep_last;
static uint32_t s_priomask;      // forevery 0<=i<=7: ((s_priomask&(1<<i)) != 0) == (s_task_next[i] != 0)
static task_t  *s_task_next[8];  // for every priority (0..7) a separate list
static scheduler_t s_sched;

/* == scheduler_t Impl == */

static inline int sys_init_scheduler(task_t *main_task/*0: No stack protection*/)
{
   int err;
   static_assert(scheduler_PRIORITY != interrupt_priority_MAX);
   static_assert(offsetof(task_t, _protection) == 3*32/*3rd subregion of 256 byte region (begin counting from 0)*/);

   if (main_task) {
      mpu_region_t region = mpu_region_INITRAM(main_task, mpu_size_256, (uint8_t)~(1<<3), mpu_access_READ, mpu_access_READ);
      err = config_mpu(1, &region, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE);
      if (err) return err;
   }

   setpriority_coreinterrupt(coreinterrupt_PENDSV, scheduler_PRIORITY);

   return 0;
}

static inline int is_task_valid(const task_t *task)
{
   return   task->priority < lengthof(s_task_next)
            && task->state == task_state_SUSPEND
            && task->next  == 0
            && task->prev  == 0
            && 0 == ((uintptr_t)task & ((scheduler_STACKPROTECT ? scheduler_STACKPROTECT_ALIGN : 4)-1));
}

void init_idmap(scheduler_t *sched)
{
   sched->freeid = 1;
   sched->idmap[0] = 0;
   sched->idmap[lengthof(sched->idmap)-1] = 0;
   for (size_t i = 1; i < lengthof(sched->idmap)-1; ++i) {
      sched->idmap[i] = (task_t*) (uintptr_t) (i + 1);
   }
}

int init_scheduler(uint32_t nrtask, task_t task[nrtask])
{
   int err = EINVAL;
   task_t *main_task = current_task();

   for (uint32_t i = 0; i < nrtask; ++i) {
      if (&task[i] == main_task) {
         err = 0;
      }
      if (! is_task_valid(&task[i])) {
         return EINVAL;
      }
   }
   if (err) return err;

   err = sys_init_scheduler(scheduler_STACKPROTECT ? main_task : 0);
   if (err) return err;

   s_wakeup_last = 0;
   s_sleep_last = 0;
   s_priomask = 0;
   for (size_t i = 0; i < lengthof(s_task_next); ++i) {
      s_task_next[i] = 0;
   }
   init_idmap(&s_sched);

   uint32_t id = 1;
   for (uint32_t i = 0; i < nrtask; ++i) {
      uint32_t nextid = &task[i] == main_task ? 0 : id++;
      uint8_t  pri    = task[i].priority;
      task_t  *last   = s_task_next[pri] ? s_task_next[pri] : &task[i]; // last in priority list
      task_t  *first  = s_task_next[pri] ? s_task_next[pri]->next : &task[i]; // first in priority list
      s_priomask |= (1 << pri);
      s_sched.idmap[nextid] = &task[i];
      s_task_next[pri] = &task[i];
      task[i].state = task_state_ACTIVE;
      task[i].id   = (uint8_t) nextid;
      task[i].next = first;
      task[i].prev = last;
      first->prev  = &task[i];
      last->next   = &task[i];
   }

   s_sched.freeid = id;
   s_task_next[main_task->priority] = main_task->next;

   return 0;
}

__attribute__ ((naked))
void pendsv_interrupt(void)
{
   __asm volatile(
      "movw r0, :lower16:g_task_current\n"  // r0 = (&g_task_current & 0x0000ffff);
      "movt r0, :upper16:g_task_current\n"  // r0 = (&g_task_current & 0xffff0000) | (r0 & 0x0000ffff);
      "ldr  r1, [r0]\n"          // r1 = g_task_current
      "mrs  r12, psp\n"          // r12 = psp
      "stm  r1, {r4-r12,r14}\n"  // save r4..r11 into g_task_current->regs, g_task_current->sp = r12, g_task_current->lr = lr
      "bl   task_scheduler\n"    // r0 = task_scheduler(/*r0*/&g_task_current, /*r1*/g_task_current)
#ifdef scheduler_STACKPROTECT
      "movw r1, #0xED90\n"       // r0 = 0xE000ED90 (lower-part)
      "movt r1, #0xE000\n"       // r0 = 0xE000ED90 (upper-part)
      "orrs r2, r0, #(1<<4)\n"   // r2 = g_task_current | HW_BIT(MPU,RBAR,VALID) | 0;
      "str  r2, [r1, #0x0c]\n"   // hMPU->rbar = (uintptr_t)g_task_current | HW_BIT(MPU,RBAR,VALID) | 0;
#endif
      "ldm  r0, {r4-r12,r14}\n"  // restore r4..r11 from g_task_current->regs, r12 = g_task_current->sp, lr = g_task_current->lr;
      "msr  psp, r12\n"          // psp = g_task_current->sp;
      "bx   lr\n"                // return from interrupt
   );
}

static inline void add_ready_list(task_t *task, bool asFirst)
{
   uint8_t pri = task->priority;
   task->state = task_state_ACTIVE;
   if (s_task_next[pri]) {                // add to ready list after/before next running task
      task_t *prev, *next;
      if (asFirst) {
         prev = s_task_next[pri];
         next = prev->next;
      } else {
         next = s_task_next[pri];
         prev = next->prev;
      }
      task->prev = prev;
      task->next = next;
      prev->next = task;
      next->prev = task;
   } else {                               // single entry in ready list
      task->next = task;
      task->prev = task;
      s_task_next[pri] = task;
      s_priomask |= (1 << pri);
   }
}

static inline void wakeup_from_waitlist(task_wait_t *wait_for)
{
   assert(wait_for);
   while (wait_for->nrevent && wait_for->last) {
      decrement_atomic(&wait_for->nrevent);     // one woken-up task therefore decrement wake-up counter by one
      task_t *last  = wait_for->last;           // remove first task from waiting list
      task_t *first = last->next;
      if (first == last) {
         wait_for->last = 0;
      } else {
         last->next = first->next;
      }
      first->wait_for = 0;
      add_ready_list(first, true);              // add removed task to ready list
   }
}

static inline void wakeup_from_sleep(task_t *task)
{
   task_t *next = task->next;    // remove task from sleep list
   task_t *prev = task->prev;
   prev->next = next;
   next->prev = prev;
   if (s_sleep_last == task) {
      s_sleep_last = (next == task ? 0 : next);
   }

   add_ready_list(task, true);   // add removed task to ready list
}

static void process_wakeup(void)
{
   task_t *task;

   do {
      task = s_wakeup_last;
      if (!task) return;
   } while (0 != swap_atomic((volatile void **)&s_wakeup_last, task, 0));

   for (;;) {
      if (task->state == task_state_WAITFOR) {
         wakeup_from_waitlist(task->wait_for);
      } else if (task->state == task_state_SLEEP) {
         wakeup_from_sleep(task);
      } else if (task->state == task_state_SUSPEND) {
         add_ready_list(task, true);
      }
      task_t *next = task->wakeup_next;
      task->wakeup_next = 0;
      if (task == next) break;
      task = next;
   }
}

static inline void add_wakeup(task_t *task)
{
   if (0 == swap_atomic((volatile void **)&task->wakeup_next, 0, task)) {
      task_t *old;
      do {
         old = s_wakeup_last;
         task->wakeup_next = old ? old : task;  // wakeup_next != 0 ==> task stored in wakeup list
      } while (0 != swap_atomic((volatile void **)&s_wakeup_last, old, task));
   }
}

task_t* task_scheduler(/*out*/task_t** current, task_t* task)
{
   extern volatile uint32_t s_ints;  // TODO: remove
   ++ s_ints;

   if (task->state != task_state_ACTIVE) {

RESCHEDULE: ;

      task_t *next = task->next;                // remove from ready list
      task_t *prev = task->prev;
      uint8_t pri = task->priority;
      prev->next = next;
      next->prev = prev;
      if (s_task_next[pri] == task) {
         if (next == task) {
            s_task_next[pri] = 0;
            s_priomask &= ~ (1 << pri);
         } else {
            s_task_next[pri] = next;
         }
      }

      if (task->state == task_state_WAITFOR) {
         assert(task->wait_for);                // append task to list of waiting tasks as last
         task_t *last = task->wait_for->last;
         if (last == 0) {
            task->next = task;
            if (task->wait_for->nrevent) add_wakeup(task);
         } else {
            task->next = last->next;
            last->next = task;
         }
         task->wait_for->last = task;

      } else if (task->state == task_state_SLEEP) {
         if (s_sleep_last) {                    // append task to list of sleeping tasks as first
            task_t *first = s_sleep_last->next;
            task->next = first;
            task->prev = s_sleep_last;
            s_sleep_last->next = task;
            first->prev = task;
         } else {
            task->next = task;
            task->prev = task;
            s_sleep_last = task;
         }

      } else {                                  // only remove task from list of active tasks
         task->next = 0;
         task->prev = 0;
         if (next->req == task_req_END) {
            task->state = task_state_END;       // prev-state: task_state_SUSPEND || task_state_ACTIVE
         } else {
            assert(task->state == task_state_SUSPEND);
         }
      }
   }

   process_wakeup();
   while (0 == s_priomask) {
      waitevent_core();
      process_wakeup();
   }

   static_assert(lengthof(s_task_next) == 8);
   uint32_t pri = 0;
   uint32_t mask = s_priomask;
   if (0 == (mask&0x0f)) {
      mask >>= 4;
      pri += 4;
   }
   if (0 == (mask&0x03)) {
      mask >>= 2;
      pri += 2;
   }
   if (0 == (mask&0x01)) {
      pri += 1;
   }

   assert(s_task_next[pri]);

   task_t *next = s_task_next[pri];
   if (next == task) {              // in case next already run
      next = next->next;            // try next if process_wakeup added a new task
   }
   s_task_next[pri] = next->next;   // round robin scheduler within list of task of same priority

   assert(next->state == task_state_ACTIVE);
   if (next->req) {
      task = next;
      goto RESCHEDULE;
   }

   *current = next;
   return next;
}

void preparewakeup_scheduler(task_wait_t *wait_for)
{
   core_priority_e oldprio = getpriority_core();
   setprioritymax_core(scheduler_PRIORITY);

   if (istask_taskwait(wait_for)) {
      add_wakeup(wait_for->last);
   }

   setpriority_core(oldprio);
}

uint32_t periodic_scheduler(uint32_t millisec)
{
   // : caller makes sure priority is higher than scheduler priority : ELSE :
   // core_priority_e oldprio = getpriority_core();
   // setprioritymax_core(scheduler_PRIORITY);

   task_t *const last = s_sleep_last;

   uint32_t wokenup_count = 0;

   if (last) {
      task_t *task = last;
      do {
         task_t *next = task->next;
         if (task->sleepms > millisec) {
            task->sleepms -= millisec;
         } else {
            task->sleepms = 0;
            add_wakeup(task);
            ++ wokenup_count;
         }
         task = next;
      } while (task != last);
   }

   // setpriority_core(oldprio);
   return wokenup_count;
}

int addtask_scheduler(task_t *task)
{
   if (! task || ! is_task_valid(task)) {
      return EINVAL;
   }

   add_wakeup(task);
   return 0;
}

void reqendtask_scheduler(task_t *task)
{
   task->req = task_req_END;
   add_wakeup(task);    // terminate sleeping (TODO: waitfor is not terminated cause of implementation limits)
}

void reqresumetask_scheduler(struct task_t *task)
{
   if (task && is_task_valid(task)) {
      add_wakeup(task);
   }
}


#ifdef KONFIG_UNITTEST

static void clear_ram(volatile uint32_t * ram, size_t size)
{
   size /= 4;
   for (size_t i = 0; i < size; ++i) {
      ram[i] = 0x12345678;
   }
}

int unittest_jrtos_scheduler()
{
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;
   task_t   * const task   = (task_t*) CCMRAM;
   size_t     const nrtask = CCMRAM_SIZE / sizeof(task_t);

   // prepare
   static_assert(scheduler_PRIORITY != 0);
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   setprio0mask_interrupt();
   clear_ram(CCMRAM, CCMRAM_SIZE);

   // TEST init_scheduler: EINVAL
   s_wakeup_last = (void*)1;
   TEST( EINVAL   == init_scheduler(nrtask, task)); // check everything unchanged
   TEST( (void*)1 == s_wakeup_last);
   for (uint32_t i = 0; i < nrtask; ++i) {
      TEST( (void*) 0x12345678 == task[i].next);
   }
   TEST( 0 == current_task());
   TEST( 0 == getpriority_coreinterrupt(coreinterrupt_PENDSV));

   // TEST init_scheduler: task array contains current_task
   for (volatile uint8_t p = 0; p < (uint8_t)lengthof(s_task_next); ++p) {
      for (volatile uint32_t nr = 1; nr <= nrtask; ++nr) {
         for (volatile uint32_t mi = 0; mi < nr; ++mi) {
            // prepare
            for (uint32_t i = 0; i < nr; ++i) {
               init_task(&task[i], p, 0, 0);
            }
            init_main_task(&task[mi], p);
            TEST( &task[mi] == current_task());       // set by init_main_task
            s_wakeup_last = (void*)1;
            s_sleep_last =  (void*)1;
            s_priomask = 0xff;
            for (uint32_t i = 0; i < lengthof(s_task_next); ++i) {
               s_task_next[i] = (void*)1;
            }
            // test
            TEST( 0 == init_scheduler(nr, task));
            TEST( nr == s_sched.freeid);
            TEST( 0 == s_wakeup_last);
            TEST( 0 == s_sleep_last);
            TEST( 1<<p == s_priomask);
            for (size_t i = 0; i < lengthof(s_task_next); ++i) {
               TEST( s_task_next[i] == (i == p ? task[mi].next : 0));
            }
            for (size_t i = nr; i < lengthof(s_sched.idmap); ++i) {
               TEST( s_sched.idmap[i] == (task_t*) (i + 1 == lengthof(s_sched.idmap) ? 0 : (uintptr_t)(i+1)));
            }
            for (uint32_t i = 0, id = 1; i < nr; ++i) {
               TEST( task[i].state == task_state_ACTIVE);
               TEST( task[i].id    == (i == mi ? 0 : id++));
               TEST( task[i].next == &task[i+1 < nr ? i+1 : 0]);
               TEST( task[i].prev == &task[i ? i-1 : nr-1]);
               TEST( s_sched.idmap[task[i].id] == &task[i]);
            }
            TEST( &task[mi] == current_task());
            TEST( scheduler_PRIORITY == getpriority_coreinterrupt(coreinterrupt_PENDSV));
            // reset
            setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
            disable_mpu();
         }
      }
   }

   // TEST init_scheduler: single task
   for (uint8_t p = 0; p < (uint8_t)lengthof(s_task_next); ++p) {
      clear_ram(CCMRAM, CCMRAM_SIZE);
      init_main_task(&task[0], p);
      TEST( 0 == init_scheduler(1, task));
      TEST( s_task_next[p] == &task[0]);
      TEST( task[0].next == &task[0]);
      TEST( task[0].prev == &task[0]);
      // reset
      setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
      disable_mpu();
   }

   // TEST trigger_scheduler
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   trigger_scheduler();
   TEST( 1 == is_coreinterrupt(coreinterrupt_PENDSV));
   clear_coreinterrupt(coreinterrupt_PENDSV);
   wait_msync();
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());

   // TEST pendsv_interrupt
   trigger_scheduler();
   uint32_t _pc;
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
      "push    {r4-r11}\n" // write wrong values onto stack
      "pop     {r4-r11}\n"
      "cpsie I\n"
      "nop\n"
      "1: cpsid I\n"
      "pop    {r0-r12, lr}\n"
      "adr    %0, 1b\n"
      : "=r" (_pc) :: "cc"
   );
   TEST( 1 == task[0].sp[0]); // r0
   TEST( 2 == task[0].sp[1]); // r1
   TEST( 3 == task[0].sp[2]); // r2
   TEST( 4 == task[0].sp[3]); // r3
   TEST( 5 == task[0].sp[4]); // r12
   TEST( 6 == task[0].sp[5]); // lr
   TEST( _pc + 4 >= task[0].sp[6]);    // pc
   TEST( _pc - 4 <= task[0].sp[6]);    // pc
   TEST( 0xf9000000 == task[0].sp[7]); // xpsr + thumb-bit

   // TEST task_scheduler
   // TODO:

   // TEST preparewakeup_scheduler
   // TODO:

   // TEST periodic_scheduler
   // TODO:

   // TEST pendsv_scheduler

   // reset
   set_current_task(0);
   TEST( 0 == is_any_interrupt());
   TEST( 0 == is_any_coreinterrupt());
   clearprio0mask_interrupt();

   return 0;
}

#endif
