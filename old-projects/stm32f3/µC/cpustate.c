/* title: CortexM4-Core impl

   Imeplementiert <CortexM4-Core>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Header file <CortexM4-Core>.

   file: TODO: µC/core.c
    Implementation file <CortexM4-Core impl>.
*/
#include "konfig.h"
#include "µC/cpustate.h"

// struct: cpustate_t

__attribute__ ((naked))
int init_cpustate(/*out*/volatile cpustate_t *state)
{
   /*
       |<-              -- PSR  --                  ->|
       ┌─┬─┬─┬─┬─┬────────────────────────────────────┐
   APSR│N│Z│C│V│Q│              reserviert            │
       ├─┴─┴─┴─┴─┴─────────────────────────┬──────────┤
   IPSR│         reserviert                │  ISR-NR  │
       ├─────────┬──────┬─┬───────┬──────┬─┴──────────┤
   EPSR│ res.    │ICI/IT│T│  res. │ICI/IT│ reserviert │
       └─────────┴──────┴─┴───────┴──────┴────────────┘
         Bits [31] N	Negative Flag
         Bits [30] Z	Zero Flag
         Bits [29] C	Carry or Borrow Flag
         Bits [28] V	Overflow Flag
         Bits [27] Q	Saturation Flag
         Bits [26:25] ICI/IT  Register position of interrupted instruction (LDM, STM, PUSH, or POP)
              [15:10] ICI/IT  or state of conditional IT inctruction
         Bits [24] T	Thumb State. This bit is always set to 1 cause Cortex-M4 supports Thumb incstructions exclusively.
         Bits [8:0] ISR_NUMBER 0 = Thread-Mode, 2 = NMI-Interrupt, 3 = Fault-Interrupt, ...
   */

#define TOSTRING1(str)  # str
#define TOSTRING2(str)  TOSTRING1(str)

   __asm volatile(
      "sub  r1, sp, #8*4\n"   // adjust sp for interrupt-frame registers
      "str  r1, [r0], #4\n"   // state->sp = sp - (8*4)
      "movs r1, #" TOSTRING2(EINTR) "\n"
      "str  r1, [r0], #4\n"   // state->iframe[0] == -1
      "stm  r0!, {r1-r3,r12,lr}\n"  // state->iframe[1..5]
      "adr  r1, init_cpustate_pc\n"
      "str  r1, [r0], #4\n"   // state->iframe[6] = &init_cpustate_pc
      "mrs  r1, psr\n"        // read psr / section epsr read always as 0
      "orr  r1, #(1<<24)\n"   // ==> set thumb state manually after reading psr
      "str  r1, [r0], #4\n"   // state->iframe[7] = PSR
      "stm  r0, {r4-r11}\n"   // state->regs[1..8]
      "movs r0, #0\n"         // return value 0: OK, EINTR: returned from interrupt
      "init_cpustate_pc:\n"
      "bx   lr\n"
   );

#undef TOSTRING1
#undef TOSTRING2

}

__attribute__ ((naked))
void ret2threadmode_cpustate(const volatile cpustate_t *state)
{
   __asm volatile(
      "ldr  r1, [r0], #4\n"
      "mov  sp, r1\n"
      "ldm  r0!, {r1-r8}\n"
      "stm  sp, {r1-r8}\n"       // stack frame used by interrupt return
      "ldm  r0, {r4-r11}\n"      // restore other regs
      "mov  lr, #0xfffffff9\n"   // code for return from interrupt to thread mode without fpu stack frame and using main stack pointer
      "bx   lr\n"                // return from interrupt
   );
}

