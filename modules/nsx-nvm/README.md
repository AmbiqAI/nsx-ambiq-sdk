# nsx-nvm

`nsx-nvm` provides a thin, stable wrapper over the AmbiqSuite IS25WX064 MSPI
NOR flash HAL for erase/read/write and XIP on supported Apollo5-family NSX
targets.

Contents:

- timing scan + device init over MSPI
- read / write / sector erase / mass erase
- optional XIP (execute-in-place) enable/disable
- optional MPU region setup for the internal DMA/TCB buffer

## Board support

The MSPI instance used by the IS25WX064 flash device varies per board BSP
(see each board's `AM_BSP_MSPI_FLASH_MODULE`), so the mapping is resolved at
compile time in `CMakeLists.txt`:

| Board | MSPI module |
| --- | --- |
| `apollo510_evb` | 1 |
| `apollo510b_evb` | 1 |
| `apollo510dL_evb` | 2 |

Adding a new board requires confirming that board's BSP defines
`AM_BSP_MSPI_FLASH_DEVICE_IS25WX064` and adding its MSPI module number to the
mapping in `CMakeLists.txt`.

## IRQ ownership

`nsx-nvm` registers its MSPI interrupt handler through `nsx-interrupt`
(`nsx_irq_register()`) rather than calling `NVIC_SetPriority` /
`NVIC_EnableIRQ` directly. This keeps interrupt priority policy and vector
ownership with the app/board layer, consistent with `nsx-psram` (the closest
sibling module — both wrap MSPI flash-class devices with the same lifecycle
shape).

## Usage

```c
#include "nsx_nvm.h"

nsx_nvm_config_t cfg;
nsx_nvm_default_config(&cfg);

cfg.enable_xip = true; // optional

uint32_t status = nsx_nvm_init(&cfg);
if (status != NSX_STATUS_SUCCESS) {
    // handle error
}

// cfg.xip_base_address and cfg.size_bytes are populated on success.

uint8_t page[256];
nsx_nvm_sector_erase(0x0);
nsx_nvm_write(0x0, page, sizeof(page), true);
nsx_nvm_read(0x0, page, sizeof(page), true);
```

Public interfaces live in `includes-api/`.
