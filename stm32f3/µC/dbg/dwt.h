/* title: Core-Debug-DWT

   Simplifies access to Cortex-M4-DWT: Data Watchpoint and Trace - Debug Unit.
   Not all features are implemented.

   Offers:
      • comparators, that support:
         — watchpoints, that cause the processor to enter Debug state or take a DebugMonitor exception
         — data tracing
         — signalling for use with an external resource, for example an ETM
         — PC value tracing
         — cycle count matching (only comparator with index 0)
      • exception trace
      • performance profiling counters

   ┌───────────────────────────────────┬─────────────────────┬───────────────────────────────┐
   │ Group                             │ Address Range       │ Notes                         │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │DWT:Data Watchpoint and Trace      │0xE0001000-0xE0001FFF│ trace support                 │
   └───────────────────────────────────┴─────────────────────┴───────────────────────────────┘

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/dbg/dwt.h
    Header file <Core-Debug-DWT>.

   file: TODO: µC/dbg/dwt.c
    Implementation file <Core-Debug-DWT impl>.
*/
#ifndef CORTEXM4_MC_DBG_DWT_HEADER
#define CORTEXM4_MC_DBG_DWT_HEADER

/* == HW-Units == */
#define DWTDBG    ((dwtdbg_t*)HW_BASEADDR_DWT)

/* == Typen == */
typedef volatile struct dwtdbg_t    dwtdbg_t;

typedef enum dwtdbg_feature_e {
   // every feature is encoded in one bit
   dwtdbg_feature_PROFILECOUNTER = 1,
   dwtdbg_feature_CYCLECOUNTER   = 2,
   dwtdbg_feature_EXTTRIGGER     = 4,
   dwtdbg_feature_TRACEPACKET    = 8,
   // mask
   dwtdbg_feature_ALL = dwtdbg_feature_PROFILECOUNTER|dwtdbg_feature_CYCLECOUNTER|dwtdbg_feature_EXTTRIGGER|dwtdbg_feature_TRACEPACKET,
} dwtdbg_feature_e;

typedef enum dwtdbg_e {
   // -- cycles counter only available if dwtdbg_feature_CYCLECOUNTER is set --
   dwtdbg_CYCLECOUNT = 1 << 0,   // cycles of all executed instructions (processor clock)

   // -- profiling counter only available if dwtdbg_feature_PROFILECOUNTER is set --
   dwtdbg_CPICOUNT   = 1 << 17,  // cycles of multi-cycle instructions but it does not count the first cycle
   dwtdbg_EXCCOUNT   = 1 << 18,  // cycles associated with exception entry or return.
   dwtdbg_SLEEPCOUNT = 1 << 19,  // cycles in power saving mode
   dwtdbg_LSUCOUNT   = 1 << 20,  // cycles of multi-cycle load-store instructions but it does not count the first cycle
   dwtdbg_FOLDCOUNT  = 1 << 21,  // counts instructions with zero cycle per instruction

   // -- tracing only avilable if dwtdbg_feature_TRACEPACKET is set --
   dwtdbg_EXCTRACE   = 1 << 16,  // generates exception trace packet (TODO: test dwtdbg_EXCTRACE)

   // mask value
   dwtdbg_COUNTER = dwtdbg_CYCLECOUNT|dwtdbg_CPICOUNT|dwtdbg_EXCCOUNT|dwtdbg_SLEEPCOUNT|dwtdbg_LSUCOUNT|dwtdbg_FOLDCOUNT,
   dwtdbg_TRACE   = dwtdbg_EXCTRACE,
   dwtdbg_ALL     = dwtdbg_COUNTER | dwtdbg_TRACE,
} dwtdbg_e;

