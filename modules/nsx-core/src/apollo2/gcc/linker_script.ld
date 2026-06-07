/******************************************************************************
 *
 * linker_script.ld - Linker script for Apollo2 applications using startup_gcc.c
 *
 * Apollo2 memory map:
 *   FLASH : 0x00000000, 1024 KB
 *   SRAM  : 0x10000000,  256 KB
 *
 * The SRAM is partitioned to mirror the Apollo3 layout so the shared
 * Cortex-M4 startup_gcc.c (which initialises .data, .tcm, .stack and .bss)
 * works unchanged. Apollo2 has no dedicated TCM, so the .tcm region is simply
 * carved from the low SRAM and is typically empty.
 *
 *****************************************************************************/
ENTRY(Reset_Handler)

MEMORY
{
    ROMEM (rx) : ORIGIN = 0x00000000, LENGTH = 1048576
    RWMEM (rwx) : ORIGIN = 0x10011000, LENGTH = 192512
    TCM (rwx) : ORIGIN = 0x10000000, LENGTH = 65536
    STACKMEM (rwx) : ORIGIN = 0x10010000, LENGTH = 4096
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        KEEP(*(.patch))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > ROMEM

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RWMEM AT>ROMEM

    /* used by startup to initialize data */
    _init_data = LOADADDR(.data);

    .tcm :
    {
        . = ALIGN(4);
        _stcm = .;
        *(.tcm)
        *(.tcm*)
        . = ALIGN(4);
        _etcm = .;
    } > TCM AT>ROMEM

    /* used by startup to initialize tcm */
    _init_tcm = LOADADDR(.tcm);

    /* User stack section initialized by startup code. */
    .stack (NOLOAD):
    {
        . = ALIGN(8);
        *(.stack)
        *(.stack*)
        . = ALIGN(8);
    } > STACKMEM

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RWMEM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
