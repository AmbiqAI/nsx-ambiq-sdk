#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "nsx_uart.h"
#include "nsx_interrupt.h"
#include "nsx_core.h"

volatile uint32_t ui32LastError;
bool g_bUARTdone = false;
#define MAX_UART_RETRIES 10 // Maximum number of retries

am_hal_uart_config_t g_sUartConfig =
{
    // Standard UART settings: 115200-8-N-1
    .ui32BaudRate = 115200,
    .eDataBits = AM_HAL_UART_DATA_BITS_8,
    .eParity = AM_HAL_UART_PARITY_NONE,
    .eStopBits = AM_HAL_UART_ONE_STOP_BIT,
    .eFlowControl = AM_HAL_UART_FLOW_CTRL_NONE,
    .eTXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16,
    .eRXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16,
};

nsx_uart_transaction_t g_sUartTransaction =
{
    .status = 0,
};

static uint32_t nsx_uart_configure_pin(const nsx_uart_pin_config_t *pin)
{
    if (pin == NULL || pin->config == NULL) {
        return AM_HAL_STATUS_SUCCESS;
    }
    return am_hal_gpio_pinconfig(pin->pin, *pin->config);
}

static IRQn_Type nsx_uart_irqn(uint32_t instance) { return (IRQn_Type)(UART0_IRQn + instance); }

static void nsx_uart_irq_handler(void *ctx)
{
    (void)ctx;
    nsx_uart_handle_isr();
}

void nsx_uart_handle_isr(void)
{
    // // Service the FIFOs as necessary, and clear the interrupts.
    uint32_t ui32Status;
    am_hal_uart_interrupt_status_get(nsx_uart_hal_handle, &ui32Status, true);
    am_hal_uart_interrupt_clear(nsx_uart_hal_handle, ui32Status);
    am_hal_uart_interrupt_service(nsx_uart_hal_handle, ui32Status);

    nsx_uart_config_t * ctx = &nsx_uart_config;

    // Set the data available flag if RX interrupt is set
    if (ui32Status & (AM_HAL_UART_INT_RX | AM_HAL_UART_INT_RX_TMOUT))
    {
        g_sUartTransaction.status = AM_HAL_UART_INT_RX;
        if (ctx->rx_cb != NULL)
        {
            ctx->rx_cb(&g_sUartTransaction);
        }
        g_nsx_uart_data_available = true;
    }
    else if (ui32Status & AM_HAL_UART_INT_TX)
    {
        g_sUartTransaction.status = AM_HAL_UART_INT_TX;
        if (ctx->tx_cb != NULL)
        {
            ctx->tx_cb(&g_sUartTransaction);
        }
    }
}

// non blocking callback function
void uart_done(uint32_t ui32ErrorStatus, void *pvContext)
{
    g_bUARTdone = true;
}

uint32_t nsx_uart_platform_init(nsx_uart_config_t *cfg)
{
    NSX_TRY(am_hal_uart_initialize(cfg->instance, &nsx_uart_hal_handle), "Failed to initialize the UART");
    NSX_TRY(am_hal_uart_power_control(nsx_uart_hal_handle, AM_HAL_SYSCTRL_WAKE, false), "Failed to power the UART");
    NSX_TRY(am_hal_uart_configure(nsx_uart_hal_handle, cfg->uart_config),  "Failed to configure the UART");

    NSX_TRY(nsx_uart_configure_pin(cfg->tx_pin), "Failed to configure UART TX pin");
    NSX_TRY(nsx_uart_configure_pin(cfg->rx_pin), "Failed to configure UART RX pin");

    if (cfg->enable_interrupts) {
        nsx_irq_config_t irq_cfg = {
            .api = &nsx_interrupt_current_version,
            .irqn = nsx_uart_irqn(cfg->instance),
            .handler = nsx_uart_irq_handler,
            .ctx = cfg,
            .priority = AM_IRQ_PRIORITY_DEFAULT,
            .enable = true,
        };
        NSX_TRY(nsx_irq_register(&irq_cfg), "Failed to register UART interrupt");
        am_hal_uart_interrupt_enable(nsx_uart_hal_handle, (AM_HAL_UART_INT_RX | AM_HAL_UART_INT_TX | AM_HAL_UART_INT_RX_TMOUT));
    }
    return AM_HAL_STATUS_SUCCESS;
}

