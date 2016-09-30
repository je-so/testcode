/* title: CortexM4-CPU-State

   Allows to save current state of program in thread-mode and
   reset CPU back to this state in case of error.

   You can jump back to the position where the currrent state was save
   either from interrupt(not nested) or from another execution location in thread-mode.

   The only condition is that the stack is not rolled forward but only rolled back.
   It is not allowed to save the current state in a function, return from that function
   and jump back into it with the saved state.
   It is only allowed to jump back from a nested function to to place which called the
   function.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: hw/cm4/cpustate.h
    Header file <CortexM4-CPU-State>.

   file: hw/cm4/cpustate.c
    Implementation file <CortexM4-CPU-State impl>.
*/
#ifndef HW_CORTEXM4_CPUSTATE_HEADER
#define HW_CORTEXM4_CPUSTATE_HEADER

/* == Typen == */
typedef struct cpustate_t cpustate_t;
            // Erlaubt das Speichern des aktuellen CPU-Zustandes und das Zurücksetzen/Zurückkehren
            // in diesen Zustand aus einem Interrupt heraus.
            //    • Speichert CPU-Zustand (FPU noch nicht implementiert).
            //    • FAULT-Interrupt kann im Fehlerfall an den gespeicherten Ausgangszustand zurückkehren.


/* == Objekte == */
struct cpustate_t {
   uint32_t sp;         // r13
   uint32_t iframe[8];  // {r0-r3,r12,lr/*r14*/,pc/*r15*/,psr}
   uint32_t regs[8];    // {r4-r11}
};

// cpustate_t: lifetime

int  init_cpustate(/*out*/cpustate_t *state);
            // Initializes state with current state of CPU. Returns 0: Normal return to caller. EINTR: Returned from interrupt who called ret2threadmode_cpustate.
void inittask_cpustate(/*out*/cpustate_t *state, void (*task) (void*arg), void *arg, uint32_t lenstack, uint32_t stack[lenstack]);
            // Initializes state so that PC is pointing to task, R0 to arg and SP is pointing to &stack[lenstack]. It is assumed that task *never* returns.
static inline void free_cpustate(cpustate_t *state);
            // Set state to invalid value.

// cpustate_t: query

static inline int isinit_cpustate(const struct cpustate_t *state);
            // Returns 0: state is invalid. 1: state is valid.

// cpustate_t: used-from-thread

void jump_cpustate(const struct cpustate_t *state);
            // Restores CPU registers and jumps to location where init was called.

// cpustate_t: used-from-interrupt

void ret2threadmode_cpustate(const struct cpustate_t *state);
            // Restores CPU registers and prepares MSP stack for interrupt return. The interrupt returns to location where init was called.
void ret2threadmodepsp_cpustate(const struct cpustate_t *state, void *msp_init);
            // Restores CPU registers and prepares PSP stack for interrupt return. MSP is set to msp_init. The interrupt returns to location where init was called.


/* == Inline == */

static inline int isinit_cpustate(const cpustate_t *state)
{
         return state->sp != 0;
}

static inline void free_cpustate(cpustate_t *state)
{
         state->sp = 0;
}

#endif
