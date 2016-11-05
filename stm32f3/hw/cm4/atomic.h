/* title: Atomic-Operations

   Implements atomic operations on (shared) memory.
      o A read-modify-write cycle will be aborted and repeated if interrupted by Interrupt.
        This prevents race conditions between interrupts and thread if modifying the same memory location.

   Implementiert atomare Operationen.

      o Durch Interrupts unterbrochene Read-Modify-Write Zyklen werden abgebrochen und wiederholt.
        Race-Conditions werden so vermieden.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: hw/cm4/atomic.h
    Header file <Atomic-Operations>.

   file: hw/cm4/atomic.c
    Implementation file <Atomic-Operations impl>.
*/
#ifndef HW_CORTEXM4_ATOMIC_HEADER
#define HW_CORTEXM4_ATOMIC_HEADER

/* == Import == */
#include "hw/cm4/msync.h"

/* == Funktionen == */

#ifdef KONFIG_UNITTEST
int unittest_hw_cortexm4_atomic(void);
#endif

static inline int trylock_atomic(volatile uint32_t *lock);
            // returns 0: lock successfully acquired and set to 1. !=0: lock could not be acquired cause someone else set it to 1.
static inline void unlock_atomic(volatile uint32_t *lock);
            // Makes sure that all pending writes are stored to memory (memory barrier)
            // and releases lock by setting it to 0.
unsigned increment_atomic(/*atomic rw*/volatile unsigned *val);
            // Does following atomic op: return ++ *val. Defined as _Generic function which selects the appropriate typed function.
unsigned decrement_atomic(/*atomic rw*/volatile unsigned *val);
            // Does following atomic op: return -- *val. Defined as _Generic function which selects the appropriate typed function.
static inline int32_t decrementpositive_atomic(/*atomic rw*/volatile int32_t *val);
            // Does following atomic op: return *val > 0 ? *val-- : *val;
static inline void clearbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits);
            // Does following atomic op: *val &= ~bits;
static inline void setbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits);
            // Does following atomic op: *val |= bits;
static inline void setclrbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t setbits, uint32_t clearbits);
            // Does following atomic op: *val = (*val & ~clearbits) | setbits;
static inline int swap_atomic(void* volatile * val, void* oldval, void* newval);
            // Does following atomic op: if (*val == oldval) { *val = newval; return 0/*OK*/; } else { return (oldval-*val)/*ERROR*/; }
static inline int swap8_atomic(uint8_t volatile * val, uint32_t oldval/*should be <= 255*/, uint32_t newval);
            // Does following atomic op: if (*val == oldval) { *val = (uint8_t)newval; return 0/*OK*/; } else { return (oldval-*val)/*ERROR*/; }
static inline uint16_t increment16_atomic(/*atomic rw*/volatile uint16_t *val);
            // Does following atomic op: return ++ *val;
static inline uint32_t increment32_atomic(/*atomic rw*/volatile uint32_t *val);
            // Does following atomic op: return ++ *val;
static inline uint16_t decrement16_atomic(/*atomic rw*/volatile uint16_t *val);
            // Does following atomic op: return -- *val;
static inline uint32_t decrement32_atomic(/*atomic rw*/volatile uint32_t *val);
            // Does following atomic op: return -- *val;
static inline uint32_t incrementmax8_atomic(/*atomic rw*/volatile uint8_t *val, uint32_t maxval/*should be <=255*/);
            // Does following atomic op: uint32_t old = *val; if (old < maxval) { *val = old+1} return old;


/* == Inline == */

#define increment_atomic(val) _Generic((val), volatile uint16_t*: increment16_atomic, default: increment32_atomic) (val)

#define decrement_atomic(val) _Generic((val), volatile uint16_t*: decrement16_atomic, default: decrement32_atomic) (val)

static inline int trylock_atomic(volatile uint32_t *lock)
{
   int err;
   __asm volatile(
      "movs    r3, #1\n"         // r3 = 1si
      "1: ldrex %0, [%1]\n"      // 1: err = *lock
      "tst     %0, %0\n"
      "bne     2f\n"             // if (err != 0) goto label 2:
      "strex   %0, r3, [%1]\n"   // if (try { *lock = 1; }) /*OK*/err = 0; else /*FAILED*/err = 1;
      "tst     %0, %0\n"
      "bne     1b\n"             // if (err != 0) goto back to label 1: and retry read-modify-write cycle
      "2:\n"                     // 2:
      : "=&r" (err) : "r" (lock) : "r3", "cc", "memory"
   );
   return err/*err==0: OK. err!=0: ERROR*/;
}

static inline void unlock_atomic(volatile uint32_t *lock)
{
   rw_msync();   // make sure shared data written before it is unlocked
   *lock = 0;
}

