/* title: Core-Debug

   Gibt Zugriff auf auf Cortex-M4 Debug Komponenten

      o Unterstützung von Debug-Monitor Ausführung durch "BKPT"-Instruction (__asm("bkpt"))
        und, falls DHCSR.C_DEBUGEN gesetzt ist, können damit feste Breakpoints per Software
        eingefügt werden, die später im Debugger zu einer Unterbrechung führen,
        ohne explizit Breakpoints setzen zu müssen.
      o TODO: Implementiere Zugriff auf weitere Komponenten

   Der Debug Access Port (DAP) ist der physikalische Zugangspunkt des externen Debuggers.

   Übersicht aller Debug Komponenten:

    - Private Peripheral Bus (address range 0xE0000000 to 0xE00FFFFF) -
   ┌───────────────────────────────────┬─────────────────────┬───────────────────────────────┐
   │ Group                             │ Address Range       │ Notes                         │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ITM:Instrumentation Trace Macrocell│0xE0000000-0xE0000FFF│ performance monitor support   │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │DWT:Data Watchpoint and Trace      │0xE0001000-0xE0001FFF│ trace support                 │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │FPB:Flash Patch and Breakpoint     │0xE0002000-0xE0002FFF│ optional                      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │SCS:SCB:System Control Block       │0xE000ED00-0xE000ED8F│ generic control features      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │SCS:DCB:Debug Control Block        │0xE000EDF0-0xE000EEFF│ debug control and config      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │TPIU:Trace Port Interface Unit     │0xE0040000-0xE0040FFF│ optional serial wire viewer   │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ETM:Embedded Trace Macrocell       │0xE0041000-0xE0041FFF│ optional instruction trace    │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ARMv7-M ROM table                  │0xE00FF000-0xE00FFFFF│ used for auto-configuration   │
   └───────────────────────────────────┴─────────────────────┴───────────────────────────────┘


   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/debug.h
    Header file <Core-Debug>.

   file: TODO: µC/debug.c
    Implementation file <Core-Debug impl>.
*/
#ifndef CORTEXM4_MC_DEBUG_HEADER
#define CORTEXM4_MC_DEBUG_HEADER

#include "dbg/dwt.h"

// == Exported Peripherals/HW-Units
#define hDBG    ((core_debug_t*) HW_REGISTER_BASEADDR_COREDEBUG)

// == Exported Types

typedef enum dbg_event_e {
   dbg_event_EXTERNAL = HW_BIT(SCB, DFSR, EXTERNAL),
   dbg_event_VCATCH   = HW_BIT(SCB, DFSR, VCATCH),
   dbg_event_DWTTRAP  = HW_BIT(SCB, DFSR, DWTTRAP),
   dbg_event_BKPT     = HW_BIT(SCB, DFSR, BKPT),
   dbg_event_HALTED   = HW_BIT(SCB, DFSR, HALTED), // (!C_DEBUGEN && MONEN && MONSTEP) || (C_DEBUGEN && (C_HALT||C_STEP))
   // -- mask value, only used for masking value of register SCB_DFSR --
   dbg_event_MASK     = dbg_event_EXTERNAL|dbg_event_VCATCH|dbg_event_DWTTRAP|dbg_event_BKPT|dbg_event_HALTED,
} dbg_event_e;

// == exported functions

// -- query --
static inline dbg_event_e event_dbg(void);
static inline int isdebugger_dbg(void);
static inline int isenabled_interrupt_dbg(void);
static inline int isenabled_trace_dbg(void);
static inline int isinterrupt_dbg(void);
// -- config --
static inline void enable_interrupt_dbg(void);
static inline void disable_interrupt_dbg(void);
static inline void clear_interrupt_dbg(void);
static inline void generate_interrupt_dbg(void);
static inline void enable_dwt_dbg(void);      // enable DWT and ITM
static inline void enable_dwt_dbg(void);


// == definitions

// TODO: remove (use from core.h)
typedef volatile struct dbg_t {
   /* DHCSR (rw): Debug Halting Control and Status Register. */
   uint32_t    dhcsr;
   /* DCRSR (wo): Debug Core Register Selector Register
    * Bit [16] REGWnR 0: Read. 1: Write.
    * Bits [6:0] REGSEL 0..12: core registers r0-r12. 13: current sp. 14: link register 15: DebugReturnAddress (next instruction executed after leaving debug state)
    *  16: xPSR. 17: MSP. 18: PSP (Process Stack Pointer). 20: CONTROL[31:24]+FAULTMASK[23:16]+BASEPRI[15:8]+PRIMASK[7:0].
    *  33: Floating-point Status and Control Register, FPSCR.  64..95: Floating-ponit registers S0-S31. */
   uint32_t    dcrsr;
   /* DCRDR (rw): Debug Core Register Data Register
    * [31:0] DBGTMP Data temporary cache, for reading and writing the ARM core registers. */
   uint32_t    dcrdr;
   /* DEMCR (rw): Debug Exception and Monitor Control Register; Reset value: 0x00000000; */
   uint32_t    demcr;
} dbg_t;

// == Register Bit Werte

