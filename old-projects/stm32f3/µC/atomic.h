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

__attribute__((naked))
static void clearbits_atomic(/*atomic rw*/volatile uint32_t *reg, uint32_t bits)
{
   __asm volatile(
      "retry_clearbits_atomic:\n"
      "ldrex   r2, [r0]\n"
      "bics    r2, r1\n"
      "strex   r3, r2, [r0]\n"
      "cmp     r3, #0\n"
      "bne     retry_clearbits_atomic\n"
      "bx      lr\n"
      ::: "r2", "r3"
   );
}

__attribute__((naked))
static void setbits_atomic(/*atomic rw*/volatile uint32_t *reg, uint32_t bits)
{
   __asm volatile(
      "retry_setbits_atomic:\n"
      "ldrex   r2, [r0]\n"
      "orrs    r2, r1\n"
      "strex   r3, r2, [r0]\n"
      "cmp     r3, #0\n"
      "bne     retry_setbits_atomic\n"
      "bx      lr\n"
      ::: "r2", "r3"
   );
}

#endif