typedef enum dwtdbg_watchpoint_e {
  dwtdbg_watchpoint_CODEADDR,
  dwtdbg_watchpoint_DATAADDR_RO,
  dwtdbg_watchpoint_DATAADDR_WO,
  dwtdbg_watchpoint_DATAADDR_RW,
  dwtdbg_watchpoint_VALUE8BIT_RO,
  dwtdbg_watchpoint_VALUE8BIT_WO,
  dwtdbg_watchpoint_VALUE8BIT_RW,
  dwtdbg_watchpoint_VALUE16BIT_RO,
  dwtdbg_watchpoint_VALUE16BIT_WO,
  dwtdbg_watchpoint_VALUE16BIT_RW,
  dwtdbg_watchpoint_VALUE32BIT_RO,
  dwtdbg_watchpoint_VALUE32BIT_WO,
  dwtdbg_watchpoint_VALUE32BIT_RW,
  dwtdbg_watchpoint_CYCLECOUNT,  // supported only by comparator 0 (dwtdbg_t.comp[0])
} dwtdbg_watchpoint_e;

/* == Funktion == */

// query
static inline uint8_t          nrcomp_dwtdbg(void);
static inline dwtdbg_feature_e feature_dwtdbg(void);
static inline uint32_t         cyclecount_dwtdbg(void);
static inline uint8_t          cpicount_dwtdbg(void);
static inline uint8_t          exccount_dwtdbg(void);
static inline uint8_t          sleepcount_dwtdbg(void);
static inline uint8_t          lsucount_dwtdbg(void);
static inline uint8_t          foldcount_dwtdbg(void);
// config
static inline void start_dwtdbg(dwtdbg_e counter/*masked with dwtdbg_ALL*/); // reset count value to 0
static inline void stop_dwtdbg(dwtdbg_e counter/*masked with dwtdbg_ALL*/);  // keeps count value
static inline int  addwatchpoint_dwtdbg(/*out*/uint8_t *wpid, dwtdbg_watchpoint_e wp, uintptr_t comp, uint8_t ignore_nr_lsb_bits); // STM32F3: ignore_nr_lsb_bits <= 15
static inline void clearwatchpoint_dwtdbg(uint32_t wpid);
static inline void clearallwatchpoint_dwtdbg(void);

/* == Definition == */

struct dwtdbg_t {
   uint32_t       ctrl;   /* Control Register, rw, Offset: 0x00, Reset: IMP DEF
                           * Provides configuration and status information for the DWT block, and used to control features of the block.
                           */
   uint32_t       cyccnt; /* Cycle Count Register, rw, Offset: 0x04, Reset: ?
                           * A 32-bit cycle counter. When enabled, CYCCNT increments on each cycle of the processor clock. When the
                           * counter overflows it wraps to zero, transparently.
                           */
   uint32_t       cpicnt; /* CPI Count Register, rw, Offset: 0x08, Reset: ?
                           * Counts additional cycles required to execute multi-cycle instructions and instruction fetch stalls except those recorded by LSUCNT.
                           * It does not count the first cycle required to execute any instruction.
                           * The counter initializes to 0 when software enables its counter overflow event by setting the CTRL.CPIEVTENA bit to 1.
                           * Bits 7:0 CPI counter value.
                           */
   uint32_t       exccnt; /* Exception Overhead Count Register, rw, Offset: 0x0C, Reset: ?
                           * The exception overhead counter. The counter increments on each cycle associated with exception entry or return.
                           * It counts the cycles associated with entry stacking, return unstacking, preemption, and other exception-related processes.
                           * The counter initializes to 0 when software enables its counter overflow event by setting the CTRL.EXCEVTENA bit to 1.
                           * Bits 7:0 Exception overhead counter value.
                           */
   uint32_t      sleepcnt;/* Sleep Count Register, rw, Offset: 0x10, Reset: ?
                           * The sleep overhead counter. The counter increments on each cycle associated with power saving, whether initiated by a WFI or WFE instruction, or by the sleep-on-exit functionality.
                           * The counter initializes to 0 when software enables its counter overflow event by setting the CTRL.SLEEPEVTENA bit to 1.
                           * Bits 7:0 Sleep counter value.
                           */
   uint32_t       lsucnt; /* LSU Count Register, rw, Offset: 0x14, Reset: ?
                           * This counter increments on each additional cycle required to execute a multi-cycle load-store instruction.
                           * It does not count the first cycle required to execute any instruction.
                           * The counter initializes to 0 when software enables its counter overflow event by setting the CTRL.LSUEVTENA bit to 1.
                           * Bits 7:0 Load-store unit counter value.
                           */
   uint32_t       foldcnt;/* Folded-instruction Count Register, rw, Offset: 0x18, Reset: ?
                           * The counter increments on any instruction that executes in zero cycles.
                           * The counter initializes to 0 when software enables its counter overflow event by setting the CTRL.FOLDEVTENA bit to 1.
                           * Bits 7:0 Folded-instruction counter. Increments on each instruction that takes 0 cycles.
                           */
   uint32_t const pcsr;   /* Program Counter Sample Register, ro, Offset: 0x1C, Reset: ?
                           * Bits 31:0 EIASAMPLE Executed Instruction Address sample value. */
   struct {          /*    */
      uint32_t    comp;   /* Comparator Reference Value Register, rw, Offset: 0x20+16*n, Reset: ?
                           * Bits 31:0 COMP Reference value for comparison.
                           */
      uint32_t    mask;   /* Comparator Mask Register, rw, Offset: 0x24+16*n, Reset: ?
                           * Bits 4:0 MASK The size of the ignore mask, 0-31 bits, applied to address range matching.
                           * The maximum mask size is IMPLEMENTATION DEFINED. You can write 0b11111 to this
                           * field and then read the register back to determine the maximum mask size supported.
                           */
      uint32_t    function;/* Comparator Function Register, rw, Offset: 0x28+16*n, Reset: 0x???????0
                           * Controls the operation of comparator n. */
      uint32_t    _res0;/* */
   } comp[15];
};

