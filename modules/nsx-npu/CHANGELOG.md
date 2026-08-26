# Changelog

## Unreleased

- Implement the `nsx_ethos_u_ticks()` / `nsx_ethos_u_ticks_per_ms()` timebase
  hooks for atomiq110 (STIMER, software-extended to 64 bits, rate derived from
  CLKMGR) and default `NSX_ETHOSU_INFERENCE_TIMEOUT_MS` to 5000 ms, so a wedged
  Ethos-U85 returns `ETHOSU_JOB_RESULT_TIMEOUT` instead of hanging forever.

- Initial `nsx-npu` module: Atomiq110 power-domain, IRQ, and performance-mode
  glue on top of `nsx-ethos-u-driver` (pristine upstream Ethos-U85 core
  driver), plus NSX init/deinit helpers.
