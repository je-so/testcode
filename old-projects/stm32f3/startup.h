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

extern uint32_t g_stack_msp[/*KONFIG_STACKSIZE/sizeof(uint32_t)*/];
extern uint32_t _romdata; // start address of initialized data stored in flash rom
extern uint32_t _data;    // initialized data ram address
extern uint32_t _edata;   // end initialized data ram address
extern uint32_t _bss;     // bss data ram address
extern uint32_t _ebss;    // end bss data ram address

// === stack management ===

#ifndef KONFIG_STACKSIZE
#define KONFIG_STACKSIZE  (sizeof(uint32_t)*128)
            // Size of the main stack (msp) used for interrupt and exceptions (handler mode).
#endif

#ifdef KONFIG_USE_PSP
extern void* startup_get_psp(void);
            // Returned value is used to set process stack pointer (psp) before main is called.
#endif


// === main thread ===

extern int main(void);
            // Main function called at end of startup code.
            // In case KONFIG_USE_PSP is defined main is called with process stack pointer (psp) as active stack.
            // In case KONFIG_USE_PSP is not defined main is called with main stack pointer (msp) as active stack.


#endif
