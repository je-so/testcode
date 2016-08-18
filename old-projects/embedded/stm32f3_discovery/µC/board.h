/* title: Board-Hardware-Features

   Konfiguriert die Hardwareausstattung des Boards.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/board.h
    Header file <Board-Hardware-Features>.

   file: TODO: µC/board.c
    Implementation file <Board-Hardware-Features impl>.
*/
#ifndef STM32F3DISCOVERY_MC_BOARD_HEADER
#define STM32F3DISCOVERY_MC_BOARD_HEADER

// CORTEXM4 µController-specific features

/* STM32F303xC Anzahl Bits für System-Exceptions (IPSR 0-15) und Interrupt Priorität. */
#define HW_KONFIG_NVIC_PRIORITY_NROFBITS  4
/* STM32F303xC Maximale Anzahl implementierter Exceptions bzw. Interrupts. */
#define HW_KONFIG_NVIC_EXCEPTION_MAXNR    97

// External hardware features

/* Frequence of HSE clock provided to CPU */
#define HW_KONFIG_CLOCK_EXTERNAL_HZ  8000000
/* External clock either quartz crystal (1) or not (0) */
#define HW_KONFIG_CLOCK_EXTERNAL_ISCRYSTAL 0
/* Frequence of HSI clock integrated in CPU */
#define HW_KONFIG_CLOCK_INTERNAL_HZ  8000000


#endif
