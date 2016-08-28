/* title: Core-Debug-DWT

   Gibt Zugriff auf auf Cortex-M4-DWT: Data Watchpoint and Trace

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

// == exported HW-Units
#define DWTDBG    ((dwtdbg_t*)HW_REGISTER_BASEADDR_DWT)

// == exported types
struct dwtdbg_t;

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
   // -- cycles counter only available if dwtdbg_feature_CYCLECOUNTER is on --
   dwtdbg_CYCLECOUNT = 1 << 0,   // cycles of all executed instructions (processor clock)

   // -- profiling counter only available if dwtdbg_feature_PROFILECOUNTER is on --
   dwtdbg_CPICOUNT   = 1 << 17,  // cycles of multi-cycle instructions but it does not count the first cycle
   dwtdbg_EXCCOUNT   = 1 << 18,  // cycles associated with exception entry or return.
   dwtdbg_SLEEPCOUNT = 1 << 19,  // cycles in power saving mode
   dwtdbg_LSUCOUNT   = 1 << 20,  // cycles of multi-cycle load-store instructions but it does not count the first cycle
   dwtdbg_FOLDCOUNT  = 1 << 21,  // counts instructions with zero cycle per instruction

   // -- tracing only avilable if dwtdbg_feature_TRACEPACKET is on --
   dwtdbg_EXCTRACE   = 1 << 16,  // generates exception trace packet (TODO: test dwtdbg_EXCTRACE)

   // mask value
   dwtdbg_COUNTER    = dwtdbg_CYCLECOUNT|dwtdbg_CPICOUNT|dwtdbg_EXCCOUNT|dwtdbg_SLEEPCOUNT|dwtdbg_LSUCOUNT|dwtdbg_FOLDCOUNT,
   dwtdbg_TRACE      = dwtdbg_EXCTRACE,
   dwtdbg_COUNTER_AND_TRACE = dwtdbg_COUNTER | dwtdbg_TRACE,
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

// == exported functions

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
static inline void start_dwtdbg(dwtdbg_e counter/*masked with dwtdbg_COUNTER_AND_TRACE*/); // reset count value to 0
static inline void stop_dwtdbg(dwtdbg_e counter/*masked with dwtdbg_COUNTER_AND_TRACE*/);  // keeps count value
static inline int  addwatchpoint_dwtdbg(/*out*/uint8_t *wpid, dwtdbg_watchpoint_e wp, uintptr_t comp, uint8_t ignore_nr_lsb_bits);
static inline void clearwatchpoint_dwtdbg(uint32_t wpid);


// == definitions