// DHCSR: Debug Halting Control and Status Register
// Bit values are only used if C_DEBUGEN is set to 1 (could only be changed by an externally attached debugger)
// S_REGRDY (ro): A handshake flag for transfers through the DCRDR: Writing to DCRSR clears the bit to 0.
// Completion of the DCRDR transfer then sets the bit to 1.
// 0: There has been a write to the DCRDR, but the transfer is not complete. 1: The transfer to or from the DCRDR is complete.
#define HW_REGISTER_BIT_SCS_DHCSR_S_REGRDY   (1u << 16)
#define HW_REGISTER_BIT_SCS_DHCSR_C_STEP     (1u << 2)   // (rw) 0: No effect. 1: Step the processor.
#define HW_REGISTER_BIT_SCS_DHCSR_C_HALT     (1u << 1)   // (rw) 0: No effect. 1: Halt the processor.
#define HW_REGISTER_BIT_SCS_DHCSR_C_DEBUGEN  (1u << 0)   // (rw) 1: Enable halting debug behavior ==> processor enters Debug state (used with external debugger)

// DEMCR: Debug Exception and Monitor Control Register
// not lsited bits are either reserved or only used in case DHCSR.C_DEBUGEN is set to 1 (used by external debugger)
#define HW_REGISTER_BIT_SCS_DEMCR_TRCENA     (1u << 24) // Global enable for all DWT and ITM features: 0: DWT and ITM blocks disabled. 1: DWT and ITM blocks enabled.
#define HW_REGISTER_BIT_SCS_DEMCR_MONREQ     (1u << 19) // Indicates Debug Monitor Exception was requested by Software
#define HW_REGISTER_BIT_SCS_DEMCR_MONSTEP    (1u << 18) // (only used if MONEN==1) 1: Sets MONPEND after every instruction. 0: Do not step the processor.
#define HW_REGISTER_BIT_SCS_DEMCR_MONPEND    (1u << 17) // Pending State of Debug Monitor Exception. 1: Pending
#define HW_REGISTER_BIT_SCS_DEMCR_MONEN      (1u << 16) // Indicates Debug Monitor Exception Enabled. 1: Enabled


// section: inline implementation

static inline dbg_event_e event_dbg(void)
{
   static_assert(dbg_event_EXTERNAL == (int) HW_BIT(SCB, DFSR, EXTERNAL));
   static_assert(dbg_event_VCATCH   == (int) HW_BIT(SCB, DFSR, VCATCH));
   static_assert(dbg_event_DWTTRAP  == (int) HW_BIT(SCB, DFSR, DWTTRAP));
   static_assert(dbg_event_BKPT     == (int) HW_BIT(SCB, DFSR, BKPT));
   static_assert(dbg_event_HALTED   == (int) HW_BIT(SCB, DFSR, HALTED));
   static_assert((dbg_event_MASK&1) == 1); // no shift needed

   return hSCB->dfsr & dbg_event_MASK;
}

/* Gibt 1 zurück, wenn ein externer Debugger angeschlossen ist.
 * Ein Debugevent stoppt den Prozessor und versetzt ihn in den Debugmodus,
 * so dass der Debugger die Kontrolle übernimmt. Software-Interrupts
 * (debugmonitor_interrupt) werden in diesem Modus nicht unterstützt. */
static inline int isdebugger_dbg(void)
{
   return (hDBG->dhcsr / HW_REGISTER_BIT_SCS_DHCSR_C_DEBUGEN) & 1;
}

static inline int isenabled_interrupt_dbg(void)
{
   return (hDBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_MONEN) & 1;
}

/* Gibt 1 zurück, wenn der coreinterrupt_DEBUGMONITOR angeschaltet ist.
 * Eingeschaltet wird er mit enable_interrupt_dbg. */
static inline int isenabled_trace_dbg(void)
{
   return (hDBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_TRCENA) & 1;
}

/* Gibt 1 zurück, wenn ein DEBUGMONITOR auf Bearbeitung wartet. */
static inline int isinterrupt_dbg(void)
{
   return (hDBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_MONPEND) & 1;
}

/* Schaltet Debug-Monitor an. Die Interruptroutine debugmonitor_interrupt wird gerufen,
 * wenn ein Debugevent ansteht und wenn kein externer Debugger angeschlossen ist
 * (isdebugger_dbg() == 0). */
static inline void enable_interrupt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_MONEN;
}

/* Schaltet Debug-Monitor aus. Falls der Prozessor nicht im Debugmodus ist (isdebugger_dbg() == 0)
 * und ein Debugevent durch eine "bkpt"-Instruktion generiert wird, so esakliert es zu einem
 * fault_interrupt. Watchpoints-Debug-Events (Comparator in dwt wird ausgelöst) werden dagegen ignoriert. */
static inline void disable_interrupt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_MONEN;
}

static inline void clear_interrupt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_MONPEND;
}

static inline void generate_interrupt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_MONPEND;
}

/* Schaltet Unterstützung Debug-Trace-Units an, speziell DWT und ITM. */
static inline void enable_dwt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_TRCENA;
}

static inline void disable_dwt_dbg(void)
{
   // TODO: atomic ?
   hDBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_TRCENA;
}

#endif
