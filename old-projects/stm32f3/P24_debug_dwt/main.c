#include "konfig.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS

static volatile int      err;
static volatile uint8_t  start;
static volatile uint8_t  end;
static volatile uint32_t start32;
static volatile uint32_t end32;
static volatile uint32_t speed;
static volatile uint32_t debugcounter;
static volatile uint32_t datablock[16];
static void  (* volatile fct) (void);

void assert_failed_exception(const char *filename, int linenr)
{
   volatile const char *f = filename;
   volatile int l = linenr;
   (void) f;
   (void) l;
   static_assert(LED_PINS == GPIO_PINS(15,8));
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
      write_gpio(LED_PORT, GPIO_PIN15, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

static void switch_led(void)
{
   static uint32_t lednr1;
   static uint32_t lednr2;
   static uint32_t counter1;
   static uint32_t counter2;
   uint16_t off = GPIO_PIN(8 + lednr2) | GPIO_PIN(8 + lednr1);
   static_assert(LED_PINS == GPIO_PINS(15,8));
   counter1 = (counter1 + 1) % 2;
   counter2 = (counter2 + 1) % 3;
   lednr1 = (lednr1 + (counter1 == 0)) % 8;
   lednr2 = (lednr2 + (counter2 == 0)) % 8;
   write_gpio(LED_PORT, GPIO_PIN(8 + lednr1) | GPIO_PIN(8 + lednr2), off);
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 100000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
   }
}

void systick_interrupt(void)
{
   for (volatile int i = 0; i < 50; ++i) ;
}

void timer6_dac_interrupt(void)
{
   clear_expired_basictimer(TIMER6);
}

void debugmonitor_interrupt(void)
{
   ++ debugcounter;
}

/*
   Testet die verschiedenen Performanz-Counter der DWT-Debugkomponente und die möglichen Arten von Watchpoints.

   Watchpoints lösen Debugevents aus, die, sofern kein externer Debugger angeschlossen ist und
   der DEBUGMONITOR Interrupt eingeschaltet ist (enable_interrupt_dbg), einen DEBUGMONITOR Interrupt
   auslösen und dessen Implementierung den Zähler debugcounter um eins erhöht.

   Ein fehlerfreier Testdurchlauf bewegt zwei LEDS im Uhrzeigersinn eine Position weiter.
*/
int main(void)
{
   int isenabled = 0;
   uint8_t wp1;

   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   enable_basictimer_clockcntrl(TIMER6_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);
   enable_interrupt(interrupt_TIMER6_DAC);

   // Wait, until debugger detached and device could use debugmonitor to catch debug events
   while (isdebugger_dbg()) {
      write_gpio(LED_PORT, GPIO_PIN(HW_KONFIG_USER_LED_MAXNR-2), LED_PINS);
   }
   write0_gpio(LED_PORT, LED_PINS);

   enable_dwt_dbg();       // enable dwt debug component which supports counter and watchpoints
   enable_interrupt_dbg(); // if (<interrupt enabled> && <debug event occurs>) then <execute debugmonitor_interrupt>

   // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_VALUE not supported by Hardware
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE8BIT_RO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE8BIT_WO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE8BIT_RW, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE16BIT_RO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE16BIT_WO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE16BIT_RW, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE32BIT_RO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE32BIT_WO, 0xe8, 0));
   assert( ENOSYS == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_VALUE32BIT_RW, 0xe8, 0));

   for (;;) {

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
         isenabled = !isenabled;
      }

      speed = getHZ_clockcntrl();

      switch_led();

      // TEST nrcomp_dwtdbg
      assert( 2 <= nrcomp_dwtdbg());

      // TEST feature_dwtdbg
      {
         volatile dwtdbg_feature_e f = feature_dwtdbg();
         assert( 0 != (f & dwtdbg_feature_CYCLECOUNTER));
         assert( 0 != (f & dwtdbg_feature_PROFILECOUNTER));
      }

      // TEST cyclecount_dwtdbg: Calculate Overhead
      start_dwtdbg(dwtdbg_CYCLECOUNT);
      stop_dwtdbg(dwtdbg_CYCLECOUNT);
      start32 = cyclecount_dwtdbg();
      assert(4 <= start32);   // enable & disable overhead
      if (speed == 8000000) {
         assert(15 > start32);
      } else  {
         assert(20 > start32);
      }

      // TEST cyclecount_dwtdbg: Counts Instruction Cycles
      if (isenabled) {
         start_dwtdbg(dwtdbg_CYCLECOUNT);   // clears counter to 0
      }
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      stop_dwtdbg(dwtdbg_CYCLECOUNT);
      end32 = cyclecount_dwtdbg() - start32;
      if (isenabled) {
         if (speed == 8000000) {
            assert(10 <= end32);
         } else {
            assert(5 <= end32);
         }
         assert(20 >= end32);
      } else {
         assert( 0 == end32);    // counter was not enabled ==> value not changed
      }
      start_dwtdbg(dwtdbg_CYCLECOUNT);   // clears counter to 0
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      __asm volatile( "nop\n" );
      stop_dwtdbg(dwtdbg_CYCLECOUNT);
      end32 = cyclecount_dwtdbg() - start32;
      if (speed == 8000000) {
         assert(5 <= end32);
      } else {
         assert(2 <= end32);
      }
      assert(10 >= end32);

      // TEST cpicount_dwtdbg: Calculate cycles per instruction of multi-cycle instructions
      if (isenabled) {
         start_dwtdbg(dwtdbg_CPICOUNT);
      } else {
         DWTDBG->cpicnt = 0;
      }
      start = cpicount_dwtdbg();
      for (volatile int i = 0; i < 10; ++i) {
         __asm volatile( "nop\nnop\n" );
      }
      stop_dwtdbg(dwtdbg_CPICOUNT);
      end = cpicount_dwtdbg() - start;
      if (isenabled) {
         if (speed == 8000000) {
            assert( 7 <= end);
         } else  {
            assert( 25 <= end);
         }
      } else {
         assert( 0 == end);   // counter was not enabled ==> value not changed
      }

      // TEST cyclecount_dwtdbg: Measure cycles used in systick_interrupt
      start_dwtdbg(dwtdbg_CYCLECOUNT);
      for (volatile int i = 0; i < 50; ++i) ;   // code in systck_interrupt
      stop_dwtdbg(dwtdbg_CYCLECOUNT);
      end32 = cyclecount_dwtdbg() - start32;
      assert(end32 > 100);

      // TEST exccount_dwtdbg: Counts Exception Overhead (without time executed in exception)
      if (isenabled) {
         start_dwtdbg(dwtdbg_EXCCOUNT);  // clears counter to 0
      } else {
         DWTDBG->exccnt = 0;
      }
      start = exccount_dwtdbg();
      assert(0 == start);
      config_systick(1000, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);
      while (!isexpired_systick()) ;
      stop_systick();
      stop_dwtdbg(dwtdbg_EXCCOUNT);
      end = exccount_dwtdbg();
      if (isenabled) {
         assert( 10 < end);
         assert( 50 > end);   // only overhead measured
      } else {
         assert( 0 == end);   // counter was not enabled ==> value not changed
      }

      // TEST sleepcount_dwtdbg: Cycles in power-save mode
      if (isenabled) {
         start_dwtdbg(dwtdbg_SLEEPCOUNT);
      } else {
         DWTDBG->sleepcnt = 0;
      }
      config_basictimer(TIMER6, 200, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT);
      start = sleepcount_dwtdbg();
      assert( 0 == start);
      start_basictimer(TIMER6);
      waitinterrupt_core();   // sleep
      stop_dwtdbg(dwtdbg_SLEEPCOUNT);
      end = sleepcount_dwtdbg();
      if (isenabled) {
         assert(150 <= end);
      } else {
         assert(0 == end); // counter was not enabled ==> value not changed
      }

      // TEST lsucount_dwtdbg: Additional Cycles used in load-store unit (first cycle not counted)
      if (isenabled) {
         start_dwtdbg(dwtdbg_LSUCOUNT);
      } else {
         DWTDBG->lsucnt = 0;
      }
      start = lsucount_dwtdbg();
      assert( 0 == start);
      __asm volatile(
            "push {r0-r8}\n"
            "ldr r0, =datablock\n"
            "ldm r0, {r0-r8}\n"
            "pop {r0-r8}\n"
         );
      stop_dwtdbg(dwtdbg_LSUCOUNT);
      end = lsucount_dwtdbg();
      if (isenabled) {
         assert( 3*9 <= end);
         assert( 4*9 >= end);
      } else {
         assert( 0 == end);   // counter was not enabled ==> value not changed
      }

      // TEST foldcount_dwtdbg: Number of instructions with zero cycles
      if (isenabled) {
         start_dwtdbg(dwtdbg_FOLDCOUNT);
      } else {
         DWTDBG->foldcnt = 0;
      }
      start = foldcount_dwtdbg();
      assert( 0 == start);
      __asm volatile(
            "push {r0-r8}\n"
            "movs r0, #10\n"
            "cmp r0, #10\n"
            "it eq\n"
            "addseq r0, r0, #1\n"
            "pop {r0-r8}\n"
         );
      stop_dwtdbg(dwtdbg_FOLDCOUNT);
      end = foldcount_dwtdbg();
      if (isenabled) {
         assert( 1 <= end);
      } else {
         assert( 0 == end);   // counter was not enabled ==> value not changed
      }

      // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_CYCLECOUNT
      err = addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_CYCLECOUNT, 100, 0);
      assert( 0 == err);
      assert( 0 == wp1);            // supported only by comparator 0
      for (volatile int w = 0; w < 200; ++w) ;
      assert( 0 == debugcounter);   // dwtdbg_CYCLECOUNT was not enabled
      start_dwtdbg(dwtdbg_CYCLECOUNT);
      for (volatile int w = 0; w < 200; ++w) ;
      assert( 1 == debugcounter);   // dwtdbg_CYCLECOUNT was enabled and fired ibk< once
      assert( 200 < cyclecount_dwtdbg());
      // reset
      stop_dwtdbg(dwtdbg_CYCLECOUNT);
      clearwatchpoint_dwtdbg(wp1);
      debugcounter = 0;

      // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_CODEADDR
      fct = &systick_interrupt;
      assert( 1 == (1 & (uintptr_t)fct)); // thumb state bit
      err = addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_CODEADDR, (uintptr_t)fct, 0);
      assert( 0 == err);
      assert( 3 == wp1);
      for (uint32_t i = 0; i < 100; ++i) {
         assert( i == debugcounter);
         fct();
         assert( i+1 == debugcounter);
         assert( 1 == ismatch_dwtdbg(wp1));  // clears value
         assert( 0 == ismatch_dwtdbg(wp1));
      }
      // reset
      clearwatchpoint_dwtdbg(wp1);
      debugcounter = 0;

      // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_DATAADDR_RO
      err = addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_DATAADDR_RO, (uintptr_t)&speed, 0);
      assert( 0 == err);
      assert( 3 == wp1);
      for (uint32_t i = 0; i < 100; ++i) {
         speed = 0;
         assert( i == debugcounter);
         assert( 0 == ismatch_dwtdbg(wp1));
         if (speed == 100) speed = 0;        // read speed ==> debug event
         for (volatile int w = 0; w < 10; ++w) ;
         assert( i+1 == debugcounter);
         assert( 1 == ismatch_dwtdbg(wp1));  // clears value
         assert( 0 == ismatch_dwtdbg(wp1));
      }
      // reset
      clearwatchpoint_dwtdbg(wp1);
      debugcounter = 0;

      // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_DATAADDR_WO
      err = addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_DATAADDR_WO, (uintptr_t)&speed, 0);
      assert( 0 == err);
      assert( 3 == wp1);
      for (uint32_t i = 0; i < 100; ++i) {
         if (speed == 100) speed = 0;
         assert( i == debugcounter);
         assert( 0 == ismatch_dwtdbg(wp1));
         speed = 0;                          // write speed ==> debug event
         for (volatile int w = 0; w < 10; ++w) ;
         assert( i+1 == debugcounter);
         assert( 1 == ismatch_dwtdbg(wp1));  // clears value
         assert( 0 == ismatch_dwtdbg(wp1));
      }
      // reset
      clearwatchpoint_dwtdbg(wp1);
      debugcounter = 0;

      // TEST addwatchpoint_dwtdbg: dwtdbg_watchpoint_DATAADDR_RW
      err = addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_DATAADDR_RW, (uintptr_t)&speed, 0);
      assert( 0 == err);
      assert( 3 == wp1);
      for (uint32_t i = 0; i < 100; i += 2) {
         if (speed == 100) speed = 0;        // read speed ==> debug event
         for (volatile int w = 0; w < 10; ++w) ;
         assert( i+1 == debugcounter);
         assert( 1 == ismatch_dwtdbg(wp1));  // clears value
         assert( 0 == ismatch_dwtdbg(wp1));
         speed = 0;                          // write speed ==> debug event
         for (volatile int w = 0; w < 10; ++w) ;
         assert( i+2 == debugcounter);
         assert( 1 == ismatch_dwtdbg(wp1));  // clears value
         assert( 0 == ismatch_dwtdbg(wp1));
      }
      // reset
      clearwatchpoint_dwtdbg(wp1);
      debugcounter = 0;

   }
}
