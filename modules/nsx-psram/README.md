# nsx-psram

`nsx-psram` provides BSP-aware external PSRAM bring-up for supported Apollo3p, Apollo5, and Apollo330 NSX targets.

Supported families in the unified SDK:
- Apollo3p boards with APS6404L SDR PSRAM
- Apollo5/Apollo330 boards with a Hex (x16) DDR PSRAM (AP Memory
  APS512XXN/APS512XXB), driven with the AmbiqSuite "N" (`am_devices_mspi_psram_aps25616n`)
  or "BA" (`am_devices_mspi_psram_aps25616ba_1p2v`) driver depending on the
  exact board (see below)

## Apollo5 (Apollo510/510B) hardware notes

The BSP macro `AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA` (used as this module's
Apollo5 `#error` presence guard) is shared, unchanged, by both
`apollo510_evb` and `apollo510b_evb` -- but the two boards are actually
populated with **different device variants** of the same Hex (x16) DDR
PSRAM family, confirmed via each board's official Ambiq Quick Start Guide
component photo:

| Board | Populated part | Required driver |
|---|---|---|
| `apollo510_evb` (regular, non-Blue) | AP Memory `APS512XXB-AOB5NI-WA` | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA") |
| `apollo510b_evb` (and other Apollo5-class boards) | AP Memory `APS512XXN-AOB4BI-WBRZ` | `am_devices_mspi_psram_aps25616n` ("N") |

This mirrors old neuralSPOT's `ns_psram.c`, which explicitly branches its
driver function selection on `#ifdef apollo510_evb` for exactly this reason
-- selecting the "BA" driver only for the regular EVB, and "N" for
everything else. `CMakeLists.txt` selects `NSX_PSRAM_DEVICE_SOURCE` (and the
`NSX_PSRAM_USE_BA_DRIVER` compile definition consumed by
`src/apollo5/nsx_psram.c`) the same way, based on `NSX_AMBIQ_BOARD_NAME`.

**Using the wrong driver for a given board does not fail cleanly** --
`am_devices_mspi_psram_*_ddr_init()` still returns success either way, but
readback produces corrupted (non-zero mismatch) data. This was the actual
root cause of an initial "PSRAM works on apollo510b_evb but not
apollo510_evb" finding investigated this session, before the board-specific
driver split above was identified from neuralSPOT's own code and applied
here.

Two more things confirmed empirically on real `apollo510b_evb` hardware
(with the "N" driver) that differ from what might otherwise seem "correct"
(not re-tested with the "BA" driver on `apollo510_evb`, but the timing-check
call is skipped there too since this module never calls it for either
driver):

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
  write/read/verify) via direct XIP access at `AM_HAL_MSPI_CLK_125MHZ`,
  using the "N" driver.
- `apollo510_evb` (regular, non-Blue): confirmed clean (zero mismatches over
  a 4KB write/read/verify) via direct XIP access at `AM_HAL_MSPI_CLK_48MHZ`,
  using the "BA" driver. Both boards now validated on real hardware.

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
