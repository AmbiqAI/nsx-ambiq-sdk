# nsx-psram

`nsx-psram` provides BSP-aware external PSRAM bring-up for supported Apollo3p, Apollo5, and Apollo330 NSX targets.

Supported families in the unified SDK:
- Apollo3p boards with APS6404L SDR PSRAM
- Apollo5/Apollo330 boards with a Hex (x16) DDR PSRAM (AP Memory
  APS512XXN/APS512XXB), driven with the AmbiqSuite "N" (`am_devices_mspi_psram_aps25616n`)
  or "BA" (`am_devices_mspi_psram_aps25616ba_1p2v`) driver depending on the
  exact board (see below)

## Apollo5 (Apollo510/510B) hardware notes

The BSP macro `AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA` (used as this module's
Apollo5 `#error` presence guard) is shared, unchanged, by both
`apollo510_evb` and `apollo510b_evb`. Both boards are populated with an AP
Memory APS512-class Hex (x16) DDR PSRAM on MSPI0. The **AmbiqSuite "BA" (1.2V)
driver** (`am_devices_mspi_psram_aps25616ba_1p2v`) is the one that actually
works on this hardware:

| Board | Required driver | Size |
|---|---|---|
| `apollo510b_evb` | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA") | 64 MB (hardware-validated) |
| `apollo510_evb` (regular, non-Blue) | `am_devices_mspi_psram_aps25616ba_1p2v` ("BA") | 64 MB (hardware-validated) |
| `apollo330mP_evb` | Explicit `NSX_PSRAM_USE_BA_DRIVER=0/1` required; staged BSP advertises N; no fitted PSRAM validated | 32 MB conservative default |
| `apollo510dL_evb` | Not defaulted -- set `-DNSX_PSRAM_USE_BA_DRIVER=0/1` explicitly until validated. |

**The physical part marking does NOT tell you which driver to use.** The
`apollo510b_evb` Quick Start Guide names the populated part as AP Memory
`APS512XXN-AOB4BI-WBRZ` (an "N"-suffix part), and each vendored BSP's
`bsp_pins.src` also carries a part-name comment. Neither is a reliable guide
to the AmbiqSuite driver family: despite the "N" part marking, this board's
DDR read/write data path is only correct with the **"BA"** driver. Only real
hardware validation (with a matching BSP) should decide a board's driver requirement.

`CMakeLists.txt` selects `NSX_PSRAM_DEVICE_SOURCE` (and the
`NSX_PSRAM_USE_BA_DRIVER` compile definition consumed by
`src/apollo5/nsx_psram.c`) based on `NSX_AMBIQ_BOARD_NAME`, with an explicit
per-board branch for each case above (no silent catch-all `else`).

