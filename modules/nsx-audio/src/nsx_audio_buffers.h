/* Internal PDM buffer validation; no hardware side effects. */
#ifndef NSX_AUDIO_BUFFERS_H
#define NSX_AUDIO_BUFFERS_H
#include "nsx_audio.h"
#include <stddef.h>

typedef struct {
    uint32_t samples;
    uint32_t frame_bytes;
} nsx_audio_buffer_layout_t;

static inline bool nsx_audio_buffer_layout(const nsx_audio_config_t *cfg,
                                         uint32_t instances, uint32_t count_limit,
                                         nsx_audio_buffer_layout_t *layout) {
    if (cfg == NULL || cfg->source != NSX_AUDIO_SOURCE_PDM || cfg->callback == NULL ||
        cfg->dma_buffer == NULL || cfg->pcm_buffer == NULL || cfg->num_samples == 0 ||
        (cfg->num_channels != 1 && cfg->num_channels != 2) ||
        (cfg->pdm.sample_width != NSX_AUDIO_PDM_16BIT && cfg->pdm.sample_width != NSX_AUDIO_PDM_24BIT) ||
        (uint32_t)cfg->pdm.mic >= instances ||
        ((uintptr_t)cfg->dma_buffer & 31U) != 0 ||
        ((uintptr_t)cfg->pcm_buffer % _Alignof(int16_t)) != 0) {
        return false;
    }
    /* One native 32-bit DMA word per sample; two frames per allocation.
     * Apollo5 stores signed 24-bit PCM in each word for either API width.
     * Apollo3's accepted 16-bit configuration retains its existing layout.
     * Bound division happens before any product, including channel count.
     * count_limit comes from the selected part's DMATOTCOUNT field. */
    const uint32_t bytes_per_sample = sizeof(uint32_t);
    if (cfg->num_samples > count_limit / bytes_per_sample / cfg->num_channels) {
        return false;
    }
    uint32_t samples = cfg->num_samples * cfg->num_channels;
    uint32_t frame = samples * bytes_per_sample;
    if (frame > UINT32_MAX / 2U || cfg->dma_buffer_size < frame * 2U ||
        cfg->pcm_buffer_size < samples * sizeof(int16_t)) {
        return false;
    }
    uintptr_t dma = (uintptr_t)cfg->dma_buffer;
    uintptr_t pcm = (uintptr_t)cfg->pcm_buffer;
    /* HAL DMA addresses are uint32_t. Check the last byte, without forming
     * an out-of-object pointer or wrapping the reverse-buffer address. */
    if (dma > UINT32_MAX || frame * 2U - 1U > UINT32_MAX - dma ||
        samples * sizeof(int16_t) - 1U > UINTPTR_MAX - pcm) {
        return false;
    }
    layout->samples = samples;
    layout->frame_bytes = frame;
    return true;
}
#endif
