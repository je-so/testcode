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

/* == Locals == */

static scheduler_t      s_sched;

/* == Forward == */

static inline scheduler_t* remove_task(scheduler_t *sched, task_t *task);
static inline scheduler_t* resume_tasks(scheduler_t *sched, uint32_t resumemask);
static scheduler_t* save_energy(scheduler_t *sched);
static scheduler_t* handle_reqint(scheduler_t *sched);
static scheduler_t* handle_req(scheduler_t *sched, task_t* task, uint32_t req);
static inline scheduler_t* process_taskwakeup(scheduler_t *sched, task_wakeup_t *queue);


/* == scheduler_t Impl == */

void clearbit_scheduler(uint32_t bitmask) // TODO: remove
{
   s_sched.priomask &= ~bitmask;
}

void setbit_scheduler(uint32_t bitmask) // TODO: remove
{
   s_sched.priomask |= bitmask;
}

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

#define PRIOBIT_(pri) (0x80000000 >> (pri))

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

static inline void register_with_scheduler(scheduler_t *sched, task_t *task, uint8_t id)
{
   task->sched = sched;
   task->id = id;
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
         uint32_t bit = PRIOBIT_(task[i].priority);
         if (0 != (priomask & bit)) {
            return EINVAL; /* two task with same priority */
         }
         priomask |= bit;
      }
   }
   if (err) return err;

   err = sys_reset_scheduler();
   if (err) return err;

   s_sched.req32 = 0;
   s_sched.sleepmask = 0;
   s_sched.resumemask = 0;
   s_sched.priomask = priomask;
   for (size_t i = 0; i < lengthof(s_sched.priotask); ++i) {
      s_sched.priotask[i] = 0;
   }
   s_sched.wakeupiq = 0;
   init_idmap(&s_sched, (uint8_t) nrtask);

   for (uint32_t i = 0; i < nrtask; ++i) {
      uint8_t  pri = task[i].priority;
      s_sched.idmap[1+i] = &task[i];
      s_sched.priotask[pri] = &task[i];
      register_with_scheduler(&s_sched, &task[i], (uint8_t) (1+i));
      task[i].state = task_state_ACTIVE;
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
   static_assert(&((task_t*)0)->sched == (scheduler_t**)&((task_t*)0)->sched);
   static_assert(sizeof(((task_t*)0)->sched) == sizeof(uint32_t));
   static_assert(sizeof(s_sched.req32)  == sizeof(uint32_t));
   static_assert(sizeof(s_sched.priomask)  == sizeof(uint32_t));
   static_assert(sizeof(s_sched.priotask[0]) == sizeof(uint32_t));
   static_assert(lengthof(s_sched.priotask) == 33);
   static_assert(sizeof(s_sched.req_int) == sizeof(uint8_t));
   __asm volatile(
      "mrs  r3, psp\n"              // /*r3*/uint32_t psp = get_psp();
      "movw r0, #:lower16:s_sched\n"// scheduler_t *sched;
      "movt r0, #:upper16:s_sched\n"// sched = &s_sched
      "lsrs r1, r3, %[log2TA]\n"    // /*r1*/task_t *task = psp >> scheduler_TASKALIGN;
      "lsls r1, r1, %[log2TA]\n"    // task <<= scheduler_TASKALIGN; /* task == current_task() */
      // TODO: could be used to support more than one scheduler instead of constant assignment!
      // "ldr  r0, [r1, %[sched]]\n"   // scheduler_t *sched = task->sched;
      "ldr  r2, [r0, %[req32]]\n"   // /*r2*/uint32_t req = sched->req32;
      "stm  r1, {r3-r11,r14}\n"     // task->sp = psp, task->regs[0..7] = r4..r11, task->lr = lr
      "cbz  r2, 7f\n"               // if (req != 0) {
      "eors r8, r8\n"               //    /*r8*/const uint32_t zero = 0;
      "lsrs r11, r2, #8\n"          //    /*r11*/uint32_t req24 = req >> 8;
      "str  r8, [r0, %[req32]]\n"   //    sched->req32 = 0/*zero*/;
      "beq  6f\n"                   //    if (req24 == 0) goto CALL_HANDLE_REQ;
      "movs r10, r1\n"              //    /*r10*/task_t *task_save = task;
      "lsrs r4, r11, #8\n"          //    /*r4*/uint32_t req16 = req24 >> 8;
      "and  r9, r2, #0xff\n"        //    /*r9*/uint32_t req8_save = req & 0xff;
      "beq  5f\n"                   //    if (req16 == 0) goto CALL_RESUME_TASKS; /*req_qd_task != 0*/
      "lsrs r4, r2, #24\n"          //    /*r4*/uint32_t req_int = req >> 24;
      "beq  4f\n"                   //    if (req_int == 0) goto CALL_PROCESS_TASKWAKEUP; /*req_qd_wakeup != 0*/
      "bl   handle_reqint\n"        //    r0 = handle_reqint(/*r0*/sched) ;
      "ands r4, r11, 0xff00\n"      //    /*r4/uint32_t req_qd_wakeup = req24 & 0xff00;
      "beq  1f\n"                   //    if (req_qd_wakeup != 0) {
      "4:\n"                        //       CALL_PROCESS_TASKWAKEUP: ;
      "adds r1, r10, %[qd_wakeup]\n"//       r1 = &task_save->qd_wakeup
      "bl  process_taskwakeup\n"    //       r0 = process_taskwakeup(/*r0*/sched, /*r1*/&task->qd_wakeup);
      "1:\n"                        //    }
      "ands r4, r11, 0xff\n"        //    /*r4/uint32_t req_qd_task = req24 & 0xff;
      "beq  1f\n"                   //    if (req_qd_task != 0) {
      "5:\n"                        //       CALL_RESUME_TASKS: ;
      "ldr  r1, [r10, %[qd_task]]\n"//       r1 = task_save->qd_task;
      "str  r8, [r10, %[qd_task]]\n"//       task_save->qd_task = 0/*zero*/;
      "bl   resume_tasks\n"         //       r0 = resume_tasks(/*r0*/sched, /*r1*/task->qd_task);
      "1:\n"                        //    }
      "movs r2, r9\n"               //    /*r2*/req = req8_save;
      "beq  7f\n"                   //    if (req) {
      "movs r1, r10\n"              //       /*r1*/task = task_save;
      "6:\n"                        //       CALL_HANDLE_REQ: ;
      "bl   handle_req\n"           //       r0 = handle_req(/*r0*/sched, /*r1*/task, /*r2*/req)
      "\n"                          //    }
      "\n"                          // }
      "7:\n"                        // RESCHEDULE: ;
      "ldr  r1, [r0, %[priomask]]\n"// /*r1*/uint32_t pri = sched->priomask
      "clz  r1, r1\n"               // pri = __builtin_clz(pri)
      "adds r1, r0, r1, lsl #2\n"   // pri = (uintptr_t)sched + sizeof(uint32_t) * pri
      "ldr  r1, [r1, %[priotask]]\n"// task = sched->priotask[pri]
      "cbnz r1, 1f\n"               // if (!task) {
      "bl   save_energy\n"          //    r0 = save_energy(/*r0*/sched);
      "b    7b\n"                   //    goto RESCHEDULE;
      "1:\n"                        // }
#if (scheduler_STACKPROTECT == 1)
      "movw r3, #0xED90\n"          // r3 = 0xE000ED90 (lower-part)
      "movt r3, #0xE000\n"          // r3 = 0xE000ED90 (upper-part)
      "orrs r2, r1, #(1<<4)\n"      // r2 = (uintptr_t)task | HW_BIT(MPU,RBAR,VALID) | 0;
      "str  r2, [r3, #0x0c]\n"      // hMPU->rbar = (uintptr_t)task | HW_BIT(MPU,RBAR,VALID) | 0;
#endif
      "ldm  r1, {r3-r11,r14}\n"     // r3 = task->sp, r4..r11 = task->regs[0..7], lr = task->lr;
      "msr  psp, r3\n"              // set_psp(task->sp);  ==> (current_task() == task)
      "bx   lr\n"                   // return; /*from interrupt*/
      :: [log2TA] "i" (31 - __builtin_clz(scheduler_TASKALIGN)),
         [sched] "i" (offsetof(task_t, sched)),
         [qd_task] "i" (offsetof(task_t, qd_task)), [qd_wakeup] "i" (offsetof(task_t, qd_wakeup)),
         [priomask] "i" (offsetof(scheduler_t, priomask)), [priotask] "i" (offsetof(scheduler_t, priotask)),
         [req32] "i" (offsetof(scheduler_t, req32)),
         "i" (&save_energy), "i" (&handle_reqint), "i" (&handle_req), "i" (&resume_tasks), "i" (&process_taskwakeup)

       : "memory", "cc"
   );
}

