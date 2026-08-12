# nsx-npu

`nsx-npu` provides NSX helpers for the Arm Ethos-U85 NPU on Atomiq targets.

It vendors the Ambiq-modified Arm `ethos-u-core-driver` (Apache-2.0, see
`src/ethos-u-core-driver/LICENSE.txt`) from AmbiqSuite `stable` revision
`b5d2b9b530` — the same revision the packaged `nsx-ambiqsuite` Atomiq110
payload is generated from, so HAL headers, libraries, and the NPU driver move
in lockstep.

## What it does

- Powers the NPU domain on/off through `am_hal_pwrctrl_periph_enable`.
- Initializes the Ethos-U core driver at the Atomiq110 NPU register base.
- Owns the `am_npu_isr` -> `ethosu_irq_handler` interrupt glue (IRQ 117).
- Selects the NPU performance mode (`am_hal_pwrctrl_npu_mode_select`),
  optionally skipped for FPGA/pre-silicon targets with fixed clock trees.

## Usage

```c
#include "nsx_npu.h"

nsx_npu_config_t cfg = {
    .perf_mode = NSX_NPU_PERF_HIGH_PERFORMANCE,
    .skip_perf_mode = false,
};
NSX_TRY(nsx_npu_init(&cfg), "NPU init failed\n");
```

After init, a TFLM runtime built with an ethos-u custom-op kernel (for
example `nsx-helia-rt` configured with `NSX_HELIA_RT_ENABLE_ETHOSU=ON`)
dispatches Vela-compiled subgraphs to the NPU automatically. Direct
`ethosu_invoke` dispatch is possible via `nsx_npu_driver()`.

Models must be compiled offline with Vela for the Atomiq110 MAC
configuration:

```sh
vela --accelerator-config ethos-u85-256 model.tflite
```

## Compatibility

Atomiq110 only. The module fails the CMake configure step for other SoC
families and declares `compatibility.socs: [atomiq110]` in its manifest.
