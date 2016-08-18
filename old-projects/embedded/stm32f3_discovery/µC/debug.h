/* title: Core-Debug

   Gibt Zugriff auf auf Cortex-M4 Debug Komponenten

      o Unterstützung von Debug-Monitor Ausführung durch "BPKT"-Instruction
        ist im STM32F303xC nicht implementiert.
      o TODO: Implementiere Zugriff auf weitere Komponenten

   Der Debug Access Port (DAP) ist der physikalische Zugangspunkt des externen Debuggers.

   Übersicht aller Debug Komponenten:

    - Private Peripheral Bus (address range 0xE0000000 to 0xE00FFFFF) -
   ┌───────────────────────────────────┬─────────────────────┬───────────────────────────────┐
   │ Group                             │ Address Range       │ Notes                         │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ITM:Instrumentation Trace Macrocell│0xE0000000-0xE0000FFF│ performance monitor support   │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │DWT:Data Watchpoint and Trace      │0xE0001000-0xE0001FFF│ trace support                 │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │FPB:Flash Patch and Breakpoint     │0xE0002000-0xE0002FFF│ optional                      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │SCS:SCB:System Control Block       │0xE000ED00-0xE000ED8F│ generic control features      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │SCS:DCB:Debug Control Block        │0xE000EDF0-0xE000EEFF│ debug control and config      │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │TPIU:Trace Port Interface Unit     │0xE0040000-0xE0040FFF│ optional serial wire viewer   │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ETM:Embedded Trace Macrocell       │0xE0041000-0xE0041FFF│ optional instruction trace    │
   ├───────────────────────────────────┼─────────────────────┼───────────────────────────────┤
   │ARMv7-M ROM table                  │0xE00FF000-0xE00FFFFF│ used for auto-configuration   │
   └───────────────────────────────────┴─────────────────────┴───────────────────────────────┘


   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/interrupt.h
    Header file <Core-Interrupt>.

   file: TODO: µC/debug.c
    Implementation file <Core-Interrupt impl>.
*/
#ifndef CORTEXM4_MC_DEBUG_HEADER
#define CORTEXM4_MC_DEBUG_HEADER

// == exported Peripherals/HW-Units

// == exported types

// == exported functions

// == definitions


// section: inline implementation


#endif
