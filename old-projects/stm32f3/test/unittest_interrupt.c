
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

static void fault_interrupt4(void)
{
   if (isinit_cpustate(&cpustate)) {
      ret2threadmode_cpustate(&cpustate);
   }
   assert(0);
}


int unittest_interrupt(void)
{
   int err;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   assert( !isinit_cpustate(&cpustate));
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable())
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_FAULT] = (uintptr_t) &fault_interrupt4;
   __asm volatile ("dsb");

   // TEST retcode_interrupt
   assert( 0xFFFFFFE1 == retcode_interrupt(interrupt_retcode_FPU|interrupt_retcode_HANDLERMODE));
   assert( 0xFFFFFFE9 == retcode_interrupt(interrupt_retcode_FPU|interrupt_retcode_THREADMODE_MSP));
   assert( 0xFFFFFFED == retcode_interrupt(interrupt_retcode_FPU|interrupt_retcode_THREADMODE_PSP));
   assert( 0xFFFFFFF1 == retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_HANDLERMODE));
   assert( 0xFFFFFFF9 == retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_THREADMODE_MSP));
   assert( 0xFFFFFFFD == retcode_interrupt(interrupt_retcode_NOFPU|interrupt_retcode_THREADMODE_PSP));

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

   // TEST STIR: generate interrupt
   for (uint32_t i = 16; i <= HW_KONFIG_NVIC_INTERRUPT_MAXNR; ++i) {
      assert( ! is_interrupt(i));
      hCORE->stir = i-16;
      assert(   is_interrupt(i));
      assert( 0 == clear_interrupt(i));
   }

   // TEST STIR: unprivileged access => BUS-FAULT
   assert(0 == is_interrupt(interrupt_PVD));
   err = init_cpustate(&cpustate);
   if (err == 0) {
      __asm volatile(
         "strt %0, [%1]\n" // BUSFAULT -> escalates to FAULT
         :: "r" (interrupt_PVD-16), "r" (&hCORE->stir)
      );
   }
   assert(EINTR == err);
   free_cpustate(&cpustate);

   // TEST STIR: supports unprivileged access if enabled
   assert(0 == is_interrupt(interrupt_PVD));
   // TODO: implement functions enable_unprivgenerate_interrupt/disable_unprivgenerate_interrupt
   hSCB->ccr |= HW_BIT(SCB, CCR, USERSETMPEND); // enable unprivileged access of STIR
   __asm volatile(
      "dsb\n"
      "strt %0, [%1]\n"    // generates interrupt with unprivileged access
      :: "r" (interrupt_PVD-16), "r" (&hCORE->stir)
   );
   assert(1 == is_interrupt(interrupt_PVD));
   // reset
   clear_interrupt(interrupt_PVD);
   hSCB->ccr &= ~HW_BIT(SCB, CCR, USERSETMPEND); // disable unprivileged access of STIR

   // TEST interrupt_TIMER6_DAC execution
   assert( 0 == generate_interrupt(interrupt_TIMER6_DAC));
   assert( 1 == is_interrupt(interrupt_TIMER6_DAC));
   assert( 0 == s_interrupt_counter6);    // not executed
   __asm volatile( "sev\nwfe\n");         // clear event flag
   assert( 0 == enable_interrupt(interrupt_TIMER6_DAC));
   for (volatile int i = 0; i < 1000; ++i) ;
   assert( 0 == is_interrupt(interrupt_TIMER6_DAC));
   assert( 1 == s_interrupt_counter6);    // executed
   assert( 0 == disable_interrupt(interrupt_TIMER6_DAC));
   waitevent_core();         // interrupt exit sets event flag ==> wfe returns immediately
   s_interrupt_counter6 = 0;

   // TEST interrupt_TIMER7 execution
   assert( 0 == is_interrupt(interrupt_TIMER7));
   assert( 0 == enable_interrupt(interrupt_TIMER7));
   assert( 0 == config_basictimer(TIMER7, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT));
   assert( 0 == s_interrupt_counter7); // not executed
   start_basictimer(TIMER7);
   assert( isstarted_basictimer(TIMER7));
   wait_interrupt();
   assert( 0 == is_interrupt(interrupt_TIMER7));
   assert( 1 == s_interrupt_counter7); // executed
   assert( 0 == disable_interrupt(interrupt_TIMER7));
   s_interrupt_counter7 = 0;

   // TEST wait_interrupt: Effekte von PRIMASK werden ignoriert
   setprio0mask_interrupt();
   assert(0 == is_any_coreinterrupt());
   assert(0 == is_any_interrupt());
   generate_coreinterrupt(coreinterrupt_PENDSV);
   wait_interrupt();
   clear_coreinterrupt(coreinterrupt_PENDSV);
   assert(0 == is_any_coreinterrupt());
   assert(0 == is_any_interrupt());
   enable_interrupt(interrupt_PVD);
   generate_interrupt(interrupt_PVD);
   wait_interrupt();
   clear_interrupt(interrupt_PVD);
   disable_interrupt(interrupt_PVD);
   clearprio0mask_interrupt();
   assert(0 == is_any_coreinterrupt());
   assert(0 == is_any_interrupt());

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
   setprio0mask_interrupt();
   generate_coreinterrupt(coreinterrupt_SYSTICK);
   assert( 0 == is_any_interrupt());
   clear_coreinterrupt(coreinterrupt_SYSTICK);
   generate_interrupt(interrupt_GPIOPIN0);
   assert( 1 == is_any_interrupt());
   clear_interrupt(interrupt_GPIOPIN0);
   clearprio0mask_interrupt();

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
            case 0: setfaultmask_interrupt(); break; // All exceptions with priority >= -1 prevented
            case 1: setprio0mask_interrupt(); break;   // All exceptions with priority >= 0 prevented
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
            clearfaultmask_interrupt();
            clearprio0mask_interrupt();
            assert( 0 == highestpriority_interrupt());
         }
      }
   }
   setpriority_coreinterrupt(coreinterrupt_PENDSV, 0);
   setpriority_interrupt(interrupt_GPIOPIN0, 0);
   disable_interrupt(interrupt_GPIOPIN0);

   // reset
   reset_interruptTable();

   return 0;
}
