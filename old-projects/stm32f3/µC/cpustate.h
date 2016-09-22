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

// == Exported Types
struct cpustate_t;


// == Exported Functions

#define _I static inline

// lifetime
int      init_cpustate(/*out*/struct cpustate_t *state);
            // Initializes state with current state of CPU. Returns 0: Normal return to caller. EINTR: Returned from interrupt who called ret2threadmode_cpustate.
void     inittask_cpustate(/*out*/struct cpustate_t *state, void (*task) (void*arg), void *arg, uint32_t lenstack, uint32_t stack[lenstack]);
            // Initializes state so that PC is pointing to task, R0 to arg and SP is pointing to &stack[lenstack]. It is assumed that task *never* returns.
_I void  free_cpustate(struct cpustate_t *state);
            // Set state to invalid value.

// query
_I int   isinit_cpustate(const struct cpustate_t *state);
            // Returns 0: state is invalid. 1: state is valid.

// used-from-thread(thread-mode)
void     jump_cpustate(const struct cpustate_t *state);
            // Restores CPU registers and jumps to location where init was called.

// used-from-interrupt(handler-mode)
void     ret2threadmode_cpustate(const struct cpustate_t *state);
            // Restores CPU registers and prepares MSP stack for interrupt return. The interrupt returns to location where init was called.
void     ret2threadmodepsp_cpustate(const struct cpustate_t *state, void *msp_init);
            // Restores CPU registers and prepares PSP stack for interrupt return. MSP is set to msp_init. The interrupt returns to location where init was called.

#undef _I

// == definition

typedef struct cpustate_t {
   uint32_t sp;         // r13
   uint32_t iframe[8];  // {r0-r3,r12,lr/*r14*/,pc/*r15*/,psr}
   uint32_t regs[8];    // {r4-r11}
} cpustate_t;



// section: inline implementation

static inline int isinit_cpustate(const cpustate_t *state)
{
   return state->sp != 0;
}

static inline void free_cpustate(cpustate_t *state)
{
   state->sp = 0;
}

#endif
