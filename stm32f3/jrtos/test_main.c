#include "konfig.h"
#include "task.h"
#include "scheduler.h"
#include "semaphore.h"
#include "fifo.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXNR)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINNR)

/* used by scheduler */

task_t _Alignas(sizeof(task_t))  g_task[3];

/* used in tasks */
semaphore_t    sem1;
fifo_t         fifo1;

// #define MEASURE_SPEED

/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;

__attribute__ ((naked))
void *memset(void *s/*r0*/, int c/*r1*/, size_t n/*r2*/)
{
   __asm volatile(
      "adds    r3, r0, r2\n"  // r3 = (uint8_t*)s + n
      "lsrs    r2, #2\n"
      "beq     4f\n"
      "lsls    r1, #24\n"
      "orrs    r1, r1, r1, lsr #8\n"
      "orrs    r1, r1, r1, lsr #16\n"
      "1: subs r2, #1\n"
      "str     r1, [r0, r2, lsl #2]\n"
      "bne     1b\n"
      "4: and  r2, r3, #3\n"
      "adr     r12, 5f+1\n"   // +1 to set Thumb-bit
      "subs    r2, r12, r2, lsl #2\n"
      "bx      r2\n"
      "strb    r1, [r3, #-1]!\n"
      "strb    r1, [r3, #-1]!\n"
      "strb    r1, [r3, #-1]!\n"
      "5: bx   lr\n"
   );
}

