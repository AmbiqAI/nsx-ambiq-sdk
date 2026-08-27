# Changelog

## Unreleased

- Harden the atomiq110 Ethos-U timebase against the silent-degrade and
  spurious-abort paths found in review (#64). The hooks now publish a
  fixed-rate 1 µs virtual clock accumulated from STIMER at the tap rate that is
  live at each read, so an application that reconfigures STIMER before, between
  or during inferences no longer rescales the deadline; a counter clear by
  another STIMER owner is no longer mistaken for a 32-bit wrap; the slow
  counter read is taken outside the (now fixed, tiny) critical section; the
  `XTAL_2KHZ` and `HFRC_16MHZ` taps are recognised; and "is STIMER running"
  uses `am_hal_stimer_is_running()` (so a frozen counter is reclaimed). A
  timebase that fails to arm is no longer silent: `nsx_npu_init()` logs a
  warning and the new `nsx_npu_timebase_status()` reports why.

- Implement the `nsx_ethos_u_ticks()` / `nsx_ethos_u_ticks_per_ms()` timebase
  hooks for atomiq110 (STIMER, software-extended to 64 bits, rate derived from
  CLKMGR) and default `NSX_ETHOSU_INFERENCE_TIMEOUT_MS` to 5000 ms, so a wedged
  Ethos-U85 returns `ETHOSU_JOB_RESULT_TIMEOUT` instead of hanging forever.

- Initial `nsx-npu` module: Atomiq110 power-domain, IRQ, and performance-mode
  glue on top of `nsx-ethos-u-driver` (pristine upstream Ethos-U85 core
  driver), plus NSX init/deinit helpers.