static scheduler_t* save_energy(scheduler_t *sched)
{
   while (0 == sched->priomask) {
      waitevent_core();
      sched->req_int = 0;
      handle_reqint(sched);
   }
   return sched;
}

static inline scheduler_t* remove_task(scheduler_t *sched, task_t *task)
{
   sched->priomask &= ~ task->priobit;
   task->state = task_state_END;
   sched->priotask[task->priority] = 0;
   sched->idmap[task->id] = 0;
   return sched;
}

static inline void activate_task(scheduler_t *sched, task_t *task)
{
   task->state = task_state_ACTIVE;
   sched->priomask |= task->priobit;
}

static inline scheduler_t* resume_tasks(scheduler_t *sched, uint32_t resumemask)
{
   while (resumemask) {
      uint32_t pri = __builtin_clz(resumemask);
      task_t *task = sched->priotask[pri];
      uint32_t priobit = PRIOBIT_(pri);
      resumemask &= ~ priobit;
      if (task && task->state <= task_state_RESUMABLE) {
         activate_task(sched, task);
      }
   }
   return sched;
}

static void process_resumemaskiq(scheduler_t *sched)
{
   uint32_t resumemask = sched->resumemask;
   clearbits_atomic(&sched->resumemask, resumemask);
   resume_tasks(sched, resumemask);
}

