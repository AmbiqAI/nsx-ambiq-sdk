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
`apollo510_evb` and `apollo510b_evb`. Both boards are populated with an AP
Memory APS512-class Hex (x16) DDR PSRAM on MSPI0. The **AmbiqSuite "BA" (1.2V)
driver** (`am_devices_mspi_psram_aps25616ba_1p2v`) is the one that actually
works on this hardware:

| Board | Required driver | Size |
|---|---|---|
| `apollo510b_evb` | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA") | 64 MB (hardware-validated) |
| `apollo510_evb` (regular, non-Blue) | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA") | 32 MB default (size not re-validated this session) |
| `apollo330mP_evb` | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA", **unvalidated**; tracks `apollo510b_evb` per product-team guidance) | 64 MB |
| `apollo510dL_evb` | Not defaulted -- set `-DNSX_PSRAM_USE_BA_DRIVER=0/1` explicitly until validated. |

**The physical part marking does NOT tell you which driver to use.** The
`apollo510b_evb` Quick Start Guide names the populated part as AP Memory
`APS512XXN-AOB4BI-WBRZ` (an "N"-suffix part), and each vendored BSP's
`bsp_pins.src` also carries a part-name comment. Neither is a reliable guide
to the AmbiqSuite driver family: despite the "N" part marking, this board's
DDR read/write data path is only correct with the **"BA"** driver. Only real
hardware validation (or explicit product-team confirmation, as used for
`apollo330mP_evb`) should decide a board's driver requirement.

`CMakeLists.txt` selects `NSX_PSRAM_DEVICE_SOURCE` (and the
`NSX_PSRAM_USE_BA_DRIVER` compile definition consumed by
`src/apollo5/nsx_psram.c`) based on `NSX_AMBIQ_BOARD_NAME`, with an explicit
per-board branch for each case above (no silent catch-all `else`).

**Using the wrong driver for a given board does not fail cleanly.** With the
"N" driver on `apollo510b_evb`, `am_devices_mspi_psram_aps25616n_ddr_init()`
and `_ddr_enable_xip()` both still return success and the XIP aperture at
`0x60000000` becomes bus-accessible, but the DDR read path is only *marginally*
correct: it can read back correct data at 48 MHz/room temperature (a 52 KB
model was observed byte-exact over XIP in one session) yet corrupts
intermittently in the field ("worked sometimes, garbage/HardFault others; DDR
flaky, SDR more reliable"). This marginal N-driver path was the module's
previous `apollo510b_evb` default and the root of the reported
"PSRAM init succeeds but the uploaded model is garbage / firmware HardFaults"
failures.

Key diagnostic that pins it to the driver: the "N" driver's
`ddr_init_timing_check()` scan reports **"no valid setting"** across its whole
RXDQS sweep on this board — it cannot find a working DDR read window at all —
whereas the "BA" driver's scan **succeeds** (valid RXDQSDELAY, e.g. 14). This
was confirmed both standalone and end-to-end through heliaPROFILER: building the
profiler firmware with the N driver + timing scan makes PSRAM init fail with
`HPX_ERROR=psram_init_failed`, while the BA driver + scan initialises and
profiles cleanly. The BA driver's timing model matches this silicon; the N
driver's does not.

The 64 MB device size was independently confirmed by an XIP address-aliasing
test: `0x62000000` (+32 MB) holds data distinct from `0x60000000`, and
`0x63FFFFF0` (+64 MB) is live — so the part does not wrap at 32 MB. APS512 =
512 Mbit = 64 MB. The BA driver's `ddr_info` also reports 65536 KB.

**DDR timing calibration is now enabled by default on Apollo5.** This module
runs `*_ddr_init_timing_check()` + `apply_ddr_timing()` before `enable_xip()`
(gated by `NSX_PSRAM_RUN_DDR_TIMING_SCAN`, default `1` on Apollo5), matching the
AmbiqSuite `mspi_hex_ddr_aps25616*_psram_example` examples. This calibrates the
RXDQS read strobe to the centre of the passing window instead of leaving it at
the marginal power-on default, which is the robust fix for the intermittent DDR
corruption. It adds a short one-time boot sweep; set
`-DNSX_PSRAM_RUN_DDR_TIMING_SCAN=0` to skip it on boards/parts where it is known
unnecessary (with the BA driver, plain `ddr_init()` at 48 MHz also round-trips
cleanly, but the calibrated path is preferred).

Status as validated this session on real `apollo510b_evb` (rev 2.0, MSPI0,
J-Link probe `1160002954`):
- "BA" driver + 64 MB + timing scan, `AM_HAL_MSPI_CLK_48MHZ`: scan succeeds,
  XIP round-trips cleanly, and the full heliaPROFILER acceptance run passes
  (KWS model in PSRAM, 13 layers, ~2.05M CPU cycles).
- "BA" driver, timing scan skipped: also zero mismatches across the aperture
  from CPU and SWD; device reports 65536 KB (64 MB).
- "N" driver (previous default): timing scan finds no valid setting; plain
  `ddr_init` is marginal/uncalibrated — this was the bug.

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
