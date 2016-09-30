/* title: CortexM4-CPU-State impl

   Implements <CortexM4-CPU-State>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: hw/cm4/cpustate.h
    Header file <CortexM4-CPU-State>.

   file: hw/cm4/cpustate.c
    Implementation file <CortexM4-CPU-State impl>.
*/
#include "konfig.h"
#include "hw/cm4/cpustate.h"

// struct: cpustate_t

#define THUMB           (1<<24)

__attribute__ ((naked))
int init_cpustate(/*out*/cpustate_t *state)
{
   /*
       |<-              -- PSR  --                  ->|
       ┌─┬─┬─┬─┬─┬──────────┬────┬────────────────────┐
   APSR│N│Z│C│V│Q│          │ GE │       res.         │
       ├─┴─┴─┴─┴─┴──────────┴────┴─────────┬──────────┤
   IPSR│           res.                    │  ISR-NR  │
       ├─────────┬──────┬─┬───────┬──────┬─┴──────────┤
   EPSR│   res.  │ICI/IT│T│  res. │ICI/IT│    res.    │
       └─────────┴──────┴─┴───────┴──────┴────────────┘
         Bits [31] N	Negative Flag
         Bits [30] Z	Zero Flag
         Bits [29] C	Carry or Borrow Flag
         Bits [28] V	Overflow Flag
         Bits [27] Q	Saturation Flag
         Bits [19:16] GE[3:0]	Greater than or Equal flags. See SEL for more information.
         Bits [26:25] ICI/IT  Register position of interrupted instruction (LDM, STM, PUSH, or POP)
              [15:10] ICI/IT  or state of conditional IT inctruction
         Bits [24] T	Thumb State. This bit is always set to 1 cause Cortex-M4 supports Thumb incstructions exclusively.
         Bits [8:0] ISR_NUMBER 0 = Thread-Mode, 2 = NMI-Interrupt, 3 = Fault-Interrupt, ...
   */

   __asm volatile(
      "str  sp, [r0], #8\n"      // state->sp = sp; r0 = &state->iframe[1];
      "stm  r0!, {r1-r3,r12,lr}\n" // save r1-r3,r12,lr into &state->iframe[1]; r0 = &state->iframe[6/*pc*/];
      "mrs  r2, xpsr\n"          // read PSR into r2 but section epsr reads always as 0
      "orrs r2, #" TOSTRING(THUMB) "\n" // set thumb state manually in r2 cause section epsr was read as 0
      "movs r1, #" TOSTRING(EINTR) "\n" // r1 = EINTR;
      "str  r1, [r0, #-6*4]\n"   // state->iframe[0] = EINTR;
      "movs r1, lr\n"            // r1 = pc;
      "stm  r0, {r1-r2,r4-r11}\n"// store pc and psr into state->iframe[6..7] and store r4-r11 into &state->regs[0];
      "movs r0, #0\n"            // set return value to 0: OK (»EINTR: returned from interrupt« stored in iframe[0])
      "bx   lr\n"                // return
   );
}

void inittask_cpustate(/*out*/struct cpustate_t *state, void (*task) (void*arg), void *arg, uint32_t lenstack, uint32_t stack[lenstack])
{
   state->sp = (uintptr_t) &stack[lenstack];
   for (unsigned i = 0; i < lengthof(state->iframe); ++i) {
      state->iframe[i] = 0;
   }
   for (unsigned i = 0; i < lengthof(state->regs); ++i) {
      state->regs[i] = 0;
   }
   state->iframe[0/*R0*/] = (uintptr_t) arg;
   state->iframe[5/*LR*/] = (uintptr_t) -1; // invalid return address
   state->iframe[6/*PC*/] = (uintptr_t) task;
   state->iframe[7/*PSW*/] = THUMB;
}

__attribute__ ((naked))
void jump_cpustate(const cpustate_t *state)
{
   __asm volatile(
      "ldr  r3, [r0], #4\n"         // r3 = state->sp; r0 = &state->iframe[0];
      "mov  sp, r3\n"               // sp = state->sp;
      "adds r4, r0, #6*4\n"         // r4 = &state->iframe[6/*pc*/];
      "ldm  r4, {r1-r2,r4-r11}\n"   // r1 = pc; r2 = PSR; restore r4-r11 from &state->regs[0];
      "push { r1 }\n"               // push pc on new stack
      "msr  apsr_nzcvqg, r2\n"      // restore apsr
      "ldm  r0, {r0-r3,r12,lr}\n"   // restore r0-r3,r12,lr
      "pop  { pc }\n"               // return to pc stored on stack
   );
}

__attribute__ ((naked))
void ret2threadmode_cpustate(const cpustate_t *state)
{
   __asm volatile(
      "ldm  r0!, {r1, r4-r11}\n" // r1 = state->sp; load state->iframe into registers r4-r11; r0 = &state->regs[0];
      "mov  sp, r1\n"            // sp = state->sp;
      "stmdb sp!, {r4-r11}\n"    // store iframe on new stack / build stack frame used by interrupt return; sp -= sizeof(iframe);
      "ldm  r0, {r4-r11}\n"      // restore regs r4-r11;
      "mov  lr, #0xfffffff9\n"   // code for return from interrupt to thread mode without fpu stack frame and using main stack pointer
      "bx   lr\n"                // return from interrupt restores r0-r3,r12,lr,pc and xpsr
   );
}

__attribute__ ((naked))
void ret2threadmodepsp_cpustate(const cpustate_t *state, void *msp_init)
{
   __asm volatile(
      "ldm  r0!, {r2, r4-r11}\n" // r2 = state->sp; load state->iframe into registers r4-r11; r0 = &state->regs[0];
      "stmdb r2!, {r4-r11}\n"    // store iframe on new stack / build stack frame used by interrupt return; r2 -= sizeof(iframe);
      "msr  psp, r2\n"           // psp = state->sp - sizeof(iframe);
      "ldm  r0, {r4-r11}\n"      // restore regs r4-r11;
      "mov  sp, r1\n"            // msp = msp_init;
      "mov  lr, #0xfffffffd\n"   // code for return from interrupt to thread mode without fpu stack frame and using process stack pointer
      "bx   lr\n"                // return from interrupt restores r0-r3,r12,lr,pc and xpsr
   );
}
