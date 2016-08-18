#include "konfig.h"

static volatile uint32_t s_lednr1;
static volatile uint32_t s_lednr2;
static volatile uint32_t s_counter6;
static volatile uint32_t s_counter7;

void assert_failed_exception(const char *filename, int linenr)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(GPIO_PORTE, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
      setpins_gpio(GPIO_PORTE, GPIO_PIN15, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void timer6_dac_interrupt(void)
{
   ++s_counter6;
}

void timer7_interrupt(void)
{
   clear_isexpired_basictimer(TIMER7); // ackn. peripheral
   ++s_counter7;
}

static void switch_led(void)
{
   write0_gpio(GPIO_PORTE, GPIO_PIN(8 + s_lednr2)|GPIO_PIN(8 + s_lednr1));
   ++ s_lednr1;
   -- s_lednr2;
   if (s_lednr1 >= 8) s_lednr1 = 0;
   if (s_lednr2 >= 8) s_lednr2 = 7;
   write1_gpio(GPIO_PORTE, GPIO_PIN(8 + s_lednr1)|GPIO_PIN(8 + s_lednr2));
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 250000; ++i) ;
   } else {
      for (volatile int i = 0; i < 50000; ++i) ;
   }
}

/* Dieses Programm ist ein reines Testprogramm für NVIC Interrupts.
 *
 * Es werden Funktionen für normale Interrupts wie setpriority_interrupt_nvic, enable_interrupt_nvic,
 * generate_interrupt_nvic use. getestet.
 *
 * Nach jedem ausgeführten Test werden zwei User-LED gegenläufig weiter im Kreis bewegt.
 *
 * Im Fehlerfall wird durch assert die Funktion assert_failed_exception aufgerufen.
 * Diese lässt alle LEDs aufblinken.
 * Dieses Programm ist zur Ausführung im Debugger gedacht.
 *
 * Ein Ausführungssequenz mit einer fehlerhafter assert Bedingung könnte so aussehen:
 * $ make debug
 * (gdb) break assert_failed_exception
 * Breakpoint 1 at 0x3c0: file main.c, line 9.
 * (gdb) cont
 * Continuing.
 *
 *  Breakpoint 1, assert_failed_exception (filename=0x134c "main.c", linenr=201)
 *      at main.c:19
 *  9       {
 * (gdb) up
 * (gdb) print i
 * ...
 */
