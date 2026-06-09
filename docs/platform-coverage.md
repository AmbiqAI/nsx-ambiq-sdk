# Platform Coverage

This file separates logical R5-family platform coverage from the artifacts that
are currently staged in this curated core SDK repo.

A SoC or board should not be treated as configure-ready until its descriptor and
its AmbiqSuite artifacts are both present.

## Coverage States

| State | Meaning |
| --- | --- |
| `staged` | Descriptor and curated AmbiqSuite artifacts are present in this repo. |
| `artifact-built` | Source drop was staged locally and HAL/BSP artifacts were built, but CMake descriptors/adapters are not fully wired yet. |
| `descriptor-only` | NSX descriptor exists, but the matching SDK artifacts are not staged yet. |
| `planned` | Expected R5-family skew or board that should be pulled from a future SDK drop. |

## SoC / Skew Coverage

| Logical skew | Status | Staged Ambiq artifacts | Notes |
| --- | --- | --- | --- |
| `apollo510` | staged | `mcu/apollo510`, `src/apollo510`, `lib/apollo510` | Base AP510 R5-family skew. |
| `apollo510b` | staged | reuses `mcu/apollo510`, `src/apollo510`, `lib/apollo510` | Logical skew with AP510B board/BSP coverage. |
| `apollo510L` | staged | `modules/nsx-ambiqsuite-r5/sdk/lib/{gcc,atfe,acfe}/apollo510L/libam_hal.a` | Native R5.2.0 source family for AP510DL board coverage. |
| `apollo510d` | planned | not staged as that exact name | R5.2.0 names the discovered board `apollo510dL_evb` and maps it to `apollo510L`. |
| `apollo5b` | descriptor-only | not staged | Descriptor exists, but `mcu/apollo5b`, `src/apollo5b`, and `lib/apollo5b` are absent from the curated payload. |
| `apollo330P` | staged | `modules/nsx-ambiqsuite-r5/sdk/lib/{gcc,atfe,acfe}/apollo330P/libam_hal.a` | R5.2.0 HAL/BSP artifacts promoted into the provider payload. |
| `apollo330` family skews | planned | not staged | Add explicit skews from SDK drop metadata instead of inferring them from board names. |

## Board Coverage

| NSX board | Status | Logical skew | Staged Ambiq artifacts | Notes |
| --- | --- | --- | --- | --- |
| `apollo510_evb` | staged | `apollo510` | `boards/apollo510_evb/bsp`, `lib/apollo510/evb/libam_bsp.*` | Configure-ready with current payload. |
| `apollo510b_evb` | staged | `apollo510b` | `boards/apollo510b_evb/bsp`, `lib/apollo510/apollo510b_evb/libam_bsp.*` | Configure-ready with current payload. |
| `apollo510dL_evb` | staged | `apollo510L` | `modules/nsx-ambiqsuite-r5/sdk/lib/{gcc,atfe,acfe}/apollo510L/apollo510dL_evb/libam_bsp.a` | Native R5.2.0 AP510DL board name. |
| `apollo5b_evb` | descriptor-only | `apollo5b` | not staged | Existing descriptor should not be advertised as configure-ready until artifacts arrive. |
| `apollo330mP_evb` | staged | `apollo330P` | `modules/nsx-ambiqsuite-r5/sdk/lib/{gcc,atfe,acfe}/apollo330P/apollo330mP_evb/libam_bsp.a` | R5.2.0 BSP artifacts promoted into the provider payload. |

## Board Button Facts

Buttons are board-specific: the GPIO pin behind each button is a BSP fact, not a
SoC fact. The generic mechanism for reading a button (input mode, edge trigger,
per-pin IRQ callback) already lives in the `nsx-gpio` module, so boards only need
to publish *which pins* are buttons. There is no separate button module and no
board policy baked into a generic driver.

Each board descriptor whose BSP defines buttons exposes the following normalized
facts on its `nsx::board_flags` interface target. The pin macros resolve to the
board BSP values (`AM_BSP_GPIO_BUTTON*`), keeping a single source of truth:

| Macro | Meaning |
| --- | --- |
| `NSX_BOARD_HAS_BUTTONS` | Defined to `1` on boards that expose buttons. |
| `NSX_BOARD_BUTTON_COUNT` | Number of buttons the board publishes. |
| `NSX_BOARD_BUTTON0_PIN` ... | GPIO number per button, resolved from the BSP. |

Application code reads a button by feeding `NSX_BOARD_BUTTONn_PIN` into
`nsx_gpio_init()` (e.g. `NSX_GPIO_MODE_INPUT` + `NSX_GPIO_TRIGGER_FALLING` with an
IRQ callback). Because the pin macros expand to `AM_BSP_GPIO_BUTTON*`, the board
BSP header must be on the include path where they are used.

Coverage: published on the Apollo3 family (3 buttons), Apollo4 family, and the
staged R5 boards (`apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb`,
`apollo330mP_evb`). Not published on `apollo5b_evb`, which is descriptor-only and
has no staged BSP button definitions.

## Intake Rule

When a new SWS AmbiqSuite R5 drop arrives, update this file from the drop
manifest before wiring CMake. The required minimum for a staged SoC/skew is:

- an explicit logical skew name;
- Ambiq part name and MCU include directory;
- system source path;
- HAL library part path;
- startup/linker source selection;
- capability facts such as core, DSP, MVE, FPU, and PMU tier.

The required minimum for a staged board is:

- board descriptor;
- Ambiq BSP board name;
- BSP include directory;
- BSP library part/subdir path;
- package/pin/default facts owned by the board, not the SoC.
