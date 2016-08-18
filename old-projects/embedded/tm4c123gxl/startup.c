/* C implementation of startup code for TM4C123GXL board.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
#include <stdint.h>
#include "startup.h"

/*
 * Define global stack memory for reset_interrupt and main program.
 * It is located at start of SRAM ==> Overflow causes Fault exception.
 */
uint32_t g_main_stack[MAIN_STACKSIZE] __attribute__ ((section(".sram_address_start")));

/*
 * Entry point of program after reset or power on.
 */
void reset_interrupt(void)
{
   startup_init_datasegment();
   main(); // run main program
   while(1) {
      // do nothing / preserve state
   }
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
void NMI_Handler(void) __attribute__((weak, alias("default_interrupt")));
void HardFault_Handler(void) __attribute__((weak, alias("default_interrupt")));
void MPUFault_Handler(void) __attribute__((weak, alias("default_interrupt")));
void BusFault_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UsageFault_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SVCall_Handler(void) __attribute__((weak, alias("default_interrupt")));
void DebugMonitor_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PendSV_Handler(void) __attribute__((weak, alias("default_interrupt")));
void systick_interrupt(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortC_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortD_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortE_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SSI0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWMFault_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWMGenerator0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWMGenerator1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWMGenerator2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void QuadratureEncoder0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADCSequence0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADCSequence1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADCSequence2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADCSequence3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WatchdogTimer_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer0SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer0SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer1SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer1SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer2SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer2SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void AnalogComparator0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void AnalogComparator1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void AnalogComparator2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SystemControl_Handler(void) __attribute__((weak, alias("default_interrupt")));
void FlashControl_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortF_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortG_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortH_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SSI1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer3SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer3SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void QuadratureEncoder1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void CAN0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void CAN1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Hibernate_Handler(void) __attribute__((weak, alias("default_interrupt")));
void USB0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWMGenerator3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void MicroDMASoftwareTransfer_Handler(void) __attribute__((weak, alias("default_interrupt")));
void MicroDMAError_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADC1Sequence0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADC1Sequence1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADC1Sequence2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void ADC1Sequence3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortJ_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortK_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortL_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SSI2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void SSI3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART4_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART5_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART6_Handler(void) __attribute__((weak, alias("default_interrupt")));
void UART7_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer4SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer4SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer5SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void Timer5SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer0SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer0SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer1SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer1SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer2SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer2SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer3SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer3SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer4SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer4SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer5SubtimerA_Handler(void) __attribute__((weak, alias("default_interrupt")));
void WideTimer5SubtimerB_Handler(void) __attribute__((weak, alias("default_interrupt")));
void FPU_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C4_Handler(void) __attribute__((weak, alias("default_interrupt")));
void I2C5_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortM_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortN_Handler(void) __attribute__((weak, alias("default_interrupt")));
void QuadratureEncoder2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP4_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP5_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP6_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortP7_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ4_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ5_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ6_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortQ7_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortR_Handler(void) __attribute__((weak, alias("default_interrupt")));
void GPIOPortS_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWM1Generator0_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWM1Generator1_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWM1Generator2_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWM1Generator3_Handler(void) __attribute__((weak, alias("default_interrupt")));
void PWM1Fault_Handler(void) __attribute__((weak, alias("default_interrupt")));

/*
 * Initialize vector table for Cortex M4 interrupt controller.
 */

uint32_t g_NVIC_vectortable[155] __attribute__ ((section(".rom_address_0x0"))) = {
   [0] = (uintptr_t)&g_main_stack + sizeof(g_main_stack), // main stack set after reset
   [1] = (uintptr_t)&reset_interrupt, // reset_interrupt called after microcontroller wakes up after reset
   [2] = (uintptr_t)&NMI_Handler, // Non Maskable Interrupt
   [3] = (uintptr_t)&HardFault_Handler,
   [4] = (uintptr_t)&MPUFault_Handler, // Memory Protection Unit (MPU)
   [5] = (uintptr_t)&BusFault_Handler,
   [6] = (uintptr_t)&UsageFault_Handler,
   [11] = (uintptr_t)&SVCall_Handler,
   [12] = (uintptr_t)&DebugMonitor_Handler,
   [14] = (uintptr_t)&PendSV_Handler,
   [15] = (uintptr_t)&systick_interrupt,
   [16] = (uintptr_t)&GPIOPortA_Handler,
   [17] = (uintptr_t)&GPIOPortB_Handler,
   [18] = (uintptr_t)&GPIOPortC_Handler,
   [19] = (uintptr_t)&GPIOPortD_Handler,
   [20] = (uintptr_t)&GPIOPortE_Handler,
   [21] = (uintptr_t)&UART0_Handler,
   [22] = (uintptr_t)&UART1_Handler,
   [23] = (uintptr_t)&SSI0_Handler, // Synchronous Serial Interface (SSI)
   [24] = (uintptr_t)&I2C0_Handler,
   [25] = (uintptr_t)&PWMFault_Handler,
   [26] = (uintptr_t)&PWMGenerator0_Handler,
   [27] = (uintptr_t)&PWMGenerator1_Handler,
   [28] = (uintptr_t)&PWMGenerator2_Handler,
   [29] = (uintptr_t)&QuadratureEncoder0_Handler,
   [30] = (uintptr_t)&ADCSequence0_Handler,
   [31] = (uintptr_t)&ADCSequence1_Handler,
   [32] = (uintptr_t)&ADCSequence2_Handler,
   [33] = (uintptr_t)&ADCSequence3_Handler,
   [34] = (uintptr_t)&WatchdogTimer_Handler,
   [35] = (uintptr_t)&Timer0SubtimerA_Handler,
   [36] = (uintptr_t)&Timer0SubtimerB_Handler,
   [37] = (uintptr_t)&Timer1SubtimerA_Handler,
   [38] = (uintptr_t)&Timer1SubtimerB_Handler,
   [39] = (uintptr_t)&Timer2SubtimerA_Handler,
   [40] = (uintptr_t)&Timer2SubtimerB_Handler,
   [41] = (uintptr_t)&AnalogComparator0_Handler,
   [42] = (uintptr_t)&AnalogComparator1_Handler,
   [43] = (uintptr_t)&AnalogComparator2_Handler,
   [44] = (uintptr_t)&SystemControl_Handler,
   [45] = (uintptr_t)&FlashControl_Handler,
   [46] = (uintptr_t)&GPIOPortF_Handler,
   [47] = (uintptr_t)&GPIOPortG_Handler,
   [48] = (uintptr_t)&GPIOPortH_Handler,
   [49] = (uintptr_t)&UART2_Handler,
   [50] = (uintptr_t)&SSI1_Handler,
   [51] = (uintptr_t)&Timer3SubtimerA_Handler,
   [52] = (uintptr_t)&Timer3SubtimerB_Handler,
   [53] = (uintptr_t)&I2C1_Handler,
   [54] = (uintptr_t)&QuadratureEncoder1_Handler,
   [55] = (uintptr_t)&CAN0_Handler,
   [56] = (uintptr_t)&CAN1_Handler,
   [59] = (uintptr_t)&Hibernate_Handler,
   [60] = (uintptr_t)&USB0_Handler,
   [61] = (uintptr_t)&PWMGenerator3_Handler,
   [62] = (uintptr_t)&MicroDMASoftwareTransfer_Handler,
   [63] = (uintptr_t)&MicroDMAError_Handler,
   [64] = (uintptr_t)&ADC1Sequence0_Handler,
   [65] = (uintptr_t)&ADC1Sequence1_Handler,
   [66] = (uintptr_t)&ADC1Sequence2_Handler,
   [67] = (uintptr_t)&ADC1Sequence3_Handler,
   [70] = (uintptr_t)&GPIOPortJ_Handler,
   [71] = (uintptr_t)&GPIOPortK_Handler,
   [72] = (uintptr_t)&GPIOPortL_Handler,
   [73] = (uintptr_t)&SSI2_Handler,
   [74] = (uintptr_t)&SSI3_Handler,
   [75] = (uintptr_t)&UART3_Handler,
   [76] = (uintptr_t)&UART4_Handler,
   [77] = (uintptr_t)&UART5_Handler,
   [78] = (uintptr_t)&UART6_Handler,
   [79] = (uintptr_t)&UART7_Handler,
   [84] = (uintptr_t)&I2C2_Handler,
   [85] = (uintptr_t)&I2C3_Handler,
   [86] = (uintptr_t)&Timer4SubtimerA_Handler,
   [87] = (uintptr_t)&Timer4SubtimerB_Handler,
   [108] = (uintptr_t)&Timer5SubtimerA_Handler,
   [109] = (uintptr_t)&Timer5SubtimerB_Handler,
   [110] = (uintptr_t)&WideTimer0SubtimerA_Handler,
   [111] = (uintptr_t)&WideTimer0SubtimerB_Handler,
   [112] = (uintptr_t)&WideTimer1SubtimerA_Handler,
   [113] = (uintptr_t)&WideTimer1SubtimerB_Handler,
   [114] = (uintptr_t)&WideTimer2SubtimerA_Handler,
   [115] = (uintptr_t)&WideTimer2SubtimerB_Handler,
   [116] = (uintptr_t)&WideTimer3SubtimerA_Handler,
   [117] = (uintptr_t)&WideTimer3SubtimerB_Handler,
   [118] = (uintptr_t)&WideTimer4SubtimerA_Handler,
   [119] = (uintptr_t)&WideTimer4SubtimerB_Handler,
   [120] = (uintptr_t)&WideTimer5SubtimerA_Handler,
   [121] = (uintptr_t)&WideTimer5SubtimerB_Handler,
   [122] = (uintptr_t)&FPU_Handler,
   [125] = (uintptr_t)&I2C4_Handler,
   [126] = (uintptr_t)&I2C5_Handler,
   [127] = (uintptr_t)&GPIOPortM_Handler,
   [128] = (uintptr_t)&GPIOPortN_Handler,
   [129] = (uintptr_t)&QuadratureEncoder2_Handler,
   [132] = (uintptr_t)&GPIOPortP_Handler,
   [133] = (uintptr_t)&GPIOPortP1_Handler,
   [134] = (uintptr_t)&GPIOPortP2_Handler,
   [135] = (uintptr_t)&GPIOPortP3_Handler,
   [136] = (uintptr_t)&GPIOPortP4_Handler,
   [137] = (uintptr_t)&GPIOPortP5_Handler,
   [138] = (uintptr_t)&GPIOPortP6_Handler,
   [139] = (uintptr_t)&GPIOPortP7_Handler,
   [140] = (uintptr_t)&GPIOPortQ_Handler,
   [141] = (uintptr_t)&GPIOPortQ1_Handler,
   [142] = (uintptr_t)&GPIOPortQ2_Handler,
   [143] = (uintptr_t)&GPIOPortQ3_Handler,
   [144] = (uintptr_t)&GPIOPortQ4_Handler,
   [145] = (uintptr_t)&GPIOPortQ5_Handler,
   [146] = (uintptr_t)&GPIOPortQ6_Handler,
   [147] = (uintptr_t)&GPIOPortQ7_Handler,
   [148] = (uintptr_t)&GPIOPortR_Handler,
   [149] = (uintptr_t)&GPIOPortS_Handler,
   [150] = (uintptr_t)&PWM1Generator0_Handler,
   [151] = (uintptr_t)&PWM1Generator1_Handler,
   [152] = (uintptr_t)&PWM1Generator2_Handler,
   [153] = (uintptr_t)&PWM1Generator3_Handler,
   [154] = (uintptr_t)&PWM1Fault_Handler,
};