enum dwtdbg_register_e {

   HW_DEF_OFF(DWT, CTRL,     0x00),          // CTRL: Control Register
   HW_DEF_BIT(DWT, CTRL, NUMCOMP,   31, 28), // (ro) Number of comparators implemented. A value of zero indicates no comparator support.
   HW_DEF_BIT(DWT, CTRL, NOTRCPKT,  27, 27), // (ro) Shows whether the implementation supports trace sampling and exception tracing. 0: supported. 1: *not* supported.
   HW_DEF_BIT(DWT, CTRL, NOEXTTRIG, 26, 26), // (ro) Shows whether the implementation includes external match signals. 0: supported. 1: *not* supported.
   HW_DEF_BIT(DWT, CTRL, NOCYCCNT,  25, 25), // (ro) Shows whether the implementation supports a cycle counter. 0: supported. 1: *not* supported.
   HW_DEF_BIT(DWT, CTRL, NOPRFCNT,  24, 24), // (ro) Shows whether the implementation supports the profiling counters. 0: supported. 1: *not* supported.
   HW_DEF_BIT(DWT, CTRL, CYCEVTENA, 22, 22), // Enables POSTCNT underflow Event counter packets generation. 0: not generated. 1: POSTCNT underflow packets generated, if PCSAMPLENA set to 0. Only valid if NOTRCPKT and NOCYCCNT both set to 0.
   HW_DEF_BIT(DWT, CTRL, FOLDEVTENA,21, 21), // Enables generation of the Folded-instruction counter overflow event. 0: Disabled. 1: Enabled. Only valid if NOPRFCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, LSUEVTENA, 20, 20), // Enables generation of the LSU counter overflow event. 0: Disabled. 1: Enabled. Only valid if NOPRFCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, SLEEPEVTENA,19,19), // Enables generation of the Sleep counter overflow event. 0: Disabled. 1: Enabled. Only valid if NOPRFCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, EXCEVTENA, 18, 18), // Enables generation of the Exception overhead counter overflow event. 0: Disabled. 1: Enabled. Only valid if NOPRFCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, CPIEVTENA, 17, 17), // Enables generation of the CPI counter overflow event. 0: Disabled. 1: Enabled. Only valid if NOPRFCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, EXCTRCENA, 16, 16), // Enables generation of exception trace. 0: Disabled. 1: Enabled. Only valid if NOTRCPKT is set to 0.
   HW_DEF_BIT(DWT, CTRL, PCSAMPLENA,12, 12), // Enables use of POSTCNT counter as a timer for Periodic PC sample packet generation. 0: Disabled. 1: Periodic PC sample packets generated, if CYCEVTENA set to 0. Only valid if NOTRCPKT and NOCYCCNT both set to 0.
   HW_DEF_BIT(DWT, CTRL, SYNCTAP,   11, 10), // A tap on the CYCCNT counter provides a timer signal for generation of periodic Synchronization packets by the ITM. 00: Disabled. 01: Bit 24, CPU-clock/16M. 10: Bit 26, CPU-clock/64M. 11: Bit 28, CPU-clock/256M. Only valid if NOCYCCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, CYCTAP,     9,  9), // Selects the position of the POSTCNT tap on the CYCCNT counter. 0: Bit 6, CPU-clock/64. 1: Bit 10, CPU-clock/1024. Only valid if NOCYCCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, POSTINIT,   8,  5), // Initial value for the POSTCNT counter. When software enables use of the POSTCNT timer, the processor loads the initial POSTCNT value from this field. Only valid if NOCYCCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, POSTPRESET, 4,  1), // Reload value for the POSTCNT counter. Subsequently whenever the CYCCNT tap bit transitions, either from 0 to 1 or from 1 to 0: • if POSTCNT holds a nonzero value, POSTCNT decrements by 1 • if POSTCNT is zero, the DWT unit reloads POSTCNT from CTRL.POSTPRESET and generates the required Periodic PC sample packets or Event counter packet. Only valid if NOCYCCNT is set to 0.
   HW_DEF_BIT(DWT, CTRL, CYCCNTENA,  0,  0), // Enables CYCCNT. 0: Disabled; 1: Enabled.