typedef volatile struct dwtdbg_t {
   /* Control register (rw); Offset: 0x00; Reset value: ?;
    * Provides configuration and status information for the DWT block, and used to control features of the block */
   uint32_t    ctrl;
   /* Cycle Count register (rw); Offset: 0x04; Reset value: ?; */
   uint32_t    cyccnt;
   /* CPI Count register (rw); Offset: 0x08; Reset value: ?;
    * Counts additional cycles required to execute multi-cycle instructions and instruction fetch stalls except those recorded by DWT_LSUCNT.
    * It does not count the first cycle required to execute any instruction.
    * The counter initializes to 0 when software enables its counter overflow event by setting the DWT_CTRL.CPIEVTENA bit to 1.
    * Bits 7:0 CPI counter value. */
   uint32_t    cpicnt;
   /* Exception Overhead Count register (rw); Offset: 0x0C; Reset value: ?;
    * The exception overhead counter. The counter increments on each cycle associated with exception entry or return.
    * It counts the cycles associated with entry stacking, return unstacking, preemption, and other exception-related processes.
    * The counter initializes to 0 when software enables its counter overflow event by setting the DWT_CTRL.EXCEVTENA bit to 1.
    * Bits 7:0 Exception overhead counter value. */
   uint32_t    exccnt;
   /* Sleep Count register (rw); Offset: 0x10; Reset value: ?;
    * The sleep overhead counter. The counter increments on each cycle associated with power saving, whether initiated by a WFI or WFE instruction, or by the sleep-on-exit functionality.
    * The counter initializes to 0 when software enables its counter overflow event by setting the DWT_CTRL.SLEEPEVTENA bit to 1.
    * Bits 7:0 Sleep counter value. */
   uint32_t    sleepcnt;
   /* LSU Count register (rw); Offset: 0x14; Reset value: ?;
    * This counter increments on each additional cycle required to execute a multi-cycle load-store instruction.
    * It does not count the first cycle required to execute any instruction.
    * The counter initializes to 0 when software enables its counter overflow event by setting the DWT_CTRL.LSUEVTENA bit to 1.
    * Bits 7:0 Load-store unit counter value. */
   uint32_t    lsucnt;
   /* Folded-instruction Count register (rw); Offset: 0x18; Reset value: ?;
    * The counter increments on any instruction that executes in zero cycles.
    * The counter initializes to 0 when software enables its counter overflow event by setting the DWT_CTRL.FOLDEVTENA bit to 1.
    * Bits 7:0 Load-store counter value. */
   uint32_t    foldcnt;
   /* Program Counter Sample register (ro); Offset: 0x1C; Reset value: ?; */
   uint32_t    pcsr;
   struct {
      /* Comparator Reference Value register (rw); Offset: 0x20+16*n; Reset value: ?;
       * [31:0] COMP Reference value for comparison. */
      uint32_t    comp;
      /* Comparator Mask register (rw); Offset: 0x24+16*n; Reset value: ?;
       * [4:0] MASK The size of the ignore mask, 0-31 bits, applied to address range matching.
       * The maximum mask size is IMPLEMENTATION DEFINED. You can write 0b11111 to this
       * field and then read the register back to determine the maximum mask size supported. */
      uint32_t    mask;
      /* Comparator Function register (rw); Offset: 0x28+16*n; Reset value: ?; */
      uint32_t    function;
      uint32_t    _reserved1;
   } comp[15];
} dwtdbg_t;

// == Bit Definitionen

