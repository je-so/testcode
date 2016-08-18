/* title: Startup-STM32F303xC

   Startup Header for STM32F303xC.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
#ifndef STM32F303xC_STARTUP_HEADER_INCLUDED
#define STM32F303xC_STARTUP_HEADER_INCLUDED

// === data segment ===

extern uint32_t _romdata; // start address of initialized data stored in flash rom
extern uint32_t _data;    // initialized data ram address
extern uint32_t _edata;   // end initialized data ram address
extern uint32_t _bss;     // bss data ram address
extern uint32_t _ebss;    // end bss data ram address

#ifndef KONFIG_STACKSIZE
#define KONFIG_STACKSIZE  (sizeof(uint32_t)*128)
#endif

/*
 * Main function called at end of startup code.
 */
extern int main(void);

#endif