int main(void)
{
   enable_basictimer_clockcntrl(TIMER7_BIT);
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*user-switch*/|GPIO_PORTE_BIT/*user-LEDs*/);
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));

   s_lednr1 = 3;  // green LED is starting point
   s_lednr2 = 3;  // green LED is starting point
   switch_led();

   for (unsigned clock = 0; 1/*repeat forever*/; ++clock) {

      if ((clock&1) == 0) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      // TEST isenabled_interrupt_nvic EINVAL
      assert( 0 == isenabled_interrupt_nvic(0));
      assert( 0 == isenabled_interrupt_nvic(16-1));
      assert( 0 == isenabled_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST enable_interrupt_nvic EINVAL
      assert( EINVAL == enable_interrupt_nvic(0));
      assert( EINVAL == enable_interrupt_nvic(16-1));
      assert( EINVAL == enable_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST disable_interrupt_nvic EINVAL
      assert( EINVAL == disable_interrupt_nvic(0));
      assert( EINVAL == disable_interrupt_nvic(16-1));
      assert( EINVAL == disable_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST is_interrupt_nvic EINVAL
      assert( 0 == is_interrupt_nvic(0));
      assert( 0 == is_interrupt_nvic(16-1));
      assert( 0 == is_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST generate_interrupt_nvic EINVAL
      assert( EINVAL == generate_interrupt_nvic(0));
      assert( EINVAL == generate_interrupt_nvic(16-1));
      assert( EINVAL == generate_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST clear_interrupt_nvic EINVAL
      assert( EINVAL == clear_interrupt_nvic(0));
      assert( EINVAL == clear_interrupt_nvic(16-1));
      assert( EINVAL == clear_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // Test setpriority_interrupt_nvic EINVAL
      assert( EINVAL == setpriority_interrupt_nvic(0, interrupt_priority_HIGH));
      assert( EINVAL == setpriority_interrupt_nvic(16-1, interrupt_priority_HIGH));
      assert( EINVAL == setpriority_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1, interrupt_priority_HIGH));

      // Test getpriority_interrupt_nvic EINVAL
      assert( 255 == getpriority_interrupt_nvic(0));
      assert( 255 == getpriority_interrupt_nvic(16-1));
      assert( 255 == getpriority_interrupt_nvic(HW_KONFIG_NVIC_EXCEPTION_MAXNR+1));

      // TEST Interrupt enable
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         assert( ! isenabled_interrupt_nvic(i));
         assert( 0 == enable_interrupt_nvic(i));
         assert( 1 == isenabled_interrupt_nvic(i));
      }

      // TEST Interrupt disable
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         assert(   isenabled_interrupt_nvic(i));
         assert( 0 == disable_interrupt_nvic(i));
         assert( ! isenabled_interrupt_nvic(i));
      }

      // TEST generate_interrupt_nvic
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         assert( ! is_interrupt_nvic(i));
         assert( 0 == generate_interrupt_nvic(i));
         assert(   is_interrupt_nvic(i));
      }

      // TEST clear_interrupt_nvic
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         assert(   is_interrupt_nvic(i));
         assert( 0 == clear_interrupt_nvic(i));
         assert( ! is_interrupt_nvic(i));
      }

      // TEST interrupt_TIMER6_DAC execution
      switch_led();
      assert( 0 == generate_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 1 == is_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 0 == s_counter6);  // not executed
      __asm( "sev\nwfe\n");      // clear event flag
      assert( 0 == enable_interrupt_nvic(interrupt_TIMER6_DAC));
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 0 == is_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 1 == s_counter6);  // executed
      assert( 0 == disable_interrupt_nvic(interrupt_TIMER6_DAC));
      __asm( "wfe\n");           // interrupt exit sets event flag ==> wfe returns immediately
      s_counter6 = 0;

      // TEST interrupt_TIMER7 execution
      switch_led();
      assert( 0 == is_interrupt_nvic(interrupt_TIMER7));
      assert( 0 == enable_interrupt_nvic(interrupt_TIMER7));
      assert( 0 == config_basictimer(TIMER7, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT));
      assert( 0 == s_counter7); // not executed
      start_basictimer(TIMER7);
      assert( isstarted_basictimer(TIMER7));
      wait_for_interrupt();
      assert( 0 == is_interrupt_nvic(interrupt_TIMER7));
      assert( 1 == s_counter7); // executed
      assert( 0 == disable_interrupt_nvic(interrupt_TIMER7));
      s_counter7 = 0;

      // TEST setpriority_interrupt_nvic
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_LOW;
         assert( 0 == getpriority_interrupt_nvic(i)); // default after reset
         assert( 0 == setpriority_interrupt_nvic(i, L));
         assert( L == getpriority_interrupt_nvic(i)); // LOW priority set
      }

      // TEST getpriority_interrupt_nvic
      switch_led();
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_EXCEPTION_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_LOW;
         assert( L == getpriority_interrupt_nvic(i)); // default after reset
         assert( 0 == setpriority_interrupt_nvic(i, interrupt_priority_HIGH));
         assert( 0 == getpriority_interrupt_nvic(i)); // HIGH priority set
      }

      // TEST setpriority_interrupt_nvic + setbasepriority_interrupt + interrupt_TIMER6_DAC
      switch_led();
      assert( 0 == setpriority_interrupt_nvic(interrupt_TIMER6_DAC, 1));
      setbasepriority_interrupt(1); // inhibit all interrupts with priority lower or equal than 1
      assert( 0 == generate_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 1 == is_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 0 == enable_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 0 == s_counter6); // not executed
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 0 == s_counter6); // not executed
      assert( 1 == is_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 0 == setpriority_interrupt_nvic(interrupt_TIMER6_DAC, 0)); // set priority higher than 1
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 1 == s_counter6); // executed
      assert( 0 == is_interrupt_nvic(interrupt_TIMER6_DAC));
      assert( 0 == disable_interrupt_nvic(interrupt_TIMER6_DAC));
      setbasepriority_interrupt(0/*off*/);
      s_counter6 = 0;

   }

}
