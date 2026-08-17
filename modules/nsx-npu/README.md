# nsx-npu

`nsx-npu` provides the Atomiq110-specific glue for the Arm Ethos-U85 NPU.

The pristine upstream `ethos-u-core-driver` is built exactly once, by
[`nsx-ethos-u-driver`](https://github.com/AmbiqAI/nsx-ethos-u-driver)
(consumed here via `nsx::ethos_u_driver`), so it is never vendored or edited
in this module. `nsx-npu` contributes only the parts that are genuinely
Atomiq110-specific: NPU power-domain sequencing, IRQ wiring, and
performance-mode selection.

## What it does

- Powers the NPU domain on/off through `am_hal_pwrctrl_periph_enable` /
  `am_hal_pwrctrl_periph_disable` — the only layer that does so; the
  upstream driver itself has no knowledge of Ambiq power control.
- Initializes the Ethos-U core driver (via `nsx_ethos_u_init`) at the
  Atomiq110 NPU register base.
- Owns the `am_npu_isr` -> `nsx_ethos_u_irq` interrupt glue (IRQ 117).
- Selects the NPU performance mode (`am_hal_pwrctrl_npu_mode_select`),
  optionally skipped for FPGA/pre-silicon targets with fixed clock trees.

Data-cache maintenance around NPU DMA (flush/invalidate) and inference
begin/end probe hooks are handled by `nsx-ethos-u-driver`, not here.

## Usage

```c
#include "nsx_npu.h"

nsx_npu_config_t cfg = {
    .perf_mode = NSX_NPU_PERF_HIGH_PERFORMANCE,
    .skip_perf_mode = false,
    .tolerate_power_ack = false,
};
NSX_TRY(nsx_npu_init(&cfg), "NPU init failed\n");
```

`tolerate_power_ack` defaults to `false`: a failed NPU power-domain enable
handshake fails init with `NSX_STATUS_INIT_FAILED` rather than continuing on
to a bus fault the first time the driver touches the NPU register block.
Set it `true` only for FPGA/pre-silicon targets (such as
`atomiq110_fpga_turbo`) that keep the NPU always-on and don't model the
power-status ack; leave it `false` on silicon so a genuine power-up failure
is caught at init.

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
