# nsx-board-atomiq110-fpga-turbo

Built-in NSX board definition for the Atomiq110 (atomiq110) FPGA "turbo" board.

- Cortex-M55 / Apollo5-class core; NPU-equipped Atomiq (R6) generation.
- The only upstream atomiq110 realization today is this FPGA board, so it is
  exposed as the canonical atomiq110 board. The HAL/BSP archives are compiled
  in FPGA mode (`ATOMIQ11X_FPGA` is baked into the device header); the core runs
  at the FPGA "turbo" frequency (25 MHz) and HBLRAM / some peripherals present
  on silicon are not available.
- Packaged with the NSX Python tooling repo.
- Vendored into generated apps under `boards/atomiq110_fpga_turbo/`.
