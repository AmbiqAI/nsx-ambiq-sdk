/* Minimal HAL boundary for NSX wrapper tests; no bus behavior is simulated. */
#ifndef TEST_AM_MCU_APOLLO_H
#define TEST_AM_MCU_APOLLO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_STATUS_INVALID_HANDLE 2u
#define AM_HAL_STATUS_OUT_OF_RANGE 5u
#define AM_HAL_STATUS_INVALID_ARG 6u
#define AM_HAL_STATUS_INVALID_OPERATION 7u
#define AM_HAL_IOM_I2C_MODE 1u
#define AM_HAL_IOM_TX 0u
#define AM_HAL_IOM_RX 1u
#define AM_HAL_SYSCTRL_WAKE 0u
#define AM_HAL_PWRCTRL_PERIPH_IOM0 0u
typedef unsigned am_hal_pwrctrl_periph_e;
#ifdef AM_PART_APOLLO3
#define AM_HAL_IOM_MAX_OFFSETSIZE 3u
#define AM_HAL_IOM_MAX_TXNSIZE_I2C 512u
#else
#define AM_HAL_IOM_MAX_OFFSETSIZE 5u
#define AM_HAL_IOM_MAX_TXNSIZE_I2C 4095u
#endif
typedef struct {
    unsigned eInterfaceMode;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength, ui32ClockFreq;
} am_hal_iom_config_t;
typedef struct {
    union { uint32_t ui32I2CDevAddr, ui32SpiChipSelect; } uPeerInfo;
    uint32_t ui32InstrLen;
#ifdef AM_PART_APOLLO3
    uint32_t ui32Instr;
#else
    uint64_t ui64Instr;
#endif
    uint32_t ui32NumBytes;
    unsigned eDirection;
    const uint32_t *pui32TxBuffer;
    uint32_t *pui32RxBuffer;
    bool bContinue;
    uint8_t ui8RepeatCount, ui8Priority;
    unsigned eFdnbMode;
    uint32_t ui32PauseCondition, ui32StatusSetClr;
} am_hal_iom_transfer_t;
uint32_t am_hal_iom_blocking_transfer(void *, am_hal_iom_transfer_t *);
static inline void am_hal_pwrctrl_periph_enable(am_hal_pwrctrl_periph_e p) { (void)p; }
static inline uint32_t am_hal_iom_initialize(int8_t iom, void **handle) { (void)iom; *handle = (void *)1; return 0; }
static inline uint32_t am_hal_iom_power_ctrl(void *h, unsigned s, bool retain) { (void)h; (void)s; (void)retain; return 0; }
static inline uint32_t am_hal_iom_configure(void *h, am_hal_iom_config_t *c) { (void)h; (void)c; return 0; }
static inline uint32_t am_hal_iom_enable(void *h) { (void)h; return 0; }
#endif