   HW_DEF_OFF(DWT, CYCCNT,   0x04),          // CYCCNT: Cycle Count Register
   HW_DEF_OFF(DWT, CPICNT,   0x08),          // CPICNT: CPI Count Register
   HW_DEF_OFF(DWT, EXCCNT,   0x0C),          // EXCCNT: Exception Overhead Count Register
   HW_DEF_OFF(DWT, SLEEPCNT, 0x10),          // SLEEPCNT: Sleep Count Register
   HW_DEF_OFF(DWT, LSUCNT,   0x14),          // LSUCNT: LSU Count Register
   HW_DEF_OFF(DWT, FOLDCNT,  0x18),          // FOLDCNT: Folded-instruction Count Register
   HW_DEF_OFF(DWT, PCSR,     0x1C),          // PCSR: Program Counter Sample Register
   HW_DEF_OFF(DWT, COMP,     0x20),          // COMP: Comparator 0 Reference Value Register
   HW_DEF_OFF(DWT, MASK,     0x24),          // MASK: Comparator 0 Mask Register
   HW_DEF_OFF(DWT, FUNCTION, 0x28),          // FUNCTION: Comparator 0 Function Register
   HW_DEF_BIT(DWT, FUNCTION, MATCHED,   24,24), // (ro) Comparator match. 0: No match. 1: Match. Cleared to 0 by reading.
   HW_DEF_BIT(DWT, FUNCTION, DATAVADDR1,19,16), // Hold comparator number of a second comparator to use for linked address comparison. Only used if DATAVMATCH and LNK1ENA bits are both 1. Disabled if pointing to itself.
   HW_DEF_BIT(DWT, FUNCTION, DATAVADDR0,15,12), // Hold comparator number of a comparator to use for linked address comparison. Only used if DATAVMATCH bit is set to 1. Disabled if pointing to itself.
   HW_DEF_BIT(DWT, FUNCTION, DATAVSIZE, 11,10), // Specifies size of matching data value comparison. 00: Byte. 01: Halfword. 10: Word. 11: Reserved. Only used if DATAVMATCH bit is set to 1.
   HW_DEF_BIT(DWT, FUNCTION, LNK1ENA,    9, 9), // (ro) Indicates whether the implementation supports use of a second linked comparator. 0: Not supported. 1: Supported.
   HW_DEF_BIT(DWT, FUNCTION, DATAVMATCH, 8, 8), // Enables data value comparison, if supported by hw. 0: Perform address comparison (or cycle count comparison when CYCMATCH==1). 1: Perform data value comparison.
   HW_DEF_BIT(DWT, FUNCTION, CYCMATCH,   7, 7), // Comparator 0 only! If the implementation supports cycle counting, enable cycle count comparison for comparator 0. 0: No cycle comparison. 1: Compare COMP with the cycle counter CYCCNT. Only supported if CTRL.NOCYCCNT == 0.
   HW_DEF_BIT(DWT, FUNCTION, EMITRANGE,  5, 5), // Enables generation of Data trace address offset packets, that hold Daddr[15:0]. 0: Disabled. 1: Enable Data trace address offset packet generation. Only supported if CTRL.NOTRCPKT == 0.
   HW_DEF_BIT(DWT, FUNCTION, FUNCTION,   3, 0), // Selects action taken on comparator match. 0: Disabled or LinkAddr. !=0: Encoding depends on DATAVMATCH / CYCMATCH. This field resets to zero.

