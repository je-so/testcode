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

// == exported Peripherals/HW-Units
#define hCORE     ((core_sys_t*) HW_REGISTER_BASEADDR_SYSTEM)
#define hSCB      ((core_scb_t*) HW_REGISTER_BASEADDR_SCB)

// == exported types
struct core_sys_t;

// == exported functions
static inline void waitevent_core(void);
static inline void sendevent_core(void);
static inline void enable_fpu(int allowUnprivilegedAccess); // TODO: own module
static inline void disable_fpu(void); // TODO: own module

// == exported macros

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
#define HW_DEF_BIT(BASE, REG, MSBit, LSBit, NAME) \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS = LSBit, \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_MAX = ((((1u << ((MSBit) - HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS))-1u)<<1u)+1u), \
         HW_REGISTER_BIT_##BASE##_##REG##_##NAME       = HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_MAX << HW_REGISTER_BIT_##BASE##_##REG##_##NAME##_POS

/* Cortex-M4 System address range 0xE0000000 to 0xFFFFFFFF:
 * Internal Private Peripheral Bus (PPB): 0xE0000000 .. 0xE003FFFF
 * External Private Peripheral Bus (PPB): 0xE0040000 .. 0xE00FFFFF
 * Vendor Specific System region   (AHB): 0xE0100000 .. 0xFFFFFFFF */
#define HW_REGISTER_BASEADDR_SYSTEM    0xE0000000
/* Cortex-M4 Data Watchpoint and Trace (address range 0xE0001000 to 0xE0001FFF) */
#define HW_REGISTER_BASEADDR_DWT       0xE0001000
/* Cortex-M4 System Control Space (address range 0xE000E000 to 0xE000EFFF) contains core peripherals */
#define HW_REGISTER_BASEADDR_SCS       0xE000E000
/* Cortex-M4 Systick timer base address */
#define HW_REGISTER_BASEADDR_SYSTICK   0xE000E010
/* Cortex-M4 Nested Vectored Interrupt Controller */
#define HW_REGISTER_BASEADDR_NVIC      0xE000E100
/* Cortex-M4 System Bontrol Block */
#define HW_REGISTER_BASEADDR_SCB       0xE000ED00
/* Cortex-M4 Memory Protection Unit */
#define HW_REGISTER_BASEADDR_MPU       0xE000ED90
/* Cortex-M4 Core Debug Base Address */
#define HW_REGISTER_BASEADDR_COREDEBUG 0xE000EDF0
/* Cortex-M4 Floating Point Unit */
#define HW_REGISTER_BASEADDR_FPU       0xE000EF30

// == definitions

typedef volatile struct core_scs_t {
   uint32_t       _reserved0[1];
   uint32_t const ictr;   /* Interrupt Controller Type Register, ro, Offset: 0x04, Reset: IMP.DEF.
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
   uint32_t const calib;  /* SysTick Calibration Register, ro, Offset: 0x0C, Reset: IMP.DEF.
                           * Reads the calibration value and parameters for SysTick.
                           */
} core_systick_t;

