#include "konfig.h"

// volatile cpustate_t  cpustate;
volatile int      isDeactivateUsageFault;
volatile uint32_t counter;
volatile uint32_t pos[16];

static void nmi_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_NMI] = counter;
   assert( 0 == isactive_coreinterrupt(coreinterrupt_NMI));    // not supported by architecture
}

static void fault_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_FAULT] = counter;
   assert( 0 == isactive_coreinterrupt(coreinterrupt_FAULT));  // not supported by architecture

   if (isinit_cpustate(&cpustate)) {
      assert( 0 == isret2threadmode_interrupt());  // (at least) two nested interrupts
      if (isDeactivateUsageFault) {
         isDeactivateUsageFault = 0;
         hSCB->shcsr &= ~HW_BIT(SCB, SHCSR, USGFAULTACT);
      }
      ret2threadmode_cpustate(&cpustate);
   }
}

static void mpufault_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_MPUFAULT] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_MPUFAULT));
}

static void busfault_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_BUSFAULT] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_BUSFAULT));
}

static void usagefault_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_USAGEFAULT] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_USAGEFAULT));

   if (isinit_cpustate(&cpustate)) {
      assert( 1 == isret2threadmode_interrupt());  // single nested interrupt
      // generate !nested! fault interrupt
      __asm volatile(
         "mov r0, #0x10000000\n" // unknown SRAM address
         "ldr r0, [r0, #-4]\n"   // precise busfault
      );
   }
}

static void svcall_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_SVCALL] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_SVCALL));
}

static void debugmonitor_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_DEBUGMONITOR] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_DEBUGMONITOR));
}

static void pendsv_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_PENDSV] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_PENDSV));
}

static void systick_interrupt2(void)
{
   ++ counter;
   pos[coreinterrupt_SYSTICK] = counter;
   assert( 1 == isactive_coreinterrupt(coreinterrupt_SYSTICK));
}