**Using the wrong driver for a given board does not fail cleanly.** With the
"N" driver on `apollo510b_evb`, `am_devices_mspi_psram_aps25616n_ddr_init()`
and `_ddr_enable_xip()` both still return success and the XIP aperture at
`0x60000000` becomes bus-accessible, but the DDR read path is only *marginally*
correct: it can read back correct data at 48 MHz/room temperature (a 52 KB
model was observed byte-exact over XIP in one session) yet corrupts
intermittently in the field ("worked sometimes, garbage/HardFault others; DDR
flaky, SDR more reliable"). This marginal N-driver path was the module's
previous `apollo510b_evb` default and the root of the reported
"PSRAM init succeeds but the uploaded model is garbage / firmware HardFaults"
failures.

Key diagnostic that pins it to the driver: the "N" driver's
`ddr_init_timing_check()` scan reports **"no valid setting"** across its whole
RXDQS sweep on this board — it cannot find a working DDR read window at all —
whereas the "BA" driver's scan **succeeds** (valid RXDQSDELAY, e.g. 14). This
was confirmed both standalone and end-to-end through heliaPROFILER: building the
profiler firmware with the N driver + timing scan makes PSRAM init fail with
`HPX_ERROR=psram_init_failed`, while the BA driver + scan initialises and
profiles cleanly. The BA driver's timing model matches this silicon; the N
driver's does not.

The 64 MB device size was independently confirmed by an XIP address-aliasing
test: `0x62000000` (+32 MB) holds data distinct from `0x60000000`, and
`0x63FFFFF0` (+64 MB) is live — so the part does not wrap at 32 MB. APS512 =
512 Mbit = 64 MB. The BA driver's `ddr_info` also reports 65536 KB.

**DDR timing calibration is now enabled by default on Apollo5.** This module
runs `*_ddr_init_timing_check()` + `apply_ddr_timing()` before `enable_xip()`
(gated by `NSX_PSRAM_RUN_DDR_TIMING_SCAN`, default `1` on Apollo5), matching the
AmbiqSuite `mspi_hex_ddr_aps25616*_psram_example` examples. This calibrates the
RXDQS read strobe to the centre of the passing window instead of leaving it at
the marginal power-on default, which is the robust fix for the intermittent DDR
corruption. It adds a short one-time boot sweep; set
`-DNSX_PSRAM_RUN_DDR_TIMING_SCAN=0` to skip it on boards/parts where it is known
unnecessary (with the BA driver, plain `ddr_init()` at 48 MHz also round-trips
cleanly, but the calibrated path is preferred).

Status as validated this session on real `apollo510b_evb` (rev 2.0, MSPI0,
J-Link probe `1160002954`):
- "BA" driver + 64 MB + timing scan, `AM_HAL_MSPI_CLK_48MHZ`: scan succeeds,
  XIP round-trips cleanly, and the full heliaPROFILER acceptance run passes
  (KWS model in PSRAM, 13 layers, ~2.05M CPU cycles).
- "BA" driver, timing scan skipped: also zero mismatches across the aperture
  from CPU and SWD; device reports 65536 KB (64 MB).
- "N" driver (previous default): timing scan finds no valid setting; plain
  `ddr_init` is marginal/uncalibrated — this was the bug.

## Apollo4 status

Apollo4 platform code supports the staged
`apollo4p_evb_disp_shield_rev2` board with APS25616N PSRAM. Other staged
Apollo4 EVBs do not advertise a populated compatible PSRAM and remain
unsupported.

The module keeps board-specific routing in CMake, where board names map to an MSPI instance only when the BSP does not already provide that fact. The platform C sources stay focused on per-family device bring-up, timing, XIP enablement, and optional MPU setup.

Public interfaces live in `includes-api/`.

## Public API

API version 1.0 provides:

- `nsx_psram_get_info()` for a snapshot of the configured aperture, capacity,
  clock, static capabilities, active XIP setting, lifecycle state, and DDR
  timing result.
- `nsx_psram_read()` / `nsx_psram_write()` for blocking, offset-based transfers
  with initialization, pointer, zero-length, overflow, and device-bound checks.

The snapshot reports timing as unavailable on Apollo3p, valid on the mandatory
Apollo4 scan path, and valid/not-run on Apollo5 according to
`NSX_PSRAM_RUN_DDR_TIMING_SCAN`. Vendor handles and timing structures are not
exposed. If platform initialization fails after touching hardware, the platform
attempts to unregister its IRQ, disable XIP where necessary, and deinitialize
the device. A confirmed rollback leaves the state retryable; an unconfirmed or
failed rollback sets `NSX_PSRAM_STATE_FAILED`, and subsequent init and transfer
calls return `NSX_PSRAM_STATUS_FAILED_STATE` until reset.

The public configuration is intentionally small and vendor-independent:
`clock_hz`, `enable_xip`, and `configure_mpu`. Unsupported clocks return
`NSX_PSRAM_STATUS_UNSUPPORTED`. `nsx_psram_get_info()` always returns the current
snapshot, including pre-init state or timing failure diagnostics. Zero-length
transfers are successful no-ops.

`capabilities` describes static support in the selected platform build:
synchronous transfers and XIP on all supported targets, plus timing scan support
on Apollo4/Apollo5. It is available before initialization and does not change
after success or failure. It must not be interpreted as active state:
`xip_enabled` reports whether the successful initialization enabled XIP,
`timing_status` and `rxdqs_delay` report the calibration result, and `state` plus
`last_init_status` report lifecycle status. In particular, disabling XIP leaves
`NSX_PSRAM_CAP_XIP` set while `xip_enabled` is false and `base_address` is zero;
blocking offset transfers remain available.

### DMA and cache coherency

On Apollo5, the synchronous API accepts arbitrary caller-buffer alignment and
read lengths with word-aligned PSRAM offsets. Write lengths must be multiples
of four; partial-word DMA writes can overwrite adjacent PSRAM bytes. DMA uses retained, noncacheable
DTCM storage; CPU copies move data between that storage and the caller. Neither
caller-buffer cache maintenance nor whole-cache invalidation is needed for this
path. The caller must not concurrently access its buffer until the call returns.
See the blocking-transfer ownership rules below for timeout and power behavior.
Calibration clears its readback cache while the XIP interface is still usable,
before teardown. This prevents calibration data from surviving into application
loads; it does not replace caller-managed coherence for later XIP accesses.
Apollo3/Apollo4 retain their existing device-driver paths.

The XIP aperture is a separate cacheable alias. A DMA write updates PSRAM but
does not evict an already-cached XIP copy; invalidate the corresponding XIP
range before observing a DMA write through XIP. Likewise, direct XIP writes
must use device-supported access widths and the platform bus-write flush before
mixing them with DMA reads.

### Apollo510B API validation

The downstream validation covers a dirty, cache-resident, unaligned 45-byte
destination spanning partial cache lines; a 32 KiB aligned transfer; true XIP
reads; supported 32-bit XIP writes; and end-of-device bounds. The XIP benchmark
invalidates the measured aperture first and performs one sequential volatile
32-bit pass over 256 KiB, a working set larger than D-cache, so reported
bytes/cycle represent external traffic plus CPU load-loop overhead rather than
repeated cache hits.

Final Apollo510B results on J-Link `1160002954`, using the BA 1.2 V driver,
Hex x16 DDR mode, and timing scan:

| MSPI clock | Timing | Sync | XIP read | 32-bit XIP write | Unaligned 45 B | Bounds | XIP bytes/cycle |
|---|---|---:|---:|---:|---:|---:|---:|
| 48 MHz | RXDQS 14 | 0 mismatches | 0 | 0 | 0 | 0 errors | 262144 / 362258 = 0.723 |
| 125 MHz | RXDQS 14 | 0 mismatches | 0 | 0 | 0 | 0 errors | 262144 / 204162 = 1.283 |

The probe reports a 64 KiB, 32-byte-line D-cache. The benchmark's 256 KiB
pre-invalidated working set is therefore four times D-cache capacity.

## Apollo5 blocking transfer ownership

Blocking reads and writes use a private 4096-byte, 4096-aligned DTCM buffer.
This adds CPU copies and up to 4095 bytes of linker alignment padding; it keeps
late DMA completion away from caller buffers after a timeout. The module checks
placement before hardware initialization, including with custom linker scripts.
PSRAM offsets and write lengths must be multiples of four. Caller buffers and
read lengths need not be word-aligned. Unsupported write lengths are rejected
before submitting any chunk; the adapter does not read/modify/write neighbors.
Concurrent blocking calls and blocking/async contention return
`NSX_PSRAM_STATUS_BUSY`. Apollo5 offset/write-length validation returns
`NSX_PSRAM_STATUS_ALIGNMENT`; actual HAL refusal statuses are preserved.

A transfer error quarantines the retained DMA state and sets `FAILED` until reset.
Caller buffers can be reused after return, but the module's DTCM must remain
powered. Prevent deep sleep during transfers and after `FAILED`; an idle-looking
software queue is not proof that timed-out DMA stopped. A failed multi-chunk
transfer may have completed a prefix. Apollo5 performs no whole-cache
maintenance on completion; callers still invalidate cacheable XIP aliases before
reading a range written through the transfer API.

## Asynchronous reads (API 1.1)

`nsx_psram_read_async()` queues one direct PSRAM-to-DTCM DMA read on Apollo5.
API 1.0 configurations remain accepted. Apollo3/Apollo4 report `UNSUPPORTED`
for nonempty async reads; their capability bit is clear and `async_depth` is zero.
The capability describes the compiled platform, not fitted hardware: check
initialization and the exact board's PSRAM support before submitting requests.

Initialize each caller-owned `nsx_psram_client_t` to zero and use it from one
thread at a time. Offset, destination and length must be multiples of four.
The whole destination must fit canonical DTCM and one 4096-byte DTCM window
(ADR 0011 and Apollo510/510B ERR014/ERR018). Cacheable aliases and SSRAM are
rejected. The queue has `NSX_PSRAM_ASYNC_DEPTH` slots (CMake cache setting,
default 8, range 1–32); contention or exhaustion returns `BUSY`.

An accepted request calls `done(ctx, status)` once from the MSPI interrupt,
with success or `TRANSFER_FAILED`. The latter records the original HAL status
in `get_info().last_transfer_status`, a diagnostic rather than a per-client
success channel. Prepare application completion state **before** submitting:
completion can run before `read_async()` returns. A refused or zero-length
request has no callback. No PSRAM API, including queries, may be called from
interrupts/callbacks; task notification must respect its RTOS IRQ-priority rules.
Initialization is a startup operation and must not overlap other API calls.

`client_pending()` reports unsigned issued-minus-completed for that client;
`pending()` reports all accepted outstanding async requests. Counters may wrap.
`nsx_psram_dma_in_progress(bool *)` separately samples hardware DMATIP on
Apollo5; check its returned status before using the Boolean sample. It does
not expose vendor handles or replace completion/sleep ownership rules.
Do not edit them while requests are pending. `client_pending(NULL)` returns
`UINT32_MAX`, and `client_drain(NULL, ...)` returns `INVALID_HANDLE`.
`client_drain(client, timeout_us)` checks immediately, then polls every
microsecond up to its budget. A zero timeout performs one check. Drain success
means completion, **not** successful data; latch errors in the callback before
consuming the destination. Timeout never cancels DMA: client, callback context
and destination remain alive and unavailable to the caller until completion.
Async reads do not use the blocking bounce buffer or add copies/cache work.
XIP is still a separate alias: invalidate its corresponding range before
observing DMA writes; direct XIP writes require a bus-write flush before DMA reads.

Blocking reads/writes and async reads exclude each other. A blocking platform
error marks the module FAILED until reset on every family; this does not cancel
DMA or repair Apollo3/Apollo4's existing timeout buffer-lifetime limitations.
Retained Apollo5 blocking storage must remain powered after failure.

`nsx_psram_can_sleep()` is a conservative snapshot: READY, no blocking operation,
and no pending async requests. It returns false before/during initialization and
after FAILED. Use it when deciding both deep-sleep entry and DTCM bank power-down,
under application scheduling/interrupt exclusion extending through that decision.
Keep DMA storage powered throughout active transfers. The snapshot is not a
persistent power lock, and says nothing about unrelated peripherals.

Local board validation is opt-in:

```sh
uv run --group hardware pytest tests/hardware/test_psram_async.py --hardware \
  --board apollo510_evb --probe PROBE_SERIAL --gcc-root /path/to/gcc
```

The suite checks DTCM sizes/edges, actual queue capacity, per-client completion,
refusals, blocking exclusion, ordinary WFI sleep, and XIP/DMA coexistence. Each
XIP pass invalidates its own range and samples hardware DMATIP at both ends;
at least one pass must observe active DMA at both endpoints. Deliberately wrong
callback/drain builds exercise the suite's failure detection. This is not a
cold-boot, deep-sleep or high-clock qualification.
