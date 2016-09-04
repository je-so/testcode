
volatile uint32_t s_interrupt_counter6;
volatile uint32_t s_interrupt_counter7;
volatile uint32_t s_interrupt_isret2threadmode;

void timer6_dac_interrupt(void)
{
   ++s_interrupt_counter6;
}

void timer7_interrupt(void)
{
   clear_expired_basictimer(TIMER7); // ackn. peripheral
   ++s_interrupt_counter7;
}

void dma2_channel3_interrupt(void)
{
   assert( 0 == s_interrupt_isret2threadmode);
   assert( 1 == isret2threadmode_interrupt());
   s_interrupt_isret2threadmode = 1;
   generate_interrupt(interrupt_DMA2_CHANNEL4);
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 2 == s_interrupt_isret2threadmode);
   assert( 1 == isret2threadmode_interrupt());
}

void dma2_channel4_interrupt(void)
{
   assert( 1 == s_interrupt_isret2threadmode);
   assert( 0 == isret2threadmode_interrupt());
   s_interrupt_isret2threadmode = 2;
}

/*
 * Dieses Programm ist ein reines Treiberprogramm für verschiedene Testmodule.
 *
 * Bei jedem erfolgreichen Ausführen eines Testmoduls werden zwei LED eine Position weiterbewegt.
 *
 * Im Fehlerfall beginnen alle LEDs zu blinken.
 *
 */
