/* title: Atomic-Operations impl

   Implements <Atomic-Operations>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: hw/cm4/atomic.h
    Header file <Atomic-Operations>.

   file: hw/cm4/atomic.c
    Implementation file <Atomic-Operations impl>.
*/
#include "konfig.h"        // needs µC/interrupt.h
#include "hw/cm4/msync.h"
#include "hw/cm4/atomic.h"


#ifdef KONFIG_UNITTEST

static volatile uint32_t s_pendsvcounter;

static void pendsv_interrupt(void)
{
   ++ s_pendsvcounter;
}

int unittest_hw_cortexm4_atomic()
{
   int err;
   volatile uint32_t value;
   volatile int32_t  svalue;
   void   * volatile ptrval;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   TEST( 0 == s_pendsvcounter);
   TEST( CCMRAM_SIZE/sizeof(uintptr_t) > len_interruptTable());
   TEST( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt;
   wait_msync();

   // TEST ldrex/strex: interrupted by pendsv_interrupt resets local monitor for x-locks
   setprio0mask_interrupt();
   generate_coreinterrupt(coreinterrupt_PENDSV);
   __asm volatile(
      "ldrex %0, [%1]\n"   // acquire local monitor lock
      : "=r" (value) : "r" (&hSCB->shcsr) :
   );
   __asm volatile(
      "cpsie i\n"    // "cpsie i" same as call to clearprio0mask_interrupt
      "nop\n"
      "strex %0, %1, [%2]\n"  // interrupt did the same as calling clrex
      : "=&r" (err) : "r" (value), "r" (&hSCB->shcsr) :
   );
   TEST( 1 == err);  // strex failed
   TEST( 1 == s_pendsvcounter);  // interrupt executed
   s_pendsvcounter = 0;

   // TEST clearbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xffffffff;
         clearbits_atomic(&value, b1|b2);
         TEST(value == (0xffffffff&~(b1|b2)));
         clearbits_atomic(&value, b1|b2);
         TEST(value == (0xffffffff&~(b1|b2)));
      }
   }

   // TEST setbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xff00ff00;
         setbits_atomic(&value, b1|b2);
         TEST(value == (0xff00ff00|b1|b2));
         setbits_atomic(&value, b1|b2);
         TEST(value == (0xff00ff00|b1|b2));
      }
   }

   // TEST setclrbits_atomic
   for (uint32_t b1 = 1; b1; b1 <<= 1) {
      for (uint32_t b2 = 1; b2; b2 <<= 1) {
         value = 0xff00ff00;
         setclrbits_atomic(&value, b1|b2, 0);
         TEST(value == (0xff00ff00|b1|b2));
         setclrbits_atomic(&value, 0, b1|b2);
         TEST(value == (0xff00ff00&~(b2|b1)));
         value = 0x00ff00ff;
         setclrbits_atomic(&value, b1, b2);
         TEST(value == ((0x00ff00ff&~b2)|b1));
         setclrbits_atomic(&value, b1, b2);
         TEST(value == ((0x00ff00ff&~b2)|b1));
      }
   }

   // TEST trylock_atomic
   for (uint32_t i = 0; i < 100; ++i) {
      value = 0;
      TEST( 0 == trylock_atomic(&value));
      TEST( 1 == value);
      TEST( 1 == trylock_atomic(&value));
      TEST( 1 == value);
      value = ~i;
      TEST( ~i == trylock_atomic(&value));
      TEST( ~i == value);
   }

   // TEST unlock_atomic
   for (uint32_t i = 0; i < 100; ++i) {
      value = i;
      unlock_atomic(&value);
      TEST( 0 == value);
      value = ~i;
      unlock_atomic(&value);
      TEST( 0 == value);
   }

   // TEST increment32_atomic
   for (uint32_t i = 0; i < 100; ++i) {
      value = i;
      TEST( i+1 == increment_atomic(&value));
      TEST( i+1 == value);
      value = ~i;
      TEST( -i == increment_atomic(&value));
      TEST( -i == value);
   }

   // TEST decrement32_atomic
   for (uint32_t i = 0; i < 100; ++i) {
      value = i;
      TEST( i-1 == decrement_atomic(&value));
      TEST( i-1 == value);
      value = -i;
      TEST( ~i == decrement_atomic(&value));
      TEST( ~i == value);
   }

   // TEST increment16_atomic
   for (uint32_t off = 0; off < 2; ++off) {
      uint16_t volatile * ptr16 = off + (uint16_t volatile*)&value;
      value = 0;
      for (uint32_t i = 0; i < 100; ++i) {
         *ptr16 = (uint16_t) i;
         TEST( i+1 == increment_atomic(ptr16));
         TEST( ((i+1)<<(16*off)) == value);
         *ptr16 = (uint16_t) ~i;
         TEST( (uint16_t) -i == increment_atomic(ptr16));
         TEST( ((uint16_t)(-i)<<(16*off)) == value);
      }
   }

   // TEST decrement16_atomic
   for (uint32_t off = 0; off < 2; ++off) {
      uint16_t volatile * ptr16 = off + (uint16_t volatile*)&value;
      value = 0;
      for (uint32_t i = 0; i < 100; ++i) {
         *ptr16 = (uint16_t) i;
         TEST( (uint16_t) (i-1) == decrement_atomic(ptr16));
         TEST( ((uint16_t)(i-1)<<(16*off)) == value);
         *ptr16 = (uint16_t) -i;
         TEST( (uint16_t) ~i == decrement_atomic(ptr16));
         TEST( ((uint16_t)(~i)<<(16*off)) == value);
      }
   }

   // TEST decrementpositive_atomic: value == INT32_MIN
   svalue = INT32_MIN;
   TEST( INT32_MIN == decrementpositive_atomic(&svalue));
   TEST( INT32_MIN == svalue);

   // TEST decrementpositive_atomic: value == INT32_MAX
   svalue = INT32_MAX;
   TEST( INT32_MAX == decrementpositive_atomic(&svalue));
   TEST( INT32_MAX-1 == svalue);

   // TEST decrementpositive_atomic: value == 0
   svalue = 0;
   TEST( 0 == decrementpositive_atomic(&svalue));
   TEST( 0 == svalue);

   // TEST decrementpositive_atomic: value > 0 && value < 0
   for (int32_t i = 1; i < 100; ++i) {
      svalue = i;
      TEST( i == decrementpositive_atomic(&svalue));
      TEST( i-1 == svalue);   // changed
      svalue = -i;
      TEST( -i == decrementpositive_atomic(&svalue));
      TEST( -i == svalue);    // not changed
   }

   // TEST incrementmax8_atomic
   for (uint32_t off = 0; off < 4; ++off) {
      volatile uint8_t *ptr8 = off + (volatile uint8_t*) &value;
      value = 0;
      for (uint32_t i = 0; i < 256; ++i) {
         *ptr8 = (uint8_t) i;
         TEST( i == incrementmax8_atomic(ptr8, i+1));
         TEST( ((uint8_t)(i+1)<<(8*off)) == value);
         *ptr8 = (uint8_t) i;
         TEST( i == incrementmax8_atomic(ptr8, (uint32_t)-1));
         TEST( ((uint8_t)(i+1)<<(8*off)) == value);
         *ptr8 = (uint8_t) i;
         TEST( i == incrementmax8_atomic(ptr8, i));
         TEST( (i<<(8*off)) == value);
         TEST( i == incrementmax8_atomic(ptr8, 0));
         TEST( (i<<(8*off)) == value);
         TEST( i == incrementmax8_atomic(ptr8, i?i-1:0));
         TEST( (i<<(8*off)) == value);
      }
   }

   // TEST swap_atomic
   ptrval = 0;
   TEST( 0 == swap_atomic(&ptrval, 0, 0));
   TEST( 0 == ptrval);
   for (uintptr_t i = 1; i < 100; ++i) {
      TEST( 0 == swap_atomic(&ptrval, 0, (void*)i));
      TEST( i == (uintptr_t) ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*)0, (void*)~i));
      TEST( i == (uintptr_t) ptrval);
      TEST( 0 == swap_atomic(&ptrval, (void*) i, (void*)~i));
      TEST( ~i == (uintptr_t) ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*) i, (void*) (i+1)));
      TEST( ~i == (uintptr_t) ptrval);
      TEST( 0 == swap_atomic(&ptrval, (void*)~i, 0));
      TEST( 0 == ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*)1, (void*)i));
      TEST( 0 == ptrval);
   }

   // TEST swap_atomic
   ptrval = 0;
   TEST( 0 == swap_atomic(&ptrval, 0, 0));
   TEST( 0 == ptrval);
   for (uintptr_t i = 1; i < 100; ++i) {
      TEST( 0 == swap_atomic(&ptrval, 0, (void*)i));
      TEST( i == (uintptr_t) ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*)0, (void*)~i));
      TEST( i == (uintptr_t) ptrval);
      TEST( 0 == swap_atomic(&ptrval, (void*) i, (void*)~i));
      TEST( ~i == (uintptr_t) ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*) i, (void*) (i+1)));
      TEST( ~i == (uintptr_t) ptrval);
      TEST( 0 == swap_atomic(&ptrval, (void*)~i, 0));
      TEST( 0 == ptrval);
      TEST( 0 != swap_atomic(&ptrval, (void*)1, (void*)i));
      TEST( 0 == ptrval);
   }

   // TEST swap8_atomic
   for (uintptr_t off = 0; off < 4; ++off) {
      volatile uint8_t *ptr8 = off + (volatile uint8_t*) &value;
      value = 0;
      for (uint32_t i = 0; i < 256; ++i) {
         TEST( 0 != swap8_atomic(ptr8, (i?i:1), i));
         TEST( 0 == value);
         TEST( 0 == swap8_atomic(ptr8, 0, i));
         TEST( (i<<(8*off)) == value);
         TEST( 0 != swap8_atomic(ptr8, i+256/*> 255*/, i));
         TEST( (i<<(8*off)) == value);
         TEST( 0 != swap8_atomic(ptr8, i+1, i));
         TEST( (i<<(8*off)) == value);
         TEST( 0 != swap8_atomic(ptr8, i-1, i));
         TEST( (i<<(8*off)) == value);
         TEST( 0 == swap8_atomic(ptr8, i, ~i));
         TEST( (((uint8_t)~i)<<(8*off)) == value);
         TEST( 0 == swap8_atomic(ptr8, (uint8_t)~i, 0));
         TEST( 0 == value);
      }
   }

   // reset
   reset_interruptTable();

   return 0;
}

#endif
