#include "konfig.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS

static volatile uint32_t s_counter6;
static volatile uint32_t s_counter7;
volatile const char     *filename/*set by assert_failed_exception*/;
volatile int             linenr/*set by assert_failed_exception*/;

void assert_failed_exception(const char *_filename, int _linenr)
{
   filename = _filename;
   linenr   = _linenr;
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
      write_gpio(LED_PORT, GPIO_PIN15, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void timer6_dac_interrupt(void)
{
   ++s_counter6;
}

void timer7_interrupt(void)
{
   clear_expired_basictimer(TIMER7); // ackn. peripheral
   ++s_counter7;
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

/* Dieses Programm ist ein reines Testprogramm für NVIC Interrupts.
 *
 * Es werden Funktionen für normale Interrupts wie setpriority_interrupt, enable_interrupt,
 * generate_interrupt use. getestet.
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
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   enable_basictimer_clockcntrl(TIMER7_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);

   for (;;) {

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      switch_led();

      // TEST isenabled_interrupt EINVAL
      assert( 0 == isenabled_interrupt(0));
      assert( 0 == isenabled_interrupt(16-1));
      assert( 0 == isenabled_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST enable_interrupt EINVAL
      assert( EINVAL == enable_interrupt(0));
      assert( EINVAL == enable_interrupt(16-1));
      assert( EINVAL == enable_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST disable_interrupt EINVAL
      assert( EINVAL == disable_interrupt(0));
      assert( EINVAL == disable_interrupt(16-1));
      assert( EINVAL == disable_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST is_interrupt EINVAL
      assert( 0 == is_interrupt(0));
      assert( 0 == is_interrupt(16-1));
      assert( 0 == is_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST generate_interrupt EINVAL
      assert( EINVAL == generate_interrupt(0));
      assert( EINVAL == generate_interrupt(16-1));
      assert( EINVAL == generate_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST clear_interrupt EINVAL
      assert( EINVAL == clear_interrupt(0));
      assert( EINVAL == clear_interrupt(16-1));
      assert( EINVAL == clear_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // Test setpriority_interrupt EINVAL
      assert( EINVAL == setpriority_interrupt(16, interrupt_priority_MIN+1));
      assert( EINVAL == setpriority_interrupt(0, interrupt_priority_MAX));
      assert( EINVAL == setpriority_interrupt(16-1, interrupt_priority_MAX));
      assert( EINVAL == setpriority_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1, interrupt_priority_MAX));

      // Test getpriority_interrupt EINVAL
      assert( 255 == getpriority_interrupt(0));
      assert( 255 == getpriority_interrupt(16-1));
      assert( 255 == getpriority_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST Interrupt enable
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert( ! isenabled_interrupt(i));
         assert( 0 == enable_interrupt(i));
         assert( 1 == isenabled_interrupt(i));
      }

      // TEST Interrupt disable
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert(   isenabled_interrupt(i));
         assert( 0 == disable_interrupt(i));
         assert( ! isenabled_interrupt(i));
      }

      // TEST generate_interrupt
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert( ! is_interrupt(i));
         assert( 0 == generate_interrupt(i));
         assert(   is_interrupt(i));
      }

      // TEST clear_interrupt
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert(   is_interrupt(i));
         assert( 0 == clear_interrupt(i));
         assert( ! is_interrupt(i));
      }

      // TEST interrupt_TIMER6_DAC execution
      assert( 0 == generate_interrupt(interrupt_TIMER6_DAC));
      assert( 1 == is_interrupt(interrupt_TIMER6_DAC));
      assert( 0 == s_counter6);  // not executed
      __asm( "sev\nwfe\n");      // clear event flag
      assert( 0 == enable_interrupt(interrupt_TIMER6_DAC));
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 0 == is_interrupt(interrupt_TIMER6_DAC));
      assert( 1 == s_counter6);  // executed
      assert( 0 == disable_interrupt(interrupt_TIMER6_DAC));
      __asm( "wfe\n");           // interrupt exit sets event flag ==> wfe returns immediately
      s_counter6 = 0;

      // TEST interrupt_TIMER7 execution
      assert( 0 == is_interrupt(interrupt_TIMER7));
      assert( 0 == enable_interrupt(interrupt_TIMER7));
      assert( 0 == config_basictimer(TIMER7, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT));
      assert( 0 == s_counter7); // not executed
      start_basictimer(TIMER7);
      assert( isstarted_basictimer(TIMER7));
      waitinterrupt_core();
      assert( 0 == is_interrupt(interrupt_TIMER7));
      assert( 1 == s_counter7); // executed
      assert( 0 == disable_interrupt(interrupt_TIMER7));
      s_counter7 = 0;

      // TEST setpriority_interrupt
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_MIN;
         assert( 0 == getpriority_interrupt(i)); // default after reset
         assert( 0 == setpriority_interrupt(i, L));
         assert( L == getpriority_interrupt(i)); // LOW priority set
      }

      // TEST getpriority_interrupt
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_MIN;
         assert( L == getpriority_interrupt(i)); // default after reset
         assert( 0 == setpriority_interrupt(i, interrupt_priority_MAX));
         assert( 0 == getpriority_interrupt(i)); // MAX priority set
      }

      // TEST setprioritymask_interrupt: interrupt_TIMER6_DAC
      assert( 0 == setpriority_interrupt(interrupt_TIMER6_DAC, 1));
      setprioritymask_interrupt(1); // inhibit all interrupts with priority lower or equal than 1
      assert( 0 == generate_interrupt(interrupt_TIMER6_DAC));
      assert( 1 == is_interrupt(interrupt_TIMER6_DAC));
      assert( 0 == enable_interrupt(interrupt_TIMER6_DAC));
      assert( 0 == s_counter6); // not executed
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 0 == s_counter6); // not executed
      assert( 1 == is_interrupt(interrupt_TIMER6_DAC));
      assert( 0 == setpriority_interrupt(interrupt_TIMER6_DAC, 0)); // set priority higher than 1
      for (volatile int i = 0; i < 1000; ++i) ;
      assert( 1 == s_counter6); // executed
      assert( 0 == is_interrupt(interrupt_TIMER6_DAC));
      assert( 0 == disable_interrupt(interrupt_TIMER6_DAC));
      setprioritymask_interrupt(0/*off*/);
      s_counter6 = 0;

   }

}
