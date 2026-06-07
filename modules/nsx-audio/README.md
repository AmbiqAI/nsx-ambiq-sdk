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
