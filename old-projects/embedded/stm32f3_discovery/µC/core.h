/* title: CortexM-Core-Peripherals

   Gibt Zugriff auf auf Cortex-M4 Prozessor Peripherie:

      o SysTick - 24-Bit clear-on-write, decrementing, wrap-on-zero Timer
      o Nested Vectored Interrupt Controller (NVIC) – Exceptions und Interrupts
      o System Control Block (SCB) - System Information und System Konfiguration
      o Memory Protection Unit (MPU) - ARMv7 Protected Memory System Architecture (PMSA)
      o Floating-Point Unit (FPU) - Single-precision HArdware support (+ fixed-point Konvertierung)

   Das STM32F303xC Spezifische ist in µC/board.h ausgegliedert.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Header file <CortexM4-Core-Peripherals>.

   file: TODO: µC/core.c
    Implementation file <CortexM4-Core-Peripherals impl>.
*/
#ifndef CORTEXM4_MC_CORE_HEADER
#define CORTEXM4_MC_CORE_HEADER

/* Access hardware unit (core or peripheral) memory mapped register with offset from base address */
#define HW_SREGISTER(HWUNIT, REGNAME) \
         (((struct { uint8_t _d[HW_REGISTER_OFFSET_##HWUNIT##_##REGNAME]; volatile uint32_t reg; }*)HW_REGISTER_BASEADDR_##HWUNIT)->reg)

/* Access hardware unit (core or peripheral) memory mapped register with address as sum of base and offset */
#define HW_REGISTER(HWUNIT, REGNAME) \
         (*((volatile uint32_t*) ((HW_REGISTER_BASEADDR_##HWUNIT) + HW_REGISTER_OFFSET_##HWUNIT##_##REGNAME)))

/* Zum Erezugen der Maske aus (HW_REGISTER_MASK_HWUNIT_REGNAME_FIELD_BITS << HW_REGISTER_MASK_HWUNIT_REGNAME_FIELD_POS) */
#define HW_REGISTER_MASK(HWUNIT, REGNAME, FIELD) \
         (HW_REGISTER_BIT_##HWUNIT##_##REGNAME##_##FIELD##_BITS << HW_REGISTER_BIT_##HWUNIT##_##REGNAME##_##FIELD##_POS)

/* Cortex-M4 System Control Space (address range 0xE000E000 to 0xE000EFFF) contains core peripherals */
#define HW_REGISTER_BASEADDR_SCS       0xE000E000
/* Cortex-M4 core systick timer base address */
#define HW_REGISTER_BASEADDR_SYSTICK   0xE000E010
/* Cortex-M4 nested vectored interrupt controller */
#define HW_REGISTER_BASEADDR_NVIC      0xE000E100
/* Cortex-M4 System control block */
#define HW_REGISTER_BASEADDR_SCB       0xE000ED00
/* Cortex-M4 Memory protection unit */
#define HW_REGISTER_BASEADDR_MPU       0xE000ED90


/* Cortex-M4 Auxiliary control register (ACTLR); Reset value: 0x00000000; Privileged;
 * Zur Einstellung der optimalen Performance des Cortex-M4. Eine Änderung wird nicht empfohlen.
 * Folgende Funktione können per Schreiben eines 1-Bits ausgeschaltet werden:
 * • FPU Instr. out-of-order bezogen auf Integer-ALU • IT-Opcode folding • Write buffer • LDM/STM interruption. */
#define HW_REGISTER_OFFSET_SCS_ACTLR  0x008


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

/* (CPAC) Coprocessor Access Control: specifies access privileges for coprocessors (FPU only). */
#define HW_REGISTER_OFFSET_SCS_CPAC   0xD88

static inline void enable_fpu(int allowUnprivilegedAccess)
{
   HW_REGISTER(SCS, CPAC) |= ((allowUnprivilegedAccess ? 0x0F : 0x05) << 20);
}

static inline void disable_fpu(void)
{
   HW_REGISTER(SCS, CPAC) &= ~ (0x0f << 20);
}

#endif
