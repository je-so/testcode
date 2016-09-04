#include "konfig.h"
#include "µC/cpustate.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXPIN)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINPIN)

volatile uint32_t    clockHZ; // System is running at this frequency
/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;
volatile uint32_t    s_faultcounter;
volatile uint32_t    s_usagefault;
volatile cpustate_t  cpustate;

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

void usagefault_interrupt(void)
{
   ++ s_usagefault;

   assert( isactive_coreinterrupt(coreinterrupt_USAGEFAULT));

   if (isinit_cpustate(&cpustate)) {
      assert( 1 == isret2threadmode_interrupt());  // single nested interrupt
      // generate fault interrupt
      __asm volatile(
         "mov r0, #0x10000000\n" // unknown SRAM address
         "ldr r0, [r0, #-4]\n"   // precise busfault
      );
   }

   // disable trapping on division by 0 (result of div by 0 is 0)
   hSCB->ccr &= ~HW_BIT(SCB, CCR, DIV_0_TRP);
   // disable trapping on unaligned access
   hSCB->ccr &= ~HW_BIT(SCB, CCR, UNALIGN_TRP);
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

   if (isinit_cpustate(&cpustate)) {
      assert( 0 == isret2threadmode_interrupt());  // (at least) two nested interrupts
      ret2threadmode_cpustate(&cpustate);
   }

   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
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
   int err;
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   enable_basictimer_clockcntrl(TIMER7_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);
   enable_dwt_dbg();
   // enable_syscfg_clockcntrl();

      // TEST atomic_setbit_interrupt
      assert( !isenabled_coreinterrupt(coreinterrupt_MPUFAULT));
      assert( !is_coreinterrupt(coreinterrupt_MPUFAULT));
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( isenabled_coreinterrupt(coreinterrupt_MPUFAULT));
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( !isenabled_coreinterrupt(coreinterrupt_MPUFAULT));

      // TEST atomic_clearbit_interrupt
      assert( !isenabled_coreinterrupt(coreinterrupt_MPUFAULT));
      generate_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( is_coreinterrupt(coreinterrupt_MPUFAULT));
      clear_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( ! is_coreinterrupt(coreinterrupt_MPUFAULT));


#define RUN(module) \
         switch_led(); \
         extern int unittest_##module(void); \
         assert( 0 == unittest_##module())

// TODO: move into exti test with buttons
      // TEST swier 0->1 generates exception only if enabled in imr1, enabling imr1 later does not work and swier must be reset to 0 to work.
      assert( 0 == (EXTI->imr1 & 1));
      assert( 0 == (EXTI->pr1 & 1));
      EXTI->swier1 = 1;
      EXTI->imr1 |= 1;
      assert( 1 == (EXTI->imr1 & 1));
      EXTI->swier1 = 1; // does not work 1->1 !
      for (volatile int i = 0; i < 1; ++i) ;
      assert( 0 == (EXTI->pr1 & 1));
      EXTI->swier1 = 0;
      EXTI->swier1 = 1; // does work 0->1 !
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

      // TEST HW_BIT(SCB, CCR, DIV_0_TRP): make div by 0 a fault exception
      s_usagefault = 0;
      hSCB->ccr |= HW_BIT(SCB, CCR, DIV_0_TRP);
      enable_coreinterrupt(coreinterrupt_USAGEFAULT);
      {
         volatile uint32_t divisor = 0;
         volatile uint32_t result = 10 / divisor;  // usagefault_interrupt clears DIV_0_TRP
         assert( 0 == result);
      }
      disable_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 1 == s_usagefault);

      // TEST HW_BIT(SCB, CCR, UNALIGN_TRP): trap on unaligned access
      s_usagefault = 0;
      hSCB->ccr |= HW_BIT(SCB, CCR, UNALIGN_TRP);
      enable_coreinterrupt(coreinterrupt_USAGEFAULT);
      {
         volatile uint32_t data[2] = { 0, 0 };
         assert( 0 == *(uint32_t*)(1+(uintptr_t)data));
      }
      disable_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 1 == s_usagefault);
// ======= end core

      // TEST HW_BIT(SCB, CCR, USERSETMPEND): privilege of stir !!


      // TEST nested fault_interrupt: set HW_BIT(SCB, CCR, NONBASETHRDENA) allows to jump from nested exception back to Thread mode and skip active interrupts
      assert( 0 == (hSCB->ccr & HW_BIT(SCB, CCR, NONBASETHRDENA)));  // default after reset
      hSCB->ccr |= HW_BIT(SCB, CCR, NONBASETHRDENA);
      assert( 0 != (hSCB->ccr & HW_BIT(SCB, CCR, NONBASETHRDENA)));  // default after reset
      s_faultcounter = 0;
      s_usagefault   = 0;
      enable_coreinterrupt(coreinterrupt_USAGEFAULT);
      err = init_cpustate(&cpustate);
      if (err == 0) {
         assert( 0 == s_faultcounter);
         generate_coreinterrupt(coreinterrupt_USAGEFAULT);
         assert( 0 /*never reached*/ );
      }
      assert( EINTR == err); // return from interrupt
      free_cpustate(&cpustate);
      assert( 1 == s_usagefault);
      assert( 1 == s_faultcounter);
      assert( 0 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
      assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));   // Thread mode priority is currently at USAGEFAULT level
      generate_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 1 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
      assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));
      assert( 1 == s_usagefault); // interrupt not called cause Thread mode priority is currently at USAGEFAULT level
      assert( HW_REGISTER_BIT_SCB_SHCSR_USGFAULTACT == (HW_REGISTER(SCB, SHCSR) & 0xfff));
      HW_REGISTER(SCB, SHCSR) &= ~HW_REGISTER_BIT_SCB_SHCSR_USGFAULTACT;
      assert( 0 == (HW_REGISTER(SCB, SHCSR) & 0xfff));   // not any coreinterrupt_e active ==> Thread mode priority set to lowest level possible
      assert( 2 == s_usagefault);   // interrupt now called cause Thread mode priority set to lowest level possible
      disable_coreinterrupt(coreinterrupt_USAGEFAULT);
      hSCB->ccr &= ~HW_BIT(SCB, CCR, NONBASETHRDENA);
      assert( 0 == (hSCB->ccr & HW_BIT(SCB, CCR, NONBASETHRDENA)));

      // TEST setpriority_coreinterrupt: setprioritymask_interrupt

   for (;;) {

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      clockHZ = getHZ_clockcntrl();

      switch_led();

      RUN(systick);
      RUN(interruptTable);
      RUN(interrupt);
      RUN(coreinterrupt);

   }

}

#include "unittest_systick.c"
#include "unittest_interruptTable.c"
#include "unittest_interrupt.c"
#include "unittest_coreinterrupt.c"

