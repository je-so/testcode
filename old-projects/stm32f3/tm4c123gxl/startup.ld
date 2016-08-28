/*
   Linker configuration file for GNU C Compiler

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00040000
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000
}

SECTIONS
{
    .text :
    {
        _text = .;
        *(.rom_address_0x0)
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    .stack (NOLOAD) :
    {
        *(.sram_address_start)
    } > SRAM

    .data :
    {
        _data = .;
        _romdata = LOADADDR (.data);
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH

    .bss :
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM
}
