/* Startup Header for TM4C123GXL

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
#ifndef TM4C123GXL_STARTUP_HEADER_INCLUDED
#define TM4C123GXL_STARTUP_HEADER_INCLUDED

// === data segment ===

extern uint32_t _romdata; // start address of initialized data stored in flash rom
extern uint32_t _data; // initialized data ram address
extern uint32_t _edata; // end initialized data ram address
extern uint32_t _bss; // bss data ram address
extern uint32_t _ebss; // end bss data ram address

#define MAIN_STACKSIZE  128

extern uint32_t g_main_stack[MAIN_STACKSIZE];

/*
 * Function called from startup code.
 */
static inline void startup_init_datasegment(void)
{
    uint32_t *src  = &_romdata;
    uint32_t *dest = &_data;

    while (dest < &_edata) {
        *dest++ = *src++;
    }

    dest = &_bss;
    while (dest < &_ebss) {
        *dest++ = 0;
    }
}

/*
 * Main function called at end of startup code.
 */
extern int main(void);


#endif
