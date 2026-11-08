/* Link-only layout; not a bootable board memory map. */
ENTRY(_start)
MEMORY { FLASH (rx) : ORIGIN = 0x100000, LENGTH = 2M
RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K }
SECTIONS {
.text : { *(.text*) *(.rodata*) } > FLASH
.ARM.extab : { *(.ARM.extab*) } > FLASH
.ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
.preinit_array : { __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .; } > FLASH
.init_array : { __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .; } > FLASH
.fini_array : { __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .; } > FLASH
.data : { *(.data*) } > RAM
.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
.heap (NOLOAD) : { . = ALIGN(8); __HeapBase = .; end = .; . = ORIGIN(RAM) + LENGTH(RAM); __HeapLimit = .; } > RAM
}
