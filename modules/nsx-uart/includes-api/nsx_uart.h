/**
 * @file ns-uart.h
 * @author Evan Chen
 * @brief API definition for NSX UART
 * @version 0.1
 * @date 2024-11-13
 *
 * @copyright Copyright (c) 2024
 *
 * \addtogroup nsx-uart
 * @{
 *
 */

#ifndef NSX_UART_H
    #define NSX_UART_H

    #include <stdbool.h>
    #include <stdint.h>

    // Ambiq/CMSIS headers include overloaded MVE intrinsics in C++ mode.
    // Keep them out of any caller-provided extern "C" block.
    #ifdef __cplusplus
extern "C++" {
    #endif
    #include "am_mcu_apollo.h"
    #include "am_util.h"
    #ifdef __cplusplus
}
    #endif

    #include "nsx_core.h"

    #ifdef __cplusplus
extern "C" {
    #endif

    #define NSX_UART_V0_0_1                                                                          \
        { .major = 0, .minor = 0, .revision = 1 }
    #define NSX_UART_API_ID 0xCA000B

extern const nsx_core_api_t nsx_uart_V0_0_1;
extern const nsx_core_api_t nsx_uart_oldest_supported_version;
extern const nsx_core_api_t nsx_uart_current_version;
extern am_hal_uart_config_t g_sUartConfig;
typedef void * nsx_uart_handle_t;
extern nsx_uart_handle_t nsx_uart_hal_handle;
extern volatile bool g_nsx_uart_data_available; // Flag to indicate data availability

typedef struct {
    uint32_t pin;
    const am_hal_gpio_pincfg_t *config;
} nsx_uart_pin_config_t;

/// @brief UART Transaction Control Stucture
typedef struct {
    // nsx_uart_handle_t handle;
    // void *rx_buffer;
    // void *tx_buffer;
    uint8_t status;
    // uint8_t itf;
    // bool dtr;
    // bool rts;
} nsx_uart_transaction_t;

typedef void (*nsx_uart_rx_cb)(nsx_uart_transaction_t *);
typedef void (*nsx_uart_tx_cb)(nsx_uart_transaction_t *);

/**
 * @brief UART Configuration Struct used to configure hardware settings
 *
 */
typedef struct
{
    const nsx_core_api_t *                 api;                            ///< API prefix
    uint32_t instance;                                                     ///< UART instance
    am_hal_uart_config_t *          uart_config;                    ///< UART Configuration
    const nsx_uart_pin_config_t *tx_pin;                                   ///< Optional TX pin config
    const nsx_uart_pin_config_t *rx_pin;                                   ///< Optional RX pin config
    nsx_uart_rx_cb rx_cb;            ///< Callback for rx events
    nsx_uart_tx_cb tx_cb;            ///< Callback for tx events
    bool enable_interrupts;
    bool tx_blocking;
    bool rx_blocking;
}
nsx_uart_config_t;

extern nsx_uart_config_t nsx_uart_config;

/**
 * @brief Initialize the UART system
 *
 * @param cfg
 * @param handle
 * @return uint32_t Status
 */
extern uint32_t nsx_uart_init(nsx_uart_config_t * cfg, nsx_uart_handle_t * h);


/**
 * @brief Register callbacks for USB events, should be called after ns_usb_init
 *
 * @param handle
 * @param rx_cb
 * @param tx_cb
 */
extern void nsx_uart_register_callbacks(nsx_uart_handle_t, nsx_uart_rx_cb, nsx_uart_tx_cb);

/**
 * @brief Send data through UART tx buffer
 *
 * @param pcStr
 */
extern uint32_t nsx_uart_blocking_send_data(nsx_uart_config_t * cfg, char *txBuffer, uint32_t size);

/**
 * @brief Send data through UART tx buffer
 *
 * @param pcStr
 */
extern uint32_t nsx_uart_nonblocking_send_data(nsx_uart_config_t * cfg, char *txBuffer, uint32_t size);

/**
 * @brief Read from the UART rx buffer
 *
 * @param cfg
 */
extern uint32_t nsx_uart_blocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size);

/**
 * @brief Read from the UART rx buffer
 *
 * @param cfg
 */
extern uint32_t nsx_uart_nonblocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size);


/**
 * @brief Check if data is available in the UART rx buffer
 *
 * @param handle
 * @return bool
 */
extern bool nsx_uart_data_available(void);

/**
 * @brief Change the baud rate of the UART
 *
 * @param baud_rate The new baud rate to set
 * @return uint32_t Status
 */
extern uint32_t nsx_uart_change_baud_rate(nsx_uart_handle_t uart_handle, uint32_t baud_rate);
extern void nsx_uart_handle_isr(void);

    #ifdef __cplusplus
}
    #endif
#endif
/** @}*/