// CTRL: Control register
// Number of comparators implemented (ro). A value of zero indicates no comparator support.
#define HW_REGISTER_BIT_DWT_CTRL_NUMCOMP     HW_REGISTER_BITFIELD(31,28)
#define HW_REGISTER_BIT_DWT_CTRL_NUMCOMP_POS 28
// Shows whether the implementation supports trace sampling and exception tracing (ro); 0: supported. 1: *not* supported.
#define HW_REGISTER_BIT_DWT_CTRL_NOTRCPKT    (1u << 27)
// Shows whether the implementation includes external match signals (ro); 0: supported. 1: *not* supported.
#define HW_REGISTER_BIT_DWT_CTRL_NOEXTTRIG   (1u << 26)
// Shows whether the implementation supports a cycle counter (ro); 0: supported. 1: *not* supported.
#define HW_REGISTER_BIT_DWT_CTRL_NOCYCCNT    (1u << 25)
// Shows whether the implementation supports the profiling counters (ro); 0: supported. 1: *not* supported.
#define HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT    (1u << 24)
// Enables POSTCNT underflow Event counter packets generation (rw); 0: not generated. 1: packets generated (and set PCSAMPLENA to 0).
// Only valid if NOTRCPKT and NOCYCCNT both set to 0
#define HW_REGISTER_BIT_DWT_CTRL_CYCEVTENA   (1u << 22)
// Enables generation of the Folded-instruction counter overflow event (rw); 0: Disabled. 1: Enabled.
// Only valid if NOPRFCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_FOLDEVTENA  (1u << 21)
// Enables generation of the LSU counter overflow event (rw); 0: Disabled. 1: Enabled.
// Only valid if NOPRFCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_LSUEVTENA   (1u << 20)
// Enables generation of the Sleep counter overflow event (rw); 0: Disabled. 1: Enabled.
// Only valid if NOPRFCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_SLEEPEVTENA (1u << 19)
// Enables generation of the Exception overhead counter overflow event (rw); 0: Disabled. 1: Enabled.
// Only valid if NOPRFCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_EXCEVTENA   (1u << 18)
// Enables generation of the CPI counter overflow event (rw); 0: Disabled. 1: Enabled.
// Only valid if NOPRFCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_CPIEVTENA   (1u << 17)
// Enables generation of exception trace (rw); 0: Disabled. 1: Enabled.
// Only valid if NOTRCPKT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_EXCTRCENA   (1u << 16)
// Enables use of POSTCNT counter as a timer for Periodic PC sample packet generation (rw); 0: Disabled. 1: Periodic PC sample packets generated (and set CYCEVTENA to 0).
// Only valid if NOTRCPKT and NOCYCCNT both set to 0
#define HW_REGISTER_BIT_DWT_CTRL_PCSAMPLENA  (1u << 12)
// A tap on the CYCCNT counter provides a timer signal for generation of periodic Synchronization packets by the ITM (rw):
// 00 -        Synchronization packet timer disabled
// 01 Bit [24] (Processor clock)/16M
// 10 Bit [26] (Processor clock)/64M
// 11 Bit [28] (Processor clock)/256M
// Only valid if NOCYCCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_SYNCTAP     HW_REGISTER_BITFIELD(11,10)
#define HW_REGISTER_BIT_DWT_CTRL_SYNCTAP_POS 10
// Selects the position of the POSTCNT tap on the CYCCNT counter (rw):
// 0 Bit [6]  (Processor clock)/64
// 1 Bit [10] (Processor clock)/1024
#define HW_REGISTER_BIT_DWT_CTRL_CYCTAP      (1u << 9)
// Initial value for the POSTCNT counter (rw).
// When software enables use of the POSTCNT timer, the processor loads the initial POSTCNT value from
// the DWT_CTRL.POSTINIT field.
// Only valid if NOCYCCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_POSTINIT    HW_REGISTER_BITFIELD(8,5)
#define HW_REGISTER_BIT_DWT_CTRL_POSTINIT_POS 5
// Reload value for the POSTCNT counter (rw).
// Subsequently whenever the CYCCNT tap bit transitions, either from 0 to 1 or from 1 to 0:
// • if POSTCNT holds a nonzero value, POSTCNT decrements by 1
// • if POSTCNT is zero, the DWT unit: — reloads POSTCNT from DWT_CTRL.POSTPRESET
//                                     — generates the required Periodic PC sample packets or Event counter packet.
// Only valid if NOCYCCNT is set to 0
#define HW_REGISTER_BIT_DWT_CTRL_POSTPRESET  HW_REGISTER_BITFIELD(4,1)
#define HW_REGISTER_BIT_DWT_CTRL_POSTPRESET_POS 1
// Enables CYCCNT (rw); 0: Disabled; 1: Enabled
#define HW_REGISTER_BIT_DWT_CTRL_CYCCNTENA   (1u << 0)

// FUNCTION: Comparator Function register
// (rw) 0: No match. 1: Match. Cleared to 0 by reading.
#define HW_REGISTER_BIT_DWT_FUNCTION_MATCHED          (1u << 24)
// (rw) (only valid if (LNK1ENA && DATAVMATCH)) holds number of a second comparator to use for linked address comparison. Disabled if pointing to itself.
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR1       HW_REGISTER_BITFIELD(19,16)
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR1_POS   16
// (rw) (only used if (DATAVMATCH)) holds the number of a comparator to use for linked address comparison. Disabled if pointing to itself.
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR0       HW_REGISTER_BITFIELD(15,12)
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR0_POS   12
// (rw) For data value matching, specifies the size of the required data comparison: 00: Byte. 01: Halfword. 10: Word.
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVSIZE        HW_REGISTER_BITFIELD(11,10)
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVSIZE_POS    10
// (ro) Indicates whether the implementation supports use of a second linked comparator: 1: supported. 0: 2nd linked comparator not supported.
#define HW_REGISTER_BIT_DWT_FUNCTION_LNK1ENA          (1u << 9)
// (rw) Enables data value comparison (if supported): 0: Perform address comparison. 1: Perform data value comparison. */
#define HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH       (1u << 8)
// (rw) (comp[0].function only && dwtdbg_feature_CYCLECOUNTER): 1: Compare DWT_COMP0 with the cycle counter, DWT_CYCCNT. 0: No comparison.
#define HW_REGISTER_BIT_DWT_FUNCTION_CYCMATCH         (1u << 7)
// (rw) 0: Disabled. 1: Enable Data trace address offset packet generation, that hold Daddr[15:0].
#define HW_REGISTER_BIT_DWT_FUNCTION_EMITRANGE        (1u << 5)
// (rw) 0000: Disabled or Linked Address Comparator. (DATAVMATCH == 0 && CYCMATCH == 0) ==> address comparison
#define HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION         HW_REGISTER_BITFIELD(3,0)
#define HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS     0