int unittest_coreinterrupt(void)
{
   int err;
   uint32_t * const CCMRAM = (uint32_t*) 0x10000000; // start addr of 8K parity checked SRAM on STM32F303 devices

   // prepare
   assert( 8*1024/sizeof(uint32_t) > len_interruptTable())
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_SYSTICK] = (uintptr_t) &systick_interrupt2;
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt2;
   CCMRAM[coreinterrupt_DEBUGMONITOR] = (uintptr_t) &debugmonitor_interrupt2;
   CCMRAM[coreinterrupt_SVCALL] = (uintptr_t) &svcall_interrupt2;
   CCMRAM[coreinterrupt_USAGEFAULT] = (uintptr_t) &usagefault_interrupt2;
   CCMRAM[coreinterrupt_BUSFAULT] = (uintptr_t) &busfault_interrupt2;
   CCMRAM[coreinterrupt_MPUFAULT] = (uintptr_t) &mpufault_interrupt2;
   CCMRAM[coreinterrupt_FAULT] = (uintptr_t) &fault_interrupt2;
   CCMRAM[coreinterrupt_NMI] = (uintptr_t) &nmi_interrupt2;

   // TEST generate_coreinterrupt: execution of interrupts
   for (volatile uint32_t i = 0; i < 16; ++i) {
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
         assert( 0 == isactive_coreinterrupt((coreinterrupt_e)i));
         if (i == coreinterrupt_FAULT) {
            assert( EINVAL == generate_coreinterrupt((coreinterrupt_e)i));
            assert( 0 == counter && 0 == pos[i]);  // not executed
            __asm volatile(
               "mov r0, #0x20000000\n"
               "str r0, [r0, #-4]\n" // imprecise (async) bus fault
               ::: "r0"
            );
         } else {
            assert( 0 == generate_coreinterrupt((coreinterrupt_e)i));
         }
         if (  i == coreinterrupt_MPUFAULT
               || i == coreinterrupt_BUSFAULT
               || i == coreinterrupt_USAGEFAULT) {
            assert( 1 == is_coreinterrupt((coreinterrupt_e)i));   // pending
            assert( 0 == counter);                                // pending
            assert( 0 == isenabled_coreinterrupt((coreinterrupt_e)i));
            assert( 0 == enable_coreinterrupt((coreinterrupt_e)i));
            assert( 1 == isenabled_coreinterrupt((coreinterrupt_e)i));
            assert( 0 == is_coreinterrupt((coreinterrupt_e)i));   // interrupt executed (no more pending)
            assert( 1 == counter && 1 == pos[i]);                 // interrupt executed
            assert( 0 == disable_coreinterrupt((coreinterrupt_e)i));
            assert( 0 == isenabled_coreinterrupt((coreinterrupt_e)i));
         } else {
            assert( 1 == counter && 1 == pos[i]);                 // interrupt executed
            assert( 0 == is_coreinterrupt((coreinterrupt_e)i));   // interrupt executed (no more pending)
            if (i == coreinterrupt_DEBUGMONITOR) {
               // interrupt is active even if disabled
               // enabling is only used to activate debugmonitor_interrupt in case of a DEBUG event.
               // Used to implement SW-Debuggers. Does only work if no HW-Debugger is attached (isdebugger_dbg() == 0).
               assert( 0 == isenabled_coreinterrupt((coreinterrupt_e)i));
               assert( 0 == enable_coreinterrupt((coreinterrupt_e)i));
               assert( 1 == isenabled_coreinterrupt((coreinterrupt_e)i));
               assert( 0 == disable_coreinterrupt((coreinterrupt_e)i));
               assert( 0 == isenabled_coreinterrupt((coreinterrupt_e)i));
            } else {
               assert( 1 == isenabled_coreinterrupt((coreinterrupt_e)i));     // always on
               assert( EINVAL == enable_coreinterrupt((coreinterrupt_e)i));
               assert( EINVAL == disable_coreinterrupt((coreinterrupt_e)i));
            }
         }
         assert( 1 == counter);  // no second interrupt
         break;
      default:
         assert( EINVAL == generate_coreinterrupt((coreinterrupt_e)i));
         assert( EINVAL == enable_coreinterrupt((coreinterrupt_e)i));
         assert( EINVAL == disable_coreinterrupt((coreinterrupt_e)i));
         assert( 1 == isenabled_coreinterrupt((coreinterrupt_e)i));     // in error case also return 1
         assert( 0 == counter);
         break;
      }
      counter = 0;
      pos[i]  = 0;
      for (size_t p = 0; p < lengthof(pos); ++p) {
         assert( 0 == pos[p]);
      }
   }

   // TEST generate_coreinterrupt: disable_fault_interrupt ==> interrupt no active only pending
   for (volatile uint32_t i = 0; i < 16; ++i) {
      switch (i) {
      case coreinterrupt_FAULT:
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         disable_fault_interrupt();
         enable_coreinterrupt(i);
         if (i == coreinterrupt_FAULT) {
            assert( EINVAL == generate_coreinterrupt((coreinterrupt_e)i));
            __asm volatile(
               "mov r0, #0x20000000\n"
               "str r0, [r0, #-4]\n" // imprecise (async) bus fault
               ::: "r0"
            );
            // FAULT is ignored in case of imprecise busfault, precise busfault would lock up the cpu.
            assert( 0 == is_coreinterrupt((coreinterrupt_e)i));   // not implemented
            assert( EINVAL == clear_coreinterrupt((coreinterrupt_e)i));
         } else {
            assert( 0 == generate_coreinterrupt((coreinterrupt_e)i));
            assert( 1 == is_coreinterrupt((coreinterrupt_e)i));
            for (unsigned p = 0; p < 16; ++p) {
               assert( (p==i) == is_coreinterrupt((coreinterrupt_e)p));
            }
            assert( 0 == clear_coreinterrupt((coreinterrupt_e)i));
            assert( 0 == is_coreinterrupt((coreinterrupt_e)i));
         }
         for (unsigned p = 0; p < 16; ++p) {
            assert( 0 == is_coreinterrupt((coreinterrupt_e)p));
         }
         disable_coreinterrupt(i);
         enable_fault_interrupt();
         break;
      case coreinterrupt_NMI: // not maskable
      default:
         assert( 0 == is_coreinterrupt((coreinterrupt_e)i));
         assert( EINVAL == clear_coreinterrupt((coreinterrupt_e)i));
         break;
      }
      assert( 0 == counter);
      for (size_t p = 0; p < lengthof(pos); ++p) {
         assert( 0 == pos[p]);
      }
   }

   // TEST generate_coreinterrupt: interrupt is executed only if priority allows it
   for (volatile uint32_t i = 0; i < 16; ++i) {
      switch (i) {
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         enable_coreinterrupt(i);
         assert( 0 == setpriority_coreinterrupt((coreinterrupt_e)i, 3));
         assert( 3 == getpriority_coreinterrupt((coreinterrupt_e)i));
         setprioritymask_interrupt(3);
         assert( 0 == generate_coreinterrupt((coreinterrupt_e)i));
         assert( 1 == is_coreinterrupt((coreinterrupt_e)i));
         assert( 0 == setpriority_coreinterrupt((coreinterrupt_e)i, 2));
         assert( 2 == getpriority_coreinterrupt((coreinterrupt_e)i));
         assert( 0 == is_coreinterrupt((coreinterrupt_e)i));   // interrupt executed
         assert( 1 == counter && 1 == pos[i]);                 // interrupt executed
         // reset
         assert( 0 == setpriority_coreinterrupt((coreinterrupt_e)i, 0));
         assert( 0 == getpriority_coreinterrupt((coreinterrupt_e)i));
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
   disable_fault_interrupt();
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
   enable_fault_interrupt();
   pos[coreinterrupt_BUSFAULT] = 0;
   counter = 0;

   // TEST disable_synchronous_busfault_interrupt: ignore precise/synchronous bus fault during running at priority -1 (priority of masked fault)
   disable_synchronous_busfault_interrupt();
   assert( 1 == isignored_synchronous_busfault_interrupt());
   disable_fault_interrupt(); // this sets execution priority of thread to -1
   enable_coreinterrupt(coreinterrupt_BUSFAULT);
   __asm volatile(
      "push {r0}\n"
      "mov r0, #0x10000000\n"
      "ldr r0, [r0, #-4]\n"   // synchronous busfault is propagated to (hard-)fault and therefore ignored
      "pop {r0}\n"
   );
   assert( 0 == is_coreinterrupt(coreinterrupt_BUSFAULT));  // ignored !
   disable_coreinterrupt(coreinterrupt_BUSFAULT);
   enable_fault_interrupt();
   enable_synchronous_busfault_interrupt();
   assert( 0 == isignored_synchronous_busfault_interrupt());

   // TEST nested fault_interrupt: removes active state from preempted usagefault and returns to Thread mode
   isDeactivateUsageFault = 1;
   enable_coreinterrupt(coreinterrupt_USAGEFAULT);
   err = init_cpustate(&cpustate);
   if (err == 0) {
      generate_coreinterrupt(coreinterrupt_USAGEFAULT);
      assert( 0 /*never reached*/ );
   }
   assert( EINTR == err); // return from interrupt
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


   // TODO: add tests to check MPUFAULT and its configurations

   // reset
   reset_interruptTable();

   return 0;
}
