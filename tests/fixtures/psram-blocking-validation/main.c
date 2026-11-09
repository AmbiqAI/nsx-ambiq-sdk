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
    info->clock_hz=cfg->clock_hz;
#ifdef TEST_AP3_TCM
    info->size_bytes=8u*1024u*1024u;
#else
    info->size_bytes=32u*1024u*1024u;
#endif
    info->safe_to_retry=true;
    return 0;
}
static uint32_t hal_transfer(uint32_t offset,const void *buffer,uint32_t length) {
    (void)buffer;
#ifdef TEST_AP3_TCM
    uintptr_t address=(uintptr_t)buffer;
    if (address>=AM_HAL_FLASH_DTCM_START && address<=AM_HAL_FLASH_DTCM_END) {
        ++hal_refusals;return 1u;
    }
#endif
    /* Pinned Apollo4p HAL refuses unaligned HEX DDR addresses before CQ post.
     * The device wrapper collapses this INVALID_OPERATION to device ERROR. */
    if ((NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE0 ||
         NSX_PSRAM_DEVICE_CONFIG==AM_HAL_MSPI_FLASH_HEX_DDR_CE1) && (offset & 3u)) {
        ++hal_refusals;return 1u;
    }
    if (length > AM_HAL_MSPI_MAX_TRANS_SIZE) { ++hal_refusals;return 1u; }
    return transfer_error;
}
uint32_t nsx_psram_platform_read(uint32_t offset,void *buffer,uint32_t length) {
    ++reads;return hal_transfer(offset,buffer,length);
}
uint32_t nsx_psram_platform_write(uint32_t offset,const void *buffer,uint32_t length) {
    ++writes;return hal_transfer(offset,buffer,length);
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
#ifdef TEST_AP3_TCM
    unsigned previous_reads=reads,previous_writes=writes;
    const uintptr_t invalid[]={AM_HAL_FLASH_DTCM_START,AM_HAL_FLASH_DTCM_END-3u};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        assert(nsx_psram_read(0,(void *)invalid[i],4)==NSX_PSRAM_STATUS_DESTINATION);
        assert(nsx_psram_write(0,(void *)invalid[i],4)==NSX_PSRAM_STATUS_DESTINATION);
    }
    assert(reads==previous_reads && writes==previous_writes && !hal_refusals);
    assert(nsx_psram_get_info(&info)==0 && info.state==NSX_PSRAM_STATE_READY);
    assert(nsx_psram_read(0,(void *)(AM_HAL_FLASH_DTCM_END+1u),4)==0);
    assert(nsx_psram_write(0,(void *)(AM_HAL_FLASH_DTCM_END+1u),4)==0);
#endif
    /* A permitted short write and unaligned SRAM pointer are not new failures. */
    assert(nsx_psram_read(4,(uint8_t *)&buffer+1,1)==0);
    assert(nsx_psram_write(4,(uint8_t *)&buffer+1,1)==0);
    assert(nsx_psram_read(1,NULL,0)==0);
    assert(nsx_psram_write(1,NULL,0)==0);
#ifndef TEST_AP3_TCM
    unsigned before_reads=reads,before_writes=writes;
    assert(nsx_psram_read(0,&buffer,AM_HAL_MSPI_MAX_TRANS_SIZE+1u)==NSX_PSRAM_STATUS_OUT_OF_RANGE);
    assert(nsx_psram_write(0,&buffer,AM_HAL_MSPI_MAX_TRANS_SIZE+1u)==NSX_PSRAM_STATUS_OUT_OF_RANGE);
    assert(reads==before_reads && writes==before_writes && !hal_refusals);
    /* Fake HAL checks the count boundary, not a physical 16 MiB SRAM buffer. */
    assert(nsx_psram_read(0,&buffer,AM_HAL_MSPI_MAX_TRANS_SIZE)==0);
#endif
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
