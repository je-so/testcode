
volatile uint32_t s_atomic_pendsvcounter;

void pendsv_interrupt3(void)
{
   ++ s_atomic_pendsvcounter;
}

int unittest_atomic(void)
{
   int err;
   uint32_t value;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   assert( !isinit_cpustate(&cpustate));
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable())
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt3;
   __asm volatile ("dsb");

   // TEST ldrex/strex: interrupted by pendsv_interrupt resets local monitor for x-locks
   setprio0mask_interrupt();
   generate_coreinterrupt(coreinterrupt_PENDSV);
   __asm volatile(
      "ldrex %0, [%1]\n"
      : "=r" (value) : "r" (&hSCB->shcsr) :
   );
   __asm volatile(
      "cpsie i\n"    // "cpsie i" same as call to clearprio0mask_interrupt
      "nop\n"
      "nop\n"
      "nop\n"
      "clrex\n"
      "strex %0, %1, [%2]\n"
      : "=&r" (err) : "r" (value), "r" (&hSCB->shcsr) :
   );
   assert(1 == err);
   assert(1 == s_atomic_pendsvcounter);
   s_atomic_pendsvcounter = 0;

   // TEST clearbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xffffffff;
         clearbits_atomic(&value, b1|b2);
         assert(value == (0xffffffff&~(b1|b2)));
         clearbits_atomic(&value, b1|b2);
         assert(value == (0xffffffff&~(b1|b2)));
      }
   }

   // TEST setbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xff00ff00;
         setbits_atomic(&value, b1|b2);
         assert(value == (0xff00ff00|b1|b2));
         setbits_atomic(&value, b1|b2);
         assert(value == (0xff00ff00|b1|b2));
      }
   }

   // TEST setclrbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xff00ff00;
         setclrbits_atomic(&value, b1|b2, 0);
         assert(value == (0xff00ff00|b1|b2));
         setclrbits_atomic(&value, 0, b1|b2);
         assert(value == (0xff00ff00&~(b2|b1)));
         value = 0x00ff00ff;
         setclrbits_atomic(&value, b1, b2);
         assert(value == ((0x00ff00ff&~b2)|b1));
         setclrbits_atomic(&value, b1, b2);
         assert(value == ((0x00ff00ff&~b2)|b1));
      }
   }

   // reset
   reset_interruptTable();

   return 0;
}
