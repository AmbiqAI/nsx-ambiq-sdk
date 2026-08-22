# Changelog

## Unreleased

- Adds staged `nsx-power` helpers for GPIO-based energy monitor signaling.
- Adds staged Apollo4/Apollo5 power-profile dump helpers and keeps the
  legacy FreeRTOS malloc wrapper out of unified runtime.
- Consolidates the power-profile JSON/CSV printers into the shared
  `src/nsx_power_profile.c`; per-arch sources now only publish their register
  table via `nsx_power_profile_platform_regs()`. Drops the per-row 1 ms delay
  from the CSV path. No public API change.

## 5.2.23 (2026-05-30)

- Aligns the module manifest with the AmbiqSuite R5.2 SDK release train.
- Uses patch component 23 for NSX-local curation and integration updates.