typedef volatile struct core_nvic_t {
   uint32_t       iser[8];      /* Interrupt Set Enable Register, rw, Offset: 0x000, Reset: 0x00000000
                                 * Enables, or reads the enable state of a group of interrupts.
                                 * reg[n] bit[m] SETENA reads: 1: Interrupt (32*n+m) enabled. 0: Disabled. writes: 1: Enable interrupt. 0: No effect.
                                 */
   uint32_t       _reserved0[24];
   uint32_t       icer[8];      /* Interrupt Clear Enable Register, rw, Offset: 0x080, Reset: 0x00000000
                                 * Disables, or reads the enable state of a group of interrupts.
                                 * reg[n] bit[m] CLRENA reads: 1: Interrupt (32*n+m) enabled. 0: Disabled. writes: 1: Disable interrupt. 0: No effect.
                                 */
   uint32_t       _reserved1[24];
   uint32_t       ispr[8];      /* Interrupt Set Pending Register, rw, Offset: 0x100, Reset: 0x00000000
                                 * Changes interrupt status to pending, or shows the current pending status of a group of interrupts.
                                 * reg[n] bit[m] SETPEND reads: 1: Interrupt (32*n+m) is pending. 0: Not pending. writes: 1: Changes interrupt state to pending. 0: No effect.
                                 */
   uint32_t       _reserved2[24];
   uint32_t       icpr[8];      /* Interrupt Clear Pending Register, rw, Offset: 0x180, Reset: 0x00000000
                                 * Clears interrupt pending status, or shows the current pending status of a group of interrupts.
                                 * reg[n] bit[m] CLRPEND reads: 1: Interrupt (32*n+m) is pending. 0: Not pending. writes: 1: Clears interrupt state to not pending. 0: No effect.
                                 */
   uint32_t       _reserved3[24];
   uint32_t const iabr[8];      /* Interrupt Active Bit Register, ro, Offset: 0x200, Reset: 0x00000000
                                 * Shows whether interrupt is active (ISR is executed) of a group of interrupts.
                                 * reg[n] bit[m] ACTIVE reads: 1: Interrupt (32*n+m) is active. 0: Not active.
                                 */
   uint32_t const _reserved4[56];
   uint8_t        ipr[240];     /* Interrupt Priority Register, rw, Offset: 0x300, Reset: 0x00000000
                                 * Sets or reads interrupt priorities of a group of interrupts.
                                 * reg[n] bit[7..8-HW_KONFIG_NVIC_INTERRUPT_PRIORITY_NROFBITS] PRI rw: Priority of interrupt n.
                                 * reg[n] bit[other] Read as 0, write ignored.
                                 */
} core_nvic_t;

typedef volatile struct core_scb_t {
   uint32_t const cpuid;        /* CPUID Base Register, ro, Offset: 0x000, Reset: IMP.DEF.
                                 * The CPUID scheme provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t       icsr;         /* Interrupt Control and State Register, rw, Offset: 0x004, Reset:
                                 * Provides pending state for NMI, PendSV and SysTick, nr of highest priority pending exception, whether there are preempted active exceptions.
                                 */
   uint32_t       vtor;         /* Vector Table Offset Register, rw, Offset: 0x008, Reset: 0x00000000
                                 * Holds the vector table address aligned to 512 bytes.
                                 */
   uint32_t       aircr;        /* Application Interrupt and Reset Control Register, rw, Offset: 0x00C, Reset:
                                 *
                                 */
   uint32_t       scr;          /* System Control Register, rw, Offset: 0x010, Reset: 0x00000000
                                 *
                                 */
   uint32_t       ccr;          /* Configuration Control Register, rw, Offset: 0x014, Reset:
                                 *
                                 */
   uint8_t        shpr[12];     /* System Handlers Priority Registers (4-7, 8-11, 12-15), rw, Offset: 0x018, Reset:
                                 *
                                 */
   uint32_t       shcsr;        /* System Handler Control and State Register, rw, Offset: 0x024, Reset:
                                 *
                                 */
   uint32_t       cfsr;         /* Configurable Fault Status Register, rw, Offset: 0x028, Reset:
                                 *
                                 */
   uint32_t       hfsr;         /* HardFault Status Register, rw, Offset: 0x02C, Reset:
                                 *
                                 */
   uint32_t       dfsr;         /* Debug Fault Status Register, rw, Offset: 0x030, Reset:
                                 *
                                 */
   uint32_t       mmfar;        /* MemManage Fault Address Register, rw, Offset: 0x034, Reset:
                                 */
   uint32_t       bfar;         /* BusFault Address Register, rw, Offset: 0x038, Reset:
                                 */
   uint32_t       afsr;         /* Auxiliary Fault Status Register, rw, Offset: 0x03C, Reset:
                                 */
   uint32_t const pfr[2];       /* Processor Feature Register, rw, Offset: 0x040, Reset: IMP.DEF.
                                 * Provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t const dfr;          /* Debug Feature Register, rw, Offset: 0x048, Reset: IMP.DEF.
                                 * Provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t const afr;          /* Auxiliary Feature Register, ro, Offset: 0x04C, Reset: IMP.DEF.
                                 * Provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t const mmfr[4];      /* Memory Model Feature Register, ro, Offset: 0x050, Reset: IMP.DEF.
                                 * Provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t const isar[5];      /* Instruction Set Attributes Register, ro, Offset: 0x060, Reset: IMP.DEF.
                                 * Provides a description of the features of an ARM processor implementation.
                                 */
   uint32_t const _reserved0[5];
   uint32_t       cpacr;        /* Coprocessor Access Control Register, rw, Offset: 0x088, Reset:
                                 * Specifies access privileges for coprocessors (FPU only).
                                 */
} core_scb_t;

