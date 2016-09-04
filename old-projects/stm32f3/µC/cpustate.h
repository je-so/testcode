/* title: CortexM4-CPU-State

   Erlaubt das Speichern des aktuellen CPU-Zustandes und das Zurücksetzen/Zurückkehren
   in diesen Zustand aus einem Interrupt heraus.

      • Speichert CPU-Zustand (FPU noch nicht implementiert).
      • FAULT-Interrupt kann im Fehlerfall an den gepsiehcerten Ausgangszustand zurückkehren.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/cpustate.h
    Header file <CortexM4-CPU-State>.

   file: TODO: µC/cpustate.c
    Implementation file <CortexM4-CPU-State impl>.
*/
#ifndef CORTEXM4_MC_CPUSTATE_HEADER
#define CORTEXM4_MC_CPUSTATE_HEADER

// == exported types
struct cpustate_t;


// == definition

typedef struct cpustate_t {
   uint32_t sp;         // r13
   uint32_t iframe[8];  // {r0-r3,r12,lr/*r14*/,pc/*r15*/,psr}
   uint32_t regs[8];    // {r4-r11}
} cpustate_t;

// group: lifetime
int init_cpustate(/*out*/volatile cpustate_t *state); // returns 0: Normal return to caller. EINTR: Returned from interrupt who called ret2threadmode_cpustate.
static inline void free_cpustate(volatile cpustate_t *state);

// group: query
static inline int isinit_cpustate(const volatile cpustate_t *state);

// group: used-from-interrupt
void ret2threadmode_cpustate(const volatile cpustate_t *state);


// section: inline implementation

static inline int isinit_cpustate(const volatile cpustate_t *state)
{
   return state->sp != 0;
}

static inline void free_cpustate(volatile cpustate_t *state)
{
   state->sp = 0;
}

#endif