// section: inline implementation

static inline uint8_t nrcomp_dwtdbg(void)
{
   static_assert( 0xf0000000 == HW_REGISTER_BIT_DWT_CTRL_NUMCOMP);
   return (uint8_t) ((DWTDBG->ctrl & HW_REGISTER_BIT_DWT_CTRL_NUMCOMP) >> HW_REGISTER_BIT_DWT_CTRL_NUMCOMP_POS);
}

static inline dwtdbg_feature_e feature_dwtdbg(void)
{
   static_assert( HW_REGISTER_BIT_DWT_CTRL_NOTRCPKT  == dwtdbg_feature_TRACEPACKET * HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT);
   static_assert( HW_REGISTER_BIT_DWT_CTRL_NOEXTTRIG == dwtdbg_feature_EXTTRIGGER * HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT);
   static_assert( HW_REGISTER_BIT_DWT_CTRL_NOCYCCNT  == dwtdbg_feature_CYCLECOUNTER * HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT);
   static_assert( HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT  == dwtdbg_feature_PROFILECOUNTER * HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT);

   static_assert(HW_REGISTER_BIT_DWT_CTRL_NOPRFCNT == 1 << 24);

   return (~ DWTDBG->ctrl >> 24) & dwtdbg_feature_ALL;
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
   counter &= dwtdbg_COUNTER_AND_TRACE;

   static_assert(dwtdbg_CYCLECOUNT == HW_REGISTER_BIT_DWT_CTRL_CYCCNTENA);
   static_assert(dwtdbg_CPICOUNT   == HW_REGISTER_BIT_DWT_CTRL_CPIEVTENA);
   static_assert(dwtdbg_EXCCOUNT   == HW_REGISTER_BIT_DWT_CTRL_EXCEVTENA);
   static_assert(dwtdbg_SLEEPCOUNT == HW_REGISTER_BIT_DWT_CTRL_SLEEPEVTENA);
   static_assert(dwtdbg_LSUCOUNT   == HW_REGISTER_BIT_DWT_CTRL_LSUEVTENA);
   static_assert(dwtdbg_FOLDCOUNT  == HW_REGISTER_BIT_DWT_CTRL_FOLDEVTENA);

   DWTDBG->ctrl &= ~counter;  // disable counter
   if (0 != (counter & dwtdbg_CYCLECOUNT)) {
      DWTDBG->cyccnt = 0;     // reset cycle counter
   }
   DWTDBG->ctrl |= counter;   // other 8-bit counters reset to 0 if enabled
}

