#ifndef NSX_PSRAM_DMA_H
#define NSX_PSRAM_DMA_H

#include "am_mcu_apollo.h"

uint32_t nsx_psram_dma_validate_storage(void);
void nsx_psram_dma_configure(void *handle, uint32_t clock_hz);
/* Caller holds PRIMASK and exclusively owns this MSPI instance. */
uint32_t nsx_psram_dma_submit(am_hal_mspi_dma_transfer_t *transfer,
                            bool previously_pending,
                            am_hal_mspi_callback_t done, void *ctx);
uint32_t nsx_psram_dma_read(uint32_t offset, void *buffer, uint32_t length);
uint32_t nsx_psram_dma_write(uint32_t offset, const void *buffer, uint32_t length);

#endif