int unittest_interrupt(void)
{

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
   assert( 0 == s_interrupt_counter6);    // not executed
   __asm( "sev\nwfe\n");                  // clear event flag
   assert( 0 == enable_interrupt(interrupt_TIMER6_DAC));
   for (volatile int i = 0; i < 1000; ++i) ;
   assert( 0 == is_interrupt(interrupt_TIMER6_DAC));
   assert( 1 == s_interrupt_counter6);    // executed
   assert( 0 == disable_interrupt(interrupt_TIMER6_DAC));
   wait_for_event_or_interrupt();         // interrupt exit sets event flag ==> wfe returns immediately
   s_interrupt_counter6 = 0;

   // TEST interrupt_TIMER7 execution
   assert( 0 == is_interrupt(interrupt_TIMER7));
   assert( 0 == enable_interrupt(interrupt_TIMER7));
   assert( 0 == config_basictimer(TIMER7, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT));
   assert( 0 == s_interrupt_counter7); // not executed
   start_basictimer(TIMER7);
   assert( isstarted_basictimer(TIMER7));
   wait_for_interrupt();
   assert( 0 == is_interrupt(interrupt_TIMER7));
   assert( 1 == s_interrupt_counter7); // executed
   assert( 0 == disable_interrupt(interrupt_TIMER7));
   s_interrupt_counter7 = 0;

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
   for (volatile int i = 0; i < 10; ++i) {
      assert( 0 == s_interrupt_counter6); // not executed
   }
   assert( 1 == is_interrupt(interrupt_TIMER6_DAC));
   assert( 0 == setpriority_interrupt(interrupt_TIMER6_DAC, 0)); // set priority higher than 1
   for (volatile int i = 0; i < 10; ++i) ;
   assert( 1 == s_interrupt_counter6);    // executed
   assert( 0 == is_interrupt(interrupt_TIMER6_DAC));
   // reset
   assert( 0 == disable_interrupt(interrupt_TIMER6_DAC));
   clearprioritymask_interrupt();
   s_interrupt_counter6 = 0;

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

   // TEST isret2threadmode_interrupt
   s_interrupt_isret2threadmode = 0;
   setpriority_interrupt(interrupt_DMA2_CHANNEL3, interrupt_priority_LOW);
   enable_interrupt(interrupt_DMA2_CHANNEL3);
   enable_interrupt(interrupt_DMA2_CHANNEL4);
   generate_interrupt(interrupt_DMA2_CHANNEL3);
   for (volatile int i = 0; i < 1; ++i) ;
   setpriority_interrupt(interrupt_DMA2_CHANNEL3, 0);
   disable_interrupt(interrupt_DMA2_CHANNEL3);
   disable_interrupt(interrupt_DMA2_CHANNEL4);
   assert( 2 == s_interrupt_isret2threadmode);

   // TEST highestpriority_interrupt: disabled interrupts are not considered
   setprioritymask_interrupt(2);
   setpriority_coreinterrupt(coreinterrupt_MPUFAULT, 2);
   setpriority_interrupt(interrupt_DMA1_CHANNEL7, 2);
   generate_interrupt(interrupt_DMA1_CHANNEL7);
   generate_coreinterrupt(coreinterrupt_MPUFAULT);
   assert( 1 == is_interrupt(interrupt_DMA1_CHANNEL7));
   assert( 1 == is_coreinterrupt(coreinterrupt_MPUFAULT));
   // test: disabled interrupts not considered !
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 0 == highestpriority_interrupt());

   // TEST highestpriority_interrupt: only enabled interrupts are considered
   for (uint32_t repeat = 0; repeat < 10; ++repeat) {
      // test coreinterrupt_MPUFAULT considered after enabling !
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      for (volatile int i = 0; i < 1; ++i) ;
      assert( coreinterrupt_MPUFAULT == highestpriority_interrupt());
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
      for (volatile int i = 0; i < 1; ++i) ;
      assert( 0 == highestpriority_interrupt());

      // test interrupt_DMA1_CHANNEL7 considered after enabling !
      enable_interrupt(interrupt_DMA1_CHANNEL7);
      for (volatile int i = 0; i < 1; ++i) ;
      assert( interrupt_DMA1_CHANNEL7 == highestpriority_interrupt());
      disable_interrupt(interrupt_DMA1_CHANNEL7);
      for (volatile int i = 0; i < 1; ++i) ;
      assert( 0 == highestpriority_interrupt());
   }
   // reset
   clear_interrupt(interrupt_DMA1_CHANNEL7);
   clear_coreinterrupt(coreinterrupt_MPUFAULT);
   disable_interrupt(interrupt_DMA1_CHANNEL7);
   disable_coreinterrupt(coreinterrupt_MPUFAULT);
   setpriority_interrupt(interrupt_DMA1_CHANNEL7, 0);
   setpriority_coreinterrupt(coreinterrupt_MPUFAULT, 0);
   clearprioritymask_interrupt();

   // TEST highestpriority_interrupt: interrupt with highest priority is returned
   assert( 0 == (hCORE->scb.icsr & HW_BIT(SCB, ICSR, VECTPENDING)));
   enable_interrupt(interrupt_GPIOPIN0);   // only enabled interrupts count
   for (uint32_t coreInt = 0; coreInt <= 2; ++coreInt) {
      setpriority_coreinterrupt(coreinterrupt_PENDSV, 1 + coreInt);
      for (uint32_t extInt = 0; extInt <= 2; ++extInt) {
         setpriority_interrupt(interrupt_GPIOPIN0, 1 + extInt);
         uint8_t intnr = 0;
         if (coreInt) {
            intnr = coreinterrupt_PENDSV;
         }
         if (extInt && (coreInt == 0 || extInt < coreInt)) {
            // with equal priority the one with the lower exc. number has higher priority
            // ==> extInt must be lower than coreInt (extInt has higher priority) to win
            intnr = interrupt_GPIOPIN0;
         }
         for (uint32_t disableType = 0; disableType <= 2; ++disableType) {
            // prepare
            switch (disableType) {
            case 0: disable_fault_interrupt(); break; // All exceptions with priority >= -1 prevented
            case 1: disable_all_interrupt(); break;   // All exceptions with priority >= 0 prevented
            case 2: setprioritymask_interrupt(2); break; // All exceptions with priority >= 2 prevented
            }
            if (coreInt) {
               generate_coreinterrupt(coreinterrupt_PENDSV);
            }
            if (extInt) {
               generate_interrupt(interrupt_GPIOPIN0);
            }

            // test
            for (volatile int i = 0; i < 1; ++i) ; // allow change to propagete to NVIC
            assert( intnr == highestpriority_interrupt());

            // reset
            clear_coreinterrupt(coreinterrupt_PENDSV);
            clear_interrupt(interrupt_GPIOPIN0);
            setprioritymask_interrupt(0);
            enable_fault_interrupt();
            enable_all_interrupt();
            assert( 0 == highestpriority_interrupt());
         }
      }
   }
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   setpriority_interrupt(interrupt_GPIOPIN0, 0);
   disable_interrupt(interrupt_GPIOPIN0);

   return 0;
}
