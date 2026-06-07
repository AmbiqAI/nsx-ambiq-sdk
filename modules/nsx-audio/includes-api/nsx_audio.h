/**
 * @file nsx_audio.h
 * @brief PDM audio capture helper for Ambiq SoCs.
 *
 * Provides a callback-driven PDM microphone interface with DMA-backed
 * double-buffered sampling.
 */
#ifndef NSX_AUDIO_H
#define NSX_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Audio source. */
typedef enum {
    NSX_AUDIO_SOURCE_PDM = 0,
} nsx_audio_source_e;

/** PDM clock source selection. */
typedef enum {
    NSX_AUDIO_CLK_PLL = 0,     /**< SYSPLL — default, most accurate */
    NSX_AUDIO_CLK_HFRC,        /**< HFRC 24 MHz */
    NSX_AUDIO_CLK_HFRC2_ADJ,   /**< HFRC2 adjusted — not available on 510L/330P */
    NSX_AUDIO_CLK_HFXTAL,      /**< HF crystal */
} nsx_audio_clk_e;

/** PDM clock frequency selection. */
typedef enum {
    NSX_AUDIO_PDM_CLK_750KHZ = 0,
    NSX_AUDIO_PDM_CLK_1_5MHZ,
} nsx_audio_pdm_clock_e;

/** PDM microphone board / module selection (maps to PDM instance). */
typedef enum {
    NSX_AUDIO_PDM_MIC0 = 0,  /**< PDM module 0 */
    NSX_AUDIO_PDM_MIC1 = 1,  /**< PDM module 1 */
    NSX_AUDIO_PDM_MIC2 = 2,  /**< PDM module 2 */
    NSX_AUDIO_PDM_MIC3 = 3,  /**< PDM module 3 */
} nsx_audio_pdm_mic_e;

/** PDM sample width. */
typedef enum {
    NSX_AUDIO_PDM_16BIT = 2,
    NSX_AUDIO_PDM_24BIT = 3,
} nsx_audio_pdm_sample_width_e;

/** PDM channel gain in 1.5 dB steps. */
typedef enum {
    NSX_AUDIO_PDM_GAIN_M120DB = 0x00,
    NSX_AUDIO_PDM_GAIN_M105DB = 0x01,
    NSX_AUDIO_PDM_GAIN_M90DB = 0x02,
    NSX_AUDIO_PDM_GAIN_M75DB = 0x03,
    NSX_AUDIO_PDM_GAIN_M60DB = 0x04,
    NSX_AUDIO_PDM_GAIN_M45DB = 0x05,
    NSX_AUDIO_PDM_GAIN_M30DB = 0x06,
    NSX_AUDIO_PDM_GAIN_M15DB = 0x07,
    NSX_AUDIO_PDM_GAIN_0DB = 0x08,
    NSX_AUDIO_PDM_GAIN_P15DB = 0x09,
    NSX_AUDIO_PDM_GAIN_P30DB = 0x0A,
    NSX_AUDIO_PDM_GAIN_P45DB = 0x0B,
    NSX_AUDIO_PDM_GAIN_P60DB = 0x0C,
    NSX_AUDIO_PDM_GAIN_P75DB = 0x0D,
    NSX_AUDIO_PDM_GAIN_P90DB = 0x0E,
    NSX_AUDIO_PDM_GAIN_P105DB = 0x0F,
    NSX_AUDIO_PDM_GAIN_P120DB = 0x10,
    NSX_AUDIO_PDM_GAIN_P135DB = 0x11,
    NSX_AUDIO_PDM_GAIN_P150DB = 0x12,
    NSX_AUDIO_PDM_GAIN_P165DB = 0x13,
    NSX_AUDIO_PDM_GAIN_P180DB = 0x14,
    NSX_AUDIO_PDM_GAIN_P195DB = 0x15,
    NSX_AUDIO_PDM_GAIN_P210DB = 0x16,
    NSX_AUDIO_PDM_GAIN_P225DB = 0x17,
    NSX_AUDIO_PDM_GAIN_P240DB = 0x18,
    NSX_AUDIO_PDM_GAIN_P255DB = 0x19,
    NSX_AUDIO_PDM_GAIN_P270DB = 0x1A,
    NSX_AUDIO_PDM_GAIN_P285DB = 0x1B,
    NSX_AUDIO_PDM_GAIN_P300DB = 0x1C,
    NSX_AUDIO_PDM_GAIN_P315DB = 0x1D,
    NSX_AUDIO_PDM_GAIN_P330DB = 0x1E,
    NSX_AUDIO_PDM_GAIN_P345DB = 0x1F,
} nsx_audio_pdm_gain_e;