static inline void wakeup_taskwait(scheduler_t *sched, task_wait_t *waitfor)
{
   if (waitfor->last) {                // wakeup from task_wait_t
      task_t *last  = waitfor->last;   // remove first task from waiting list
      task_t *first = last->next;
      if (first == last) {
         waitfor->last = 0;
      } else {
         last->next = first->next;
      }
      first->wait_for = 0;
      if (first->req_stop) {
         remove_task(sched, first);
      } else {
         activate_task(sched, first);  // add removed task to ready list
      }
   } else {
      ++ waitfor->nrevent;             // remember event
   }
}

static inline scheduler_t* process_taskwakeup(scheduler_t *sched, task_wakeup_t *queue)
{
   uint32_t size = size_taskwakeup(queue);
   clear_taskwakeup(queue);
   for (uint32_t i = 0; i < size; ++i) {
      wakeup_taskwait(sched, read_taskwakeup(queue, i));
   }

   return sched;
}

static inline scheduler_t* process_wakeupiq(scheduler_t *sched)
{
   task_wait_t *wait;
   task_wait_t *next;
   uint32_t nrevent;

   if (sched->wakeupiq) {
      do {                          // read list start and clear it
         wait = sched->wakeupiq;
      } while (0 != swap_atomic((void**)&s_sched.wakeupiq, wait, 0));

      for (; wait; wait = next) {   // process whole list
         next = wait->nextiq;       // get next in list
         sw_msync();
         static_assert(sizeof(wait->nreventiq) == sizeof(void*));
         do {                       // read number of wakeup events
            nrevent = wait->nreventiq;
         } while (0 != swap_atomic((void**)&wait->nreventiq, (void*)nrevent, 0));
         for (; nrevent; --nrevent) {
            wakeup_taskwait(sched, wait);
         }
      }
   }
   return sched;
}

static scheduler_t* handle_reqint(scheduler_t *sched)
{
   if (sched->resumemask) process_resumemaskiq(sched);
   process_wakeupiq(sched);
   return sched;
}