void assert_failed_exception(const char *_filename, int _linenr)
{
   filename = _filename;
   linenr   = _linenr;
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
      write_gpio(LED_PORT, LED_MAXPIN, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void switch_led(void)
{
   static uint32_t lednr1 = 0;
   static uint32_t lednr2 = 0;
   static uint32_t counter1 = 0;
   static uint32_t counter2 = 0;
   uint16_t off = GPIO_PIN(8 + lednr2) | GPIO_PIN(8 + lednr1);
   static_assert(LED_PINS == GPIO_PINS(15,8));
   counter1 = (counter1 + 1) % 2;
   counter2 = (counter2 + 1) % 3;
   lednr1 = (lednr1 + (counter1 == 0)) % 8;
   lednr2 = (lednr2 + (counter2 == 0)) % 8;
   write_gpio(LED_PORT, GPIO_PIN(8 + lednr1) | GPIO_PIN(8 + lednr2), off);
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 140000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
   }
}

void fault_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void nmi_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

volatile uint32_t s_timems;   // incremented in systick_interrupt
volatile uint32_t s_10ms;     // incremented in systick_interrupt, if 10 reached scheduler is triggered and value is reset to 0
volatile uint32_t s_cycles1;  // counts number of loops cycles of task1 (if MEASURE_SPEED is defined)
volatile uint32_t s_cycles2;  // counts number of loops cycles of task2 (if MEASURE_SPEED is defined)

void systick_interrupt(void)
{
   ++ s_timems;
#ifdef MEASURE_SPEED
   stop_systick();
   // signaliq_semaphore(&sem1);
#else
   if (periodic_scheduler(1)) {
      trigger_scheduler();
   }
   if (++s_10ms == 10) {
      s_10ms = 0;
      trigger_scheduler();
   }
#endif
}

static void task_main(uintptr_t id/*0..3*/)
{
   static volatile uint32_t s_count;
   static uint32_t s_task_nr[lengthof(g_task)];

   // g_task[0]._protection[0] = 0;
   // g_task[1]._protection[0] = 0;

#if 0
   // use cooperative scheduling
   if (id == 0) {
      while (0 == s_timems) {    // task1+2 : 112 cycles
         ++ s_cycles1;
         clearbit_scheduler(g_task[0].priobit); // if removed ==> task1 only: 88 cycles
         trigger_scheduler();
      }
      return;
   } else {
      while (0 == s_timems) {
         ++ s_cycles2;
         setbit_scheduler(g_task[0].priobit);
         trigger_scheduler();
      }
      setbit_scheduler(g_task[0].priobit);
      end_task();
   }
#endif

#if 0
   static task_wakeup_t s_wakequeue;

   // use cooperative fifo
   if (id == 0) {
      init_taskwakeup(&s_wakequeue);
      while (0 == s_timems) { // task1+2: 189 cycles
         ++ s_cycles1;
         assert(0 == write_taskwakeup(&s_wakequeue, (task_wait_t*)0));
         assert(0 == write_taskwakeup(&s_wakequeue, (task_wait_t*)0));
         assert(0 == write_taskwakeup(&s_wakequeue, (task_wait_t*)0));
         assert(0 == write_taskwakeup(&s_wakequeue, (task_wait_t*)0));
         clearbit_scheduler(g_task[0].priobit);
         trigger_scheduler();
      }
      return;
   } else {
      while (0 == s_timems) {
         ++ s_cycles2;
         assert(4 == size_taskwakeup(&s_wakequeue));
         assert(0 == read_taskwakeup(&s_wakequeue, 0));
         assert(0 == read_taskwakeup(&s_wakequeue, 1));
         assert(0 == read_taskwakeup(&s_wakequeue, 2));
         assert(0 == read_taskwakeup(&s_wakequeue, 3));
         clear_taskwakeup(&s_wakequeue);
         setbit_scheduler(g_task[0].priobit);
         trigger_scheduler();
      }
      setbit_scheduler(g_task[0].priobit);
      end_task();
   }
#endif

#if 0
   // use semaphore 1
   if (id == 0) {
#define V1
      while (0 == s_timems) {
         ++ s_cycles1;
         wait_semaphore(&sem1);
      }
      return ;
   } else {
      while (0 == s_timems) {
         ++ s_cycles2;
#ifdef V1
         signal_semaphore(&sem1);   // 205 cycles
#else
         signalqd_semaphore(&sem1); // 232 cycles
         yield_task();
#endif
      }
      signal_semaphore(&sem1);
      end_task();
   }
#endif


#if 0
   // use suspend/resume
//#define V1
   if (id == 0) {
      while (g_task[1].state != task_state_END) {
         ++ s_cycles1;
#ifdef V1
         suspend_task();   // 168 cycles
#else
         suspend_task();   // 242 (cycles) mask
         resumeqd_task(&g_task[1]);
#endif
      }
      return ;
   } else {
      while (0 == s_timems) {
         ++ s_cycles2;
#ifdef V1
         resume_task(&g_task[0]);
#else
         resumeqd_task(&g_task[0]);
         suspend_task();
#endif
      }
      resumeqd_task(&g_task[0]);
      end_task();
   }
#endif

   if (id == 0) {
      assert(s_count == 0);
      wait_semaphore(&sem1);
      assert(s_count > 0);
   }
   if (id == 1) {
      assert(s_count == 0);
      wait_semaphore(&sem1);
      assert(s_count > 0);
   }
   if (id == 2) {
      assert(s_count == 0);
      s_count = 1;
      signal_semaphore(&sem1);
      signal_semaphore(&sem1);
   }

   sleepms_task(id*330);

   while (1) {
      for (uint32_t i = 0; i < 3; ++i) {
         s_task_nr[id] = (s_task_nr[id] + 1) % 8;
         uint32_t pin = 0;
         for (uint32_t ti = 0; ti < lengthof(g_task); ++ti) {
            pin |= GPIO_PIN(HW_KONFIG_USER_LED_MINNR + s_task_nr[ti]);
         }
         write_gpio(LED_PORT, (uint16_t) pin, LED_PINS);
         increment_atomic(&s_count);
         sleepms_task(110);
      }
      sleepms_task((lengthof(g_task)-1)*330);
      if (id == 0/*main thread*/ && s_count >= 30) {
         uint32_t starttime = s_timems;
         for (uint32_t i = 1; i < lengthof(g_task); ++i) {
            stop_task(&g_task[i]);
         }
         uint32_t i;
         do {
            yield_task();
            for (i = 1; i < lengthof(g_task); ++i) {
               if (g_task[i].state != task_state_END) break;
            }
         } while (i != lengthof(g_task));
         assert(s_timems - starttime <= 1);
         return; // main thread
      }
   }

}

void* getmainpsp_startup(void)
{
   return initialstack_task(&g_task[0]);  // task[0] is main thread
}

/*
 * Test routine calling all unittest and testing the jrtos scheduler.
 */
int main(void)
{
   uint32_t data1[10];
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);

   setsysclock_clockcntrl(clock_PLL/*72MHZ*/);
   assert(getHZ_clockcntrl() == 72000000);

   for (volatile int i = 0; i < 10; ++i) {
      data1[i] = 0;
   }

   assert(data1 == memset(data1, 0xff, 1));
   assert(data1[0] == 0xff);
   assert(data1 == memset(data1, 0x33, 2));
   assert(data1[0] == 0x3333);
   assert(data1 == memset(data1, 0x55, 3));
   assert(data1[0] == 0x555555);
   assert(data1 == memset(data1, 0x66, 4));
   assert(data1[0] == 0x66666666);
   assert(data1[1] == 0);
   assert(data1 == memset(data1, 0x61, 5));
   assert(data1[0] == 0x61616161);
   assert(data1[1] == 0x61);
   assert(data1[2] == 0);
   assert(data1 == memset(data1, 0x61, 6));
   assert(data1[0] == 0x61616161);
   assert(data1[1] == 0x6161);
   assert(data1[2] == 0);
   assert(data1 == memset(data1, 0x61, 7));
   assert(data1[0] == 0x61616161);
   assert(data1[1] == 0x616161);
   assert(data1[2] == 0);
   assert(data1 == memset(data1, 0x88, 8));
   assert(data1[0] == 0x88888888);
   assert(data1[1] == 0x88888888);
   assert(data1[2] == 0);
   assert(data1 == memset(data1, 0x88, 9));
   assert(data1[0] == 0x88888888);
   assert(data1[1] == 0x88888888);
   assert(data1[2] == 0x88);

   for (volatile int i = 0; i < 125000; ++i) ;

   sem1  = (semaphore_t) semaphore_INIT(0);
   fifo1 = (fifo_t) fifo_INIT;

#define RUN(module) \
         switch_led(); \
         extern int unittest_##module(void); \
         assert( 0 == unittest_##module())

   for (unsigned i = 0; i < 3; ++i) {
      switch_led();
      RUN(hw_cortexm4_atomic);
      RUN(hw_cortexm4_core);
      RUN(jrtos_task);
#if 0
      RUN(jrtos_semaphore);
      RUN(jrtos_scheduler);
#endif
   }

   init_task(&g_task[0], 0, 0, 0);
   for (uint32_t i = 1; i < lengthof(g_task); ++i) {
      init_task(&g_task[i], (uint8_t) i, &task_main, i);
   }
#ifdef MEASURE_SPEED
   assert(0 == init_scheduler(2/*lengthof(g_task)*/, g_task));
#else
   assert(0 == init_scheduler(lengthof(g_task), g_task));
#endif

   enable_trace_dbg();
   setpriority_coreinterrupt(coreinterrupt_SYSTICK, interrupt_priority_MIN-1);   // could preempt scheduler
#ifdef MEASURE_SPEED
   config_systick(getHZ_clockcntrl()/10/*100ms*/, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);
#else
   config_systick(getHZ_clockcntrl()/1000/*1ms*/, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);
#endif
   start_dwtdbg(dwtdbg_CYCLECOUNT);

   // Stack overflow generates an interrupt
   // g_task[0].stack[-1] = 0;

   task_main(0);
#ifdef MEASURE_SPEED
   s_10ms = cyclecount_dwtdbg();
#endif
   sleepms_task(390);

   while (1) switch_led();
}