/** Forward declaration. */
typedef struct nsx_audio_config nsx_audio_config_t;

/**
 * Audio callback — invoked from ISR context when a DMA transfer completes.
 *
 * @param cfg     Pointer to the owning audio config.
 * @param buffer  Pointer to the PCM sample buffer (int16_t samples).
 * @param num_samples  Number of samples available in @p buffer.
 */
typedef void (*nsx_audio_callback_t)(nsx_audio_config_t *cfg, void *buffer, uint32_t num_samples);

/** PDM-specific configuration. */
typedef struct {
    nsx_audio_clk_e         clock;         /**< Clock source */
    nsx_audio_pdm_clock_e   clock_freq;    /**< PDM clock frequency */
    nsx_audio_pdm_mic_e     mic;           /**< Microphone / PDM module index */
    nsx_audio_pdm_sample_width_e sample_width; /**< 16-bit or 24-bit */
    nsx_audio_pdm_gain_e    left_gain;     /**< Left channel gain */
    nsx_audio_pdm_gain_e    right_gain;    /**< Right channel gain */
} nsx_audio_pdm_config_t;

/** Main audio configuration. */
struct nsx_audio_config {
    nsx_audio_source_e      source;        /**< Audio source (PDM only) */
    uint8_t                 num_channels;  /**< 1 = mono, 2 = stereo */
    uint32_t                num_samples;   /**< Samples per callback (per channel) */

    nsx_audio_pdm_config_t  pdm;           /**< PDM-specific config */

    /** DMA sample buffer — must be 32-byte aligned. Size must be at least
     *  num_samples * num_channels * sample_width * 2 (double-buffered). */
    uint32_t               *dma_buffer;
    uint32_t                dma_buffer_size; /**< Total size in bytes */

    /** PCM output buffer — caller-provided, filled before callback. */
    int16_t                *pcm_buffer;
    uint32_t                pcm_buffer_size; /**< Size in bytes */

    /** Callback invoked when a DMA transfer completes. */
    nsx_audio_callback_t    callback;

    /** Opaque user context passed through to callback. */
    void                   *user_ctx;

    /* --- internal state (set by nsx_audio_init, do not modify) --- */
    void                   *_pdm_handle;
    uint8_t                 _started;
};

/** Default PDM configuration for the active SDK/SoC family. */
extern const nsx_audio_pdm_config_t nsx_audio_pdm_default;

/**
 * Initialize the audio subsystem.
 *
 * Validates configuration, sets up the PDM hardware, and prepares DMA.
 * Does NOT start sampling — call nsx_audio_start() separately.
 *
 * @return 0 on success, non-zero NS_STATUS error code on failure.
 */
uint32_t nsx_audio_init(nsx_audio_config_t *cfg);

/**
 * Start audio sampling.
 *
 * Triggers the first DMA transfer. The callback will fire each time
 * num_samples are ready.
 *
 * @return 0 on success.
 */
uint32_t nsx_audio_start(nsx_audio_config_t *cfg);

/**
 * Stop audio sampling and power down PDM.
 *
 * @return 0 on success.
 */
uint32_t nsx_audio_stop(nsx_audio_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* NSX_AUDIO_H */
