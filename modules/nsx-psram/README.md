# nsx-psram

`nsx-psram` provides BSP-aware external PSRAM bring-up for supported Apollo3p, Apollo5, and Apollo330 NSX targets.

Supported families in the unified SDK:
- Apollo3p boards with APS6404L SDR PSRAM
- Apollo5/Apollo330 boards with APS25616BA DDR PSRAM

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
