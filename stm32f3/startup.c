/* title: Startup-Code-CortexM4 impl

   C implementation of startup code for CortexM4 based boards.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: startup.h
    Header file <Startup-Code-CortexM4>.

   file: startup.c
    Implementation file <Startup-Code-CortexM4 impl>.

   file: startup.s
    Implementation alternative in Assembler instead of startup.c.
*/
#include "konfig.h"
#include "startup.h"

/*
 * Define global stack memory for interrupts and reset and for main program.
 * The main program could run on msp stack the same used by interrupts or it
 * could use the psp stack which is separate from the msp stack.
 * The msp stack is located at start of SRAM ==> Overflow causes Fault exception.
 * If psp stack is used for main program it should be protected by use of MPU.
 */
__attribute__ ((section(".sram_address_start")))
uint32_t g_stack_msp[KONFIG_STACKSIZE/sizeof(uint32_t)];

/*
 * 1. Kopiert Initialisierungswerte vom Rom in das Ram.
 *
 * 2. Setzt den nicht initialisierten Datenbereich auf 0,
 *    wie es im C Standard vorgesehen ist.
 */
void init_datasegment_startup(void)
{
   uint32_t *src  = &_romdata;
   uint32_t *dest = &_data;

   // copy init values of data segment from rom into ram
   while (dest < &_edata) {
      *dest++ = *src++;
   }

   // clear bss segment
   dest = &_bss;
   while (dest < &_ebss) {
      *dest++ = 0;
   }
}

static inline void assert_values_startup(void)
{
   static_assert(KONFIG_STACKSIZE % sizeof(uint32_t) == 0);
#ifdef KONFIG_STACKSIZE_PSP
   static_assert(KONFIG_STACKSIZE_PSP % sizeof(uint32_t) == 0);
#endif
}

/*
 * Einsprungspunkt, der nach Einschalten der Spannungsversorgung oder
 * Reset ausgeführt wird. Der MSP (Main-Stack-Pointer) wurde von der
 * CPU auf das Ende von g_stack_msp gesetzt, bevor diese Funktion
 * ausgeführt wird.
 *
 * Falls KONFIG_USE_PSP gesetzt ist, wird vor Aufruf von main
 * mittels getmainpsp_startup der Wert des psp Stackpointers ermittelt und die CPU
 * von MSP auf PSP Stackpointer umgeschaltet.
 */
__attribute__((naked))
void reset_interrupt(void)
{
   __asm volatile(
      "bl   init_datasegment_startup\n"
#ifdef KONFIG_USE_PSP
      "bl   getmainpsp_startup\n"
      "msr  psp, r0\n"        // psp = getmainpsp_startup()
      "mrs  r0, control\n"    // r0 = control
      "orrs r0, #(1<<1)\n"    // set bit 1 in r0
      "msr  control, r0\n"    // switch cpu to using psp as stack pointer
#endif
      "bl   main\n"
      "1:   b 1b;\n"  // do nothing / preserve state
   );
}

/*
 * This interrupt handler simply preserves the system state.
 */
void default_interrupt(void)
{
   while(1) {
      // do nothing / preserve state
   }
}

/*
 * Map all interrupt handlers to Default_handler except reset_interrupt.
 */
void default_interrupt(void) __attribute__((weak));
void reset_interrupt(void) __attribute__((weak));
void nmi_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void fault_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void mpufault_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void busfault_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void usagefault_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void svcall_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void debugmonitor_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void pendsv_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void systick_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void WWDG_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void pvd_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void TAMP_STAMP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void RTC_WKUP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void FLASH_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void RCC_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void gpiopin0_interrupt(void) __attribute__((weak, alias("default_interrupt"))); // EXTI0
void EXTI1_IRQHandler(void) __attribute__((weak, alias("default_interrupt"))); // EXTI1
void gpiopin2_tsc_interrupt(void) __attribute__((weak, alias("default_interrupt"))); // EXTI2
void EXTI3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void EXTI4_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel1_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel2_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel3_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel4_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel5_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel6_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma1_channel7_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void ADC1_2_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USB_HP_CAN_TX_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USB_LP_CAN_RX0_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void CAN_RX1_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void CAN_SCE_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void EXTI9_5_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM1_BRK_TIM15_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM1_UP_TIM16_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM1_TRG_COM_TIM17_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM1_CC_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM2_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM4_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void I2C1_EV_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void I2C1_ER_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void I2C2_EV_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void I2C2_ER_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void SPI1_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void SPI2_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USART1_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USART2_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USART3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void EXTI15_10_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void RTC_Alarm_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USBWakeUp_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM8_BRK_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM8_UP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM8_TRG_COM_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void TIM8_CC_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void ADC3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void SPI3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void UART4_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void UART5_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void timer6_dac_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void timer7_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma2_channel1_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma2_channel2_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma2_channel3_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma2_channel4_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void dma2_channel5_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void ADC4_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void COMP1_2_3_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void COMP4_5_6_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void COMP7_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USB_HP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USB_LP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void USBWakeUp_RMP_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));
void FPU_IRQHandler(void) __attribute__((weak, alias("default_interrupt")));

/*
 * Initialize vector table for Cortex M4 interrupt controller.
 */

