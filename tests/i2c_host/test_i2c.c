#include "nsx_i2c.h"
#include <assert.h>
#include <string.h>

static am_hal_iom_transfer_t seen[2];
static unsigned calls;
static uint32_t injected_status;
static int endpoint;
uint32_t am_hal_iom_blocking_transfer(void *handle, am_hal_iom_transfer_t *t) {
    assert(handle == &endpoint && calls < 2);
    seen[calls++] = *t;
    if (injected_status && t->eDirection == AM_HAL_IOM_RX && t->pui32RxBuffer) ((uint8_t *)t->pui32RxBuffer)[0] = 0x99;
    return injected_status;
}

int main(void) {
    nsx_i2c_config_t cfg = { .iomHandle = &endpoint,
                           .sIomCfg = { .eInterfaceMode = AM_HAL_IOM_I2C_MODE } };
    /* Deliberately unaligned prefix, odd byte count, high bits, and leading zero. */
    _Alignas(uint32_t) uint8_t prefix[] = {0xee, 0x00, 0xd0, 0x81, 0x23, 0xfe, 0xff};
    uint32_t receive[(NSX_I2C_COMBINED_MAX_READ_SIZE + 3) / 4] = {0};
    for (size_t start = 1; start <= 2; ++start) {
    for (size_t length = 1; length <= NSX_I2C_COMBINED_MAX_WRITE_SIZE; ++length) {
        calls = 0;
        assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix + start, length, receive, 17) == 0);
        assert(calls == 1);
        const am_hal_iom_transfer_t *t = &seen[0];
        assert(t->eDirection == AM_HAL_IOM_RX && t->ui32InstrLen == length);
        assert(t->ui32NumBytes == 17 && t->uPeerInfo.ui32I2CDevAddr == 0x76);
        assert(t->pui32RxBuffer == receive && t->pui32TxBuffer == NULL);
        assert(!t->bContinue && !t->ui8RepeatCount && !t->eFdnbMode);
        assert(!t->ui32PauseCondition && !t->ui32StatusSetClr && t->ui8Priority == 1);
#ifdef AM_PART_APOLLO3
        uint64_t instruction = t->ui32Instr;
#else
        uint64_t instruction = t->ui64Instr;
#endif
        /* Reconstruct wire order from the HAL's MSB-first instruction phase. */
        for (size_t i = 0; i < length; ++i)
            assert(((instruction >> (8 * (length - 1 - i))) & 0xff) == prefix[i + start]);
    }
    }
    calls = 0;
    assert(nsx_i2c_write_read_combined(&cfg, 0x77, prefix, 1, receive, NSX_I2C_COMBINED_MAX_READ_SIZE) == 0);
    assert(calls == 1 && seen[0].ui32NumBytes == NSX_I2C_COMBINED_MAX_READ_SIZE);
    calls = 0;
    assert(nsx_i2c_write_read_combined(NULL, 0x76, prefix, 1, receive, 1) == AM_HAL_STATUS_INVALID_HANDLE);
    cfg.iomHandle = NULL;
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, 1) == AM_HAL_STATUS_INVALID_HANDLE);
    cfg.iomHandle = &endpoint;
    assert(nsx_i2c_write_read_combined(&cfg, 0x80, prefix, 1, receive, 1) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, NULL, 1, receive, 1) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, NULL, 1) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 0, receive, 1) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, 0) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, (uint8_t *)receive + 1, 1) == AM_HAL_STATUS_INVALID_ARG);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, NSX_I2C_COMBINED_MAX_WRITE_SIZE + 1, receive, 1) == AM_HAL_STATUS_OUT_OF_RANGE);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, NSX_I2C_COMBINED_MAX_READ_SIZE + 1) == AM_HAL_STATUS_OUT_OF_RANGE);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, SIZE_MAX, receive, 1) == AM_HAL_STATUS_OUT_OF_RANGE);
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, SIZE_MAX) == AM_HAL_STATUS_OUT_OF_RANGE);
    cfg.sIomCfg.eInterfaceMode = 0;
    assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, 1) == AM_HAL_STATUS_INVALID_OPERATION);
    cfg.sIomCfg.eInterfaceMode = AM_HAL_IOM_I2C_MODE;
    assert(calls == 0);

    /* NACK/arbitration/timeout-like errors pass through without retries or a
     * second transaction; HAL owns recovery. Partial RX data is not hidden. */
    const uint32_t errors[] = {17, 29, 53};
    for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        calls = 0; injected_status = errors[i];
        assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, 1) == errors[i]);
        assert(calls == 1 && !seen[0].bContinue && ((uint8_t *)receive)[0] == 0x99);
        calls = 0; injected_status = 0;
        assert(nsx_i2c_write_read_combined(&cfg, 0x76, prefix, 1, receive, 1) == 0 && calls == 1);
    }
    /* Preserve the legacy STOP-separated helper's two-transaction behavior. */
    uint32_t aligned_prefix = 0xd0;
    calls = 0;
    assert(nsx_i2c_write_read(&cfg, 0x76, &aligned_prefix, 1, receive, 1) == 0);
    assert(calls == 2 && seen[0].eDirection == AM_HAL_IOM_TX && seen[1].eDirection == AM_HAL_IOM_RX);
    assert(!seen[0].bContinue && !seen[1].bContinue);
    calls = 0; injected_status = 17;
    assert(nsx_i2c_write_read(&cfg, 0x76, &aligned_prefix, 1, receive, 1) == NSX_I2C_STATUS_ERROR);
    assert(calls == 1);
    return 0;
}
