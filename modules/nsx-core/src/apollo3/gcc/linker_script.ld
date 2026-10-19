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

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > ROMEM

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > ROMEM

    .preinit_array :
    {
        /* ATfE 22.1 picolibc walks preinit + init through these bounds. */
        PROVIDE_HIDDEN (__bothinit_array_start = .);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > ROMEM

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
        PROVIDE_HIDDEN (__bothinit_array_end = .);
    } > ROMEM

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
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

