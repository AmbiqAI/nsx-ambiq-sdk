# Split Repo Exit Checklist

This file records the minimum evidence required before `nsx-ambiq-sdk-r3`,
`nsx-ambiq-sdk-r4`, and `nsx-ambiq-sdk-r5` can be treated as retired in favor
of the unified `nsx-ambiq-sdk` repo.

## Exit Criteria

- Provider parity: each staged tier keeps its provider, HAL wrapper, and BSP wrapper in the unified repo.
- Useful module parity: anything materially present in the split repo and still useful to NSX apps or platform bring-up is either present in the unified repo or explicitly retired.
- Executable coverage: at least one configure-level contract test exists for the useful module slice of each tier.
- Intentional exclusions: anything not carried forward is documented as absent by design, not silently lost.

## Current Status

### R3

- Provider parity confirmed for `nsx-ambiqsuite-r3`, `nsx-ambiq-hal-r3`, and `nsx-ambiq-bsp-r3`.
- Useful runtime slice configures on `apollo3p_evb`: `nsx-core`, `nsx-interrupt`, `nsx-timer`, `nsx-perf`, `nsx-audio`, `nsx-gpio`, `nsx-i2c`, `nsx-spi`, `nsx-uart`, `nsx-psram`.
- Executable guardrail is present in `tests/test_cmake_contract.py` via the dedicated R3 contract test.
- Intentional exclusion: the split R3 repo has no `nsx-usb`, `nsx-power`, or `nsx-pmu-armv8m` module, so there is no missing R3 intake for those surfaces.

### R4

- Provider parity confirmed for `nsx-ambiqsuite-r4`, `nsx-ambiq-hal-r4`, and `nsx-ambiq-bsp-r4`.
- Optional middleware parity confirmed for `nsx-ambiq-usb-r4`.
- Useful runtime slice configures on `apollo4p_evb`: `nsx-core`, `nsx-interrupt`, `nsx-timer`, `nsx-i2c`, `nsx-spi`, `nsx-uart`, `nsx-psram`.
- Executable guardrails are present for the R4 runtime slice, the R4 USB substrate, and the unified `nsx-usb` wrapper path.
- Intake defect already fixed in unified repo: `nsx-usb` now selects `nsx-ambiq-usb-r4` when `NSX_SDK_PROVIDER=ambiqsuite-r4` instead of hard-wiring the R5 substrate.

### R5

- Provider parity confirmed for `nsx-ambiqsuite-r5`, `nsx-ambiq-hal-r5`, and `nsx-ambiq-bsp-r5`.
- Optional middleware parity confirmed for `nsx-ambiq-usb-r5` and the higher-level `nsx-usb` wrapper.
- Useful runtime slice configures through the existing R5 contract tests, including `nsx-pmu-armv8m`, `nsx-power`, `nsx-audio`, `nsx-perf`, and the shared foundation modules.
- Non-module surfaces are supersets rather than gaps: unified `boards/` and `cmake/` intentionally include Apollo3/Apollo4 descriptors in addition to the R5 content.
- Refactor cleanup already landed in unified repo: `nsx-pmu-armv8m` now uses namespaced `nsx::core` wiring instead of the stale standalone-style `nsx_core` target.

## Intentional Unified Supersets

- The unified repo intentionally carries Apollo3, Apollo4, and Apollo5 board and SoC descriptors under one `boards/` and `cmake/` tree.
- `nsx-usb` is a provider-gated wrapper over tier-specific TinyUSB substrates (`nsx-ambiq-usb-r4` and `nsx-ambiq-usb-r5`), not a single-tier R5 module anymore.
- `nsx-power` in the unified repo is broader than the old R5-only surface because Apollo3 and Apollo4 backends were intentionally pulled forward.

## Remaining Deletion Gate

- Re-run the tiered contract subset before deleting the split repos:
  - `pytest -q tests/test_cmake_contract.py -k 'r3_ or r4_ or runtime_modules_configure_through_soc_hal_contract or usb_module_configures_inside_sdk_repo or usb_substrate_configures_as_optional_sdk_module'`
- Decide whether repo-wide metadata cleanup is required before retirement. The main remaining policy-only issue is the broad manifest version-alignment expectation in `tests/test_manifests.py`, which is separate from module parity.