uint32_t nsx_uart_blocking_send_data(nsx_uart_config_t* cfg, char * txBuffer, uint32_t size) {
    uint32_t ui32BytesWritten = 0;
    uint32_t retries = MAX_UART_RETRIES;
    uint32_t status = AM_HAL_STATUS_SUCCESS;
    while(retries > 0) {
        const am_hal_uart_transfer_t sUartWrite =
        {
            .eType = AM_HAL_UART_BLOCKING_WRITE,
            .pui8Data = (uint8_t *)txBuffer,
            .ui32NumBytes = size,
            .pui32BytesTransferred = &ui32BytesWritten,
            .ui32TimeoutMs = 1000,
            .pfnCallback = &uart_done,
            .pvContext = NULL,
            .ui32ErrorStatus = 0
        };
        status = am_hal_uart_transfer(nsx_uart_hal_handle, &sUartWrite);
        if (status == AM_HAL_STATUS_SUCCESS && ui32BytesWritten == size) {
            // Successfully sent the whole string
            am_hal_uart_tx_flush(nsx_uart_hal_handle);
            return AM_HAL_STATUS_SUCCESS;
        }
        retries--;
    }
    // If we reach here, it means send operation failed all retries
    nsx_printf("[ERROR] nsx_uart_send_data exhausted retries\n");
    return status;
}


uint32_t nsx_uart_nonblocking_send_data(nsx_uart_config_t* cfg, char * txBuffer, uint32_t size) {
    uint32_t ui32BytesWritten = 0;
    uint32_t retries = MAX_UART_RETRIES;
    uint32_t status = AM_HAL_STATUS_SUCCESS;
    while(retries > 0) {
        const am_hal_uart_transfer_t sUartWrite =
        {
            .eType = AM_HAL_UART_NONBLOCKING_WRITE,
            .pui8Data = (uint8_t *)txBuffer,
            .ui32NumBytes = size,
            .pui32BytesTransferred = &ui32BytesWritten,
            .ui32TimeoutMs = 0,
            .pfnCallback = &uart_done,
            .pvContext = NULL,
            .ui32ErrorStatus = 0
        };
        status = am_hal_uart_transfer(nsx_uart_hal_handle, &sUartWrite);
        if (status == AM_HAL_STATUS_SUCCESS && ui32BytesWritten == size) {
            // Successfully sent the whole string
            am_hal_uart_tx_flush(nsx_uart_hal_handle);
            return AM_HAL_STATUS_SUCCESS;
        }
        retries--;
    }
    // If we reach here, it means send operation failed all retries
    nsx_printf("[ERROR] nsx_uart_send_data exhausted retries\n");
    return status;
}


uint32_t nsx_uart_blocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size) {
    uint32_t retries = MAX_UART_RETRIES;
    uint32_t status = AM_HAL_STATUS_SUCCESS;
    uint32_t ui32BytesRead = 0;
    while (retries > 0) {
        const am_hal_uart_transfer_t sUartRead =
        {
            .eType = AM_HAL_UART_BLOCKING_READ, // uses blocking read
            .pui8Data = (uint8_t *)rxBuffer,
            .ui32NumBytes = size,
            .pui32BytesTransferred = &ui32BytesRead,
            .ui32TimeoutMs = 1000,
            .pfnCallback = &uart_done,
            .pvContext = NULL,
            .ui32ErrorStatus = ui32LastError
        };
        status = am_hal_uart_transfer(nsx_uart_hal_handle, &sUartRead);
        if (status == AM_HAL_STATUS_SUCCESS && ui32BytesRead == size) {
            // Successfully read the whole string
            g_nsx_uart_data_available = false; // Clear the data available flag
            return AM_HAL_STATUS_SUCCESS;
        }
        retries--;
    }
    if(ui32BytesRead < size) {
        nsx_printf("[ERROR] RX error, asked for %d, got %d\n", size, ui32BytesRead);
    }
    // If we reach here, it means receive operation failed all retries
    nsx_printf("[ERROR] nsx_uart_receive_data exhausted retries\n");
    return status;
}



uint32_t nsx_uart_nonblocking_receive_data(nsx_uart_config_t *cfg, char * rxBuffer, uint32_t size) {
    uint32_t retries = MAX_UART_RETRIES;
    uint32_t status = AM_HAL_STATUS_SUCCESS;
    uint32_t ui32BytesRead = 0;
    while (retries > 0) {
        const am_hal_uart_transfer_t sUartRead =
        {
            .eType = AM_HAL_UART_NONBLOCKING_READ, // uses blocking read
            .pui8Data = (uint8_t *)rxBuffer,
            .ui32NumBytes = size,
            .pui32BytesTransferred = &ui32BytesRead,
            .ui32TimeoutMs = 0,
            .pfnCallback = &uart_done,
            .pvContext = NULL,
            .ui32ErrorStatus = ui32LastError
        };
        status = am_hal_uart_transfer(nsx_uart_hal_handle, &sUartRead);
        if (status == AM_HAL_STATUS_SUCCESS && ui32BytesRead == size) {
            // Successfully read the whole string
            g_nsx_uart_data_available = false; // Clear the data available flag
            return AM_HAL_STATUS_SUCCESS;
        }
        retries--;
    }
    if(ui32BytesRead < size) {
        nsx_printf("[ERROR] RX error, asked for %d, got %d\n", size, ui32BytesRead);
    }
    // If we reach here, it means receive operation failed all retries
    nsx_printf("[ERROR] nsx_uart_receive_data exhausted retries\n");
    return status;
}
