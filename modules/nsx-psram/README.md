# nsx-psram

`nsx-psram` provides BSP-aware external PSRAM bring-up for supported Apollo3p, Apollo5, and Apollo330 NSX targets.

Supported families in the unified SDK:
- Apollo3p boards with APS6404L SDR PSRAM
- Apollo5/Apollo330 boards with a Hex (x16) DDR PSRAM (AP Memory APS512XXN /
  APS512XXB), driven with AmbiqSuite's `am_devices_mspi_psram_aps25616n`
  driver

## Apollo5 (Apollo510/510B) hardware notes

The BSP macro `AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA` (used as this module's
Apollo5 `#error` presence guard) is misleadingly named: on real
`apollo510_evb`/`apollo510b_evb` hardware, the populated PSRAM is actually a
**Hex (x16) DDR** part (AP Memory `APS512XXN`/`APS512XXB`), not the Octal
`APS25616BA` part the macro name implies. This module drives it with the Hex
driver (`am_devices_mspi_psram_aps25616n`) accordingly.

Two things confirmed empirically on real hardware (both `apollo510_evb` and
`apollo510b_evb`) that differ from what might otherwise seem "correct":

- **Do not call `am_devices_mspi_psram_aps25616n_ddr_init_timing_check()` /
  `apply_ddr_timing()`.** The automatic RXDQSDELAY calibration scan
  consistently reports "no valid setting" across its entire sweep on this
  hardware, even though the plain `ddr_init()` call succeeds and XIP-mapped
  access works cleanly. Gating on the scan's result makes init fail outright
  for no good reason.
- **Access PSRAM via the XIP-mapped `base_address` this module returns
  (direct `volatile` pointer reads/writes), not via the AmbiqSuite driver's
  DMA-based bulk-transfer calls
  (`am_devices_mspi_psram_aps25616n_ddr_read()`/`ddr_write()`).** The DMA
  bulk-transfer path showed a reproducible periodic data-corruption pattern
  (a ~32-byte corrupted window recurring roughly every 128-136 bytes,
  consistent with a DDR read-strobe/DMA-FIFO timing issue) on every board
  tested; direct memory-mapped XIP access does not hit this and reads/writes
  cleanly. This module's public API is XIP-only by design (it hands back
  `base_address`/`size_bytes`, not read/write functions), so this is already
  the intended usage pattern.

Status as validated this session:
- `apollo510b_evb`: confirmed clean (zero mismatches over a 4KB
  write/read/verify) via direct XIP access at `AM_HAL_MSPI_CLK_125MHZ`.
- `apollo510_evb` (regular, non-Blue): init succeeds, but data verification
  showed a different (non-shift, high-mismatch-rate) corruption pattern with
  the same code, independent of clock speed (tried both 48MHz and 125MHz).
  Root cause not yet identified — possibly a genuine difference between the
  `APS512XXN` (AP510B) and `APS512XXB` (AP510) chip variants, or a
  board-specific issue. Treat `apollo510_evb` PSRAM as unverified until this
  is resolved.

## Apollo4 status

Apollo4 platform code (`src/apollo4/nsx_psram.c`, APS25616N support) exists
but is currently gated behind a compile-time `#error` requiring
`AM_BSP_MSPI_PSRAM_DEVICE_APS25616N`. None of the Apollo4 boards currently
staged in this SDK (`apollo4l_evb`, `apollo4l_blue_evb`, `apollo4p_evb`,
`apollo4p_blue_kbr_evb`, `apollo4p_blue_kxr_evb`) declare that macro in their
AmbiqSuite BSP — i.e. none of them have PSRAM populated. Real Apollo4 PSRAM
support in this SDK (e.g. via AmbiqSuite's `apollo4p_evb_disp_shield_rev2`,
which does declare `AM_BSP_MSPI_PSRAM_DEVICE_APS25616N`) requires staging a
PSRAM-capable board into the SDK provider first; `apollo4` is intentionally
left out of `nsx-module.yaml`'s `compatibility.boards`/`socs` until that
happens.

The module keeps board-specific routing in CMake, where board names map to an MSPI instance only when the BSP does not already provide that fact. The platform C sources stay focused on per-family device bring-up, timing, XIP enablement, and optional MPU setup.

Public interfaces live in `includes-api/`.