static inline uint32_t increment32_atomic(/*atomic rw*/volatile uint32_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"
      "adds    %0, #1\n"
      "strex   r3, %0, [%1]\n"
      "tst     r3, r3\n"
      "bne     1b\n"       // repeat if increment was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r3", "cc", "memory"
   );
   return newval;
}

static inline uint32_t decrement32_atomic(/*atomic rw*/volatile uint32_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"
      "subs    %0, #1\n"
      "strex   r3, %0, [%1]\n"
      "tst     r3, r3\n"
      "bne     1b\n"       // repeat if decrement was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r3", "cc", "memory"
   );
   return newval;
}

static inline uint16_t increment16_atomic(/*atomic rw*/volatile uint16_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrexh  %0, [%1]\n"
      "adds    %0, #1\n"
      "strexh  r3, %0, [%1]\n"
      "tst     r3, r3\n"
      "bne     1b\n"       // repeat if increment was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r3", "cc", "memory"
   );
   return (uint16_t) newval;
}

static inline uint16_t decrement16_atomic(/*atomic rw*/volatile uint16_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrexh  %0, [%1]\n"
      "subs    %0, #1\n"
      "strexh  r3, %0, [%1]\n"
      "tst     r3, r3\n"
      "bne     1b\n"       // repeat if decrement was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r3", "cc", "memory"
   );
   return (uint16_t) newval;
}

static inline int32_t decrementpositive_atomic(/*atomic rw*/volatile int32_t *val)
{
   int32_t oldval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"       // 1: oldval = *val
      "subs    %0, #1\n"            // oldval -= 1;
      "blt     2f\n"                // if (oldval < 0) goto label 2
      "strex   r3, %0, [%1]\n"      // try { *val = oldval; }
      "tst     r3, r3\n"
      "bne     1b\n"                // if (decrement not atomic) goto back to label 1:
      "2: adds %0, #1\n"            // 2: in case of error ==> (oldval <= 0)
      : "=&r" (oldval) : "r" (val) : "r3", "cc", "memory"
   );
   return oldval;
}

static inline uint32_t incrementmax8_atomic(/*atomic rw*/volatile uint8_t *val, uint32_t maxval)
{
   uint32_t oldval;
   __asm volatile(
      "1: ldrexb  %0, [%1]\n"       // 1: oldval = *val
      "cmp     %0, %2\n"            // ? oldval - maxval
      "bhs     2f\n"                // if (oldval >= maxval) goto label 2
      "adds    %0, #1\n"            // oldval += 1;
      "strexb  r12, %0, [%1]\n"     // try { *val = oldval; }
      "tst     r12, r12\n"          // if (not atomic)
      "bne     1b\n"                //    goto back to label 1:
      "subs    %0, #1\n"            // oldval -= 1;
      "2:\n"                        // 2:
      : "=&r" (oldval) : "r" (val), "r" (maxval) : "r12", "cc", "memory"
   );
   return oldval;
}


static inline void clearbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits)
{
   __asm volatile(
      "1: ldrex   r12, [%0]\n"
      "bics    r12, %1\n"
      "strex   r3, r12, [%0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      :: "r" (val), "r" (bits) : "r3", "r12", "cc", "memory"
   );
}

static inline void setbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits)
{
   __asm volatile(
      "1: ldrex   r12, [%0]\n"
      "orrs    r12, %1\n"
      "strex   r3, r12, [%0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      :: "r" (val), "r" (bits) : "r3", "r12", "cc", "memory"
   );
}

static inline void setclrbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t setbits, uint32_t clearbits)
{
   __asm volatile(
      "1: ldrex   r12, [%0]\n"
      "bics    r12, %2\n"
      "orrs    r12, %1\n"
      "strex   r3, r12, [%0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      :: "r" (val), "r" (setbits), "r" (clearbits) : "r3", "r12", "cc", "memory"
   );
}

static inline int swap_atomic(void* volatile * val, void* oldval, void* newval)
{
   int err;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"
      "subs    %0, %2\n"
      "bne     2f\n"
      "strex   %0, %3, [%1]\n"
      "tst     %0, %0\n"
      "bne     1b\n"
      "2:\n"
      : "=&r" (err) : "r" (val), "r" (oldval), "r" (newval) : "cc", "memory"
   );
   return err;
}

static inline int swap8_atomic(uint8_t volatile * val, uint32_t oldval, uint32_t newval)
{
   int err;
   __asm volatile(
      "1: ldrexb   %0, [%1]\n"
      "subs    %0, %2\n"
      "bne     2f\n"
      "strexb  %0, %3, [%1]\n"
      "tst     %0, %0\n"
      "bne     1b\n"
      "2:\n"
      : "=&r" (err) : "r" (val), "r" (oldval), "r" (newval) : "cc", "memory"
   );
   return err;
}

#endif
