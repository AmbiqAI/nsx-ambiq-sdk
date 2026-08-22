# Platform Coverage

This file tracks the SoC/skew and board coverage actually carried by this
unified SDK repo. The repo spans the Apollo2, Apollo3, Apollo4, and Apollo5
families through a single SDK provider (`nsx-ambiqsuite`), plus an experimental
Atomiq (AT110) preview.

A SoC or board should not be treated as configure-ready until both its NSX
descriptor (`cmake/socs/<skew>.cmake` / `boards/<board>/board.cmake`) and the
matching AmbiqSuite prebuilt artifacts (`modules/nsx-ambiqsuite/sdk/lib/...`)
are present.

## Platform Maturity

Maturity is independent of whether a part's artifacts are staged:

- **released** — officially supported for production use.
- **experimental** — present for early evaluation only. **Not officially
  released**: not production-supported, and its NSX descriptors, linker maps,
  and module wiring may change or break without notice.

> **AT110 (Atomiq) is experimental.** The `atomiq110` SoC and its
> `atomiq110_fpga_turbo` FPGA board are an unreleased preview for early
> evaluation only. They are **not officially released** and must not be used in
> production.

On the AT110 FPGA, `atomiq110` is wired for the core runtime stack plus power
and NPU support: `nsx-cmsis-core`, `nsx-cmsis-startup`, `nsx-soc-hal`,
`nsx-core`, `nsx-interrupt`, `nsx-timer`, `nsx-gpio`, `nsx-perf`, `nsx-uart`,
`nsx-power` (Atomiq110-specific backend; see
`modules/nsx-power/src/atomiq110`), and `nsx-npu` (Ethos-U85 NPU glue on top
of `nsx-ethos-u-driver`, an external module resolved from
AmbiqAI/nsx-ethos-u-driver by the neuralspotx registry; see
`modules/nsx-npu`). The following are
intentionally **not** enabled for `atomiq110` yet (follow-up as the FPGA /
silicon exposes them): `nsx-i2c` and `nsx-spi` (basic buses), `nsx-freertos`
(RTOS port facts are ready), `nsx-psram` (external MSPI memory not present on
the FPGA), and the audio/USB middleware (`nsx-audio`, `nsx-usb`,
`nsx-ambiq-usb`).

### Family Overview

| Series | Core | SoCs | Boards (EVB) | Maturity |
| --- | --- | --- | --- | --- |
| `apollo2` | cortex-m4 | apollo2 | apollo2_evb | released |
| `apollo3` | cortex-m4 | apollo3, apollo3p | apollo3_evb, apollo3_evb_cygnus, apollo3p_evb, apollo3p_evb_cygnus | released |
| `apollo4` | cortex-m4 | apollo4l, apollo4p | apollo4l_evb, apollo4l_blue_evb, apollo4p_evb, apollo4p_blue_kbr_evb, apollo4p_blue_kxr_evb | released |
| `apollo330` | cortex-m55 | apollo330P | apollo330mP_evb | released |
| `apollo5` | cortex-m55 | apollo510, apollo510b, apollo510L, apollo5b | apollo510_evb, apollo510b_evb, apollo510dL_evb, apollo5b_evb | released (`apollo5b` descriptor-only) |
| `atomiq` | cortex-m55 + NPU | atomiq110 | atomiq110_fpga_turbo | **experimental** (not officially released) |

## Coverage States

| State | Meaning |
| --- | --- |
| `staged` | Descriptor and curated AmbiqSuite HAL/BSP artifacts are present in this repo. |
| `descriptor-only` | NSX descriptor exists, but the matching prebuilt HAL/BSP artifacts are not staged yet. |
| `planned` | Expected skew or board that should be pulled from a future SDK drop. |

HAL/BSP for every staged part are shipped as prebuilt static libraries under
`modules/nsx-ambiqsuite/sdk/lib/<toolchain>/<part>/...` for the `gcc`, `atfe`,
and `acfe` toolchains. There is no per-family "build from source" path: all
families use the same prebuilt-artifact intake.

## SoC / Skew Coverage

Per-SoC facts (core, DSP/MVE/FPU, PMU tier, toolchain selectors, and SEGGER
defaults) are the single source of truth in `cmake/socs/facts/<skew>.cmake`,
loaded by `nsx_load_soc_facts()`; the descriptor `cmake/socs/<skew>.cmake`
builds targets on top. See [cmake/README.md](../cmake/README.md).

