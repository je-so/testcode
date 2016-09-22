#include "konfig.h"

volatile int      isDeactivateUsageFault;
volatile uint32_t counter;
volatile uint32_t pos[16];

static void nmi_interrupt4(void)
{
   pos[coreinterrupt_NMI] = (++ counter);
   assert( 1 == getfaultmask_interrupt());
   clearfaultmask_interrupt();  // clearing does work
   assert( 0 == getfaultmask_interrupt());
}

static void nmi_interrupt3(void)
{
   pos[coreinterrupt_NMI] = (++ counter);
   assert( 1 == getfaultmask_interrupt());   // not reset at return from NMI
}

static void nmi_interrupt2(void)
{
   pos[coreinterrupt_NMI] = (++ counter);
   assert( 0 == isactive_coreinterrupt(coreinterrupt_NMI));    // not supported by architecture

   setfaultmask_interrupt(); // does not work in NMI
   assert( 0 == getfaultmask_interrupt());
}

static void fault_interrupt2(void)
{
   pos[coreinterrupt_FAULT] = (++ counter);
   assert( 0 == isactive_coreinterrupt(coreinterrupt_FAULT));  // not supported by architecture

   setfaultmask_interrupt(); // does not work in FAULT
   assert( 0 == getfaultmask_interrupt());

   if (isinit_cpustate(&cpustate)) {
      if (isDeactivateUsageFault) {
         isDeactivateUsageFault = 0;
         hSCB->shcsr &= ~HW_BIT(SCB, SHCSR, USGFAULTACT);
         assert( 0 == isret2threadmode_interrupt());  // (at least) two nested interrupts
      } else if (isenabled_nested2threadmode_interrupt()) {
         assert( 0 == isret2threadmode_interrupt());  // (at least) two nested interrupts
      } else {
         assert( 1 == isret2threadmode_interrupt());  // no nested interrupts
      }
      ret2threadmode_cpustate(&cpustate);
   }
}

static void mpufault_interrupt2(void)
{
   pos[coreinterrupt_MPUFAULT] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_MPUFAULT));

   if (isinit_cpustate(&cpustate)) {
      assert( 1 == isret2threadmode_interrupt());  // no nested interrupts
      ret2threadmode_cpustate(&cpustate);
   }

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void busfault_interrupt2(void)
{
   pos[coreinterrupt_BUSFAULT] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_BUSFAULT));

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void usagefault_interrupt2(void)
{
   pos[coreinterrupt_USAGEFAULT] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));

   if (isinit_cpustate(&cpustate)) {
      assert( 1 == isret2threadmode_interrupt());  // single nested interrupt
      // generate !nested! fault interrupt
      __asm volatile(
         "mov r0, #0x10000000\n" // unknown SRAM address
         "ldr r0, [r0, #-4]\n"   // precise busfault
      );
   }

   disable_unalignedaccess_interrupt();
   disable_divby0_interrupt();

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void svcall_interrupt2(void)
{
   pos[coreinterrupt_SVCALL] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_SVCALL));

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void debugmonitor_interrupt2(void)
{
   pos[coreinterrupt_DEBUGMONITOR] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_DEBUGMONITOR));

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void pendsv_interrupt2(void)
{
   pos[coreinterrupt_PENDSV] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_PENDSV));

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

static void systick_interrupt2(void)
{
   pos[coreinterrupt_SYSTICK] = (++ counter);
   assert( 1 == isactive_coreinterrupt(coreinterrupt_SYSTICK));

   setfaultmask_interrupt();
   assert( 1 == getfaultmask_interrupt());   // reset at return
}