   HW_DEF_OFF(DWT, COMP1,    0x30),          // COMP1: Comparator 1 Reference Value Register
   HW_DEF_OFF(DWT, MASK1,    0x34),          // MASK1: Comparator 1 Mask Register
   HW_DEF_OFF(DWT, FUNCTION1,0x38),          // FUNCTION1: Comparator 1 Function Register

};


/* == Inline == */

static inline uint8_t nrcomp_dwtdbg(void)
{
   static_assert( 0xf0000000 == HW_BIT(DWT, CTRL, NUMCOMP));
   return (uint8_t) ((DWTDBG->ctrl & HW_BIT(DWT, CTRL, NUMCOMP)) >> HW_BIT(DWT, CTRL, NUMCOMP_POS));
}

static inline dwtdbg_feature_e feature_dwtdbg(void)
{
   static_assert( HW_BIT(DWT,CTRL,NOTRCPKT)  == dwtdbg_feature_TRACEPACKET    << HW_BIT(DWT,CTRL,NOPRFCNT_POS));
   static_assert( HW_BIT(DWT,CTRL,NOEXTTRIG) == dwtdbg_feature_EXTTRIGGER     << HW_BIT(DWT,CTRL,NOPRFCNT_POS));
   static_assert( HW_BIT(DWT,CTRL,NOCYCCNT)  == dwtdbg_feature_CYCLECOUNTER   << HW_BIT(DWT,CTRL,NOPRFCNT_POS));
   static_assert( HW_BIT(DWT,CTRL,NOPRFCNT)  == dwtdbg_feature_PROFILECOUNTER << HW_BIT(DWT,CTRL,NOPRFCNT_POS));

   return (~ DWTDBG->ctrl >> HW_BIT(DWT,CTRL,NOPRFCNT_POS)) & dwtdbg_feature_ALL;
}

static inline uint32_t cyclecount_dwtdbg(void)
{
   return DWTDBG->cyccnt;
}

static inline uint8_t cpicount_dwtdbg(void)
{
   return (uint8_t) DWTDBG->cpicnt;
}

static inline uint8_t exccount_dwtdbg(void)
{
   return (uint8_t) DWTDBG->exccnt;
}

static inline uint8_t sleepcount_dwtdbg(void)
{
   return (uint8_t) DWTDBG->sleepcnt;
}

static inline uint8_t lsucount_dwtdbg(void)
{
   return (uint8_t) DWTDBG->lsucnt;
}

static inline uint8_t foldcount_dwtdbg(void)
{
   return (uint8_t) DWTDBG->foldcnt;
}