| Logical skew | Status | Core | PMU tier | HAL artifact | Notes |
| --- | --- | --- | --- | --- | --- |
| `apollo2` | staged | cortex-m4 | none | `.../apollo2/libam_hal.a` | Apollo2 family. |
| `apollo3` | staged | cortex-m4 | none | `.../apollo3/libam_hal.a` | Apollo3 Blue family. |
| `apollo3p` | staged | cortex-m4 | none | `.../apollo3p/libam_hal.a` | Apollo3P Blue family. |
| `apollo4l` | staged | cortex-m4 | none | `.../apollo4l/libam_hal.a` | Apollo4 Lite family. |
| `apollo4p` | staged | cortex-m4 | none | `.../apollo4p/libam_hal.a` | Apollo4 Plus family. |
| `apollo330P` | staged | cortex-m55 | armv8m | `.../apollo330P/libam_hal.a` | Apollo330P (Apollo5-gen Cortex-M55). |
| `apollo510` | staged | cortex-m55 | armv8m | `.../apollo510/libam_hal.a` | Base Apollo510 skew. |
| `apollo510b` | staged | cortex-m55 | armv8m | reuses `.../apollo510/libam_hal.a` | Apollo510B board/BSP coverage on the Apollo510 HAL. |
| `apollo510L` | staged | cortex-m55 | armv8m | `.../apollo510L/libam_hal.a` | Apollo510L (AP510DL) family. |
| `atomiq110` | staged | cortex-m55 | armv8m | `.../atomiq110/libam_hal.a` | **Experimental — not officially released.** Atomiq (AT110) family; FPGA target, first Ambiq part with an NPU. |
| `apollo5b` | descriptor-only | cortex-m55 | armv8m | not staged | Descriptor + facts exist, but no `apollo5b` HAL/BSP artifacts are present. |

Artifact paths above are relative to `modules/nsx-ambiqsuite/sdk/lib/<toolchain>/`.

## Board Coverage

| NSX board | Status | Logical skew | BSP artifact | Notes |
| --- | --- | --- | --- | --- |
| `apollo2_evb` | staged | `apollo2` | `apollo2/apollo2_evb/libam_bsp.a` | |
| `apollo3_evb` | staged | `apollo3` | `apollo3/apollo3_evb/libam_bsp.a` | |
| `apollo3_evb_cygnus` | staged | `apollo3` | `apollo3/apollo3_evb_cygnus/libam_bsp.a` | Cygnus board variant. |
| `apollo3p_evb` | staged | `apollo3p` | `apollo3p/apollo3p_evb/libam_bsp.a` | |
| `apollo3p_evb_cygnus` | staged | `apollo3p` | `apollo3p/apollo3p_evb_cygnus/libam_bsp.a` | Cygnus board variant. |
| `apollo4l_evb` | staged | `apollo4l` | `apollo4l/apollo4l_evb/libam_bsp.a` | |
| `apollo4l_blue_evb` | staged | `apollo4l` | `apollo4l/apollo4l_blue_evb/libam_bsp.a` | BLE variant. |
| `apollo4p_evb` | staged | `apollo4p` | `apollo4p/apollo4p_evb/libam_bsp.a` | |
| `apollo4p_blue_kbr_evb` | staged | `apollo4p` | `apollo4p/apollo4p_blue_kbr_evb/libam_bsp.a` | KBR package, BLE. |
| `apollo4p_blue_kxr_evb` | staged | `apollo4p` | `apollo4p/apollo4p_blue_kxr_evb/libam_bsp.a` | KXR package, BLE. |
| `apollo330mP_evb` | staged | `apollo330P` | `apollo330P/apollo330mP_evb/libam_bsp.a` | |
| `apollo510_evb` | staged | `apollo510` | `apollo510/apollo510_evb/libam_bsp.a` | |
| `apollo510b_evb` | staged | `apollo510b` | `apollo510/apollo510b_evb/libam_bsp.a` | |
| `apollo510dL_evb` | staged | `apollo510L` | `apollo510L/apollo510dL_evb/libam_bsp.a` | AP510DL board. |
| `atomiq110_fpga_turbo` | staged | `atomiq110` | `atomiq110/atomiq110_fpga_turbo/libam_bsp.a` | **Experimental — not officially released.** Atomiq (AT110) FPGA board; no AM package, FPGA-only target. |
| `apollo5b_evb` | descriptor-only | `apollo5b` | not staged | Not configure-ready until `apollo5b` artifacts arrive. |

BSP artifact paths above are relative to
`modules/nsx-ambiqsuite/sdk/lib/<toolchain>/`.

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

Coverage: published on every staged board whose BSP defines buttons, across the
Apollo2/Apollo3 (typically 3 buttons), Apollo4, and Apollo5 (Apollo330P/510)
families. Not published on `apollo5b_evb`, which is descriptor-only and has no
staged BSP button definitions.

## Intake Rule

When a new SWS AmbiqSuite drop arrives, update this file from the drop
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