typedef volatile struct core_mpu_t {
                  // Offset: 0x00, ro, MPU Type Register
   uint32_t const type;
                  // Offset: 0x04, rw, MPU Control Register
   uint32_t       ctrl;
                  // Offset: 0x08, rw, MPU Region RNRber Register
   uint32_t       rnr;
                  // Offset: 0x0C, rw, MPU Region Base Address Register
   uint32_t       rbar;
                  // Offset: 0x10, rw, MPU Region Attribute and Size Register
   uint32_t       rasr;
                  // Offset: 0x14, rw, MPU Alias 1 Region Base Address Register
   uint32_t       rbar_a1;
                  // Offset: 0x18, rw, MPU Alias 1 Region Attribute and Size Register
   uint32_t       rasr_a1;
                  // Offset: 0x1C, rw, MPU Alias 2 Region Base Address Register
   uint32_t       rbar_a2;
                  // Offset: 0x20, rw, MPU Alias 2 Region Attribute and Size Register
   uint32_t       rasr_a2;
                  // Offset: 0x24, rw, MPU Alias 3 Region Base Address Register
   uint32_t       rbar_a3;
                  // Offset: 0x28, rw, MPU Alias 3 Region Attribute and Size Register
   uint32_t       rasr_a3;
} core_mpu_t;

typedef volatile struct core_debug_t {
                  // Offset: 0x00, rw, Debug Halting Control and Status Register
   uint32_t       dhcsr;
                  // Offset: 0x04, wo, Debug Core Register Selector Register
   uint32_t       dcrsr;
                  // Offset: 0x08, rw, Debug Core Register Data Register
   uint32_t       dcrdr;
                  // Offset: 0x0C, rw, Debug Exception and Monitor Control Register
   uint32_t       demcr;
} core_debug_t;

