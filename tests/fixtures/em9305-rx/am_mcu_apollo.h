/* Host HAL boundary. Cordio and EM9305 public declarations remain real. */
#ifndef TEST_EM9305_HAL_H
#define TEST_EM9305_HAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define AM_HAL_STATUS_SUCCESS 0
#define AM_HAL_STATUS_FAIL 1
#define AM_CRITICAL_BEGIN
#define AM_CRITICAL_END
#define AM_HAL_GPIO_OUTPUT_CLEAR 0
#define AM_HAL_GPIO_OUTPUT_SET 1
#define AM_HAL_IOM_FULLDUPLEX 1
#define AM_HAL_IOM_RX 2
#define AM_HAL_IOM_SPI_MODE 1
#define AM_HAL_IOM_SPI_MODE_0 0
#define AM_HAL_IOM_16MHZ 16000000
#define AM_HAL_MCUCTRL_INFO_DEVICEID 1
#define GPIO0_607F_IRQn 0
#define am_hal_iom_buffer(n) union { uint32_t words[((n)+3)/4]; uint8_t bytes[n]; }
typedef struct { unsigned eInterfaceMode, ui32ClockFreq, eSpiMode, ui32NBTxnBufLength; void *pNBTxnBuf; } am_hal_iom_config_t;
typedef struct {
    unsigned ui8RepeatCount, ui32PauseCondition, ui32StatusSetClr, eDirection;
    unsigned ui32InstrLen, ui32NumBytes;
    uint32_t *pui32TxBuffer, *pui32RxBuffer;
    union { unsigned ui32SpiChipSelect; } uPeerInfo;
    bool bContinue;
} am_hal_iom_transfer_t;
typedef struct { uint32_t ui32ChipID0, ui32ChipID1; } am_hal_mcuctrl_device_t;
typedef void (*am_hal_gpio_handler_t)(void *);
typedef struct { void *buffer; size_t size, capacity, count, head; } am_hal_queue_t;
#define am_hal_queue_from_array(q, a) (*(q) = (am_hal_queue_t){a, sizeof((a)[0]), sizeof(a)/sizeof((a)[0]), 0, 0})
bool am_hal_queue_empty(am_hal_queue_t *);
bool am_hal_queue_full(am_hal_queue_t *);
void *am_hal_queue_peek(am_hal_queue_t *);
void *am_hal_queue_next_slot(am_hal_queue_t *);
void am_hal_queue_item_add(am_hal_queue_t *, void *, unsigned);
void am_hal_queue_item_get(am_hal_queue_t *, void *, unsigned);
uint32_t am_hal_gpio_input_read(unsigned);
void am_hal_gpio_state_write(unsigned, unsigned);
void am_hal_gpio_interrupt_register(unsigned, unsigned, am_hal_gpio_handler_t, void *);
void am_hal_mcuctrl_info_get(unsigned, void *);
void NVIC_EnableIRQ(unsigned);
uint32_t am_hal_iom_spi_blocking_fullduplex(void *, am_hal_iom_transfer_t *);
uint32_t am_hal_iom_blocking_transfer(void *, am_hal_iom_transfer_t *);
#endif