int unittest_coreinterrupt(void)
{
   int err;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   assert( !isinit_cpustate(&cpustate));
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable())
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_SYSTICK] = (uintptr_t) &systick_interrupt2;
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt2;
   CCMRAM[coreinterrupt_DEBUGMONITOR] = (uintptr_t) &debugmonitor_interrupt2;
   CCMRAM[coreinterrupt_SVCALL] = (uintptr_t) &svcall_interrupt2;
   CCMRAM[coreinterrupt_USAGEFAULT] = (uintptr_t) &usagefault_interrupt2;
   CCMRAM[coreinterrupt_BUSFAULT] = (uintptr_t) &busfault_interrupt2;
   CCMRAM[coreinterrupt_MPUFAULT] = (uintptr_t) &mpufault_interrupt2;
   CCMRAM[coreinterrupt_FAULT] = (uintptr_t) &fault_interrupt2;
   CCMRAM[coreinterrupt_NMI] = (uintptr_t) &nmi_interrupt4;
   __asm volatile ("dsb");

   // TEST coreinterrupt_NMI: clearing FAULTMASK is possible in NMI
   setfaultmask_interrupt();
   assert( 0 == generate_coreinterrupt(coreinterrupt_NMI));
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 1 == counter && 1 == pos[coreinterrupt_NMI]); // interrupt executed
   assert( 0 == getfaultmask_interrupt());               // FAULTMASK cleared in NMI
   // reset
   counter = 0;
   pos[coreinterrupt_NMI] = 0;

   // TEST coreinterrupt_NMI: does not clear FAULTMASK at return of interrupt
   CCMRAM[coreinterrupt_NMI] = (uintptr_t) &nmi_interrupt3;
   __asm volatile ("dsb");
   setfaultmask_interrupt();
   assert( 0 == generate_coreinterrupt(coreinterrupt_NMI));
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 1 == counter && 1 == pos[coreinterrupt_NMI]); // interrupt executed
   assert( 1 == getfaultmask_interrupt());               // not reset during return from NMI
   // reset
   clearfaultmask_interrupt();
   counter = 0;
   pos[coreinterrupt_NMI] = 0;

   // TEST generate_coreinterrupt: execution of interrupts
   CCMRAM[coreinterrupt_NMI] = (uintptr_t) &nmi_interrupt2;
   __asm volatile ("dsb");
   for (volatile coreinterrupt_e i = 0; i < 16; ++i) {
      switch (i) {
      case coreinterrupt_NMI:
      case coreinterrupt_FAULT:
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         assert( 0 == isactive_coreinterrupt(i));
         if (i == coreinterrupt_FAULT) {
            assert( EINVAL == generate_coreinterrupt(i));
            assert( 0 == counter && 0 == pos[i]);  // not executed
            __asm volatile(
               "mov r0, #0x20000000\n"
               "str r0, [r0, #-4]\n" // imprecise (async) bus fault
               ::: "r0"
            );
         } else {
            assert( 0 == generate_coreinterrupt(i));
         }
         if (  i == coreinterrupt_MPUFAULT
               || i == coreinterrupt_BUSFAULT
               || i == coreinterrupt_USAGEFAULT) {
            assert( 1 == is_coreinterrupt(i));           // pending
            assert( 0 == counter);                       // pending
            assert( 0 == isenabled_coreinterrupt(i));
            assert( 0 == enable_coreinterrupt(i));
            assert( 1 == isenabled_coreinterrupt(i));
            assert( 0 == is_coreinterrupt(i));           // interrupt executed (no more pending)
            assert( 1 == counter && 1 == pos[i]);        // interrupt executed
            assert( 0 == disable_coreinterrupt(i));
            assert( 0 == isenabled_coreinterrupt(i));
         } else {
            assert( 1 == counter && 1 == pos[i]);        // interrupt executed
            assert( 0 == is_coreinterrupt(i));           // interrupt executed (no more pending)
            if (i == coreinterrupt_DEBUGMONITOR) {
               // interrupt is active even if disabled
               // enabling is only used to activate debugmonitor_interrupt in case of a DEBUG event.
               // Used to implement SW-Debuggers. Does only work if no HW-Debugger is attached (isdebugger_dbg() == 0).
               assert( 0 == isenabled_coreinterrupt(i));
               assert( 0 == enable_coreinterrupt(i));
               assert( 1 == isenabled_coreinterrupt(i));
               assert( 0 == disable_coreinterrupt(i));
               assert( 0 == isenabled_coreinterrupt(i));
            } else {
               assert( 1 == isenabled_coreinterrupt(i));     // always on
               assert( EINVAL == enable_coreinterrupt(i));
               assert( EINVAL == disable_coreinterrupt(i));
            }
         }
         assert( 1 == counter);  // no second interrupt
         assert( 0 == getfaultmask_interrupt());
         break;
      default:
         assert( EINVAL == generate_coreinterrupt(i));
         assert( EINVAL == enable_coreinterrupt(i));
         assert( EINVAL == disable_coreinterrupt(i));
         assert( 1 == isenabled_coreinterrupt(i));       // in error case also return 1
         assert( 0 == counter);
         break;
      }
      counter = 0;
      pos[i]  = 0;
      for (size_t p = 0; p < lengthof(pos); ++p) {
         assert( 0 == pos[p]);
      }
   }

   // TEST generate_coreinterrupt: setfaultmask_interrupt ==> interrupt no active only pending
   for (volatile coreinterrupt_e i = 0; i < 16; ++i) {
      switch (i) {
      case coreinterrupt_FAULT:
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         setfaultmask_interrupt();
         enable_coreinterrupt(i);
         if (i == coreinterrupt_FAULT) {
            assert( EINVAL == generate_coreinterrupt(i));
            __asm volatile(
               "mov r0, #0x20000000\n"
               "str r0, [r0, #-4]\n" // imprecise (async) bus fault
               ::: "r0"
            );
            // FAULT is ignored in case of imprecise busfault, precise busfault would lock up the cpu.
            assert( 0 == is_coreinterrupt(i));   // not implemented
            assert( EINVAL == clear_coreinterrupt(i));
         } else {
            assert( 0 == generate_coreinterrupt(i));
            assert( 1 == is_coreinterrupt(i));
            for (unsigned p = 0; p < 16; ++p) {
               assert( (p==i) == is_coreinterrupt(p));
            }
            assert( 0 == clear_coreinterrupt(i));
            assert( 0 == is_coreinterrupt(i));
         }
         for (unsigned p = 0; p < 16; ++p) {
            assert( 0 == is_coreinterrupt(p));
         }
         disable_coreinterrupt(i);
         clearfaultmask_interrupt();
         break;
      case coreinterrupt_NMI: // not maskable
      default:
         assert( 0 == is_coreinterrupt(i));
         assert( EINVAL == clear_coreinterrupt(i));
         break;
      }
      assert( 0 == counter);
      for (size_t p = 0; p < lengthof(pos); ++p) {
         assert( 0 == pos[p]);
      }
   }

   // TEST generate_coreinterrupt: interrupt is executed only if priority allows it
   for (volatile coreinterrupt_e i = 0; i < 16; ++i) {
      switch (i) {
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         enable_coreinterrupt(i);
         assert( 0 == setpriority_coreinterrupt(i, 3));
         assert( 3 == getpriority_coreinterrupt(i));
         setprioritymask_interrupt(3);
         assert( 0 == is_any_coreinterrupt());
         assert( 0 == generate_coreinterrupt(i));
         assert( 1 == is_coreinterrupt(i));
         assert( 1 == is_any_coreinterrupt());
         assert( 0 == setpriority_coreinterrupt(i, 2));
         assert( 2 == getpriority_coreinterrupt(i));
         assert( 0 == is_coreinterrupt(i));           // interrupt executed
         assert( 1 == counter && 1 == pos[i]);        // interrupt executed
         // reset
         assert( 0 == setpriority_coreinterrupt(i, 0));
         assert( 0 == getpriority_coreinterrupt(i));
         clearprioritymask_interrupt();
         disable_coreinterrupt(i);
         break;
      case coreinterrupt_NMI: // not priority maskable
      default:
         break;
      }
      counter = 0;
      pos[i]  = 0;
      for (size_t p = 0; p < lengthof(pos); ++p) {
         assert( 0 == pos[p]);
      }
   }

   // TEST disable_coreinterrupt: MPUFAULT, BUSFAULT, USAGEFAULT
   assert( 0 == generate_coreinterrupt(coreinterrupt_MPUFAULT));
   assert( 0 == generate_coreinterrupt(coreinterrupt_BUSFAULT));
   assert( 0 == generate_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 1 == is_coreinterrupt(coreinterrupt_MPUFAULT));
   assert( 1 == is_coreinterrupt(coreinterrupt_BUSFAULT));
   assert( 1 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 0 == clear_coreinterrupt(coreinterrupt_MPUFAULT));
   assert( 0 == clear_coreinterrupt(coreinterrupt_BUSFAULT));
   assert( 0 == clear_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 0 == is_coreinterrupt(coreinterrupt_MPUFAULT));
   assert( 0 == is_coreinterrupt(coreinterrupt_BUSFAULT));
   assert( 0 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 0 == counter);

   // TEST coreinterrupt_BUSFAULT: imprecise (asynchronous) data access errors are set pending and not propagated to FAULT
   enable_coreinterrupt(coreinterrupt_BUSFAULT);   // if not enabled asynchronous busfault would be ignored completely
   __asm volatile(
      "mov r0, #0x20000000\n"
      "str r0, [r0, #-4]\n"   // BUSFAULT is generated later cause of write-buffer (called asynchronous or imprecise)
      ::: "r0"
   );
   for (volatile uint32_t i = 0; i < 2; ++i) ;
   assert( 1 == counter && 1 == pos[coreinterrupt_BUSFAULT]);  // interrupt executed
   setprio0mask_interrupt();   // same result if called setfaultmask_interrupt or setpriority_coreinterrupt/setprioritymask_interrupt instead
   __asm volatile(
      "mov r0, #0x10000000\n"
      "str r0, [r0, #-4]\n"   // BUSFAULT is generated later cause of write-buffer (called asynchronous or imprecise)
      ::: "r0"
   );
   assert( 1 == is_coreinterrupt(coreinterrupt_BUSFAULT));
   assert( coreinterrupt_BUSFAULT == highestpriority_interrupt());
   // reset
   clear_coreinterrupt(coreinterrupt_BUSFAULT);
   disable_coreinterrupt(coreinterrupt_BUSFAULT);
   clearprio0mask_interrupt();
   pos[coreinterrupt_BUSFAULT] = 0;
   counter = 0;

   // TEST enable_ignoresyncbusfault_interrupt: ignore precise/synchronous bus fault during running at priority -1 (priority of masked fault)
   assert( 0 == isenabled_ignoresyncbusfault_interrupt());  // default after reset
   enable_ignoresyncbusfault_interrupt();
   assert( 1 == isenabled_ignoresyncbusfault_interrupt());
   setfaultmask_interrupt(); // this sets execution priority of thread to -1
   enable_coreinterrupt(coreinterrupt_BUSFAULT);
   __asm volatile(
      "mov r0, #0x10000000\n"
      "ldr r0, [r0, #-4]\n"   // synchronous busfault is propagated to (hard-)fault and therefore ignored
      "mov %0, r0\n"
      : "=r" (err) :: "r0"    // returns r0 in err
   );
   assert( 0 == err);         // register r0 set to 0 cause of ignored busfault
   assert( 0 == is_coreinterrupt(coreinterrupt_BUSFAULT));  // ignored !
   disable_coreinterrupt(coreinterrupt_BUSFAULT);
   clearfaultmask_interrupt();
   disable_ignoresyncbusfault_interrupt();
   assert( 0 == isenabled_ignoresyncbusfault_interrupt());

   // TEST coreinterrupt_MPUFAULT: try executing data in a XN (execute never) region
   enable_coreinterrupt(coreinterrupt_MPUFAULT);
   err = init_cpustate(&cpustate);
   if (err == 0) {
      __asm volatile(
         "movw r0, #0xed00\n" // SCB at PPB bus is marked XN in the default system map
         "movt r0, #0xe000\n"
         "bx  r0\n"           // Try to execute data in a XN region ==> MPUFAULT
      );
      assert( 0 /*never reached*/ );
   }
   assert( EINTR == err); // ret2threadmode_cpustate called in fault_interrupt2
   free_cpustate(&cpustate);
   assert( 1 == counter);
   assert( 1 == pos[coreinterrupt_MPUFAULT]);
   // reset
   disable_coreinterrupt(coreinterrupt_MPUFAULT);
   counter = 0;
   pos[coreinterrupt_MPUFAULT] = 0;

   // TEST coreinterrupt_MPUFAULT: Disallow access to memory region with enabled MPU && (execution priority >= 0)
   enable_coreinterrupt(coreinterrupt_MPUFAULT);
   mpu_region_t reg = mpu_region_INIT(0x10000000, mpu_size_8K, 0, mpu_mem_NORMAL(mpu_cache_WB), mpu_access_NONE, mpu_access_NONE);
   config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE);   // disallow privileged access to 8K CCMRAM
   err = init_cpustate(&cpustate);
   if (err == 0) {
      __asm volatile(
         "mov r0, #0x10000000\n" // privileged access of 0x10000000 with privileged MPU access level of mpu_access_NONE
         "ldr r0, [r0]\n"        // MPU used cause execution priority >= 0 ==> MPUFAULT generated!
         ::: "r0"
      );
      assert( 0 /*never reached*/ );
   }
   assert( EINTR == err); // ret2threadmode_cpustate called in mpu_interrupt
   free_cpustate(&cpustate);
   assert( 1 == counter);
   assert( 1 == pos[coreinterrupt_MPUFAULT]);
   // reset
   disable_mpu();
   disable_coreinterrupt(coreinterrupt_MPUFAULT);
   counter = 0;
   pos[coreinterrupt_MPUFAULT] = 0;

   // TEST HW_BIT(MPU, CTRL, HFNMIENA) == 0: Disallow access to memory region with enabled MPU && (execution priority < 0)
   enable_coreinterrupt(coreinterrupt_MPUFAULT);
   config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE);   // disallow privileged access to 8K CCMRAM
   // TRY using
   // hMPU->ctrl |= HW_BIT(MPU, CTRL, HFNMIENA);
   // which would enable MPU even for execution priority < 0
   // this leads to a lockup of the CPU, cause of masked fault interrupt.
   setfaultmask_interrupt();
   __asm volatile(
      "mov r0, #0x10000000\n" // privileged access of 0x10000000 with privileged MPU access level of mpu_access_NONE
      "ldr r0, [r0]\n"        // MPU not used cause execution priority == -1 (< 0) ==> NO MPUFAULT!
      ::: "r0"
   );
   clearfaultmask_interrupt();
   assert( 0 == counter);
   // reset
   disable_mpu();
   disable_coreinterrupt(coreinterrupt_MPUFAULT);

   // TEST nested fault_interrupt: removes active state from preempted usagefault and returns to Thread mode
   isDeactivateUsageFault = 1;
   enable_coreinterrupt(coreinterrupt_USAGEFAULT);
   err = init_cpustate(&cpustate);
   if (err == 0) {
      generate_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 0 /*never reached*/ );
   }
   assert( EINTR == err); // ret2threadmode_cpustate called in fault_interrupt2
   free_cpustate(&cpustate);
   assert( 2 == counter);
   assert( 1 == pos[coreinterrupt_USAGEFAULT]); // preempted interrupt
   assert( 2 == pos[coreinterrupt_FAULT]);      // interrupt which preempted USAGEFAULT
   assert( 0 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT)); // set deactive in fault_interrupt
   assert( 0 == isDeactivateUsageFault);        // reset in fault_interrupt
   // reset
   disable_coreinterrupt(coreinterrupt_USAGEFAULT);
   counter = 0;
   pos[coreinterrupt_USAGEFAULT] = 0;
   pos[coreinterrupt_FAULT] = 0;

   // TEST nested fault_interrupt: enable_nested2threadmode_interrupt() allows it to return to Thread mode despite preempted active usagefault
   assert( 0 == isenabled_nested2threadmode_interrupt());  // default after reset
   enable_nested2threadmode_interrupt();
   assert( 1 == isenabled_nested2threadmode_interrupt());
   enable_coreinterrupt(coreinterrupt_USAGEFAULT);
   err = init_cpustate(&cpustate);
   if (err == 0) {
      generate_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 0 /*never reached*/ );
   }
   assert( EINTR == err); // ret2threadmode_cpustate called in fault_interrupt2
   free_cpustate(&cpustate);
   assert( 2 == counter);
   assert( 1 == pos[coreinterrupt_USAGEFAULT]); // preempted interrupt
   assert( 2 == pos[coreinterrupt_FAULT]);      // interrupt which preempted USAGEFAULT
   assert( 0 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));   // Thread mode priority is currently at USAGEFAULT level
   generate_coreinterrupt(coreinterrupt_USAGEFAULT);
   assert( 1 == is_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));
   assert( 2 == counter);  // usagefault not activated cause Thread mode priority is currently at USAGEFAULT level
   clearbits_atomic(&hSCB->shcsr, HW_BIT(SCB, SHCSR, USGFAULTACT));
   assert( 0 == (HW_REGISTER(SCB, SHCSR) & 0xfff));   // no other coreinterrupt_e active
   assert( 3 == counter);                             // interrupt now called
   assert( 3 == pos[coreinterrupt_USAGEFAULT]);       // cause Thread mode priority set back to normal (lowest)
   // reset
   disable_coreinterrupt(coreinterrupt_USAGEFAULT);
   disable_nested2threadmode_interrupt();
   assert( 0 == isenabled_nested2threadmode_interrupt());
   counter = 0;
   pos[coreinterrupt_USAGEFAULT] = 0;
   pos[coreinterrupt_FAULT] = 0;

   // TEST coreinterrupt_USAGEFAULT: Trap on unaligned access
   assert( 0 == isenabled_unalignedaccess_interrupt()); // default after reset
   enable_coreinterrupt(coreinterrupt_USAGEFAULT);
   for (unsigned isOn = 1; isOn <= 1; --isOn) {
      if (isOn) {
         enable_unalignedaccess_interrupt();
      } else {
         disable_unalignedaccess_interrupt();
      }
      assert( (int)isOn == isenabled_unalignedaccess_interrupt());
      volatile uint32_t data[2] = { 0, 0 };
      assert( 0 == *(uint32_t*)(1+(uintptr_t)data));  // usagefault_interrupt disables unalignedaccess
      assert( isOn == counter);
      assert( isOn == pos[coreinterrupt_USAGEFAULT]);
      // reset
      counter = 0;
      pos[coreinterrupt_USAGEFAULT] = 0;
   }
   disable_coreinterrupt(coreinterrupt_USAGEFAULT);

   // TEST coreinterrupt_USAGEFAULT: Trap on div by 0
   assert( 0 == isenabled_divby0_interrupt()); // default after reset
   enable_coreinterrupt(coreinterrupt_USAGEFAULT);
   for (unsigned isOn = 1; isOn <= 1; --isOn) {
      if (isOn) {
         enable_divby0_interrupt();
      } else {
         disable_divby0_interrupt();
      }
      assert( (int)isOn == isenabled_divby0_interrupt());
      volatile uint32_t divisor = 0;
      volatile uint32_t result = 10 / divisor;  // usagefault_interrupt disables divby0
      assert( 0 == result);
      assert( isOn == counter);
      assert( isOn == pos[coreinterrupt_USAGEFAULT]);
      // reset
      counter = 0;
      pos[coreinterrupt_USAGEFAULT] = 0;
   }
   disable_coreinterrupt(coreinterrupt_USAGEFAULT);

   // TEST waitinterrupt_core: effects of PRIMASK/setprio0mask_interrupt are ignored
   config_systick(1000, systickcfg_START|systickcfg_INTERRUPT);
   setprio0mask_interrupt();
   waitinterrupt_core();   // wakes up with systick active interrupt (which will be not active cause of PRIMASK)
   clearprio0mask_interrupt();
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 1 == isexpired_systick());
   stop_systick();
   assert( 0 == is_coreinterrupt(coreinterrupt_SYSTICK));
   assert( 1 == counter)
   assert( 1 == pos[coreinterrupt_SYSTICK]);
   // reset
   assert( 0 == is_any_coreinterrupt());
   counter = 0;
   pos[coreinterrupt_SYSTICK] = 0;

   // TEST setevent_onpending_interrupt(0): only active interrupt generates event
   assert( 0 == isevent_onpending_interrupt()); // default after reset
   setevent_core();  // set eventflag
   waitevent_core(); // clear eventflag
   generate_coreinterrupt(coreinterrupt_MPUFAULT); // generates no event
   config_systick(1000, systickcfg_START|systickcfg_INTERRUPT);
   waitevent_core(); // wakes up with systick active interrupt
   assert( 1 == isexpired_systick());
   stop_systick();
   assert( 0 == is_coreinterrupt(coreinterrupt_SYSTICK));
   assert( 1 == counter)
   assert( 1 == pos[coreinterrupt_SYSTICK]);
   // reset
   clear_coreinterrupt(coreinterrupt_MPUFAULT);
   assert( 0 == is_any_coreinterrupt());
   counter = 0;
   pos[coreinterrupt_SYSTICK] = 0;

   // TEST setevent_onpending_interrupt(1): pending interrupt generates event
   setevent_core();  // set eventflag
   waitevent_core(); // clear eventflag
   setevent_onpending_interrupt(1);
   assert( 1 == isevent_onpending_interrupt());
   assert( 0 == isenabled_coreinterrupt(coreinterrupt_MPUFAULT));
   generate_coreinterrupt(coreinterrupt_MPUFAULT); // generates event
   waitevent_core(); // wait for event, after return eventflag cleared
   generate_coreinterrupt(coreinterrupt_MPUFAULT); // already pending ==> no new event generated
   setprio0mask_interrupt();
   clear_coreinterrupt(coreinterrupt_SYSTICK);
   config_systick(2000, systickcfg_CORECLOCK|systickcfg_START|systickcfg_INTERRUPT);
   waitevent_core(); // wakes up with systick pending interrupt
   for (volatile int i = 0; i < 1; ++i) ;
   assert( 1 == is_coreinterrupt(coreinterrupt_SYSTICK));
   assert( 1 == isexpired_systick());
   stop_systick();
   // reset
   setevent_onpending_interrupt(0);
   assert( 0 == isevent_onpending_interrupt());
   clear_coreinterrupt(coreinterrupt_MPUFAULT);
   clear_coreinterrupt(coreinterrupt_SYSTICK);
   assert( 0 == is_any_coreinterrupt());
   clearprio0mask_interrupt();

   // reset
   reset_interruptTable();

   return 0;
}