typedef volatile struct core_fpu_t {
   uint32_t       _reserved0[1];
                  // Offset: 0x04, rw, Floating-Point Context Control Register
   uint32_t       fpccr;
                  // Offset: 0x08, rw, Floating-Point Context Address Register
   uint32_t       fpcar;
                  // Offset: 0x0C, rw, Floating-Point Default Status Control Register
   uint32_t       fpdscr;
                  // Offset: 0x10, ro, Media and FP Feature Register 0
   uint32_t const mvfr0;
                  // Offset: 0x14, ro, Media and FP Feature Register 1
   uint32_t const mvfr1;
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


// == Register Offsets and Bit-Fields

enum core_register_e {
   /* ICTR: Interrupt Controller Type Register */
   HW_DEF_OFF(SCS, ICTR, 0x004),
   /* ICTR_INTLINESNUM[3:0]
    * The total number of interrupt lines supported by an implementation, defined in groups of 32.
    * That is, the total number of interrupt lines is up to (INTLINESNUM+1)*32. */
   HW_DEF_BIT(SCS, ICTR, 3, 0, INTLINESNUM),

   /* ACTLR: Auxiliary control register */
   HW_DEF_OFF(SCS, ACTLR, 0x008),
   /* ACTLR_DISOOFP[9]
    * Disables floating point instructions completing out of order with respect to integer instructions. */
   HW_DEF_BIT(SCS, ACTLR, 9, 9, DISOOFP),
   /* ACTLR_DISFPCA[8]
    * 1: Disables automatic update of CONTROL.FPCA.
    * The value of this bit should be written as zero or preserved (SBZP). */
   HW_DEF_BIT(SCS, ACTLR, 8, 8, DISFPCA),
   /* ACTLR_DISFOLD[2]
    * 1: Disables folding of IT instructions: IT folding can cause jitter in looping.
    * If a task must avoid jitter, set the DISFOLD bit to 1 before executing the task, to disable IT folding. */
   HW_DEF_BIT(SCS, ACTLR, 2, 2, DISFOLD),
   /* ACTLR_DISDEFWBUF[1]
    * 1: Disables write buffer use during default memory map accesses: This causes all BusFaults to
    * be precise BusFaults but decreases performance because any store to memory must
    * complete before the processor can execute the next instruction.
    * 0: Enable write buffer use. */
   HW_DEF_BIT(SCS, ACTLR, 1, 1, DISDEFWBUF),
   /* ACTLR_DISMCYCINT[0]
    * 1: Disables interruption of load multiple and store multiple instructions (LDM, STM).
    * This increases the interrupt latency of the processor because any LDM or STM must complete
    * before the processor can stack the current state and enter the interrupt handler. */
   HW_DEF_BIT(SCS, ACTLR, 0, 0, DISMCYCINT),

   /* CSR: SysTick Control and Status Register, Reset: 0x00000000
    * Controls the system timer and provides status data. */
   HW_DEF_OFF(SYSTICK, CSR, 0x00),
   /* CSR_COUNFLAG[16]
    * ro, 1: Timer transition occurred from 1 to 0. 0: Timer has not counted to 0.
    * COUNTFLAG is cleared to 0 by a read of this register, and by any write to SYSTICK_CVR. */
   HW_DEF_BIT(SYSTICK, CSR, 16, 16, COUNTFLAG),
   /* CSR_CLKSOURCE[2]
    * rw, 1: SysTick uses the processor clock. 0: SysTick uses IMP.DEF. external reference clock. */
   HW_DEF_BIT(SYSTICK, CSR,  2,  2, CLKSOURCE),
   /* CSR_TICKINT[1]
    * rw, 1: Count to 0 changes the SysTick exception status to pending. 0: Count to 0 does not affect the SysTick exception status. */
   HW_DEF_BIT(SYSTICK, CSR,  1,  1, TICKINT),
   /* CSR_ENABLE[1]
    * rw, 1: Counter is operating. 0: Counter is disabled. */
   HW_DEF_BIT(SYSTICK, CSR,  0,  0, ENABLE),

   /* RVR: SysTick Reload Value Register */
   HW_DEF_OFF(SYSTICK, RVR, 0x04),
   /* RVR_RELOAD[23:0]
    * The value to load into the SYSTICK_CVR when the counter reaches 0. */
   HW_DEF_BIT(SYSTICK, RVR, 23,  0, RELOAD),

   /* CVR: SysTick Current Value Register */
   HW_DEF_OFF(SYSTICK, CVR, 0x08),
   /* RVR_CURRENT[31:0]
    * This is the value of the counter at the time it is read. Any write clears the register to zero. */
   HW_DEF_BIT(SYSTICK, CVR, 31,  0, CURRENT),

   /* CALIB: SysTick Calibration value Register */
   HW_DEF_OFF(SYSTICK, CALIB, 0x0C),
   /* CALIB_NOREF[31]
    * 0: The IMP.DEF. reference clock is implemented. 1: It is not implemented. */
   HW_DEF_BIT(SYSTICK, CALIB, 31, 31, NOREF),
   /* CALIB_SKEW[30]
    * 1: Calibration value is inexact, because of the clock frequency. 0: Calibration value is exact. */
   HW_DEF_BIT(SYSTICK, CALIB, 30, 30, SKEW),
   /* CALIB_TENMS[23:0]
    * Optionally, holds a reload value to be used for 10ms (100Hz) timing, subject to system clock
    * skew errors. If this field is zero, the calibration value is not known.
    * STM32F3 holds value for 1ms period if systick HCLK is programmed with max value and systick uses HCLK/8 as external clock. */
   HW_DEF_BIT(SYSTICK, CALIB, 23,  0, TENMS),

   /* ISER: Interrupt Set-Enable Registers */
   HW_DEF_OFF(NVIC, ISER, 0x000),
   /* ICER: Interrupt Clear-Enable Registers */
   HW_DEF_OFF(NVIC, ICER, 0x080),
   /* ISPR: Interrupt Set-Pending Registers */
   HW_DEF_OFF(NVIC, ISPR, 0x100),
   /* ICPR: Interrupt Clear-Pending Registers */
   HW_DEF_OFF(NVIC, ICPR, 0x180),
   /* IABR: Interrupt Active Bit Registers */
   HW_DEF_OFF(NVIC, IABR, 0x200),
   /* IPR: Interrupt Priority Registers */
   HW_DEF_OFF(NVIC, IPR,  0x300),

   /* CPUID: CPUID Base Register */
   HW_DEF_OFF(SCB, CPUID, 0x00),
   /* CPUID_IMPLEMENTER[31:24]
    * Implementer code assigned by ARM. Reads as 0x41 for a processor implemented by ARM. */
   HW_DEF_BIT(SCB, CPUID, 31, 24, IMPLEMENTER),
   /* CPUID_VARIANT[23:20]
    * Variant number. */
   HW_DEF_BIT(SCB, CPUID, 23, 20, VARIANT),
   /* CPUID_ARCHITECTURE[19:16]
    * Reads as 0xF indicating use of the CPUID scheme. */
   HW_DEF_BIT(SCB, CPUID, 19, 16, ARCHITECTURE),
   /* CPUID_PARTNO[15:4]
    * Part number. */
   HW_DEF_BIT(SCB, CPUID, 15,  4, PARTNO),
   /* CPUID_REVISION[3:0]
    * Revision number. */
   HW_DEF_BIT(SCB, CPUID, 3,  0, REVISION),

   /* ICSR: Interrupt Control and State Register */
   HW_DEF_OFF(SCB, ICSR,  0x04),
   /* ICSR_NMIPENDSET[31]
    * reads 1: NMI is active. 0: NMI is inactive. writes 1: Make NMI exception active. 0: No effect. */
   HW_DEF_BIT(SCB, ICSR, 31, 31, NMIPENDSET),
   /* ICSR_PENDSVSET[28]
    * reads 1: PendSV is pending. 0: Not pending. writes 1: Make PendSV exception pending. 0: No effect. */
   HW_DEF_BIT(SCB, ICSR, 28, 28, PENDSVSET),
   /* ICSR_PENDSVCLR[27], wo
    * writes 1: Remove PendSV pending status. 0: No effect. */
   HW_DEF_BIT(SCB, ICSR, 27, 27, PENDSVCLR),
   /* ICSR_PENDSTSET[26]
    * reads 1: SysTick is pending. 0: Not pending. writes 1: Make SysTick exception pending. 0: No effect. */
   HW_DEF_BIT(SCB, ICSR, 26, 26, PENDSTSET),
   /* ICSR_PENDSTCLR[25], wo
    * writes 1: Remove SysTick pending status. 0: No effect. */
   HW_DEF_BIT(SCB, ICSR, 25, 25, PENDSTCLR),
   /* ICSR_ISRPREEMPT[23], ro
    * reads 1: Pending exception will be serviced on exit from debug halt state. 0: Will not service. */
   HW_DEF_BIT(SCB, ICSR, 23, 23, ISRPREEMPT),
   /* ICSR_ISRPENDING[22], ro
    * reads 1: External interrupt, generated by the NVIC, is pending (nr >= 16). 0: No external interrupt pending. */
   HW_DEF_BIT(SCB, ICSR, 22, 22, ISRPENDING),
   /* ICSR_VECTPENDING[20:12], ro
    * reads >0: Number of the pending and *enabled* exception with highest priority. 0: No pending exception. */
   HW_DEF_BIT(SCB, ICSR, 20, 12, VECTPENDING),
   /* ICSR_RETTOBASE[11], ro
    * (Only valid in "Handler mode") reads 0: There is an active exception other than the exception shown by IPSR. 1: There is no active exception other than the executing one. */
   HW_DEF_BIT(SCB, ICSR, 11, 11, RETTOBASE),
   /* ICSR_VECTACTIVE[8:0], ro
    * reads 0: Processor is in "Thread mode". !=0: Exception number of current executing exception ("Handler mode"), same number as in processor register IPSR. */
   HW_DEF_BIT(SCB, ICSR,  8,  0, VECTACTIVE),

   /* VTOR: Vector Table Offset Register */
   HW_DEF_OFF(SCB, VTOR,  0x08),
   /* VTOR_TBLOFF[29:9]
    * Bits [29:9] of the vector table base address, other bits are always 0, therefore aligned to 512 bytes within the first GB. */
   HW_DEF_BIT(SCB, VTOR, 29,  9, TBLOFF),
   /* AIRCR: Application Interrupt and Reset Control Register */
   HW_DEF_OFF(SCB, AIRCR, 0x0C),
   /* SCR: System Control Register */
   HW_DEF_OFF(SCB, SCR,   0x10),
   /* CCR: System Control Register */
   HW_DEF_OFF(SCB, CCR,   0x14),
   /* SHPR: System Control Register */
   HW_DEF_OFF(SCB, SHPR,  0x18),
   /* SHCSR: System Handler Control and State Register */
   HW_DEF_OFF(SCB, SHCSR, 0x24),
   /* CFSR: Configurable Fault Status Register */
   HW_DEF_OFF(SCB, CFSR, 0x28),
   /* HFSR: HardFault Status Register */
   HW_DEF_OFF(SCB, HFSR, 0x2C),
   /* DFSR: Debug Fault Status Register */
   HW_DEF_OFF(SCB, DFSR, 0x30),
   /* MMFAR: MemManage Fault Address Register */
   HW_DEF_OFF(SCB, MMFAR, 0x34),
   /* BFAR: BusFault Address Register */
   HW_DEF_OFF(SCB, BFAR, 0x38),
   /* AFSR: Auxiliary Fault Status Register */
   HW_DEF_OFF(SCB, AFSR, 0x3C),
   /* PFR: Processor Feature Register */
   HW_DEF_OFF(SCB, PFR, 0x40),
   /* DFR: Debug Feature Register */
   HW_DEF_OFF(SCB, DFR, 0x48),
   /* AFR: Auxiliary Feature Register */
   HW_DEF_OFF(SCB, AFR, 0x4C),
   /* MMFR: Memory Model Feature Register */
   HW_DEF_OFF(SCB, MMFR, 0x50),
   /* ISAR: Memory Model Feature Register */
   HW_DEF_OFF(SCB, ISAR, 0x60),
   /* CPACR: Coprocessor Access Control Register */
   HW_DEF_OFF(SCB, CPACR, 0x088),
   // TODO: CPACR BITS

   // TODO: MPU

   // TODO: DEBUG

   /* STIR: Software Trigger Interrupt Register */
   HW_DEF_OFF(SCS, STIR, 0xF00),
   /* STIR_INTID[8:0], wo
    * writes I: Triggers external interrupt with exception number (I+16), core interrupts (0..15) could not be triggered with this register. */
   HW_DEF_BIT(SCS, STIR, 8, 0, INTID),

   // TODO: FPU
};



// section: inline implementation

static inline void assert_offset_core(void)
{
   static_assert( offsetof(core_sys_t, scs)       == HW_REGISTER_BASEADDR_SCS - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, scs.ictr)  == offsetof(core_sys_t, scs) + HW_OFF(SCS, ICTR));
   static_assert( offsetof(core_sys_t, scs.actlr) == offsetof(core_sys_t, scs) + HW_OFF(SCS, ACTLR));
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM == 15);
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM_POS == 0);
   static_assert( HW_REGISTER_BIT_SCS_ICTR_INTLINESNUM_MAX == 15);
   static_assert( offsetof(core_sys_t, systick)     == HW_REGISTER_BASEADDR_SYSTICK - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, systick.csr) == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, CSR));
   static_assert( offsetof(core_sys_t, systick.rvr) == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, RVR));
   static_assert( offsetof(core_sys_t, systick.cvr) == offsetof(core_sys_t, systick) + HW_OFF(SYSTICK, CVR));
   static_assert( offsetof(core_sys_t, systick.calib) == HW_REGISTER_BASEADDR_SYSTICK - HW_REGISTER_BASEADDR_SYSTEM + HW_OFF(SYSTICK, CALIB));
   static_assert( HW_BIT(SYSTICK, CSR, CLKSOURCE) == 1<<2);
   static_assert( HW_BIT(SYSTICK, CSR, ENABLE)  == 1<<0);
   static_assert( HW_BIT(SYSTICK, RVR, RELOAD)  == 0x00ffffff);
   static_assert( HW_BIT(SYSTICK, CVR, CURRENT) == 0xffffffff);
   static_assert( offsetof(core_sys_t, nvic)      == HW_REGISTER_BASEADDR_NVIC - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, nvic.iser) == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ISER));
   static_assert( offsetof(core_sys_t, nvic.icer) == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ICER));
   static_assert( offsetof(core_sys_t, nvic.ispr) == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ISPR));
   static_assert( offsetof(core_sys_t, nvic.icpr) == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, ICPR));
   static_assert( offsetof(core_sys_t, nvic.iabr) == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, IABR));
   static_assert( offsetof(core_sys_t, nvic.ipr)  == offsetof(core_sys_t, nvic) + HW_OFF(NVIC, IPR));
   static_assert( offsetof(core_sys_t, scb)       == HW_REGISTER_BASEADDR_SCB - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, scb.cpacr) == offsetof(core_sys_t, scb) + HW_OFF(SCB, CPACR));
   static_assert( offsetof(core_sys_t, mpu)     == HW_REGISTER_BASEADDR_MPU - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, debug)   == HW_REGISTER_BASEADDR_COREDEBUG - HW_REGISTER_BASEADDR_SYSTEM);
   static_assert( offsetof(core_sys_t, stir)    == offsetof(core_sys_t, scs) + HW_OFF(SCS, STIR));
   static_assert( offsetof(core_sys_t, fpu)     == HW_REGISTER_BASEADDR_FPU - HW_REGISTER_BASEADDR_SYSTEM);
}


/* function: waitevent_core
 * Versetzt den Prozessor in den Schlafmodus, falls das CPU-interne Event nicht gesetzt ist.
 * Daraust erwacht er mit dem nächsten Interrupt, dem nächsten Event (+Reset oder Debug).
 * Ist das CPU-interne Eventflag gesetzt, wird es gelöscht und der Befehl kehrt sofort zurück. */
static inline void waitevent_core(void)
{
   __asm( "WFE" );
}

/* function: sendevent_core
 * Setzt das interne Eventflag und sendet es an Eventout der CPU (falls Multicore-CPU). */
static inline void sendevent_core(void)
{
   __asm( "SEV" );
}


/* == FPU == */

// TODO: extract into module fpu.h

static inline void enable_fpu(int allowUnprivilegedAccess)
{
   hCORE->scb.cpacr |= ((allowUnprivilegedAccess ? 0x0F : 0x05) << 20);
}

static inline void disable_fpu(void)
{
   hCORE->scb.cpacr &= ~ (0x0f << 20);
}


#endif
