/* title: Atomic-OP

   Implementiert atomare Operationen.

      o Read-Modify-Write Zyklen können so nicht durch Interrupts zunichte gemacht werden,
        falls die Interruptroutine auf dasselbe µC-Register zugreift.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/atomic.h
    Header file <Atomic-OP>.

   file: TODO: µC/atomic.c
    Implementation file <Atomic-OP impl>.
*/
#ifndef CORTEXM4_MC_ATOMIC_HEADER
#define CORTEXM4_MC_ATOMIC_HEADER

static inline int trylock_atomic(volatile uint32_t *lock);
            // returns 0: lock successfully acquired and set to 1. !=0: lock could not be acquired cause someone else set it to 1.
static inline void unlock_atomic(volatile uint32_t *lock);
            // Makes sure that all pending writes are stored to memory (memory barrier)
            // and releases lock by setting it to 0.
static inline uint32_t increment_atomic(/*atomic rw*/volatile uint32_t *val);
            // Does following atomic op: ++ *val;
static inline uint32_t decrement_atomic(/*atomic rw*/volatile uint32_t *val);
            // Does following atomic op: -- *val;
static void clearbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits);
            // Does following atomic op: *val &= ~bits;
static void setbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits);
            // Does following atomic op: *val |= bits;
static void setclrbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t setbits, uint32_t clearbits);
            // Does following atomic op: *val = (*val & ~clearbits) | setbits;


// == inline implementation

static inline int trylock_atomic(volatile uint32_t *lock)
{
   int err;
   __asm volatile(
      "movs    r2, #1\n"         // r2 = 1
      "1: ldrex %0, [%1]\n"      // 1: err = *lock
      "tst     %0, %0\n"
      "bne     2f\n"             // if (err != 0) goto label 2:
      "strex   %0, r2, [%1]\n"   // if (try { *lock = 1; }) /*OK*/err = 0; else /*FAILED*/err = 1;
      "tst     %0, %0\n"
      "bne     1b\n"             // if (err != 0) goto back to label 1: and retry read-modify-write cycle
      "2:\n"                     // 2:
      : "=&r" (err) : "r" (lock) : "r2", "cc", "memory"
   );
   return err/*err==0: OK. err!=0: ERROR*/;
}

static inline void unlock_atomic(volatile uint32_t *lock)
{
   __asm volatile( "dmb" );   // make sure shared data written before it is unlocked
   *lock = 0;
}

static inline uint32_t increment_atomic(/*atomic rw*/volatile uint32_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"
      "adds    %0, #1\n"
      "strex   r2, %0, [%1]\n"
      "tst     r2, r2\n"
      "bne     1b\n"       // repeat if increment was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r2", "cc", "memory"
   );
   return newval;
}

static inline uint32_t decrement_atomic(/*atomic rw*/volatile uint32_t *val)
{
   uint32_t newval;
   __asm volatile(
      "1: ldrex   %0, [%1]\n"
      "subs    %0, #1\n"
      "strex   r2, %0, [%1]\n"
      "tst     r2, r2\n"
      "bne     1b\n"       // repeat if decrement was interrupted or failed
      : "=&r" (newval) : "r" (val) : "r2", "cc", "memory"
   );
   return newval;
}

__attribute__((naked))
static void clearbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits)
{
   __asm volatile(
      "1:\n"
      "ldrex   r2, [r0]\n"
      "bics    r2, r1\n"
      "strex   r3, r2, [r0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      "bx      lr\n"
      ::: "r2", "r3"
   );
}

__attribute__((naked))
static void setbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t bits)
{
   __asm volatile(
      "1:\n"
      "ldrex   r2, [r0]\n"
      "orrs    r2, r1\n"
      "strex   r3, r2, [r0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      "bx      lr\n"
      ::: "r2", "r3"
   );
}

__attribute__((naked))
static void setclrbits_atomic(/*atomic rw*/volatile uint32_t *val, uint32_t setbits, uint32_t clearbits)
{
   __asm volatile(
      "1:\n"
      "ldrex   r12, [r0]\n"
      "bics    r12, r2\n"
      "orrs    r12, r1\n"
      "strex   r3, r12, [r0]\n"
      "tst     r3, r3\n"
      "bne     1b\n"
      "bx      lr\n"
      ::: "r2", "r3", "r12"
   );
}

#endif
