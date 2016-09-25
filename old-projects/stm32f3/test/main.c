#include "konfig.h"
#include "µC/cpustate.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXNR)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINNR)

volatile uint32_t    clockHZ; // System is running at this frequency
/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;
cpustate_t           cpustate;


volatile uint32_t    s_pendsvcounter;
volatile uint32_t    s_faultcounter;
volatile uint32_t    s_nmicounter;
volatile uint32_t    s_usagefault;

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
      for (volatile int i = 0; i < 100000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
   }
}

#if 0
static void switch_unprivileged(void)
{
   __asm volatile(
         "mrs r0, CONTROL\n"
         "orrs r0, #1\n"
         "msr CONTROL, r0\n"
         ::: "cc", "r0"
      );
}

static void switch_privileged(void)
{
   __asm volatile(
         "mrs r0, CONTROL\n"
         "bics r0, #1\n"
         "msr CONTROL, r0"
         ::: "cc", "r0"
      );
}

static uint32_t is_unprivileged(void)
{
   uint32_t ctrl;
   __asm volatile(
         "mrs %0, CONTROL\n"
         "ands %0, #1\n"
         : "=r" (ctrl) :: "cc"
      );
   return ctrl;
}
#endif

cpustate_t           cpustate2;
volatile uint32_t    s_mpufault;

void mpufault_interrupt(void) // TODO: remove
{
   ++ s_mpufault;
   assert( isactive_coreinterrupt(coreinterrupt_MPUFAULT));
   if (isinit_cpustate(&cpustate2)) {
      ret2threadmode_cpustate(&cpustate2);
   }
}

void usagefault_interrupt(void)
{
   ++ s_usagefault;
   assert( isactive_coreinterrupt(coreinterrupt_USAGEFAULT));
}

void busfault_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void fault_interrupt(void)
{
   ++ s_faultcounter;

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
   ++ s_nmicounter;

   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void pendsv_interrupt(void)
{
   ++s_pendsvcounter;
}

static uint32_t stack2[128];

void called_function(void)
{
   switch_led();
   for (volatile int i = 0; i < 125000; ++i) ;
   switch_led();
   for (volatile int i = 0; i < 125000; ++i) ;
}

void call_function(void (* fct) (void))
{
   fct();
   jump_cpustate(&cpustate);
}

void test_before(void)
{
   int err = init_cpustate(&cpustate);
   if (err == 0) {
      init_cpustate(&cpustate2);
      cpustate2.sp = (uintptr_t) &stack2[lengthof(stack2)];
      cpustate2.iframe[0/*r0*/] = (uintptr_t) &called_function;
      cpustate2.iframe[6/*pc*/] = (uintptr_t) &call_function;
      jump_cpustate(&cpustate2);
   }
   assert(EINTR == err);
   free_cpustate(&cpustate);
   free_cpustate(&cpustate2);
}

/*
 * Dieses Programm ist ein reines Treiberprogramm für verschiedene Testmodule.
 *
 * Bei jedem erfolgreichen Ausführen eines Testmoduls werden zwei LED eine Position weiterbewegt.
 *
 * Im Fehlerfall beginnen alle LEDs zu blinken.
 *
 */
int main(void)
{
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   enable_basictimer_clockcntrl(TIMER7_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);
   enable_dwt_dbg();
   // enable_syscfg_clockcntrl();

   for (volatile int i = 0; i < 125000; ++i) ;

   test_before();

#if 0
   // TODO: dwt remembers configuration ==> could use to trigger reset in watchdog and restart testprogram !!
   enable_dwt_dbg();
   uint8_t wp1;
   assert(0 == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_DATAADDR_RW, (uintptr_t)&wp1, 0));

   if (3 == wp1) {
      reset_core();
      clearallwatchpoint_dwtdbg();
      assert(0 == addwatchpoint_dwtdbg(&wp1, dwtdbg_watchpoint_DATAADDR_RW, (uintptr_t)&wp1, 0));
      assert(3 == wp1);
      clearwatchpoint_dwtdbg(wp1);
   }
#endif

// TODO: move into exti test with buttons
      // TEST swier 0->1 generates exception only if enabled in imr1, enabling imr1 later does not work and swier must be reset to 0 to work.
      assert( 0 == (EXTI->imr1 & 1));
      assert( 0 == (EXTI->pr1 & 1));
      EXTI->swier1 = 1;
      EXTI->imr1 |= 1;
      assert( 1 == (EXTI->imr1 & 1));
      EXTI->swier1 = 1; // does not work 1->1 !
      __asm volatile ("dsb\n");
      for (volatile int i = 0; i < 1; ++i) ;
      assert( 0 == (EXTI->pr1 & 1));
      EXTI->swier1 = 0;
      EXTI->swier1 = 1; // does work 0->1 !
      __asm volatile ("dsb\n");
      for (volatile int i = 0; i < 1; ++i) ;
      assert( 1 == (EXTI->pr1 & 1));
      assert( 1 == (EXTI->swier1 & 1));
      EXTI->imr1 &= ~1;
      EXTI->pr1 |= 1;
      assert( 0 == (EXTI->pr1 & 1));
      assert( 0 == (EXTI->swier1 & 1));
      assert( 1 == is_interrupt(interrupt_GPIOPIN0));
      clear_interrupt(interrupt_GPIOPIN0);

// ======= core

// ======= end core


   for (;;) {

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      clockHZ = getHZ_clockcntrl();

      switch_led();

#define RUN(module) \
         switch_led(); \
         extern int unittest_##module(void); \
         assert( 0 == unittest_##module())

      RUN(atomic);
      RUN(coreinterrupt);
      RUN(cpuid);
      RUN(systick);
      RUN(interruptTable);
      RUN(interrupt);
      RUN(mpu);
      RUN(cpustate);

   }

}

#include "unittest_atomic.c"
#include "unittest_cpuid.c"
#include "unittest_systick.c"
#include "unittest_interruptTable.c"
#include "unittest_interrupt.c"
#include "unittest_coreinterrupt.c"
#include "unittest_mpu.c"
#include "unittest_cpustate.c"

