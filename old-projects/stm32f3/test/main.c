#include "konfig.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXPIN)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINPIN)

volatile uint32_t    clockHZ;
/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;

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

volatile uint32_t s_counter6;
volatile uint32_t s_counter7;

void timer6_dac_interrupt(void)
{
   ++s_counter6;
}

void timer7_interrupt(void)
{
   clear_expired_basictimer(TIMER7); // ackn. peripheral
   ++s_counter7;
}

volatile int xxx = 0;

void mpufault_interrupt(void)
{
   xxx = 2;
   assert( 0 == isret2threadmode_interrupt());
   assert( 0 == is_interrupt(coreinterrupt_MPUFAULT));      // No more pending
   assert( coreinterrupt_MPUFAULT == active_interrupt());   // but active.
}

void busfault_interrupt(void)
{
   assert(xxx == 0);
   xxx = 1;
   generate_coreinterrupt(coreinterrupt_MPUFAULT);
   assert( 0 == is_interrupt(coreinterrupt_BUSFAULT));      // No more pending
   assert( coreinterrupt_BUSFAULT == active_interrupt());   // but active.
   for (volatile int i = 0; i < 1; ++i) ;
   assert(xxx == 2);
   assert( 1 == isret2threadmode_interrupt());
return;  // TODO: remove
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
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   enable_basictimer_clockcntrl(TIMER7_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);
   enable_dwt_dbg();
   // enable_syscfg_clockcntrl();

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

   // TODO:
      // TEST isret2threadmode_interrupt
      assert(xxx == 0);
      setpriority_coreinterrupt(coreinterrupt_BUSFAULT, interrupt_priority_LOW);
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      enable_coreinterrupt(coreinterrupt_BUSFAULT);
      generate_coreinterrupt(coreinterrupt_BUSFAULT);
      for (volatile int i = 0; i < 80000; ++i) ;
      setpriority_coreinterrupt(coreinterrupt_BUSFAULT, 0);
      disable_coreinterrupt(coreinterrupt_BUSFAULT);
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
      assert(xxx == 2);

      // TEST setpriority_coreinterrupt: setprioritymask_interrupt


      // TEST highestpriority_interrupt: disabled interrupts are not considered, only enabled
      setprioritymask_interrupt(2);
      setpriority_coreinterrupt(coreinterrupt_MPUFAULT, 2);
      setpriority_interrupt(interrupt_DMA1_CHANNEL7, 2);
      generate_interrupt(interrupt_DMA1_CHANNEL7);
      generate_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( 1 == is_interrupt(interrupt_DMA1_CHANNEL7));
      assert( 1 == is_coreinterrupt(coreinterrupt_MPUFAULT));
      assert( 0 == highestpriority_interrupt());   // disabled interrupts not considered !
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( 1 == is_coreinterrupt(coreinterrupt_MPUFAULT));
      assert( coreinterrupt_MPUFAULT == highestpriority_interrupt());   // considered after enabling !
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
      enable_interrupt(interrupt_DMA1_CHANNEL7);
      assert( 1 == is_interrupt(interrupt_DMA1_CHANNEL7));
      assert( interrupt_DMA1_CHANNEL7 == highestpriority_interrupt());  // considered after enabling !
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      clear_interrupt(interrupt_DMA1_CHANNEL7);
      clear_coreinterrupt(coreinterrupt_MPUFAULT);
      assert( 0 == is_interrupt(interrupt_DMA1_CHANNEL7));
      assert( 0 == is_coreinterrupt(coreinterrupt_MPUFAULT));
      assert( 0 == highestpriority_interrupt());
      disable_interrupt(interrupt_DMA1_CHANNEL7);
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
      setpriority_interrupt(interrupt_DMA1_CHANNEL7, 0);
      setpriority_coreinterrupt(coreinterrupt_MPUFAULT, 0);
      clearprioritymask_interrupt();

      // TEST highestpriority_interrupt: coreinterrupt_BUSFAULT
      enable_coreinterrupt(coreinterrupt_BUSFAULT);
      setpriority_coreinterrupt(coreinterrupt_BUSFAULT, 2);
      setprioritymask_interrupt(2);
      __asm volatile(
         "push {r0}\n"
         "ldr r0, =0x30000000\n"
         "str r0, [r0]\n"  // BUS-fault is generated one instruction later cause of write-buffer
         "pop {r0}\n"      // in this case masked busfault is not propagated to fault_interrupt
         :::
      );
      assert( 1 == is_coreinterrupt(coreinterrupt_BUSFAULT));
      assert( coreinterrupt_BUSFAULT == highestpriority_interrupt());
      clear_coreinterrupt(coreinterrupt_BUSFAULT);
      clearprioritymask_interrupt();
      setpriority_coreinterrupt(coreinterrupt_BUSFAULT, 0);
      disable_coreinterrupt(coreinterrupt_BUSFAULT);

      // TEST highestpriority_interrupt: returns (core + external interrupts ready for execution if priority allows it)
      assert( 0 == (hCORE->scb.icsr & HW_BIT(SCB, ICSR, VECTPENDING)));
      for (unsigned coreInt = 0; coreInt <= 2; ++coreInt) {
         for (unsigned extInt = 0; extInt <= 2; ++extInt) {
            for (unsigned disableType = 0; disableType <= 2; ++disableType) {
               switch (disableType) {
               case 0: disable_fault_interrupt(); break; // All exceptions with priority >= -1 prevented
               case 1: disable_all_interrupt(); break;   // All exceptions with priority >= 0 prevented
               case 2: setprioritymask_interrupt(2); break; // All exceptions with priority >= 2 prevented
               }
               volatile uint8_t intnr = 0;
               if (coreInt) {
                  setpriority_coreinterrupt(coreinterrupt_PENDSV, 1 + coreInt);
                  generate_coreinterrupt(coreinterrupt_PENDSV);
                  intnr = coreinterrupt_PENDSV;
               }
               if (extInt) {
                  enable_interrupt(interrupt_GPIOPIN0);   // only enabled interrupts count
                  setpriority_interrupt(interrupt_GPIOPIN0, 1 + extInt);
                  generate_interrupt(interrupt_GPIOPIN0);
                  if (coreInt == 0 || extInt < coreInt) {
                     // with equal priority the one with the lower exc. number has higher priority
                     // ==> extInt must be lower than coreInt (extInt has higher priority) to win
                     intnr = interrupt_GPIOPIN0;
                  }
               }
               assert( intnr == highestpriority_interrupt());
               // reset
               clear_coreinterrupt(coreinterrupt_PENDSV);
               clear_interrupt(interrupt_GPIOPIN0);
               for (volatile int i = 0; i < 1; ++i) ; // allow to propagate change to NVIC
               assert( 0 == (hCORE->scb.icsr & HW_BIT(SCB, ICSR, VECTPENDING)));
               disable_interrupt(interrupt_GPIOPIN0);
               setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
               setpriority_interrupt(interrupt_GPIOPIN0, 0);
               setprioritymask_interrupt(0);
               enable_fault_interrupt();
               enable_all_interrupt();
            }
         }
      }

   for (;;) {

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      clockHZ = getHZ_clockcntrl();

      switch_led();

      RUN(systick);

      // TEST isenabled_interrupt_nvic EINVAL
      assert( 0 == isenabled_interrupt_nvic(0));
      assert( 0 == isenabled_interrupt_nvic(16-1));
      assert( 0 == isenabled_interrupt_nvic(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

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
      assert( EINVAL == setpriority_interrupt(0, interrupt_priority_HIGH));
      assert( EINVAL == setpriority_interrupt(16-1, interrupt_priority_HIGH));
      assert( EINVAL == setpriority_interrupt(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1, interrupt_priority_HIGH));

      // Test getpriority_interrupt_nvic EINVAL
      assert( 255 == getpriority_interrupt_nvic(0));
      assert( 255 == getpriority_interrupt_nvic(16-1));
      assert( 255 == getpriority_interrupt_nvic(HW_KONFIG_NVIC_INTERRUPT_MAXNR+1));

      // TEST Interrupt enable
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert( ! isenabled_interrupt_nvic(i));
         assert( 0 == enable_interrupt(i));
         assert( 1 == isenabled_interrupt_nvic(i));
      }

      // TEST Interrupt disable
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         assert(   isenabled_interrupt_nvic(i));
         assert( 0 == disable_interrupt(i));
         assert( ! isenabled_interrupt_nvic(i));
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
      wait_for_interrupt();
      assert( 0 == is_interrupt(interrupt_TIMER7));
      assert( 1 == s_counter7); // executed
      assert( 0 == disable_interrupt(interrupt_TIMER7));
      s_counter7 = 0;

      // TEST setpriority_interrupt
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_LOW;
         assert( 0 == getpriority_interrupt_nvic(i)); // default after reset
         assert( 0 == setpriority_interrupt(i, L));
         assert( L == getpriority_interrupt_nvic(i)); // LOW priority set
      }

      // TEST getpriority_interrupt_nvic
      for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
         const uint8_t L = interrupt_priority_LOW;
         assert( L == getpriority_interrupt_nvic(i)); // default after reset
         assert( 0 == setpriority_interrupt(i, interrupt_priority_HIGH));
         assert( 0 == getpriority_interrupt_nvic(i)); // HIGH priority set
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

      // TEST is_any_interrupt: indicates only external interrupts
      assert( 0 == (hCORE->scb.icsr & HW_BIT(SCB, ICSR, ISRPENDING)));
      disable_all_interrupt();
      generate_coreinterrupt(coreinterrupt_SYSTICK);
      assert( 0 == is_any_interrupt());
      clear_coreinterrupt(coreinterrupt_SYSTICK);
      generate_interrupt(interrupt_GPIOPIN0);
      assert( 1 == is_any_interrupt());
      clear_interrupt(interrupt_GPIOPIN0);
      enable_all_interrupt();

   }

}

#include "unittest_systick.c"

