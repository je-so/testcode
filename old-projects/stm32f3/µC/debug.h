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

// == exported Peripherals/HW-Units
#define DBG    ((dbg_t*)(HW_REGISTER_BASEADDR_SCS+HW_REGISTER_OFFSET_SCS_DHCSR))

// == exported types
struct dbg_t;

typedef enum dbg_event_e {
   dbg_event_EXTERNAL = 1 << 4,
   dbg_event_VCATCH   = 1 << 3,
   dbg_event_DWTTRAP  = 1 << 2,
   dbg_event_BKPT     = 1 << 1,
   dbg_event_HALTED   = 1 << 0,  // (!C_DEBUGEN && MONEN && MONSTEP) || (C_DEBUGEN && (C_HALT||C_STEP))
   // -- mask value, only used for masking --
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

// == Register Offsets

// TODO: DFSR move into in SCB block
#define HW_REGISTER_OFFSET_SCB_DFSR   0x030  // DFSR (rw): Debug Fault Status Register; Resetvalue: 0x00000000;
#define HW_REGISTER_OFFSET_SCS_DHCSR  0xDF0  // (1. Register in dbg_t)
#define HW_REGISTER_OFFSET_SCS_DCRSR  0xDF4
#define HW_REGISTER_OFFSET_SCS_DCRDR  0xDF8
#define HW_REGISTER_OFFSET_SCS_DEMCR  0xDFC

// == Register Bit Werte

// DFSR: Debug Fault Status Register
// Shows, at the top level, why a debug event occurred. Writing 1 to a register bit clears the bit to 0.
// EXTERNAL: Indicates a debug event generated because of the assertion of EDBGRQ: 0: No EDBGRQ debug event. 1: EDBGRQ debug event.
#define HW_REGISTER_BIT_SCS_DFSR_EXTERNAL    (1u << 4)
// VCATCH: Indicates triggering of a Vector catch: 0: No Vector catch triggered. 1: Vector catch triggered.
#define HW_REGISTER_BIT_SCS_DFSR_VCATCH      (1u << 3)
// DWTTRAP: Indicates a debug event generated by the DWT:
// 0: Not generated by DWT. 1: At least one current debug event generated by the DWT.
#define HW_REGISTER_BIT_SCS_DFSR_DWTTRAP     (1u << 2)
// BKPT: Indicates a debug event generated by BKPT instruction execution or a breakpoint match in FPB:
// 0: No current breakpoint debug event. 1: At least one current breakpoint debug event.
#define HW_REGISTER_BIT_SCS_DFSR_BKPT        (1u << 1)
// HALTED: Indicates a debug event generated by either: a C_HALT or C_STEP request (C_DEBUGEN == 1)
// a step request triggered by setting DEMCR.MON_STEP to 1 (Debug monitor stepping).
// 0: No active halt request debug event. 1: Halt request debug event active.
#define HW_REGISTER_BIT_SCS_DFSR_HALTED      (1u << 0)

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
   static_assert(dbg_event_EXTERNAL == HW_REGISTER_BIT_SCS_DFSR_EXTERNAL);
   static_assert(dbg_event_VCATCH   == HW_REGISTER_BIT_SCS_DFSR_VCATCH);
   static_assert(dbg_event_DWTTRAP  == HW_REGISTER_BIT_SCS_DFSR_DWTTRAP);
   static_assert(dbg_event_BKPT     == HW_REGISTER_BIT_SCS_DFSR_BKPT);
   static_assert(dbg_event_HALTED   == HW_REGISTER_BIT_SCS_DFSR_HALTED);
   static_assert((dbg_event_MASK&1) == 1); // no shift needed

   return HW_REGISTER(SCB, DFSR) & dbg_event_MASK;
}

/* Gibt 1 zurück, wenn ein externer Debugger angeschlossen ist.
 * Ein Debugevent stoppt den Prozessor und versetzt ihn in den Debugmodus,
 * so dass der Debugger die Kontrolle übernimmt. Software-Interrupts
 * (debugmonitor_interrupt) werden in diesem Modus nicht unterstützt. */
static inline int isdebugger_dbg(void)
{
   return (DBG->dhcsr / HW_REGISTER_BIT_SCS_DHCSR_C_DEBUGEN) & 1;
}

static inline int isenabled_interrupt_dbg(void)
{
   return (DBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_MONEN) & 1;
}

/* Gibt 1 zurück, wenn der coreinterrupt_DEBUGMONITOR angeschaltet ist.
 * Eingeschaltet wird er mit enable_interrupt_dbg. */
static inline int isenabled_trace_dbg(void)
{
   return (DBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_TRCENA) & 1;
}

/* Gibt 1 zurück, wenn ein DEBUGMONITOR auf Bearbeitung wartet. */
static inline int isinterrupt_dbg(void)
{
   return (DBG->demcr / HW_REGISTER_BIT_SCS_DEMCR_MONPEND) & 1;
}

/* Schaltet Debug-Monitor an. Die Interruptroutine debugmonitor_interrupt wird gerufen,
 * wenn ein Debugevent ansteht und wenn kein externer Debugger angeschlossen ist
 * (isdebugger_dbg() == 0). */
static inline void enable_interrupt_dbg(void)
{
   DBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_MONEN;
}

/* Schaltet Debug-Monitor aus. Falls der Prozessor nicht im Debugmodus ist (isdebugger_dbg() == 0)
 * und ein Debugevent durch eine "bkpt"-Instruktion generiert wird, so esakliert es zu einem
 * fault_interrupt. Watchpoints-Debug-Events (Comparator in dwt wird ausgelöst) werden dagegen ignoriert. */
static inline void disable_interrupt_dbg(void)
{
   DBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_MONEN;
}

static inline void clear_interrupt_dbg(void)
{
   DBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_MONPEND;
}

static inline void generate_interrupt_dbg(void)
{
   DBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_MONPEND;
}

/* Schaltet Unterstützung Debug-Trace-Units an, speziell DWT und ITM. */
static inline void enable_dwt_dbg(void)
{
   DBG->demcr |= HW_REGISTER_BIT_SCS_DEMCR_TRCENA;
}

static inline void disable_dwt_dbg(void)
{
   DBG->demcr &= ~HW_REGISTER_BIT_SCS_DEMCR_TRCENA;
}

#endif
