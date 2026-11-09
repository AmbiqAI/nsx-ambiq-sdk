#include <assert.h>
#include "nsx_psram_internal.h"

static unsigned irq, reads, writes, hal_refusals;
static uint32_t transfer_error;
uint32_t nsx_psram_platform_irq_save(void) { unsigned old=irq; irq=1; return old; }
void nsx_psram_platform_irq_restore(uint32_t state) { irq=state; }
void nsx_psram_platform_delay_us(uint32_t us) { (void)us; }
uint32_t nsx_psram_platform_dma_in_progress(bool *value) {
    (void)value; return NSX_PSRAM_STATUS_UNSUPPORTED;
}
uint32_t nsx_psram_platform_check_destination(const void *buffer,uint32_t length) {
    (void)buffer;(void)length;return NSX_PSRAM_STATUS_UNSUPPORTED;
}
uint32_t nsx_psram_platform_read_async(uint32_t offset,void *buffer,uint32_t length,
    nsx_psram_async_slot_t *slot,bool previous) {
    (void)offset;(void)buffer;(void)length;(void)slot;(void)previous;
    assert(0);return NSX_PSRAM_STATUS_UNSUPPORTED;
}
uint32_t nsx_psram_platform_init(const nsx_psram_config_t *cfg,nsx_psram_platform_info_t *info) {
    info->clock_hz=cfg->clock_hz;info->size_bytes=64;info->safe_to_retry=true;
    return 0;
}
static uint32_t hal_transfer(uint32_t offset) {
    /* Pinned Apollo4p HAL refuses unaligned HEX DDR addresses before CQ post.
     * The device wrapper collapses this INVALID_OPERATION to device ERROR. */
    if ((NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE0 ||
         NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE1) && (offset & 3u)) {
        ++hal_refusals;return 1u;
    }
    return transfer_error;
}
uint32_t nsx_psram_platform_read(uint32_t offset,void *buffer,uint32_t length) {
    (void)buffer;(void)length;++reads;return hal_transfer(offset);
}
uint32_t nsx_psram_platform_write(uint32_t offset,const void *buffer,uint32_t length) {
    (void)buffer;(void)length;++writes;return hal_transfer(offset);
}
int main(void) {
    nsx_psram_config_t cfg;
    nsx_psram_info_t info;
    uint32_t buffer=0;
    assert(nsx_psram_default_config(&cfg)==0);
    cfg.api=&nsx_psram_V1_0_0;
    assert(nsx_psram_init(&cfg)==0);
    bool hex=NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE0 ||
             NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE1;
    uint32_t bad=nsx_psram_read(1,&buffer,4);
    assert(nsx_psram_get_info(&info)==0);
#ifdef EXPECT_OLD_REGRESSION
    assert(hex && bad==1u && hal_refusals==1u && !writes && reads==1u);
    assert(info.state==NSX_PSRAM_STATE_FAILED);
    assert(nsx_psram_init(&cfg)==NSX_PSRAM_STATUS_FAILED_STATE);
    return 0;
#else
    assert(bad==(hex ? NSX_PSRAM_STATUS_ALIGNMENT : 0u));
    assert(info.state==NSX_PSRAM_STATE_READY && nsx_psram_can_sleep());
    assert(nsx_psram_write(3,&buffer,1)==(hex ? NSX_PSRAM_STATUS_ALIGNMENT : 0u));
    if (hex) assert(!reads && !writes);
    assert(!hal_refusals);
    /* A permitted short write and unaligned SRAM pointer are not new failures. */
    assert(nsx_psram_read(4,(uint8_t *)&buffer+1,1)==0);
    assert(nsx_psram_write(4,(uint8_t *)&buffer+1,1)==0);
    assert(nsx_psram_read(1,NULL,0)==0);
    assert(nsx_psram_write(1,NULL,0)==0);
    assert(nsx_psram_get_info(&info)==0 && info.state==NSX_PSRAM_STATE_READY);
    assert(nsx_psram_init(&cfg)==NSX_PSRAM_STATUS_ALREADY_INITIALIZED);
    transfer_error=99;
    assert(nsx_psram_read(0,&buffer,4)==99);
    assert(nsx_psram_get_info(&info)==0 && info.state==NSX_PSRAM_STATE_FAILED);
    assert(!nsx_psram_can_sleep());
    assert(nsx_psram_read(0,&buffer,4)==NSX_PSRAM_STATUS_FAILED_STATE);
    assert(!irq);
    return 0;
#endif
}
