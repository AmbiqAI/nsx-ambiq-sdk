/******************************************************************************
 *
 * binary_counter.ld - Linker script for applications using startup_gnu.c
 *
 *****************************************************************************/
ENTRY(Reset_Handler)

MEMORY
{
    ROMEM (rx) : ORIGIN = 0x0000C000, LENGTH = 976K
    RWMEM (rwx) : ORIGIN = 0x10000000, LENGTH = 384K
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

    /* User stack section initialized by startup code. */
    .stack (NOLOAD):
    {
        . = ALIGN(8);
        *(.stack)
        *(.stack*)
        . = ALIGN(8);
    } > RWMEM

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

    /* Heap occupies all remaining RWMEM after .bss (the stack is a static
     * .stack section placed before .data). _sbrk() in nsx-core enforces
     * __HeapBase <= heap_end <= __HeapLimit so unbounded malloc cannot
     * silently corrupt .data/.bss. */
    .heap (NOLOAD):
    {
        . = ALIGN(8);
        __HeapBase     = .;
        end            = .;
        _end           = .;
        __end__        = .;
        __heap_start__ = .;
        KEEP(*(.heap))
        . = ORIGIN(RWMEM) + LENGTH(RWMEM);
        __HeapLimit    = .;
        __heap_end__   = .;
    } > RWMEM

    /* Guard: ensure data + bss leave room for a minimum heap */
    ASSERT( __HeapLimit > __HeapBase + 1024,
            "RWMEM overflow: data + bss leave 1 KiB of heap or less" )

    .ARM.attributes 0 : { *(.ARM.attributes) }
}

