/* title: STM32F3-Discovery-Features

   Konfiguriert die Hardwareausstattung des Boards.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/board.h
    Header file <STM32F3-Discovery-Features>.

   file: TODO: µC/board.c
    Implementation file <STM32F3-Discovery-Features impl>.
*/
#ifndef STM32F303xC_MC_BOARD_HEADER
#define STM32F303xC_MC_BOARD_HEADER

// == CORTEX M4 µController ==

/* Anzahl Bits Interrupt Priorität */
#define HW_KONFIG_NVIC_INTERRUPT_PRIORITY_NROFBITS 4
/* Höchste Nummer aller implementierter Interrupts. */
#define HW_KONFIG_NVIC_INTERRUPT_MAXNR             97

// == Clock Features ==

/* Frequence of HSE clock provided to CPU */
#define HW_KONFIG_CLOCK_EXTERNAL_HZ          8000000
/* External clock either quartz crystal (1) or not (0) */
#define HW_KONFIG_CLOCK_EXTERNAL_ISCRYSTAL   0
/* Frequence of HSI clock integrated in CPU */
#define HW_KONFIG_CLOCK_INTERNAL_HZ          8000000

// == User LEDs + Switch ==

#define HW_KONFIG_USER_LED_NROF        8
#define HW_KONFIG_USER_LED_PORT        GPIOE
#define HW_KONFIG_USER_LED_PORT_BIT    GPIOE_BIT
#define HW_KONFIG_USER_LED_PINS        GPIO_PINS(HW_KONFIG_USER_LED_MAXNR, HW_KONFIG_USER_LED_MINNR)
#define HW_KONFIG_USER_LED_MINNR       8
#define HW_KONFIG_USER_LED_MAXNR       15

#define HW_KONFIG_USER_SWITCH_NROF     1
#define HW_KONFIG_USER_SWITCH_PORT     GPIOA
#define HW_KONFIG_USER_SWITCH_PORT_BIT GPIOA_BIT
#define HW_KONFIG_USER_SWITCH_PIN      GPIO_PIN0
#define HW_KONFIG_USER_SWITCH_MINPIN   0
#define HW_KONFIG_USER_SWITCH_MAXPIN   0


#endif
