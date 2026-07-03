# nsx-emmc

`nsx-emmc` provides a thin, stable wrapper over AmbiqSuite's SDIO/eMMC HAL
(`am_hal_card_*`/`am_hal_sdhc_*`) for block-level read/write on supported
Apollo510-family NSX boards.

Unlike `nsx-nvm`/`nsx-psram`, no vendored or directly-referenced AmbiqSuite
device-driver source file is required: `am_hal_card_*`/`am_hal_sdhc_*` are
fully precompiled into `libam_hal.a` for Apollo5, confirmed via `nm` on the
actual `.a` file.

This module registers its SDIO/SDHC interrupt line (`SDIO0_IRQn`/
`SDIO1_IRQn`, matching `nsx_emmc_config_t.sdio_instance`) with `nsx-interrupt`
the same way `nsx-psram`/`nsx-nvm` register their MSPI IRQ -- the module owns
device-level servicing, `nsx-interrupt` owns the raw vector table and NVIC
priority policy, and the app/board retains control over interrupt priority.
This avoids the module reaching directly into NVIC/global MCU state, which
is exactly the kind of app-policy overreach the old neuralSPOT modules were
prone to and that this SDK's modules are designed to avoid.

## Hardware notes

Both `apollo510_evb` (regular, non-Blue) and `apollo510b_evb` are populated
with the same ISSI `IS21EF08G-JCLI` (8GB) eMMC device on SDIO instance 0,
8-bit bus.

**Critical bring-up step, found empirically this session:** the eMMC's
`VCC_EMMC0`/`VCCQ_EMMC0` power rails are **not powered by default**. Every
`am_bsp_*_evb` BSP declares `am_bsp_emmc_power_on(uint8_t ui8SdioNum)` /
`am_bsp_emmc_power_off()` functions, but **no AmbiqSuite reference example
in this SDK's provider ever calls them** (confirmed by searching every
`emmc_*`/`sdio_*` example `.c` file in the AmbiqSuite tree this SDK vendors
from). Without this call, `am_hal_card_host_find_card()` and every
subsequent SDIO command **hangs indefinitely** waiting for a
command-complete interrupt that never arrives, with no error ever returned
-- a very easy trap to fall into if following Ambiq's own examples as-is.

`nsx_emmc_init()` calls `am_bsp_emmc_power_on()` automatically (with a 50ms
settle delay) before touching SDIO, so callers do not need to rediscover
this.

## Status as validated this session

Both boards confirmed working with a single-block (512 byte) write, read,
and verify:
- `apollo510b_evb` (probe `1160002954`): card found, init succeeded, write
  status 0 (success), read status 0 (success), **zero mismatches**.
- `apollo510_evb` (probe `1160002204`): identical result -- card found,
  init succeeded, write/read succeeded, **zero mismatches**.

Not yet exercised: multi-block transfers beyond a single 512-byte block,
erase, DDR50/UHS modes, card capacity introspection (CSD/EXT-CSD parsing --
`nsx_emmc_config_t.block_count` is currently always left at 0), and
power-saving (sleep/wakeup) paths.

## Usage

```c
nsx_emmc_config_t cfg;
nsx_emmc_default_config(&cfg);
uint32_t status = nsx_emmc_init(&cfg);
if (status == NSX_STATUS_SUCCESS) {
    nsx_emmc_block_write(3000, 1, my_write_buf);
    nsx_emmc_block_read(3000, 1, my_read_buf);
}
```

Public interfaces live in `includes-api/`.
