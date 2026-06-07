/**
 * @file ns-uart.c
 * @author Evan Chen
 * @brief NSX UART Utilities
 *
 * @version 0.1
 * @date 2024-11-13
 *
 * Helper library for NSX features using UART
 * @copyright Copyright (c) 2024
 *
 */

#include "nsx_uart.h"
#include "nsx_core.h"

// UART handle
nsx_uart_handle_t nsx_uart_hal_handle;
volatile bool g_nsx_uart_data_available = false;
const nsx_core_api_t nsx_uart_V0_0_1 = {.apiId = NSX_UART_API_ID, .version = NSX_UART_V0_0_1};

const nsx_core_api_t nsx_uart_oldest_supported_version = {
    .apiId = NSX_UART_API_ID, .version = NSX_UART_V0_0_1};

const nsx_core_api_t nsx_uart_current_version = {.apiId = NSX_UART_API_ID, .version = NSX_UART_V0_0_1};

nsx_uart_config_t nsx_uart_config = {
    .api = &nsx_uart_V0_0_1,
    .instance = 0,
    .uart_config = NULL};

extern uint32_t nsx_uart_platform_init(nsx_uart_config_t *cfg);
extern uint32_t nsx_uart_blocking_send_data(nsx_uart_config_t * cfg, char *txBuffer, uint32_t size);
extern uint32_t nsx_uart_nonblocking_send_data(nsx_uart_config_t * cfg, char *txBuffer, uint32_t size);
extern uint32_t nsx_uart_blocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size);
extern uint32_t nsx_uart_nonblocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size);


void nsx_uart_register_callbacks(nsx_uart_handle_t handle, nsx_uart_rx_cb rxcb, nsx_uart_tx_cb txcb) {
    ((nsx_uart_config_t *)handle)->rx_cb = rxcb;
    ((nsx_uart_config_t *)handle)->tx_cb = txcb;
}

uint32_t nsx_uart_init(nsx_uart_config_t *cfg, nsx_uart_handle_t * handle) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_uart_oldest_supported_version, &nsx_uart_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif
    if(cfg == NULL || cfg->uart_config == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    nsx_uart_config = *cfg;
    *handle = (void *)&nsx_uart_config;
    return nsx_uart_platform_init(&nsx_uart_config);
}

bool nsx_uart_data_available(void) {
    return g_nsx_uart_data_available;
}


uint32_t nsx_uart_change_baud_rate(nsx_uart_handle_t handle, uint32_t baud_rate) {
    NSX_TRY(am_hal_uart_interrupt_disable(nsx_uart_hal_handle, (AM_HAL_UART_INT_RX | AM_HAL_UART_INT_TX | AM_HAL_UART_INT_RX_TMOUT)), "Failed to disable UART interrupts");
    ((nsx_uart_config_t *)handle)->uart_config->ui32BaudRate = baud_rate;
    NSX_TRY(am_hal_uart_configure(nsx_uart_hal_handle,
        ((nsx_uart_config_t *)handle)->uart_config),
        "Failed to reconfigure baud rate");
    NSX_TRY(am_hal_uart_interrupt_enable(nsx_uart_hal_handle, (AM_HAL_UART_INT_RX | AM_HAL_UART_INT_TX | AM_HAL_UART_INT_RX_TMOUT)), "Failed to enable UART interrupts");

    return NSX_STATUS_SUCCESS;
}
