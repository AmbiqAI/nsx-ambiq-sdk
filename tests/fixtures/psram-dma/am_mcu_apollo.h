#ifndef TEST_AM_MCU_APOLLO_H
#define TEST_AM_MCU_APOLLO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifndef DTCM_BASEADDR
#define DTCM_BASEADDR 0x1000u
#endif
#define DTCM_MAX_SIZE UINT32_MAX
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_STATUS_FAIL 1u
#define AM_HAL_STATUS_IN_USE 3u
#define AM_HAL_STATUS_TIMEOUT 4u
#define AM_HAL_STATUS_INVALID_ARG 6u
#define AM_HAL_STATUS_INVALID_OPERATION 7u
#define AM_HAL_CLKMGR_CLK_ID_HFRC 0
#define AM_HAL_CLKMGR_USER_ID_MSPI0 2
#define AM_HAL_MSPI_TRANS_DMA 1
#define AM_HAL_MSPI_RX 0
#define AM_HAL_MSPI_TX 1
typedef unsigned am_hal_clkmgr_user_id_e;
typedef void (*am_hal_mspi_callback_t)(void *, uint32_t);
typedef struct {
    uint8_t ui8Priority;
    unsigned eDirection;
    uint32_t ui32TransferCount, ui32DeviceAddress, ui32SRAMAddress;
    uint32_t ui32PauseCondition, ui32StatusSetClr;
} am_hal_mspi_dma_transfer_t;
uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t state);
void am_hal_delay_us(uint32_t us);
void test_dmb(void);
#define __DMB() test_dmb()
uint32_t am_hal_clkmgr_clock_request(unsigned clock, am_hal_clkmgr_user_id_e user);
uint32_t am_hal_clkmgr_clock_release(unsigned clock, am_hal_clkmgr_user_id_e user);
uint32_t am_hal_mspi_nonblocking_transfer(void *handle, void *transfer, unsigned mode,
                                        am_hal_mspi_callback_t done, void *ctx);
#endif
