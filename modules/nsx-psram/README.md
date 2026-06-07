# nsx-psram

`nsx-psram` provides BSP-aware external PSRAM bring-up for supported Apollo3p, Apollo4, Apollo5, and Apollo330 NSX targets.

Supported families in the unified SDK:
- Apollo3p boards with APS6404L SDR PSRAM
- Apollo4 boards with APS25616N PSRAM
- Apollo5/Apollo330 boards with APS25616BA DDR PSRAM

The module keeps board-specific routing in CMake, where board names map to an MSPI instance only when the BSP does not already provide that fact. The platform C sources stay focused on per-family device bring-up, timing, XIP enablement, and optional MPU setup.

Public interfaces live in `includes-api/`.