uint32_t g_NVIC_vectortable[HW_KONFIG_NVIC_INTERRUPT_MAXNR + 1] __attribute__ ((section(".rom_address_0x0"))) = {
   [0] = (uintptr_t)&g_stack_msp[0] + sizeof(g_stack_msp), // main stack set after reset
   [1] = (uintptr_t)&reset_interrupt,     // reset_interrupt called in thread-mode after microcontroller wakes up from reset
   [2] = (uintptr_t)&nmi_interrupt,       // Non Maskable Interrupt
   [3] = (uintptr_t)&fault_interrupt,
   [4] = (uintptr_t)&mpufault_interrupt,  // Memory Protection Unit (MPU)
   [5] = (uintptr_t)&busfault_interrupt,
   [6] = (uintptr_t)&usagefault_interrupt,
   [11] = (uintptr_t)&svcall_interrupt,
   [12] = (uintptr_t)&debugmonitor_interrupt,
   [14] = (uintptr_t)&pendsv_interrupt,
   [15] = (uintptr_t)&systick_interrupt,
   [16] = (uintptr_t)&WWDG_IRQHandler,
   [17] = (uintptr_t)&pvd_interrupt,
   [18] = (uintptr_t)&TAMP_STAMP_IRQHandler,
   [19] = (uintptr_t)&RTC_WKUP_IRQHandler,
   [20] = (uintptr_t)&FLASH_IRQHandler,
   [21] = (uintptr_t)&RCC_IRQHandler,
   [22] = (uintptr_t)&gpiopin0_interrupt,
   [23] = (uintptr_t)&EXTI1_IRQHandler,
   [24] = (uintptr_t)&gpiopin2_tsc_interrupt,
   [25] = (uintptr_t)&EXTI3_IRQHandler,
   [26] = (uintptr_t)&EXTI4_IRQHandler,
   [27] = (uintptr_t)&dma1_channel1_interrupt,
   [28] = (uintptr_t)&dma1_channel2_interrupt,
   [29] = (uintptr_t)&dma1_channel3_interrupt,
   [30] = (uintptr_t)&dma1_channel4_interrupt,
   [31] = (uintptr_t)&dma1_channel5_interrupt,
   [32] = (uintptr_t)&dma1_channel6_interrupt,
   [33] = (uintptr_t)&dma1_channel7_interrupt,
   [34] = (uintptr_t)&ADC1_2_IRQHandler,
   [35] = (uintptr_t)&USB_HP_CAN_TX_IRQHandler,
   [36] = (uintptr_t)&USB_LP_CAN_RX0_IRQHandler,
   [37] = (uintptr_t)&CAN_RX1_IRQHandler,
   [38] = (uintptr_t)&CAN_SCE_IRQHandler,
   [39] = (uintptr_t)&EXTI9_5_IRQHandler,
   [40] = (uintptr_t)&TIM1_BRK_TIM15_IRQHandler,
   [41] = (uintptr_t)&TIM1_UP_TIM16_IRQHandler,
   [42] = (uintptr_t)&TIM1_TRG_COM_TIM17_IRQHandler,
   [43] = (uintptr_t)&TIM1_CC_IRQHandler,
   [44] = (uintptr_t)&TIM2_IRQHandler,
   [45] = (uintptr_t)&TIM3_IRQHandler,
   [46] = (uintptr_t)&TIM4_IRQHandler,
   [47] = (uintptr_t)&I2C1_EV_IRQHandler,
   [48] = (uintptr_t)&I2C1_ER_IRQHandler,
   [49] = (uintptr_t)&I2C2_EV_IRQHandler,
   [50] = (uintptr_t)&I2C2_ER_IRQHandler,
   [51] = (uintptr_t)&SPI1_IRQHandler,
   [52] = (uintptr_t)&SPI2_IRQHandler,
   [53] = (uintptr_t)&USART1_IRQHandler,
   [54] = (uintptr_t)&USART2_IRQHandler,
   [55] = (uintptr_t)&USART3_IRQHandler,
   [56] = (uintptr_t)&EXTI15_10_IRQHandler,
   [57] = (uintptr_t)&RTC_Alarm_IRQHandler,
   [58] = (uintptr_t)&USBWakeUp_IRQHandler,
   [59] = (uintptr_t)&TIM8_BRK_IRQHandler,
   [60] = (uintptr_t)&TIM8_UP_IRQHandler,
   [61] = (uintptr_t)&TIM8_TRG_COM_IRQHandler,
   [62] = (uintptr_t)&TIM8_CC_IRQHandler,
   [63] = (uintptr_t)&ADC3_IRQHandler,
   [67] = (uintptr_t)&SPI3_IRQHandler,
   [68] = (uintptr_t)&UART4_IRQHandler,
   [69] = (uintptr_t)&UART5_IRQHandler,
   [70] = (uintptr_t)&timer6_dac_interrupt,
   [71] = (uintptr_t)&timer7_interrupt,
   [72] = (uintptr_t)&dma2_channel1_interrupt,
   [73] = (uintptr_t)&dma2_channel2_interrupt,
   [74] = (uintptr_t)&dma2_channel3_interrupt,
   [75] = (uintptr_t)&dma2_channel4_interrupt,
   [76] = (uintptr_t)&dma2_channel5_interrupt,
   [77] = (uintptr_t)&ADC4_IRQHandler,
   [80] = (uintptr_t)&COMP1_2_3_IRQHandler,
   [81] = (uintptr_t)&COMP4_5_6_IRQHandler,
   [82] = (uintptr_t)&COMP7_IRQHandler,
   [90] = (uintptr_t)&USB_HP_IRQHandler,
   [91] = (uintptr_t)&USB_LP_IRQHandler,
   [92] = (uintptr_t)&USBWakeUp_RMP_IRQHandler,
   [97] = (uintptr_t)&FPU_IRQHandler,
};
