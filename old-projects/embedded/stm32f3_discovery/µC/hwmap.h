/* title: Hardware-Memory-Map

   Zeigt die Abbildung der Hardware-Baustene auf den 32-Bit
   Speicheradressraum.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: TODO: µC/hwmap.c
    Implementation file <Hardware-Memory-Map impl>.
*/
#ifndef STM32F303xC_MC_MAP_HEADER
#define STM32F303xC_MC_MAP_HEADER

/* == Bus AHB3 == */
/* Basisadresse von (ADC) Analog-to-digital converters 3(master) und 4(slave) am Bus AHB3 */
#define HW_REGISTER_BASEADDR_ADC3   0x50000400
/* Basisadresse von (ADC) Analog-to-digital converters 1(master) und 2(slave) am Bus AHB3 */
#define HW_REGISTER_BASEADDR_ADC1   0x50000000
/* == Bus AHB2 == */
/* Basisadresse von General Purpose In/Output (GPIO) Ports A..F am Bus AHB2 */
#define HW_REGISTER_BASEADDR_GPIOF  0x48001400
#define HW_REGISTER_BASEADDR_GPIOE  0x48001000
#define HW_REGISTER_BASEADDR_GPIOD  0x48000C00
#define HW_REGISTER_BASEADDR_GPIOC  0x48000800
#define HW_REGISTER_BASEADDR_GPIOB  0x48000400
#define HW_REGISTER_BASEADDR_GPIOA  0x48000000
/* == Bus AHB1 (HCLK) == */
/* Basisadresse von (TSC) Touch sensing controller am Bus AHB1 */
#define HW_REGISTER_BASEADDR_TSC    0x40024000
/* Basisadresse von (CRC) Cyclic redundancy check unit am Bus AHB1 */
#define HW_REGISTER_BASEADDR_CRC    0x40023000
/* Basisadresse von (FLASH) Flash memory interface am Bus AHB1 */
#define HW_REGISTER_BASEADDR_FLASH  0x40022000
/* Basisadresse von (RCC) Reset and clock control am Bus AHB1 */
#define HW_REGISTER_BASEADDR_RCC    0x40021000
/* Basisadresse von (DMA1) Direct memory access controller 2 am Bus AHB1 */
#define HW_REGISTER_BASEADDR_DMA2   0x40020400
/* Basisadresse von (DMA1) Direct memory access controller 1 am Bus AHB1 */
#define HW_REGISTER_BASEADDR_DMA1   0x40020000

/* == Bus APB1 (PCLK1) == */
/* Basisadresse von (DAC1) Digital-to-analog converter am Bus APB1 */
#define HW_REGISTER_BASEADDR_DAC1   0x40007400
/* Basisadresse von (UART5) Universal-Asynchronous-Receiver-Transmitter 5 am Bus APB1 */
#define HW_REGISTER_BASEADDR_UART5  0x40005000
/* Basisadresse von (UART4) Universal-Asynchronous-Receiver-Transmitter 4 am Bus APB1 */
#define HW_REGISTER_BASEADDR_UART4  0x40004C00
/* Basisadresse von (TIM7) Basic timers 7 am Bus APB1 */
#define HW_REGISTER_BASEADDR_TIM7   0x40001400
/* Basisadresse von (TIM6) Basic timers 6 am Bus APB1 */
#define HW_REGISTER_BASEADDR_TIM6   0x40001000

/* == Bus APB2 (PCLK2) == */
/* Basisadresse von (EXTI) Extended interrupts and events controller am Bus APB2 */
#define HW_REGISTER_BASEADDR_EXTI   0x40010400
/* Basisadresse von (SYSCFG) System configuration controller (also COMP and OPAMP) am Bus APB2 */
#define HW_REGISTER_BASEADDR_SYSCFG 0x40010000

/* == Memory Regions == */

/* Main Flash Memory of Controller, Mapped to Adress range 0..0x3ffff */
#define HW_MEMORYREGION_MAINFLASH_START 0x08000000
#define HW_MEMORYREGION_MAINFLASH_SIZE  0x40000    // 256K

#endif
