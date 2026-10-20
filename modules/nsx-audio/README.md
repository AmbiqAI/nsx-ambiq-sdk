# nsx-audio

PDM audio capture helper for AmbiqSuite-backed NSX targets with staged PDM support.

This module exposes the shared NSX audio API over the PDM HAL and board PDM pin
helpers staged in the AmbiqSuite provider tiers used by the selected SoC. It
initializes the PDM peripheral, sets up DMA-backed capture, and invokes an
application callback when samples are available. Buffer ownership, sample
processing, transport, and application protocols remain the caller's
responsibility.

## Design notes

- **PDM-only** — feature extraction and transport are separate concerns.
- **No feature extraction** — MFCC / mel-spectrogram are a separate concern.
- **No prints in driver code** — errors are returned as status codes.
- **Fixed `pdm_deinit` handle bug** — legacy code passed the config struct
  instead of the PDM HAL handle; this is corrected.
- **Fixed frequency config logic** — the missing `else` that caused
  HFRC/HFRC2_ADJ settings to be silently overwritten is fixed.
- **32-byte DMA alignment enforced** at init time.

## Supported SoCs

| SoC | PDM |
|-----|-----|
| Apollo3 | Yes |
| Apollo3P | Yes |
| Apollo5B | Yes |
| Apollo510 | Yes |
| Apollo510B | Yes |
| Apollo510L | Yes |
| Apollo330P | Yes |

## Public API

```c
#include "nsx_audio.h"

uint32_t nsx_audio_init(nsx_audio_config_t *cfg);
uint32_t nsx_audio_start(nsx_audio_config_t *cfg);
uint32_t nsx_audio_stop(nsx_audio_config_t *cfg);
```

## Buffer contract

Keep the configuration and its buffers alive and unchanged between successful
init and stop; serialize lifecycle operations with the ISR/callback. Each DMA
frame preserves the existing backend convention of `samples * channels * width * 2`
bytes, with two frames reserved. PCM needs `samples * channels * sizeof(int16_t)`
bytes. Validation rejects arithmetic/address overflow, invalid widths, unaligned
buffers and frames exceeding the part's 20-bit DMATOTCOUNT field before hardware
setup. The HAL's threshold/count divisibility requirements still apply. This
bounds work does not change packing, audio conversion or clock selection.

## Failure handling

Init returns `NSX_STATUS_INIT_FAILED` for HAL setup failures; start and stop
return `NSX_STATUS_FAILURE` for HAL operation failures. Capture becomes started
only after enable and DMA start succeed. `nsx_audio_get_status()` reports a
latched lifecycle or ISR error for an owned instance, including DMA service errors.
It returns invalid-handle before init or after successful stop. Poll it when
callbacks stop arriving; callbacks carry sample data, not error notifications.

A failed start/capture requires stop, then init before another start. Keep the
configuration and buffers alive until stop succeeds. Failed teardown retains
the handle and instance ownership so stop can be retried; it never reports
success merely because cleanup was attempted. IRQ delivery is disabled before
teardown, and powered-off register accesses are avoided on a retry. Init failure
attempts the same cleanup; if that fails, call stop again before reusing buffers
or the instance. Shared clock configuration is not restored because other
consumers may own that clock. Lifecycle calls require application serialization.

## Continuous capture

On Apollo5-family targets, `am_hal_pdm_dma_start()` starts capture once. The
ISR then calls the HAL interrupt service for completion and threshold events,
and obtains each completed ping/pong buffer through
`am_hal_pdm_dma_get_buffer()`. It does not restart an already active DMA transfer.
An interrupt-service failure or an unexpected completed-buffer address latches
an error and disables further IRQ delivery; stop must still succeed before
buffers can be reused. The HAL may already have queued another DMA frame.

Callbacks run after HAL service and PCM conversion. A callback can stop the
capture, or stop and initialize a replacement capture; the ISR does not access
the previous configuration after the callback returns. As with other DMA
capture paths, the callback must finish before hardware reuses its frame.