static inline void start_dwtdbg(dwtdbg_e counter)
{
   counter &= dwtdbg_ALL;

   static_assert(dwtdbg_CYCLECOUNT == (dwtdbg_e) HW_BIT(DWT,CTRL,CYCCNTENA));
   static_assert(dwtdbg_CPICOUNT   == (dwtdbg_e) HW_BIT(DWT,CTRL,CPIEVTENA));
   static_assert(dwtdbg_EXCCOUNT   == (dwtdbg_e) HW_BIT(DWT,CTRL,EXCEVTENA));
   static_assert(dwtdbg_SLEEPCOUNT == (dwtdbg_e) HW_BIT(DWT,CTRL,SLEEPEVTENA));
   static_assert(dwtdbg_LSUCOUNT   == (dwtdbg_e) HW_BIT(DWT,CTRL,LSUEVTENA));
   static_assert(dwtdbg_FOLDCOUNT  == (dwtdbg_e) HW_BIT(DWT,CTRL,FOLDEVTENA));

   uint32_t ctrl = DWTDBG->ctrl & ~counter;
   DWTDBG->ctrl = ctrl;    // disable counter
   ctrl |= counter;        // set enable bits
   if (0 != (counter & dwtdbg_CYCLECOUNT)) {
      DWTDBG->cyccnt = 0;  // reset cycle counter (not reset by enable bit)
   }                       // other 8-bit counters reset to 0 if enabled
   DWTDBG->ctrl = ctrl;    // enable counter
}

static inline void stop_dwtdbg(dwtdbg_e counter)
{
   counter &= dwtdbg_ALL;
   DWTDBG->ctrl &= ~counter;
}

