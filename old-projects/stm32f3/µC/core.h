/* title: CortexM4-Core

   Beschreibt die Register-Struktur der dem Cortex-M4 Prozessor internen Peripherie:

      • SysTick - 24-Bit clear-on-write, decrementing, wrap-on-zero Timer
      • Nested Vectored Interrupt Controller (NVIC) – Exceptions und Interrupts
      • System Control Block (SCB) - System Information und System Konfiguration
      • Memory Protection Unit (MPU) - ARMv7 Protected Memory System Architecture (PMSA)
      • Floating-Point Unit (FPU) - Single-precision HArdware support (+ fixed-point Konvertierung)

   Das STM32F303xC Spezifische ist in µC/board.h ausgegliedert.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Header file <CortexM4-Core>.

   file: TODO: µC/core.c
    Implementation file <CortexM4-Core impl>.
*/
#ifndef CORTEXM4_MC_CORE_HEADER
#define CORTEXM4_MC_CORE_HEADER

// == Exported Address Ranges of Core HW-Units
#define HW_REGISTER_BASEADDR_SYSTEM    0xE0000000  /* Cortex-M4 System address range 0xE0000000 to 0xFFFFFFFF:
                                                    * Internal Private Peripheral Bus (PPB): 0xE0000000 .. 0xE003FFFF
                                                    * External Private Peripheral Bus (PPB): 0xE0040000 .. 0xE00FFFFF
                                                    * Vendor Specific System region   (AHB): 0xE0100000 .. 0xFFFFFFFF */
#define HW_REGISTER_BASEADDR_DWT       0xE0001000  /* Cortex-M4 Data Watchpoint and Trace (0xE0001000 .. 0xE0001FFF) */
#define HW_REGISTER_BASEADDR_SCS       0xE000E000  /* Cortex-M4 System Control Space contains core peripherals (0xE000E000 .. 0xE000EFFF) */
#define HW_REGISTER_BASEADDR_SYSTICK   0xE000E010  /* Cortex-M4 Systick timer base address */
#define HW_REGISTER_BASEADDR_NVIC      0xE000E100  /* Cortex-M4 Nested Vectored Interrupt Controller */
#define HW_REGISTER_BASEADDR_SCB       0xE000ED00  /* Cortex-M4 System Bontrol Block */
#define HW_REGISTER_BASEADDR_MPU       0xE000ED90  /* Cortex-M4 Memory Protection Unit */
#define HW_REGISTER_BASEADDR_COREDEBUG 0xE000EDF0  /* Cortex-M4 Core Debug Base Address */
#define HW_REGISTER_BASEADDR_FPU       0xE000EF30  /* Cortex-M4 Floating Point Unit */

// == Exported Peripherals/HW-Units
#define hCORE     ((core_sys_t*) HW_REGISTER_BASEADDR_SYSTEM)
#define hSCS      ((core_scs_t*) HW_REGISTER_BASEADDR_SCS)
#define hSCB      ((core_scb_t*) HW_REGISTER_BASEADDR_SCB)

// == Exported Types
struct core_scs_t;
struct core_systick_t;
struct core_nvic_t;
struct core_scb_t;
struct core_mpu_t;
struct core_debug_t;
struct core_fpu_t;
struct core_sys_t;

// == Exported Functions

// == Sleep mode / Power save mode
// TODO: add support for deep sleep states !
static inline void waitinterrupt_core(void); // Enter sleep mode (see WFI) and wakes up with next interrupt (effects of PRIMASK (setprio0mask_interrupt) are ignored. pending interrupt which would execute if PRIMASK is cleared let the CPU wake-up).
static inline void waitevent_core(void);     // Wait for event-flag to be set, clears it before return. Enter sleep mode if flag not set. (see also WFE).
static inline void setevent_core(void);      // Set cpu-internal event flag to 1.

// == Reset
static inline void reset_core(void);

// == FPU
// TODO: move fpu into own module
static inline void enable_fpu(int allowUnprivilegedAccess);
static inline void disable_fpu(void);

// == Exported Macros

/* HW_BIT is used to generate the name for the bit(-field) value of a certain register within same base region. */
#define HW_BIT(BASE, REG, NAME) \
         HW_REGISTER_BIT_ ## BASE ##_## REG ##_## NAME

/* HW_OFF is used to generate the name for the offset value of a register relative to a base memory region. */
#define HW_OFF(BASE, REG) \
         HW_REGISTER_OFFSET_ ## BASE ##_## REG

/* HW_BASE is used to generate the name for the base memory address of a certain memory region. */
#define HW_BASE(BASE) \
         HW_REGISTER_BASEADDR_ ## BASE

