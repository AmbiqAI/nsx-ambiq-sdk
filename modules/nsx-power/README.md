# nsx-power

`nsx-power` provides NSX power-management helpers for supported AmbiqSuite Apollo targets.

The staged module currently targets these unified-repo boards:

- `apollo3_evb`
- `apollo3_evb_cygnus`
- `apollo3p_evb`
- `apollo3p_evb_cygnus`
- `apollo4l_evb`
- `apollo4l_blue_evb`
- `apollo4p_evb`
- `apollo4p_blue_kbr_evb`
- `apollo4p_blue_kxr_evb`
- `apollo510_evb`
- `apollo510b_evb`
- `apollo510dL_evb`
- `apollo330mP_evb`

The module keeps power configuration as a first-class capability:

- MCU performance mode selection
- shutdown of unused peripheral blocks
- SRAM and memory retention configuration
- deep-sleep helpers
- family-specific low-power controls where available
- GPIO-based energy monitor state signaling for external current probes
- staged power-profile register dumps for Apollo4 and Apollo5 families

Target-specific behavior is selected from `NSX_SOC_FAMILY` in CMake and from
the board/SoC compile definitions exported by `NSX_BOARD_FLAGS_TARGET`.

Apollo3 and Apollo4 support in the unified repo comes from the retired standalone `nsx-power` implementation, adapted onto the current `nsx_` API surface.

On Apollo4, `need_tempco` is implemented as a private `nsx-power` helper layered on top of `nsx::timer` rather than by reviving the legacy timer and harness globals. It is only supported on non-Lite Apollo4 parts whose trim revision reports TempCo support; unsupported parts fail the TempCo request instead of silently enabling partial behavior.

The staged helper surface also absorbs the old `nsx-utils` energy-monitor and
power-profile dump capabilities without reviving the rest of that mixed utility
bundle. `nsx-power` now exposes GPIO state signaling for external power tools,
plus target-specific power-profile dumps intended for low-power bring-up and
instrumentation.

The public API is exported as `nsx_power.h`.
