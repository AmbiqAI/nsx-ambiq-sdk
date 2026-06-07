# Preserved: Apollo4 PDM driver

`ns_pdm_apollo4.c` — Apollo4-family (4B/4P/4L) PDM capture driver, preserved
from the retired `nsx-audio` repo before that repo's deletion.

## Why it's here, not in `modules/nsx-audio`

The unified `nsx-audio` module ships only the apollo3 and apollo5 PDM backends
and does **not** declare apollo4 in its `nsx-module.yaml` SoC list. No active
board profile uses Apollo4 audio today. This file is therefore kept as an
unintegrated intake artifact rather than a live module source.

## Integrating it later

Before this can be added to `modules/nsx-audio/src/apollo4/`:

- Rename to the `nsx_` convention (`nsx_pdm_apollo4.c`) to match siblings.
- Replace the legacy `#include "ns_core.h"` with `nsx_core.h` and migrate any
  `ns_`-prefixed symbols — the unified module is pure `nsx_` and its test suite
  (`tests/test_repo_shape.py`) forbids legacy NeuralSpot compat terms.
- Add an `apollo4` branch to `modules/nsx-audio/CMakeLists.txt` and declare the
  apollo4 SoCs in `modules/nsx-audio/nsx-module.yaml`.

## Provenance

- Source: `nsx-modules-retired/nsx-audio/src/apollo4/ns_pdm_apollo4.c`
- Notable fixes carried in this file: `pdm_deinit` HAL-handle fix, no printf in
  driver code, HFRC2_ADJ kick-start sequence, `am_bsp_pdm_pins_enable()` usage.