/* HW_SREGISTER is used to access a memory mapped register with offset from base address. */
#define HW_SREGISTER(BASE, REGNAME) \
         (((struct { uint8_t _d[HW_REGISTER_OFFSET_ ## BASE ##_## REGNAME]; volatile uint32_t reg; }*) (HW_REGISTER_BASEADDR_ ## BASE))->reg)

/* HW_REGISTER is used to access a memory mapped register as sum of base address and offset. */
#define HW_REGISTER(BASE, REGNAME) \
         (*((volatile uint32_t*) ((HW_REGISTER_BASEADDR_ ## BASE) + (HW_REGISTER_OFFSET_ ## BASE ##_## REGNAME))))

// TODO: remove HW_REGISTER_MASK
/* Creates Mask as (HW_REGISTER_MASK_BASE_REGNAME_FIELD_BITS << HW_REGISTER_MASK_BASE_REGNAME_FIELD_POS) */
#define HW_REGISTER_MASK(BASE, REGNAME, FIELD) \
         (HW_REGISTER_BIT_##BASE##_##REGNAME##_##FIELD##_BITS << HW_REGISTER_BIT_##BASE##_##REGNAME##_##FIELD##_POS)

// TODO: remove HW_REGISTER_BITFIELD

/* Zum Erezugen einer Maske aus höchstem und niederwertigstem Bit */
#define HW_REGISTER_BITFIELD(high,low)   (((1u << (high)) + ((1u << (high))-1u)) & ~((1u << (low))-1u))

/* HW_DEF_OFF is used to generate an offset for a register relative to some base address. */
#define HW_DEF_OFF(BASE, REG, OFFSET) \
         HW_REGISTER_OFFSET_ ## BASE ##_## REG = OFFSET

/* HW_DEF_BIT is used to generate a bit(-field) value for a register relative to some base address.
 * The name HW_BIT(BASE, REG, NAME) contains the bit-mask where every bit between MSBit and LSBit is 1 all other 0.
 * The name HW_BIT(BASE, REG, NAME_POS) contains the value LSBit which is the position of the first one bit in HW_BIT(BASE, REG, NAME).
 * The name HW_BIT(BASE, REG, NAME_MAX) contains the value HW_BIT(BASE, REG, NAME) shifted HW_BIT(BASE, REG, NAME_POS) bit positions to the right. */
#define HW_DEF_BIT(BASE, REG, NAME, MSBit, LSBit) \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS = LSBit, \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_MAX = ((((1u << ((MSBit) - HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS))-1u)<<1u)+1u), \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME       = HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_MAX << HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS

// == Definitions

typedef volatile struct core_scs_t {
   uint32_t       _reserved0[1];
   uint32_t const ictr;   /* Interrupt Controller Type Register, ro, Offset: 0x04, Reset: IMPLDEF
                           * Returns number of interrupts supported by NVIC in granularities of 32.
                           */
   uint32_t       actlr;  /* Auxiliary Control Register, rw, Offset: 0x08, Reset: 0x00000000
                           * Implementation defined.
                           */
} core_scs_t;

typedef volatile struct core_systick_t {
   uint32_t       csr;    /* SysTick Control and Status Register, rw, Offset: 0x00, Reset: 0x00000000
                           * Controls the system timer and provides status data.
                           */
   uint32_t       rvr;    /* SysTick Reload Value Register, rw, Offset: 0x04, Reset: Unknown
                           * Holds the reload value of the SYSTICK_CVR.
                           */
   uint32_t       cvr;    /* SysTick Current Value Register, rw, Offset: 0x08, Reset: Unknown
                           * Reads or clears the current counter value.
                           */
   uint32_t const calib;  /* SysTick Calibration Register, ro, Offset: 0x0C, Reset: IMPLDEF
                           * Reads the calibration value and parameters for SysTick.
                           */
} core_systick_t;

typedef volatile struct core_nvic_t {
   uint32_t       iser[8];      /* Interrupt Set Enable Register, rw, Offset: 0x000, Reset: 0x00000000
                                 * Enables, or reads the enable state of a group of interrupts.
                                 * reg[n] bit[m] SETENA reads: 1: Interrupt (32*n+m) enabled. 0: Disabled. writes: 1: Enable interrupt. 0: No effect. */
   uint32_t       _res0[24]; /*  */
   uint32_t       icer[8];      /* Interrupt Clear Enable Register, rw, Offset: 0x080, Reset: 0x00000000
                                 * Disables, or reads the enable state of a group of interrupts.
                                 * reg[n] bit[m] CLRENA reads: 1: Interrupt (32*n+m) enabled. 0: Disabled. writes: 1: Disable interrupt. 0: No effect. */
   uint32_t       _res1[24]; /*  */
   uint32_t       ispr[8];      /* Interrupt Set Pending Register, rw, Offset: 0x100, Reset: 0x00000000
                                 * Changes interrupt status to pending, or shows the current pending status of a group of interrupts.
                                 * reg[n] bit[m] SETPEND reads: 1: Interrupt (32*n+m) is pending. 0: Not pending. writes: 1: Changes interrupt state to pending. 0: No effect. */
   uint32_t       _res2[24]; /*  */
   uint32_t       icpr[8];      /* Interrupt Clear Pending Register, rw, Offset: 0x180, Reset: 0x00000000
                                 * Clears interrupt pending status, or shows the current pending status of a group of interrupts.
                                 * reg[n] bit[m] CLRPEND reads: 1: Interrupt (32*n+m) is pending. 0: Not pending. writes: 1: Clears interrupt state to not pending. 0: No effect. */
   uint32_t       _res3[24]; /*  */
   uint32_t const iabr[8];      /* Interrupt Active Bit Register, ro, Offset: 0x200, Reset: 0x00000000
                                 * Shows whether interrupt is active (ISR is executed) of a group of interrupts.
                                 * reg[n] bit[m] ACTIVE reads: 1: Interrupt (32*n+m) is active. 0: Not active. */
   uint32_t const _res4[56]; /*  */
   uint8_t        ipr[240];     /* Interrupt Priority Register, rw, Offset: 0x300, Reset: 0x00000000
                                 * Sets or reads interrupt priorities of a group of interrupts.
                                 * reg[n] bit[7..8-HW_KONFIG_NVIC_INTERRUPT_PRIORITY_NROFBITS] PRI rw: Priority of interrupt n.
                                 * reg[n] bit[other] Read as 0, write ignored.
                                 */
} core_nvic_t;

typedef volatile struct core_scb_t {
   uint32_t const cpuid;     /* CPUID Base Register, ro, Offset: 0x000, Reset: IMPLDEF
                              * The CPUID scheme provides a description of the features of an ARM processor implementation.
                              */
   uint32_t       icsr;      /* Interrupt Control and State Register, rw, Offset: 0x004, Reset:
                              * Provides pending state for NMI, PendSV and SysTick, nr of highest priority pending exception, whether there are preempted active exceptions.
                              */
   uint32_t       vtor;      /* Vector Table Offset Register, rw, Offset: 0x008, Reset: 0x00000000
                              * Holds the vector table address aligned to 512 bytes.
                              */
   uint32_t       aircr;     /* Application Interrupt and Reset Control Register, rw, Offset: 0x00C, Reset: 0xFA050000
                              * Sets or returns interrupt control data.
                              */
   uint32_t       scr;       /* System Control Register, rw, Offset: 0x010, Reset: 0x00000000
                              * Determines sleep mode(low power state) and pending wakeup events.
                              */
   uint32_t       ccr;       /* Configuration Control Register, rw, Offset: 0x014, Reset: 0x00000200
                              * Configures behaviour of exc. handlers, trapping of divide by zero and unaligned accesses and unprivileged access STIR.
                              */
   uint8_t        shpr[12];  /* System Handlers Priority Registers (4-7, 8-11, 12-15), rw, Offset: 0x018, Reset: 0x00000000
                              * Control the priority of the handlers for the system faults that have configurable priority.
                              * Exceptions 1, 2, and 3, Reset, NMI, and HardFault, have fixed priorities.
                              * Therefore, the first defined priority field is 4 that controls coreinterrupt_MPUFAULT exception.
                              * Registers are byte accessible. Bits 3:0 read always as 0.
                              */
   uint32_t       shcsr;     /* System Handler Control and State Register, rw, Offset: 0x024, Reset: 0x00000000
                              * Controls and provides the active and pending status of system exceptions.
                              */
   uint32_t       cfsr;      /* Configurable Fault Status Register, rw, Offset: 0x028, Reset: 0x00000000
                              * Comprises the status registers for the faults that have configurable priority.
                              • If more than one fault occurs, all associated bits are set to 1.
                              * Note: Write a one to a register bit to clear the corresponding fault.
                              */
   uint32_t       hfsr;      /* HardFault Status Register, rw, Offset: 0x02C, Reset: 0x00000000
                              * Shows the cause of any hard faults (FAULT).
                              * Note: Write a one to a register bit to clear the corresponding fault.
                              */
   uint32_t       dfsr;      /* Debug Fault Status Register, rw, Offset: 0x030, Reset: 0x00000000
                              * Shows, at the top level, why a debug event occurred.
                              * Note: Write a one to a register bit to clear the corresponding fault.
                              */
   uint32_t       mmfar;     /* MemManage Fault Address Register, rw, Offset: 0x034, Reset: Undefined
                              * Shows the address of the memory location that caused an MPU fault.
                              */
   uint32_t       bfar;      /* BusFault Address Register, rw, Offset: 0x038, Reset: Undefined
                              * Shows the address associated with a precise data access fault.
                              */
   uint32_t       afsr;      /* Auxiliary Fault Status Register, rw, Offset: 0x03C, Reset: 0x00000000
                              * Provides implementation-specific fault status information and control.
                              * Note: Write a one to a register bit to clear the corresponding fault.
                              */
   uint32_t const pfr[2];    /* Processor Feature Register, rw, Offset: 0x040, Reset: IMPLDEF
                              * Provides a description of the features of an ARM processor implementation.
                              */
   uint32_t const dfr;       /* Debug Feature Register, rw, Offset: 0x048, Reset: IMPLDEF
                              * Provides a description of the features of an ARM processor implementation.
                              */
   uint32_t const afr;       /* Auxiliary Feature Register, ro, Offset: 0x04C, Reset: IMPLDEF
                              * Provides a description of the features of an ARM processor implementation.
                              */
   uint32_t const mmfr[4];   /* Memory Model Feature Register, ro, Offset: 0x050, Reset: IMPLDEF
                              * Provides a description of the features of an ARM processor implementation.
                              */
   uint32_t const isar[5];   /* Instruction Set Attributes Register, ro, Offset: 0x060, Reset: IMPLDEF
                              * Provides a description of the features of an ARM processor implementation. */
   uint32_t const _res0[5];/* */
   uint32_t       cpacr;     /* Coprocessor Access Control Register, rw, Offset: 0x088, Reset: 0x0000000
                              * Specifies access privileges for coprocessors (FPU only).
                              */
} core_scb_t;

typedef volatile struct core_mpu_t {
   uint32_t const type;      /* MPU Type Register, ro, Offset: 0x00, Reset: 0x00000800
                              * Indicates how many regions the MPU support.
                              */
   uint32_t       ctrl;      /* MPU Control Register, rw, Offset: 0x04, Reset: 0x00000000
                              * Enables MPU and background region for privileged accesses,
                              * and whether MPU is enabled for exceptions handlers executing with priority <= -1.
                              */
   uint32_t       rnr;       /* MPU Region Number Register, rw, Offset: 0x08, Reset: 0x00000000
                              * Selects the region currently accessed by MPU_RBAR and MPU_RSAR.
                              */
   uint32_t       rbar;      /* MPU Region Base Address Register, rw, Offset: 0x0C, Reset: 0x00000000
                              * Holds the base address of the region identified by MPU_RNR.
                              * On a write, can also be used to update MPU_RNR with a new region number in the range 0 to 15.
                              */
   uint32_t       rasr;      /* MPU Region Attribute and Size Register, rw, Offset: 0x10, Reset: 0x00000000
                              * Defines the size and access behavior of the region identified by MPU_RNR, and enables that region.
                              */
   uint32_t       rbar_a1;   /* MPU Alias 1 Region Base Address Register, rw, Offset: 0x14, Reset: -
                              * Alias 1 of MPU_RBAR. Used with LDM/STM to configure multiple regions at once.
                              */
   uint32_t       rasr_a1;   /* MPU Alias 1 Region Attribute and Size Register, rw, Offset: 0x18, Reset: -
                              * Alias 1 of MPU_RASR. Used with LDM/STM to configure multiple regions at once.
                              */
   uint32_t       rbar_a2;   /* MPU Alias 2 Region Base Address Register, rw, Offset: 0x1C, Reset: -
                              * Alias 2 of MPU_RBAR. Used with LDM/STM to configure multiple regions at once.
                              */
   uint32_t       rasr_a2;   /* MPU Alias 2 Region Attribute and Size Register, rw, Offset: 0x20, Reset: -
                              * Alias 2 of MPU_RASR. Used with LDM/STM to configure multiple regions at once.
                              */
   uint32_t       rbar_a3;   /* MPU Alias 3 Region Base Address Register, rw, Offset: 0x24, Reset: -
                              * Alias 3 of MPU_RBAR. Used with LDM/STM to configure multiple regions at once.
                              */
   uint32_t       rasr_a3;   /* MPU Alias 3 Region Attribute and Size Register, rw, Offset: 0x28, Reset: -
                              * Alias 3 of MPU_RASR. Used with LDM/STM to configure multiple regions at once.
                              */
} core_mpu_t;

typedef volatile struct core_debug_t {
   uint32_t       dhcsr;     /* Debug Halting Control and Status Register, rw, Offset: 0x00, Reset:
                              */
   uint32_t       dcrsr;     /* Debug Core Register Selector Register, wo, Offset: 0x04, Reset:
                              */
   uint32_t       dcrdr;     /* Debug Core Register Data Register, rw, Offset: 0x08, Reset:
                              */
   uint32_t       demcr;     /* Debug Exception and Monitor Control Register, rw, Offset: 0x0C, Reset:
                              */
} core_debug_t;

typedef volatile struct core_fpu_t {
   uint32_t       _res[1]; /* */
   uint32_t       fpccr;     /* Floating-Point Context Control Register, rw, Offset: 0x04, Reset:
                              */
   uint32_t       fpcar;     /* Floating-Point Context Address Register, rw, Offset: 0x08, Reset:
                              */
   uint32_t       fpdscr;    /* Floating-Point Default Status Control Register, rw, Offset: 0x0C, Reset:
                              */
   uint32_t const mvfr0;     /* Media and FP Feature Register 0, ro, Offset: 0x10, Reset:
                              */
   uint32_t const mvfr1;     /* Media and FP Feature Register 1, ro, Offset: 0x14, Reset:
                              */
} core_fpu_t;

typedef volatile struct core_sys_t {

   uint32_t       _reserved0[(HW_REGISTER_BASEADDR_SCS - HW_REGISTER_BASEADDR_SYSTEM) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_SCS (Privileged)
   core_scs_t     scs;

   uint32_t       _reserved1[(HW_REGISTER_BASEADDR_SYSTICK - HW_REGISTER_BASEADDR_SCS - 0x0C) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_SYSTICK (Privileged)
   core_systick_t systick;

   uint32_t       _reserved2[(HW_REGISTER_BASEADDR_NVIC - HW_REGISTER_BASEADDR_SYSTICK - 0x10) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_NVIC (Privileged)
   core_nvic_t    nvic;

   uint32_t       _reserved3[(HW_REGISTER_BASEADDR_SCB - HW_REGISTER_BASEADDR_NVIC - 0x3F0) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_SCB (Privileged)
   core_scb_t     scb;

   uint32_t       _reserved4[(HW_REGISTER_BASEADDR_MPU - HW_REGISTER_BASEADDR_SCB - 0x08C) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_MPU (Privileged)
   core_mpu_t     mpu;

   uint32_t       _reserved5[(HW_REGISTER_BASEADDR_COREDEBUG - HW_REGISTER_BASEADDR_MPU - 0x2C) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_COREDEBUG (Privileged)
   core_debug_t   debug;

   uint32_t       _reserved6[(0xE000EF00 - HW_REGISTER_BASEADDR_COREDEBUG - 0x10) / sizeof(uint32_t)];

   // startaddr: 0xE000EF00 (Privileged or Unprivileged)
   uint32_t       stir;      /* Software Trigger Interrupt Register, wo, Offset: 0xF00, Reset: Unknown
                              * Provides a mechanism for software to generate an external interrupt (exception nr >= 16).
                              */

   uint32_t       _reserved7[(HW_REGISTER_BASEADDR_FPU - 0xE000EF00 - 0x04) / sizeof(uint32_t)];

   // startaddr: HW_REGISTER_BASEADDR_FPU (Privileged)
   core_fpu_t     fpu;

} core_sys_t;


// == Register Descriptions

enum core_register_e {
   // TODO: comments

   HW_DEF_OFF(SCS, ICTR, 0x004),             // ICTR: Interrupt Controller Type Register
   HW_DEF_BIT(SCS, ICTR, INTLINESNUM, 3, 0), // The total number of interrupt lines supported by an implementation, defined in groups of 32. That is, the total number of interrupt lines is up to (INTLINESNUM+1)*32.

   HW_DEF_OFF(SCS, ACTLR, 0x008),            // ACTLR: Auxiliary control register
   HW_DEF_BIT(SCS, ACTLR, DISOOFP, 9, 9),    // 0: Enabled. 1: Disables floating point instructions completing out of order with respect to integer instructions.
   HW_DEF_BIT(SCS, ACTLR, DISFPCA, 8, 8),    // 0: Enabled. 1: Disables automatic update of CONTROL.FPCA. The value of this bit should be written as zero or preserved.
   HW_DEF_BIT(SCS, ACTLR, DISFOLD, 2, 2),    // 0: Enabled. 1: Disables folding of IT instructions: IT folding can cause jitter in looping. If a task must avoid jitter, set the DISFOLD bit to 1 before executing the task, to disable IT folding.
   HW_DEF_BIT(SCS, ACTLR, DISDEFWBUF, 1, 1), // 0: Enabled. 1: Disables write buffer use during default memory map accesses: This causes all BusFaults to be precise BusFaults but decreases performance because any store to memory must complete before the processor can execute the next instruction.
   HW_DEF_BIT(SCS, ACTLR, DISMCYCINT, 0, 0), // 0: Enabled. 1: Disables interruption of load multiple and store multiple instructions (LDM, STM). This increases the interrupt latency of the processor because any LDM or STM must complete before the processor can stack the current state and enter the interrupt handler.

   HW_DEF_OFF(SYSTICK, CSR, 0x00),              // CSR: SysTick Control and Status Register
   HW_DEF_BIT(SYSTICK, CSR, COUNTFLAG, 16, 16), // (ro) 1: Timer transition occurred from 1 to 0. 0: Timer has not counted to 0. COUNTFLAG is cleared to 0 by a read of this register, and by any write to SYSTICK_CVR.
   HW_DEF_BIT(SYSTICK, CSR, CLKSOURCE, 2, 2),   // 1: SysTick uses the processor clock. 0: SysTick uses IMPLDEF external reference clock (processor clock/8).
   HW_DEF_BIT(SYSTICK, CSR, TICKINT, 1, 1),     // 1: Count to 0 changes the SysTick exception status to pending. 0: Count to 0 does not affect the SysTick exception status.
   HW_DEF_BIT(SYSTICK, CSR, ENABLE, 0, 0),      // 1: Counter is operating. 0: Counter is disabled.
   HW_DEF_OFF(SYSTICK, RVR, 0x04),              // RVR: SysTick Reload Value Register
   HW_DEF_BIT(SYSTICK, RVR, RELOAD, 23, 0),     // The value to load into the SYSTICK_CVR when the counter reaches 0.
   HW_DEF_OFF(SYSTICK, CVR, 0x08),              // CVR: SysTick Current Value Register
   HW_DEF_BIT(SYSTICK, CVR, CURRENT, 31, 0),    // This is the value of the counter at the time it is read. Any write clears the register to zero.
   HW_DEF_OFF(SYSTICK, CALIB, 0x0C),            // CALIB: SysTick Calibration value Register
   HW_DEF_BIT(SYSTICK, CALIB, NOREF, 31, 31),   // 0: The IMPLDEF reference clock is implemented. 1: It is not implemented.
   HW_DEF_BIT(SYSTICK, CALIB, SKEW, 30, 30),    // 1: Calibration value is inexact, because of the clock frequency. 0: Calibration value is exact.
   HW_DEF_BIT(SYSTICK, CALIB, TENMS, 23, 0),    // Holds a reload value to be used for 10ms (100Hz) timing, subject to system clock skew errors. If this field is zero, the calibration value is not known. STM32F3 holds value for 1ms period if systick HCLK is programmed with max value and systick uses HCLK/8 as external clock.

   HW_DEF_OFF(NVIC, ISER, 0x000),   // ISER: Interrupt Set-Enable Registers
   HW_DEF_OFF(NVIC, ICER, 0x080),   // ICER: Interrupt Clear-Enable Registers
   HW_DEF_OFF(NVIC, ISPR, 0x100),   // ISPR: Interrupt Set-Pending Registers
   HW_DEF_OFF(NVIC, ICPR, 0x180),   // ICPR: Interrupt Clear-Pending Registers
   HW_DEF_OFF(NVIC, IABR, 0x200),   // IABR: Interrupt Active Bit Registers
   HW_DEF_OFF(NVIC, IPR,  0x300),   // IPR: Interrupt Priority Registers

   HW_DEF_OFF(SCB, CPUID, 0x00),                // CPUID: CPUID Base Register
   HW_DEF_BIT(SCB, CPUID, IMPLEMENTER, 31, 24), // Implementer code assigned by ARM. Reads as 0x41 for a processor implemented by ARM.
   HW_DEF_BIT(SCB, CPUID, VARIANT, 23, 20),     // Variant number.
   HW_DEF_BIT(SCB, CPUID, ARCHITECTURE, 19, 16),// Reads as 0xF indicating use of the CPUID scheme.
   HW_DEF_BIT(SCB, CPUID, PARTNO, 15, 4),       // Part number.
   HW_DEF_BIT(SCB, CPUID, REVISION, 3, 0),      // Revision number.
   HW_DEF_OFF(SCB, ICSR,  0x04),                // ICSR: Interrupt Control and State Register
   HW_DEF_BIT(SCB, ICSR, NMIPENDSET, 31, 31),   // reads 1: NMI is pending. 0: NMI is inactive. writes 1: Make NMI exception active. 0: No effect.
   HW_DEF_BIT(SCB, ICSR, PENDSVSET, 28, 28),    // reads 1: PendSV is pending. 0: Not pending. writes 1: Make PendSV exception pending. 0: No effect.
   HW_DEF_BIT(SCB, ICSR, PENDSVCLR, 27, 27),    // (wo) 1: Remove PendSV pending status. 0: No effect.
   HW_DEF_BIT(SCB, ICSR, PENDSTSET, 26, 26),    // reads 1: SysTick is pending. 0: Not pending. writes 1: Make SysTick exception pending. 0: No effect.
   HW_DEF_BIT(SCB, ICSR, PENDSTCLR, 25, 25),    // (wo) 1: Remove SysTick pending status. 0: No effect.
   HW_DEF_BIT(SCB, ICSR, ISRPREEMPT, 23, 23),   // (ro) 1: Pending exception will be serviced on exit from debug halt state. 0: Will not service.
   HW_DEF_BIT(SCB, ICSR, ISRPENDING, 22, 22),   // (ro) 1: External interrupt, generated by the NVIC, is pending (nr >= 16). 0: No external interrupt pending.
   HW_DEF_BIT(SCB, ICSR, VECTPENDING, 20, 12),  // (ro) !=0: Number of the pending and *enabled* exception with highest priority. 0: No pending exception.
   HW_DEF_BIT(SCB, ICSR, RETTOBASE, 11, 11),    // (ro) (Only valid in "Handler mode") 0: There is an active exception other than the exception shown by IPSR. 1: There is no active exception other than the executing one.
   HW_DEF_BIT(SCB, ICSR, VECTACTIVE, 8, 0),     // (ro) 0: Processor is in "Thread mode". !=0: Exception number of current executing exception ("Handler mode"), same number as in processor register IPSR.
   HW_DEF_OFF(SCB, VTOR, 0x08),                 // VTOR: Vector Table Offset Register
   HW_DEF_BIT(SCB, VTOR, TBLOFF, 31, 9),        // Vector table base address, other bits are always 0, therefore aligned to 512 bytes.
   HW_DEF_OFF(SCB, AIRCR, 0x0C),                // AIRCR: Application Interrupt and Reset Control Register
   HW_DEF_BIT(SCB, AIRCR, VECTKEY, 31, 16),     // reads always 0xFA05. writes 0x05FA: Allows writing to register. !=0x05FA: Write ignored to this register.
   HW_DEF_BIT(SCB, AIRCR, ENDIANNESS, 15, 15),  // (ro) 0: Little endian. 1: Big endian.
   HW_DEF_BIT(SCB, AIRCR, PRIGROUP, 10, 8),     // 0<=x<=7: Interrupt Priority value is divided into "Group Priority Bits[7:x+1]" and "Subpriority Bits[x:0]". The group priority field defines the priority for preemption. If multiple pending exceptions have the same group priority, the exception processing logic uses the subpriority field to resolve priority within the group.
   HW_DEF_BIT(SCB, AIRCR, SYSRESETREQ, 2, 2),   // (rw) 0: Do not request a reset. 1: Request an external system reset. The architecture does not guarantee that the reset takes place immediately!
   HW_DEF_BIT(SCB, AIRCR, VECTCLRACTIVE, 1, 1), // (wo) 0: No effect. 1: Clears all active state information for fixed and configurable exceptions. This includes clearing the IPSR to zero. The effect of writing a 1 to this bit if the processor is not halted in Debug state is UNPREDICTABLE.
   HW_DEF_BIT(SCB, AIRCR, VECTRESET, 0, 0),     // (wo) 0: Do not request a reset. 1: Request a local system reset. The effect of writing a 1 to this bit if the processor is not halted in Debug state is UNPREDICTABLE. When the processor is halted in Debug state, if a write to the register writes a 1 to both VECTRESET and SYSRESETREQ, the behavior is UNPREDICTABLE.
   HW_DEF_OFF(SCB, SCR, 0x10),                  // SCR: System Control Register
   HW_DEF_BIT(SCB, SCR, SEVEONPEND, 4, 4),      // 1: Newly arrived pending (possibley disabled) interrupts can wakeup the processor (see WFE). 0: Only enabled and active interrupts or events can wakeup the processor (see WFE), pending or disabled interrupts are excluded.
   HW_DEF_BIT(SCB, SCR, SLEEPDEEP, 2, 2),       // 1: Processor uses deep sleep as its low power mode. 0: Uses sleep as its low power mode.
   HW_DEF_BIT(SCB, SCR, SLEEPONEXIT, 1, 1),     // 1: Enter sleep, or deep sleep, on return from Handler mode (interrupt service routine) to Thread mode an. 0: Do not sleep when returning to Thread mode.
   HW_DEF_OFF(SCB, CCR, 0x14),                  // CCR: Configuration Control Register
   HW_DEF_BIT(SCB, CCR, STKALIGN, 9, 9),        // 1: On exception entry stack is 8-byte aligned and the processor uses bit 9 of the stacked PSR to indicate if additional alignment was necessary. 0: Default alignment is 4-Byte.
   HW_DEF_BIT(SCB, CCR, BFHFNMIGN, 8, 8),       // 1: Precise bus fault is ignored if running priority is -1 (FAULT handler or "Thread mode" + setfaultmask_interrupt()) or -2 (NMI Handler). 0: Not ignored ("ldr r0, [r0]") causes a lockup if running priority is <= -1.
   HW_DEF_BIT(SCB, CCR, DIV_0_TRP, 4, 4),       // 1: Dividing by 0 (SDIV/UDIV) causes FAULT (USAGEFAULT if enabled) exception. 0: A divide by zero returns a quotient of 0 (no fault).
   HW_DEF_BIT(SCB, CCR, UNALIGN_TRP, 3, 3),     // 1: Unaligned word or halfword accesses FAULT (USAGEFAULT if enabled) exception. 0: Unaligned access allowed. (Unaligned load-store multiples (push,pop,stm,ldm) and word or halfword exclusive accesses (strex,ldrex) always fault).
   HW_DEF_BIT(SCB, CCR, USERSETMPEND, 1, 1),    // 1: Enables unprivileged software access to register STIR. 0: Unprivileged software cannot access STIR.
   HW_DEF_BIT(SCB, CCR, NONBASETHRDENA, 0, 0),  // 1: Allows return from Handler mode to Thread mode with nested exceptions active. 0: Trying to do so results in INVPC USAGEFAULT (LR = 0xF0000000 + EXC_RETURN).
   HW_DEF_OFF(SCB, SHPR, 0x18),                 // SHPR: System Handlers Priority Registers
   HW_DEF_BIT(SCB, SHPR, PRI, 7, (8-HW_KONFIG_NVIC_INTERRUPT_PRIORITY_NROFBITS)), // 0-15: Priority of system exception (0=highest, 15=lowest).
   HW_DEF_OFF(SCB, SHCSR, 0x24),                   // SHCSR: System Handler Control and State Register
   HW_DEF_BIT(SCB, SHCSR, USGFAULTENA, 18, 18),    // 1: USAGEFAULT exception enabled. 0: USAGEFAULT exception disabled, use FAULT exception instead.
   HW_DEF_BIT(SCB, SHCSR, BUSFAULTENA, 17, 17),    // 1: BUSFAULT exception enabled. 0: BUSFAULT exception disabled, use FAULT exception instead.
   HW_DEF_BIT(SCB, SHCSR, MEMFAULTENA, 16, 16),    // 1: MPUFAULT exception enabled. 0: MPUFAULT exception disabled, use FAULT exception instead.
   HW_DEF_BIT(SCB, SHCSR, SVCALLPENDED, 15, 15),   // 1: Exception SVCALL is pending. 0: Not pending.
   HW_DEF_BIT(SCB, SHCSR, BUSFAULTPENDED, 14, 14), // 1: Exception BUSFAULT is pending. 0: Not pending.
   HW_DEF_BIT(SCB, SHCSR, MEMFAULTPENDED, 13, 13), // 1: Exception MPUFAULT is pending. 0: Not pending.
   HW_DEF_BIT(SCB, SHCSR, USGFAULTPENDED, 12, 12), // 1: Exception USAGEFAULT is pending. 0: Not pending.
   HW_DEF_BIT(SCB, SHCSR, SYSTICKACT, 11, 11),     // 1: Exception SYSTICK is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, PENDSVACT, 10, 10),      // 1: Exception PENDSV is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, MONITORACT, 8, 8),       // 1: Exception DEBUGMONITOR is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, SVCALLACT, 7, 7),        // 1: Exception SVCALL is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, USGFAULTACT, 3, 3),      // 1: Exception USAGEFAULT is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, BUSFAULTACT, 1, 1),      // 1: Exception BUSFAULT is active. 0: Not active.
   HW_DEF_BIT(SCB, SHCSR, MEMFAULTACT, 0, 0),      // 1: Exception MPUFAULT is active. 0: Not active.
   HW_DEF_OFF(SCB, CFSR, 0x28),              // CFSR: Configurable Fault Status Register
                                             // == coreinterrupt_USAGEFAULT
   HW_DEF_BIT(SCB, CFSR, DIVBYZERO, 25, 25), // 1: Divide by zero error has occurred (only if DIV_0_TRP enabled). 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, UNALIGNED, 24, 24), // 1: Unaligned access error has occurred (see also UNALIGN_TRP). 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, NOCP, 19, 19),      // 1: A coprocessor access error has occurred. This shows that the coprocessor is disabled or not present. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, INVPC, 18, 18),     // 1: An integrity check error has occurred on EXC_RETURN. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, INVSTATE, 17, 17),  // 1: Instruction executed with invalid EPSR.T or EPSR.IT field. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, UNDEFINSTR, 16, 16),// 1: The processor has attempted to execute an undefined instruction (also coprocessor instructions). 0: No such error occurred.
                                             // == coreinterrupt_BUSFAULT
   HW_DEF_BIT(SCB, CFSR, BFARVALID, 15, 15), // 1: BFAR has valid contents. 0: Content of BFAR is not valid.
   HW_DEF_BIT(SCB, CFSR, LSPERR, 13, 13),    // 1: A BUSFAULT occurred during FP lazy state preservation. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, STKERR, 12, 12),    // 1: A derived BUSFAULT has occurred on exception entry. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, UNSTKERR, 11, 11),  // 1: A derived BUSFAULT has occurred on exception return. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, IMPRECISERR, 10, 10),// 1: Imprecise data access error has occurred. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, PRECISERR, 9, 9),   // 1: A precise data access error has occurred (BFAR points to faulting address). 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, IBUSERR, 8, 8),     // 1: A BUSFAULT on an instruction prefetch has occurred. The fault is signalled only if the instruction is issued. 0: No such error occurred.
                                             // == coreinterrupt_MPUFAULT
   HW_DEF_BIT(SCB, CFSR, MMARVALID, 7, 7),   // 1: MMAR has valid contents. 0: Content of MMAR is not valid.
   HW_DEF_BIT(SCB, CFSR, MLSPERR, 5, 5),     // 1: A MPUFAULT occurred during FP lazy state preservation. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, MSTKERR, 4, 4),     // 1: A derived MPUFAULT fault occurred on exception entry. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, MUNSTKERR, 3, 3),   // 1: A derived MPUFAULT fault occurred on exception return. 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, DACCVIOL, 1, 1),    // 1: Data access violation (MMAR shows the data address the load/store accessed). 0: No such error occurred.
   HW_DEF_BIT(SCB, CFSR, IACCVIOL, 0, 0),    // 1: MPU or Execute Never (XN) default memory map access violation on an instruction fetch has occurred. The fault is signalled only if the instruction is issued. 0: No such error occurred.

   HW_DEF_OFF(SCB, HFSR, 0x2C),              // HFSR: HardFault Status Register
   HW_DEF_BIT(SCB, HFSR, DEBUGEVT, 31, 31),  // 1: Debug event has occurred. The Debug Fault Status Register has been updated. 0: No Debug event has occurred.
   HW_DEF_BIT(SCB, HFSR, FORCED, 30, 30),    // 1: Processor has escalated a configurable-priority exception to FAULT. 0: No priority escalation has occurred.
   HW_DEF_BIT(SCB, HFSR, VECTTBL, 1, 1),     // 1: Vector table read fault has occurred. 0: No vector table read fault has occurred.
   HW_DEF_OFF(SCB, DFSR, 0x30),              // DFSR: Debug Fault Status Register
   HW_DEF_BIT(SCB, DFSR, EXTERNAL, 4, 4),    // 1: Assertion of external debug request (EDBGRQ). 0: No external debug event.
   HW_DEF_BIT(SCB, DFSR, VCATCH, 3, 3),      // 1: Vector catch triggered. 0: No vector catch triggered.
   HW_DEF_BIT(SCB, DFSR, DWTTRAP, 2, 2),     // 1: At least one current debug event generated by the DWT. 0: No current debug events generated by the DWT.
   HW_DEF_BIT(SCB, DFSR, BKPT, 1, 1),        // 1: At least one current breakpoint debug event. 0: No current breakpoint debug event.
   HW_DEF_BIT(SCB, DFSR, HALTED, 0, 0),      // 1: Halt request debug event triggered by DHCSR.C_HALT or DHCSR.C_STEP or by DEMCR.MON_STEP. 0: No active halt request debug event.
   HW_DEF_OFF(SCB, MMFAR, 0x34),             // MMFAR: MemManage Fault Address Register
   HW_DEF_BIT(SCB, MMFAR, ADDRESS, 31, 0),   // Data address for an MPU fault. This is the location addressed by an attempted load or store access that was faulted.
   HW_DEF_OFF(SCB, BFAR, 0x38),              // BFAR: BusFault Address Register
   HW_DEF_BIT(SCB, BFAR, ADDRESS, 31, 0),    // Data address for a precise bus fault. This is the location addressed by an attempted data access that was faulted.
   HW_DEF_OFF(SCB, AFSR, 0x3C),              // AFSR: Auxiliary Fault Status Register
   HW_DEF_OFF(SCB, PFR0, 0x40),              // PFR0: Processor Feature Register
   HW_DEF_BIT(SCB, PFR0, THUMBINST, 7, 4),   // 3: Support for Thumb encoding including Thumb-2 technology, with all basic 16-bit and 32-bit instructions. 0-2: ARMv7-M reserved.
   HW_DEF_BIT(SCB, PFR0, ARMINST, 3, 0),     // 0: The processor does not support the ARM instruction set. 1: ARMv7-M reserved.
   HW_DEF_OFF(SCB, PFR1, 0x44),              // PFR1: Processor Feature Register
   HW_DEF_BIT(SCB, PFR1, MODEL, 11, 8),      // 2: Two-stack programmers’ model supported. 0-1: Reserved.
   HW_DEF_OFF(SCB, DFR, 0x48),               // DFR: Debug Feature Register
   HW_DEF_BIT(SCB, DFR, DEBUGMODEL, 23, 20), // 1: Support for M profile Debug architecture, with memory-mapped access. 0: Not supported.
   HW_DEF_OFF(SCB, AFR, 0x4C),               // AFR: Auxiliary Feature Register
   HW_DEF_OFF(SCB, MMFR0, 0x50),             // MMFR0: Memory Model Feature Register 0
   HW_DEF_BIT(SCB, MMFR0, AUXREG, 23, 20),   // 1: Support for Auxiliary Control Register only. 0: Not supported.
   HW_DEF_BIT(SCB, MMFR0, SHARE, 15, 12),    // 0: One level of shareability implemented. 1: ARMv7-M Reserved.
   HW_DEF_BIT(SCB, MMFR0, OUTSHARE, 11, 8),  // 0: Outermost shareability domain implemented as Non-cacheable. 1: ARMv7-M reserved. 15: Shareability ignored.
   HW_DEF_BIT(SCB, MMFR0, PMSA, 7, 4),       // 0: Not supported. 1-2: ARMv7-M reserved. 3: Providing support for a base region and subregions, PMSAv7: Protected Memory System Architecture.
   HW_DEF_OFF(SCB, MMFR1, 0x54),             // MMFR1: Memory Model Feature Register 1 (Reserved)
   HW_DEF_OFF(SCB, MMFR2, 0x58),             // MMFR2: Memory Model Feature Register 2
   HW_DEF_BIT(SCB, MMFR2, WFISTALL, 27, 24), // 1: Support for WFI stalling. 0: Not supported.
   HW_DEF_OFF(SCB, MMFR3, 0x5C),             // MMFR3: Memory Model Feature Register 3 (Reserved)
   HW_DEF_OFF(SCB, ISAR0, 0x60),             // ISAR: Memory Model Feature Register 0
   HW_DEF_BIT(SCB, ISAR0, DIV, 27, 24),      // 1: Adds support for the SDIV and UDIV instructions. 0: None supported.
   HW_DEF_BIT(SCB, ISAR0, DEBUG, 23, 20),    // 1: Adds support for the BKPT instruction. 0: None supported.
   HW_DEF_BIT(SCB, ISAR0, COPROC, 19, 16),   // 0: None supported, except for example the FPU. 1: Supports generic CDP,LDC,MCR,MRC, and STC instructions. 2: As for 1, and adds support for generic CDP2,LDC2,MCR2,MRC2, and STC2 instructions. 3: As for 2, and adds support for generic MCRR and MRRC instructions. 4: As for 3, and adds support for generic MCRR2 and MRRC2 instructions.
   HW_DEF_BIT(SCB, ISAR0, CMPBRA, 15, 12),   // 1: Adds support for the CBNZ and CBZ instructions. 0: None supported.
   HW_DEF_BIT(SCB, ISAR0, BITFIELD, 11, 8),  // 1: Adds support for the BFC,BFI,SBFX, and UBFX instructions. 0: None supported.
   HW_DEF_BIT(SCB, ISAR0, BITCOUNT, 7, 4),   // 1: Adds support for the CLZ instruction. 0: None supported.
   HW_DEF_OFF(SCB, ISAR1, 0x64),             // ISAR: Memory Model Feature Register 1
   HW_DEF_BIT(SCB, ISAR1, INTERWORK, 27, 24),// 2: As for 1, and adds support for the BLX instruction, and PC loads have BX-like behavior. 1: Adds support for the BX instruction, and the T bit in the PSR. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR1, IMMEDIATE, 23, 20),// 1: Adds support for the ADDW,MOVW,MOVT, and SUBW instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR1, IFTHEN, 19, 16),   // 1: Adds support for the IT instructions, and for the IT bits in the PSRs. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR1, EXTEND, 15, 12),   // 2: As for 1, and adds support for the SXTAB,SXTAB16,SXTAH,SXTB16,UXTAB,UXTAB16,UXTAH, and UXTB16 instructions. 1: Adds support for the SXTB,SXTH,UXTB, and UXTH instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_OFF(SCB, ISAR2, 0x68),             // ISAR: Memory Model Feature Register 2
   HW_DEF_BIT(SCB, ISAR2, REVERSAL, 31, 28), // 1: Adds support for the REV,REV16, and REVSH instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR2, MULTU, 23, 20),    // 2: As for 1, and adds support for the UMAAL instruction. 1: Adds support for the UMULL and UMLAL instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR2, MULTS, 19, 16),    // 3: As for 2, and adds support for the SMLAD,SMLADX,SMLALD,SMLALDX,SMLSD,SMLSDX,SMLSLD,SMLSLDX,SMMLA,SMMLAR,SMMLS,SMMLSR,SMMUL,SMMULR,SMUAD,SMUADX,SMUSD, and SMUSDX instructions. 2: As for 1, and adds support for the SMLABB,SMLABT,SMLALBB,SMLALBT,SMLALTB,SMLALTT,SMLATB,SMLATT,SMLAWB,SMLAWT,SMULBB,SMULBT,SMULTB,SMULTT,SMULWB, and SMULWT instructions. Also adds support for the Q bit in the PSRs (ARMv7-M reserved). 1: Adds support for the SMULL and SMLAL instructions.
   HW_DEF_BIT(SCB, ISAR2, MULT,  15, 12),    // 2: As for 1, and adds support for the MLS instruction. 1: Adds support for the MLA instruction. 0: None supported. This means only MUL is supported.
   HW_DEF_BIT(SCB, ISAR2, LDMSTMINT, 11, 8), // 2: LDM and STM instructions are continuable. 1: LDM and STM instructions are restartable. 0: None supported. This means the LDM and STM instructions are not interruptible (ARMv7-M reserved).
   HW_DEF_BIT(SCB, ISAR2, MEMHINT, 7, 4),    // 3: As for 2, and adds support for the PLI instruction. 1-2: Adds support for the PLD instruction, ARMv7-M reserved. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR2, LDRSTR, 3, 0),     // 1: Adds support for the LDRD and STRD instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_OFF(SCB, ISAR3, 0x6C),             // ISAR: Memory Model Feature Register 3
   HW_DEF_BIT(SCB, ISAR3, TRUENOP, 27, 24),  // 1: Adds support for the true NOP instruction. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR3, THMBCOPY, 23, 20), // 1: Supports non flag-setting MOV instructions copying from a low register to a low register. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR3, TABBRANCH, 19, 16), // 1: Adds support for the TBB and TBH instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR3, SYNCHPRIM, 15, 12), // Must be interpreted with the ISAR4.SYNFRAC field to determine the supported Synchronization Primitives.
   HW_DEF_BIT(SCB, ISAR3, SVC, 11, 8),       // 1: Adds support for the SVC instruction. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR3, SIMD, 7, 4),       // 3: As for 1, and adds support for SIMD instr. PKHBT,PKHTB,QADD16,QADD8,QASX QSUB16,QSUB8,QSAX,SADD16,SADD8,SASX,SEL,SHADD16,SHADD8,SHASX,SHSUB16,SHSUB8,SHSAX,SSAT16,SSUB16,SSUB8,SSAX,SXTAB16,SXTB16,UADD16,UADD8,UASX,UHADD16,UHADD8,UHASX,UHSUB16,UHSUB8,UHSAX,UQADD16,UQADD8,UQASX,UQSUB16,UQSUB8,UQSAX,USAD8,USADA8,USAT16,USUB16,USUB8,USAX,UXTAB16, and UXTB16 instructions. Also adds support for the GE[3:0] bits in the PSRs. 2: Reserved. 1: Adds support for the SSAT and USAT instructions, and for the Q bit in the PSRs. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR3, SATURATE, 3, 0),   // 1: Adds support for Saturate instr. QADD,QDADD,QDSUB, and QSUB instructions, and for the Q bit in the PSRs. 0: None supported.
   HW_DEF_OFF(SCB, ISAR4, 0x70),             // ISAR: Memory Model Feature Register 4
   HW_DEF_BIT(SCB, ISAR4, PSR_M, 27, 24),    // 1: Adds support for the M-profile forms of the CPS,MRS and MSR instructions, to access the PSRs. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR4, SYNFRAC, 23, 20),  // [SYNCHPRIM,SYNFRAC], [1,3]: As for [1,0], and adds support for the CLREX,LDREXB,LDREXH,STREXB, and STREXH instructions. [1,0]: Adds support for the LDREX and STREX instructions. [0,0]: None supported.
   HW_DEF_BIT(SCB, ISAR4, BARRIER, 19, 16),  // 1: Adds support for the DMB,DSB, and ISB barrier instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR4, WRITEBACK, 11, 8), // 1: Adds support for all of the writeback addressing modes defined in the ARMv7-M architecture. 0: Basic support. Only the LDM,STM,PUSH, and POP instructions support writeback addressing modes. ARMv7-M reserved.
   HW_DEF_BIT(SCB, ISAR4, WITHSHIFT, 7, 4),  // 3: As for 1, and adds support for other constant shift options, on loads, stores, and other instructions. 2: Reserved. 1: Adds support for shifts of loads and stores over the range LSL 0-3. 0: Nonzero shifts supported only in MOV and shift instructions.
   HW_DEF_BIT(SCB, ISAR4, UNPRIV, 3, 0),     // 2: As for 1, and adds support for the LDRHT,LDRSBT,LDRSHT, and STRHT instructions. 1: Adds support for the LDRBT,LDRT,STRBT, and STRT instructions. 0: None supported, ARMv7-M reserved.
   HW_DEF_OFF(SCB, CPACR, 0x088),         // CPACR: Coprocessor Access Control Register
   HW_DEF_BIT(SCB, CPACR, CP11, 23, 22),  // 0b11: Full access. 0b10: Reserved. 0b01: Privileged access only. An unprivileged access generates a NOCP USAGEFAULT. 0b00: Access denied. Any attempted access generates a NOCP USAGEFAULT.
   HW_DEF_BIT(SCB, CPACR, CP10, 21, 20),  // See CPACR_CP11. Fields CP10 and CP11 together control access to the FPU (different values in bothe fields results in unpredictable behaviour).

   HW_DEF_OFF(MPU, TYPE, 0x00),           // TYPE: MPU Type Register
   HW_DEF_BIT(MPU, TYPE, IREGION, 23, 16),// (ro) Number of Instruction region. Reads as 0. ARMv7-M only supports a unified MPU.
   HW_DEF_BIT(MPU, TYPE, DREGION, 15, 8), // (ro) Number of regions supported by the MPU. 8: STM32F3 supports eight MPU regions. 0: Processor does not implement an MPU.
   HW_DEF_BIT(MPU, TYPE, SEPARATE, 0, 0), // (ro) 0: MPU uses unified instruction and data memory maps. 1: MPU separates instruction and data memory maps.
   HW_DEF_OFF(MPU, CTRL, 0x04),           // CTRL: MPU Control Register
   HW_DEF_BIT(MPU, CTRL, PRIVDEFENA,2,2), // 0: Disables the default memory map. Any instruction or data access that does not access a defined region faults. 1: Enables the default memory map as a background region for privileged access. The background region acts as region number -1.
   HW_DEF_BIT(MPU, CTRL, HFNMIENA, 1, 1), // 0: Disables MPU for memory access (privileged or unprivileged) with priority < 0. 1: Enables MPU for memory access even for execution priority less than 0 (NMI, FAULT, FAULTMASK set to 1(setfaultmask_interrupt)). Then any MPUFAULT would cause a lockup of the cpu. Note: When the MPU is disabled, if this bit is set to 1 the behavior is unpredictable.
   HW_DEF_BIT(MPU, CTRL, ENABLE, 0, 0),   // 0: The MPU is disabled. 1: The MPU is enabled. Other config values are only relevant if MPU is enabled.
   HW_DEF_OFF(MPU, RNR,  0x08),           // RNR: MPU Region Number Register
   HW_DEF_BIT(MPU, RNR,  REGION, 7, 0),   // 0-(TYPE.DREGION-1): Indicates the MPU memory region accessed by MPU_RBAR and MPU_RSAR. Other bits beyond REGION reads as zero and writes are ignored.
   HW_DEF_OFF(MPU, RBAR, 0x0C),           // RBAR: MPU Region Base Address Register
   HW_DEF_BIT(MPU, RBAR, ADDR, 31, 5),    // Base address of the region. It must be aligned to the size of the region. For example, a 64 KB region must be aligned on a multiple of 64 KB (0x00010000, 0x00020000, ...).
   HW_DEF_BIT(MPU, RBAR, VALID, 4, 4),    // reads 0: always reads as 0. writes 1: RBAR.REGION is valid and RNR is set to its value and ADDR updates the base address for the region specified in RBAR.REGION. 0: 0: Register RNR is not changed, and the processor updates the base address for the region specified in RNR and ignores the value of the REGION field.
   HW_DEF_BIT(MPU, RBAR, REGION, 3, 0),   // 0-min(15, TYPE.DREGION-1): MPU region field. Sets new value in MPU_RNR during write If VALID is set. Reads return bits [3:0] of the last value set in RNR.
   HW_DEF_OFF(MPU, RASR, 0x10),           // RASR: MPU Region Attribute and Size Register
   HW_DEF_BIT(MPU, RASR, XN, 28, 28),     // 0: Execution of an instruction fetched from this region permitted. 1: Execution of an instruction fetched from this region not permitted
   HW_DEF_BIT(MPU, RASR, AP, 26, 24),     // (P=privileged access,U=unprivileged access) 000:P-U- 001: PrwU- 010: PrwUro 011: PrwUrw 100: Reserved. 101: ProU- 110: ProUro 111: ProUro
   HW_DEF_BIT(MPU, RASR, TEX, 21, 19),    // Determines memory type together with RASR.C and RASR.B.
   HW_DEF_BIT(MPU, RASR, S, 18, 18),      // 1: Region is shareable, only for Normal memory regions. For Strongly-ordered and Device memory, the S bit is ignored. 0: Not shareable.
   HW_DEF_BIT(MPU, RASR, C, 17, 17),      // 0: Memory supports no caching or determines memory type. 1: Supports caching or determines memory type.
   HW_DEF_BIT(MPU, RASR, B, 16, 16),      // 0: Cache uses write-through. 1: Cache uses write-back with either "no write allocate" or "write and read allocate".
   HW_DEF_BIT(MPU, RASR, SRD, 15,  8),    // S=0..7 bit[S] 0: Subregion S enabled. Size is 1/8 of region size. Only regions with size 256 bytes or larger are supported. Address range of subregion is [RBAR.ADDR+S*(2**(SIZE-2))..RBAR.ADDR-1+(S+1)*(2**(SIZE-2))] 1: Subregion S disabled.
   HW_DEF_BIT(MPU, RASR, SIZE, 5,  1),    // 0-3: Reserved. 4-31: The region size in bytes is 2**(SIZE+1). The smallest supported size is 32 bytes.
   HW_DEF_BIT(MPU, RASR, ENABLE, 0, 0),   // 0: This region is disabled. 1: When the MPU is enabled, this region is enabled.
   HW_DEF_OFF(MPU, RBAR_A1, 0x14),        // RBAR_A1: MPU Alias 1 Region Base Address Register
   HW_DEF_OFF(MPU, RASR_A1, 0x18),        // RASR_A1: MPU Alias 1 Region Attribute and Size Register
   HW_DEF_OFF(MPU, RBAR_A2, 0x1C),        // RBAR_A2: MPU Alias 2 Region Base Address Register
   HW_DEF_OFF(MPU, RASR_A2, 0x20),        // RASR_A2: MPU Alias 2 Region Attribute and Size Register
   HW_DEF_OFF(MPU, RBAR_A3, 0x24),        // RBAR_A3: MPU Alias 3 Region Base Address Register
   HW_DEF_OFF(MPU, RASR_A3, 0x28),        // RASR_A3: MPU Alias 3 Region Attribute and Size Register

   // TODO: DEBUG

   HW_DEF_OFF(SCS, STIR, 0xF00),          // STIR: Software Trigger Interrupt Register
   HW_DEF_BIT(SCS, STIR, INTID, 8, 0),    // writes I: Triggers external interrupt with exception number (I+16), core interrupts (0..15) could not be triggered with this register. Bit USERSETMPEND of register CCR determines if unprivileged access is possible.

   // TODO: FPU
};



// section: inline implementation

static inline void assert_offset_core(void)
{
   static_assert( offsetof(core_sys_t, scs)        == HW_REGISTER_BASEADDR_SCS - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, scs.ictr)   == offsetof(core_sys_t, scs) + HW_OFF(SCS, ICTR));
   static_assert( offsetof(core_sys_t, scs.actlr)  == offsetof(core_sys_t, scs) + HW_OFF(SCS, ACTLR));
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM     == 15);
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM_POS == 0);
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM_MAX == 15);
   static_assert( offsetof(core_sys_t, systick)       == HW_REGISTER_BASEADDR_SYSTICK - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, systick.csr)   == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, CSR));
   static_assert( offsetof(core_sys_t, systick.rvr)   == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, RVR));
   static_assert( offsetof(core_sys_t, systick.cvr)   == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, CVR));
   static_assert( offsetof(core_sys_t, systick.calib) == HW_REGISTER_BASEADDR_SYSTICK - HW_REGISTER_BASEADDR_SYSTEM + HW_OFF(SYSTICK, CALIB));
   static_assert( HW_BIT(SYSTICK, CSR, CLKSOURCE) == 1<<2);
   static_assert( HW_BIT(SYSTICK, CSR, ENABLE)    == 1<<0);
   static_assert( HW_BIT(SYSTICK, RVR, RELOAD)    == 0x00ffffff);
   static_assert( HW_BIT(SYSTICK, CVR, CURRENT)   == 0xffffffff);
   static_assert( offsetof(core_sys_t, nvic)       == HW_REGISTER_BASEADDR_NVIC - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, nvic.iser)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ISER));
   static_assert( offsetof(core_sys_t, nvic.icer)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ICER));
   static_assert( offsetof(core_sys_t, nvic.ispr)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ISPR));
   static_assert( offsetof(core_sys_t, nvic.icpr)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ICPR));
   static_assert( offsetof(core_sys_t, nvic.iabr)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, IABR));
   static_assert( offsetof(core_sys_t, nvic.ipr)   == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, IPR));
   static_assert( offsetof(core_sys_t, scb)        == HW_REGISTER_BASEADDR_SCB - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, scb.cpuid)  == offsetof(core_sys_t, scb) + HW_OFF(SCB, CPUID));
   static_assert( offsetof(core_sys_t, scb.icsr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, ICSR));
   static_assert( offsetof(core_sys_t, scb.vtor)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, VTOR));
   static_assert( offsetof(core_sys_t, scb.aircr)  == offsetof(core_sys_t, scb) + HW_OFF(SCB, AIRCR));
   static_assert( offsetof(core_sys_t, scb.scr)    == offsetof(core_sys_t, scb) + HW_OFF(SCB, SCR));
   static_assert( offsetof(core_sys_t, scb.ccr)    == offsetof(core_sys_t, scb) + HW_OFF(SCB, CCR));
   static_assert( offsetof(core_sys_t, scb.shpr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, SHPR));
   static_assert( offsetof(core_sys_t, scb.shcsr)  == offsetof(core_sys_t, scb) + HW_OFF(SCB, SHCSR));
   static_assert( offsetof(core_sys_t, scb.cfsr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, CFSR));
   static_assert( offsetof(core_sys_t, scb.hfsr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, HFSR));
   static_assert( offsetof(core_sys_t, scb.mmfar)  == offsetof(core_sys_t, scb) + HW_OFF(SCB, MMFAR));
   static_assert( offsetof(core_sys_t, scb.bfar)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, BFAR));
   static_assert( offsetof(core_sys_t, scb.afsr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, AFSR));
   static_assert( offsetof(core_sys_t, scb.pfr)    == offsetof(core_sys_t, scb) + HW_OFF(SCB, PFR0));
   static_assert( offsetof(core_sys_t, scb.pfr)+4  == offsetof(core_sys_t, scb) + HW_OFF(SCB, PFR1));
   static_assert( offsetof(core_sys_t, scb.dfr)    == offsetof(core_sys_t, scb) + HW_OFF(SCB, DFR));
   static_assert( offsetof(core_sys_t, scb.afr)    == offsetof(core_sys_t, scb) + HW_OFF(SCB, AFR));
   static_assert( offsetof(core_sys_t, scb.mmfr)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, MMFR0));
   static_assert( offsetof(core_sys_t, scb.mmfr)+4 == offsetof(core_sys_t, scb) + HW_OFF(SCB, MMFR1));
   static_assert( offsetof(core_sys_t, scb.mmfr)+8 == offsetof(core_sys_t, scb) + HW_OFF(SCB, MMFR2));
   static_assert( offsetof(core_sys_t, scb.mmfr)+12== offsetof(core_sys_t, scb) + HW_OFF(SCB, MMFR3));
   static_assert( offsetof(core_sys_t, scb.isar)   == offsetof(core_sys_t, scb) + HW_OFF(SCB, ISAR0));
   static_assert( offsetof(core_sys_t, scb.isar)+4 == offsetof(core_sys_t, scb) + HW_OFF(SCB, ISAR1));
   static_assert( offsetof(core_sys_t, scb.isar)+8 == offsetof(core_sys_t, scb) + HW_OFF(SCB, ISAR2));
   static_assert( offsetof(core_sys_t, scb.isar)+12== offsetof(core_sys_t, scb) + HW_OFF(SCB, ISAR3));
   static_assert( offsetof(core_sys_t, scb.isar)+16== offsetof(core_sys_t, scb) + HW_OFF(SCB, ISAR4));
   static_assert( offsetof(core_sys_t, scb.cpacr)  == offsetof(core_sys_t, scb) + HW_OFF(SCB, CPACR));
   static_assert( HW_BIT(SCB, SHPR, PRI)           == 0xf0);
   static_assert( HW_BIT(SCB, SHPR, PRI_MAX)       == 0xf);
   static_assert( HW_BIT(SCB, SHPR, PRI_POS)       == 0x4);
   static_assert( HW_BIT(SCB, SHCSR, USGFAULTENA)  == 1 << 18);
   static_assert( HW_BIT(SCB, SHCSR, MEMFAULTACT)  == 1 << 0);
   static_assert( offsetof(core_sys_t, mpu)        == HW_REGISTER_BASEADDR_MPU - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, mpu.type)   == offsetof(core_sys_t, mpu) + HW_OFF(MPU, TYPE));
   static_assert( offsetof(core_sys_t, mpu.ctrl)   == offsetof(core_sys_t, mpu) + HW_OFF(MPU, CTRL));
   static_assert( offsetof(core_sys_t, mpu.rnr)    == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RNR));
   static_assert( offsetof(core_sys_t, mpu.rbar)   == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RBAR));
   static_assert( offsetof(core_sys_t, mpu.rasr)   == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RASR));
   static_assert( offsetof(core_sys_t, mpu.rbar_a1) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RBAR_A1));
   static_assert( offsetof(core_sys_t, mpu.rasr_a1) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RASR_A1));
   static_assert( offsetof(core_sys_t, mpu.rbar_a2) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RBAR_A2));
   static_assert( offsetof(core_sys_t, mpu.rasr_a2) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RASR_A2));
   static_assert( offsetof(core_sys_t, mpu.rbar_a3) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RBAR_A3));
   static_assert( offsetof(core_sys_t, mpu.rasr_a3) == offsetof(core_sys_t, mpu) + HW_OFF(MPU, RASR_A3));
   static_assert( offsetof(core_sys_t, debug)      == HW_REGISTER_BASEADDR_COREDEBUG - HW_REGISTER_BASEADDR_SYSTEM);
   // TODO: debug
   static_assert( offsetof(core_sys_t, stir)       == offsetof(core_sys_t, scs) + HW_OFF(SCS, STIR));
   static_assert( HW_BIT(SCS, STIR, INTID)         == 0x1ff);
   static_assert( offsetof(core_sys_t, fpu)        == HW_REGISTER_BASEADDR_FPU - HW_REGISTER_BASEADDR_SYSTEM);
   // TODO: fpu
}