static inline int addwatchpoint_dwtdbg(/*out*/uint8_t *wpid, dwtdbg_watchpoint_e wp, uintptr_t comp, uint8_t ignore_nr_lsb_bits)
{
   uint32_t cid = nrcomp_dwtdbg();

   if (!cid) return ENOMEM;

   if (wp == dwtdbg_watchpoint_CYCLECOUNT) {
      cid = 0; // only comparator 0 support match of CYCLECOUNT (do not forget to enable cycle counter )
   } else {
      do {
         --cid;
      } while ((DWTDBG->comp[cid].function & HW_BIT(DWT,FUNCTION,FUNCTION)) != 0 && cid > 0);
   }

   if ((DWTDBG->comp[cid].function & HW_BIT(DWT,FUNCTION,FUNCTION)) != 0) {
      return ENOMEM;
   }

   uint32_t mask = 0;
   uint32_t fct  = DWTDBG->comp[cid].function;
   fct &= ~( HW_BIT(DWT,FUNCTION,DATAVADDR1)
           | HW_BIT(DWT,FUNCTION,DATAVADDR0)
           | HW_BIT(DWT,FUNCTION,DATAVSIZE)
           | HW_BIT(DWT,FUNCTION,DATAVMATCH)
           | HW_BIT(DWT,FUNCTION,CYCMATCH)
           | HW_BIT(DWT,FUNCTION,EMITRANGE)
           | HW_BIT(DWT,FUNCTION,FUNCTION) );

   // disable linked address comparison (not supported by hw)
   fct |= (cid << HW_BIT(DWT,FUNCTION,DATAVADDR1_POS))
        | (cid << HW_BIT(DWT,FUNCTION,DATAVADDR0_POS));

   switch (wp) {
   case dwtdbg_watchpoint_CODEADDR:
      comp &= ~1; // clear thumb state bit
   case dwtdbg_watchpoint_DATAADDR_RO:
   case dwtdbg_watchpoint_DATAADDR_WO:
   case dwtdbg_watchpoint_DATAADDR_RW:
      if (ignore_nr_lsb_bits > 31) {
         return EINVAL;
      }
      mask = ignore_nr_lsb_bits;
      static_assert(dwtdbg_watchpoint_DATAADDR_RO == 1 + dwtdbg_watchpoint_CODEADDR);
      static_assert(dwtdbg_watchpoint_DATAADDR_WO == 2 + dwtdbg_watchpoint_CODEADDR);
      static_assert(dwtdbg_watchpoint_DATAADDR_RW == 3 + dwtdbg_watchpoint_CODEADDR);
      fct |= (4 + wp-dwtdbg_watchpoint_CODEADDR) << HW_BIT(DWT,FUNCTION,FUNCTION_POS);
      break;
   case dwtdbg_watchpoint_VALUE8BIT_RO:
   case dwtdbg_watchpoint_VALUE8BIT_WO:
   case dwtdbg_watchpoint_VALUE8BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE8BIT_WO == 1 + dwtdbg_watchpoint_VALUE8BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE8BIT_RW == 2 + dwtdbg_watchpoint_VALUE8BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE8BIT_RO)) << HW_BIT(DWT,FUNCTION,FUNCTION_POS))
          |  HW_BIT(DWT,FUNCTION,DATAVMATCH);
      comp &= 0xff;
      comp |= comp << 8;
      comp |= comp << 16;
      break;
   case dwtdbg_watchpoint_VALUE16BIT_RO:
   case dwtdbg_watchpoint_VALUE16BIT_WO:
   case dwtdbg_watchpoint_VALUE16BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE16BIT_WO == 1 + dwtdbg_watchpoint_VALUE16BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE16BIT_RW == 2 + dwtdbg_watchpoint_VALUE16BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE16BIT_RO)) << HW_BIT(DWT,FUNCTION,FUNCTION_POS))
          | ( HW_BIT(DWT,FUNCTION,DATAVMATCH)|(1 << HW_BIT(DWT,FUNCTION,DATAVSIZE_POS)));
      comp &= 0xffff;
      comp |= comp << 16;
      break;
   case dwtdbg_watchpoint_VALUE32BIT_RO:
   case dwtdbg_watchpoint_VALUE32BIT_WO:
   case dwtdbg_watchpoint_VALUE32BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE32BIT_WO == 1 + dwtdbg_watchpoint_VALUE32BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE32BIT_RW == 2 + dwtdbg_watchpoint_VALUE32BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE32BIT_RO)) << HW_BIT(DWT,FUNCTION,FUNCTION_POS))
          | ( HW_BIT(DWT,FUNCTION,DATAVMATCH)|(2 << HW_BIT(DWT,FUNCTION,DATAVSIZE_POS)));
      break;
   case dwtdbg_watchpoint_CYCLECOUNT:
      fct |= (4 << HW_BIT(DWT,FUNCTION,FUNCTION_POS))
          | HW_BIT(DWT,FUNCTION,CYCMATCH);
      break;
   default:
      return EINVAL;
   }

   DWTDBG->comp[cid].comp = comp;
   DWTDBG->comp[cid].mask = mask;
   DWTDBG->comp[cid].function = fct;

   if (  mask != DWTDBG->comp[cid].mask   // nr of ignored bits not supported ?
         || 0 != ( (fct ^ DWTDBG->comp[cid].function) // value match or cycle match supported ?
                   & (HW_BIT(DWT,FUNCTION,DATAVMATCH)|HW_BIT(DWT,FUNCTION,CYCMATCH)))) {
      clearwatchpoint_dwtdbg(cid);
      return ENOSYS; // not supported
   }

   // set out param
   *wpid = (uint8_t) cid;

   return 0;
}

static inline void clearwatchpoint_dwtdbg(uint32_t wpid)
{
   if (wpid < nrcomp_dwtdbg() && (DWTDBG->comp[wpid].function & HW_BIT(DWT, FUNCTION, FUNCTION)) != 0) {
      DWTDBG->comp[wpid].comp      = 0;
      DWTDBG->comp[wpid].mask      = 0;
      DWTDBG->comp[wpid].function &= ~HW_BIT(DWT,FUNCTION,FUNCTION);
   }
}

static inline void clearallwatchpoint_dwtdbg(void)
{
   for (uint32_t i = nrcomp_dwtdbg(); i > 0; ) {
      clearwatchpoint_dwtdbg(--i);
   }
}

static inline int ismatch_dwtdbg(uint32_t wpid)
{
   if (wpid < nrcomp_dwtdbg()) {
      return (DWTDBG->comp[wpid].function >> HW_BIT(DWT,FUNCTION,MATCHED_POS)) & 1;
   }

   return 0;
}

#endif