static inline void stop_dwtdbg(dwtdbg_e counter)
{
   counter &= dwtdbg_COUNTER_AND_TRACE;
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
      } while ((DWTDBG->comp[cid].function & HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION) != 0 && cid > 0);
   }

   if ((DWTDBG->comp[cid].function & HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION) != 0) {
      return ENOMEM;
   }

   uint32_t mask = 0;
   uint32_t fct  = DWTDBG->comp[cid].function;
   fct &= ~(   HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR1
               | HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR0
               | HW_REGISTER_BIT_DWT_FUNCTION_DATAVSIZE
               | HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH
               | HW_REGISTER_BIT_DWT_FUNCTION_CYCMATCH
               | HW_REGISTER_BIT_DWT_FUNCTION_EMITRANGE
               | HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION
         );

   // disable linked address comparison (not implemented)
   fct |=   (cid << HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR1_POS)
            | (cid << HW_REGISTER_BIT_DWT_FUNCTION_DATAVADDR0_POS);

   switch (wp) {
   case dwtdbg_watchpoint_CODEADDR:
      comp &= ~1; // clear thumb state bit
   case dwtdbg_watchpoint_DATAADDR_RO:
   case dwtdbg_watchpoint_DATAADDR_WO:
   case dwtdbg_watchpoint_DATAADDR_RW:
      if (ignore_nr_lsb_bits > 31) {
         return EINVAL;
      }
      static_assert(dwtdbg_watchpoint_DATAADDR_RO == 1 + dwtdbg_watchpoint_CODEADDR);
      static_assert(dwtdbg_watchpoint_DATAADDR_WO == 2 + dwtdbg_watchpoint_CODEADDR);
      static_assert(dwtdbg_watchpoint_DATAADDR_RW == 3 + dwtdbg_watchpoint_CODEADDR);
      fct |= (4 + (wp-dwtdbg_watchpoint_CODEADDR)) << HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS;
      break;
   case dwtdbg_watchpoint_VALUE8BIT_RO:
   case dwtdbg_watchpoint_VALUE8BIT_WO:
   case dwtdbg_watchpoint_VALUE8BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE8BIT_WO == 1 + dwtdbg_watchpoint_VALUE8BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE8BIT_RW == 2 + dwtdbg_watchpoint_VALUE8BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE8BIT_RO)) << HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS)
          |  HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH;
      comp &= 0xff;
      comp |= comp << 8;
      comp |= comp << 16;
      break;
   case dwtdbg_watchpoint_VALUE16BIT_RO:
   case dwtdbg_watchpoint_VALUE16BIT_WO:
   case dwtdbg_watchpoint_VALUE16BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE16BIT_WO == 1 + dwtdbg_watchpoint_VALUE16BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE16BIT_RW == 2 + dwtdbg_watchpoint_VALUE16BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE16BIT_RO)) << HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS)
          | ( HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH
              | (1 << HW_REGISTER_BIT_DWT_FUNCTION_DATAVSIZE_POS));
      comp &= 0xffff;
      comp |= comp << 16;
      break;
   case dwtdbg_watchpoint_VALUE32BIT_RO:
   case dwtdbg_watchpoint_VALUE32BIT_WO:
   case dwtdbg_watchpoint_VALUE32BIT_RW:
      static_assert(dwtdbg_watchpoint_VALUE32BIT_WO == 1 + dwtdbg_watchpoint_VALUE32BIT_RO);
      static_assert(dwtdbg_watchpoint_VALUE32BIT_RW == 2 + dwtdbg_watchpoint_VALUE32BIT_RO);
      fct |= ((5 + (wp-dwtdbg_watchpoint_VALUE32BIT_RO)) << HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS)
          | ( HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH
              | (2 << HW_REGISTER_BIT_DWT_FUNCTION_DATAVSIZE_POS));
      break;
   case dwtdbg_watchpoint_CYCLECOUNT:
      fct |= (4 << HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION_POS)
           | HW_REGISTER_BIT_DWT_FUNCTION_CYCMATCH;
      break;
   default:
      return EINVAL;
   }

   DWTDBG->comp[cid].comp = comp;
   DWTDBG->comp[cid].mask = mask;
   DWTDBG->comp[cid].function = fct;

   if (  mask != DWTDBG->comp[cid].mask
         || 0 != ( (fct ^ DWTDBG->comp[cid].function)
                   & (HW_REGISTER_BIT_DWT_FUNCTION_DATAVMATCH|HW_REGISTER_BIT_DWT_FUNCTION_CYCMATCH))) {
      clearwatchpoint_dwtdbg(cid);
      return ENOSYS;
   }

   // set out param
   *wpid = (uint8_t) cid;

   return 0;
}

static inline void clearwatchpoint_dwtdbg(uint32_t wpid)
{
   if (wpid < nrcomp_dwtdbg() && (DWTDBG->comp[wpid].function & HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION) != 0) {
      DWTDBG->comp[wpid].comp      = 0;
      DWTDBG->comp[wpid].mask      = 0;
      DWTDBG->comp[wpid].function &= ~HW_REGISTER_BIT_DWT_FUNCTION_FUNCTION;
   }
}

static inline int ismatch_dwtdbg(uint32_t wpid)
{
   if (wpid < nrcomp_dwtdbg()) {
      return (DWTDBG->comp[wpid].function / HW_REGISTER_BIT_DWT_FUNCTION_MATCHED) & 1;
   }

   return 0;
}

#endif