static scheduler_t* handle_req(scheduler_t *sched, task_t* task, uint32_t req)
{
   switch (req) {
   default:                { }
                           break;
   case task_req_END:      {
                              remove_task(sched, task);
                           }
                           break;
   case task_req_SUSPEND:  {
                              sched->priomask &= ~ task->priobit;
                              task->state = task_state_SUSPEND;
                           }
                           break;
   case task_req_RESUME:   if (task->req_task->state <= task_state_RESUMABLE) {
                              activate_task(sched, task->req_task);
                           }
                           break;
   case task_req_SLEEP:    {
                              sched->priomask  &= ~ task->priobit;
                              task->state = task_state_SLEEP;     // state of task checked in periodic_scheduler
                              rw_msync();
                              sched->sleepmask |= task->priobit;  // could be interrupted by periodic_scheduler which clears these bits
                           }
                           break;
   case task_req_WAITFOR:  {
                              if (task->wait_for->nrevent) {      // already woken-up ?
                                 -- task->wait_for->nrevent;
                              } else {                            // append task as last to list of waiting tasks
                                 sched->priomask &= ~ task->priobit;
                                 task->state = task_state_WAITFOR;
                                 task_t *last = task->wait_for->last;
                                 if (last == 0) {
                                    task->next = task;
                                 } else {
                                    task->next = last->next;
                                    last->next = task;
                                 }
                                 task->wait_for->last = task;
                              }
                           }
                           break;
   case task_req_WAKEUP:   {
                              wakeup_taskwait(sched, task->wait_for);
                           }
                           break;
   case task_req_STOP:     if (task->req_task->state <= task_state_RESUMABLE) {
                              remove_task(sched, task->req_task);
                           } else if (task->req_task->state == task_state_WAITFOR) {
                              task->req_task->req_stop = task_req_STOP;
                           }
                           break;
   }

   return sched;
}

static inline void resumeiq_tasks(uint32_t taskmask)
{           // iq: callable also from interrupt context
   setbits_atomic(&s_sched.resumemask, taskmask);
   s_sched.req_int = 1;
}

uint32_t periodic_scheduler(uint32_t millisec)
{
   // : caller makes sure priority is higher than scheduler priority : ELSE :
   // core_priority_e oldprio = getpriority_core();
   // setprioritymax_core(scheduler_PRIORITY);

   uint32_t sleepmask = s_sched.sleepmask;
   uint32_t resumemask = 0;
   uint32_t clearmask = 0;
   uint32_t wokenup_count = 0;

   while (sleepmask) {
      uint32_t pri = __builtin_clz(sleepmask);
      uint32_t bit = PRIOBIT_(pri);
      sleepmask &= ~ bit;
      task_t *task = s_sched.priotask[pri];
      if (task->state != task_state_SLEEP) {
         clearmask |= bit;
      } else if (task->sleepms > millisec) {
         task->sleepms -= millisec;
      } else {
         task->sleepms = 0;
         resumemask |= bit;
         ++ wokenup_count;
      }
   }

   clearmask |= resumemask;
   if (clearmask) {
      s_sched.sleepmask &= ~clearmask;   // if pendsv_interrupt was interrupted then this could result in a no-op
   }
   if (resumemask) {
      resumeiq_tasks(resumemask);
   }
   // setpriority_core(oldprio);
   return wokenup_count;
}

int addtask_scheduler(task_t *task)
{
   if (! is_init_task(task)) {
      return EINVAL;
   }

   for (uint32_t rep = 0; rep < 2; ++rep) {
      for (uint32_t id = s_sched.freeid; id < lengthof(s_sched.idmap); ++id) {
         static_assert(sizeof(s_sched.idmap[0]) == sizeof(void*));
         if (0 == s_sched.idmap[id] && 0 == swap_atomic((void**)&s_sched.idmap[id], 0, task)) {
            static_assert(sizeof(s_sched.priotask[0]) == sizeof(void*));
            if (0 != swap_atomic((void**)&s_sched.priotask[task->priority], 0, task)) {
               s_sched.idmap[id] = 0;
               s_sched.freeid = id;
               return EALREADY;  // priority is in use by some other task
            }
            s_sched.freeid = id + 1;
            register_with_scheduler(&s_sched, task, id);
            resumeiq_tasks(task->priobit);
            return 0;
         }
      }
      s_sched.freeid = 2;
   }

   return ENOMEM; // no more free id value
}

void wakeupiq_scheduler(task_wait_t *waitfor)
{
   if (1 == increment_atomic(&waitfor->nreventiq)) {
      static_assert(sizeof(s_sched.wakeupiq) == sizeof(void*));
      do {
         waitfor->nextiq = s_sched.wakeupiq;
      } while (0 != swap_atomic((void**)&s_sched.wakeupiq, waitfor->nextiq, waitfor));
   } else {
      // already stored in list s_sched.wakeupiq
   }
   s_sched.req_int = 1;
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