/* Versetzt den Prozessor in den Schlafmodus.
 * Daraust erwacht er mit dem nächsten Interrupt.
 * Die Effekte von PRIMASK werden ignoriert, d.h. ein wartender Interrupt, der ausgeführt würde,
 * wäre nur PRIMASK nicht gesetzt (PRIMASK wird gesetzt mit setprio0mask_interrupt), lässt WFI
 * zurückkehren. So kann es vermieden werden, dass ein Interrupt kurz vor dem Aufruf von WFI auftritt,
 * der dann die CPU aus WFI nicht mehr aufwecken kann. */
static inline void waitinterrupt_core(void)
{
   __asm volatile( "WFI" );
}

/* function: waitevent_core
 * Versetzt den Prozessor in den Schlafmodus, falls das CPU-interne Event nicht gesetzt ist.
 * Daraust erwacht er mit dem nächsten Interrupt, dem nächsten Event (auch Reset oder Debug).
 * Ist das CPU-interne Eventflag gesetzt, wird es gelöscht und der Befehl kehrt sofort zurück. */
static inline void waitevent_core(void)
{
   __asm volatile( "WFE" );
}

/* function: setevent_core
 * Setzt das interne Eventflag und sendet es an Eventout der CPU (falls Multicore-CPU). */
static inline void setevent_core(void)
{
   __asm volatile( "SEV" );
}

static inline void reset_core(void)
{
   __asm volatile( "DSB" );
   hSCB->aircr = (0x05FA << HW_BIT(SCB, AIRCR, VECTKEY_POS)) | HW_BIT(SCB, AIRCR, SYSRESETREQ);
   while (1) ;
}


/* == FPU == */

// TODO: extract into module fpu.h

static inline void enable_fpu(int allowUnprivilegedAccess)
{
   static_assert(HW_BIT(SCB, CPACR, CP10_MAX) == 3);
   static_assert(HW_BIT(SCB, CPACR, CP11_MAX) == 3);
   static_assert( HW_BIT(SCB, CPACR, CP11_POS) == HW_BIT(SCB, CPACR, CP10_POS) + 2);
   hSCB->cpacr |= ((allowUnprivilegedAccess ? 0x0F : 0x05) << HW_BIT(SCB, CPACR, CP10_POS));
}

static inline void disable_fpu(void)
{
   hSCB->cpacr &= ~ (0x0F << HW_BIT(SCB, CPACR, CP10_POS));
}


#endif
