/******************************************************************************
 *
 * linker_script.ld - Linker script for applications using startup_gnu.c
 *
 *****************************************************************************/
ENTRY(Reset_Handler)

MEMORY
{
    ROMEM (rx) : ORIGIN = 0x0000C000, LENGTH = 2048000
    RWMEM (rwx) : ORIGIN = 0x10011000, LENGTH = 716800
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

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > ROMEM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > ROMEM
    __exidx_end = .;

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

    /*--------------------------------------------------------------------------
     * Uninitialized data in the real low-latency TCM (0x10000000, 64 KB).
     * Place with __attribute__((section(".tcm_bss"))) — see nsx_mem.h
     * NSX_MEM_FAST_BSS. Unlike .bss (which targets Main SRAM/RWMEM on this
     * SoC), this is the only path onto the physical TCM for data.
     *
     * NOTE: this rule MUST precede the `.tcm` rule below. GNU ld assigns
     * each input section to the first SECTIONS rule that matches it, and
     * `.tcm`'s `*(.tcm*)` wildcard would otherwise also match `.tcm_bss`
     * input sections, silently pulling this "uninitialized" data into the
     * loaded/initialized `.tcm` output section instead.
     *--------------------------------------------------------------------------*/
    .tcm_bss (NOLOAD) :
    {
        . = ALIGN(4);
        _stcm_bss = .;
        *(.tcm_bss)
        *(.tcm_bss*)
        . = ALIGN(4);
        _etcm_bss = .;
    } > TCM

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
