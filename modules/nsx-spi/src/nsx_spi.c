
/**
 * @file nsx_spi.c
 * @author Ambiq
 * @brief Generic SPI driver
 * @version 0.1
 * @date 2022-08-26
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 */

#include <string.h>
#include "nsx_spi.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"

nsx_spi_config_t nsx_spi_config;

#define NSX_SPI_DMA_MAX_XFER_SIZE 4095
uint32_t nsx_spi_tcb_command_buffer[NSX_SPI_DMA_MAX_XFER_SIZE + 1];



//*****************************************************************************
//
//! IOM ISRs
//! Take over correct IOM ISR.
//
//*****************************************************************************
void nsx_spi_handle_iom_isr (void) {
    uint32_t ui32Status;
    // nsx_printf("IOM ISR\n");
    // uint32_t foo = am_hal_iom_interrupt_status_get(nsx_spi_config.iomHandle, true, &ui32Status);
    // nsx_printf("IOM1 ISR %d, status %d\n", foo, ui32Status);
    if (!am_hal_iom_interrupt_status_get(nsx_spi_config.iomHandle, true, &ui32Status)) {
        if (ui32Status) {
            am_hal_iom_interrupt_clear(nsx_spi_config.iomHandle, ui32Status);
            am_hal_iom_interrupt_service(nsx_spi_config.iomHandle, ui32Status);
        }
    }
}

uint32_t nsx_spi_interface_init(nsx_spi_config_t *cfg, uint32_t speed, am_hal_iom_spi_mode_e mode) {
    cfg->sIomCfg.pNBTxnBuf = NULL;
    cfg->sIomCfg.eInterfaceMode = AM_HAL_IOM_SPI_MODE;
    cfg->sIomCfg.eSpiMode = mode;
    cfg->sIomCfg.ui32NBTxnBufLength = 0;
    cfg->sIomCfg.ui32ClockFreq = speed;
    cfg->sIomCfg.ui32NBTxnBufLength = sizeof(nsx_spi_tcb_command_buffer) / sizeof(uint32_t);
    cfg->sIomCfg.pNBTxnBuf = nsx_spi_tcb_command_buffer;

    IRQn_Type gc_iomIrq = (IRQn_Type)(cfg->iom + IOMSTR0_IRQn);
    // nsx_printf("SPI Init IOM %d, gc_iomIrq %d\n", cfg->iom, gc_iomIrq);
    am_bsp_iom_pins_enable(cfg->iom, AM_HAL_IOM_SPI_MODE);

    // if (cfg->iom != NS_IOM_ISR) {
    //     nsx_printf("nsx_spi_interface_init: Configured IOM %d does not match NS_IOM_ISR\n", cfg->iom, NS_IOM_ISR);
    //     return NSX_SPI_STATUS_ERROR;
    // }

    if (am_hal_iom_initialize(cfg->iom, &(cfg->iomHandle)) ||
        am_hal_iom_power_ctrl(cfg->iomHandle, AM_HAL_SYSCTRL_WAKE, false) ||
        am_hal_iom_configure(cfg->iomHandle, &(cfg->sIomCfg))) {
        nsx_printf("nsx_spi_interface_init: Failed to initialize IOM %d\n", cfg->iom);
        return NSX_SPI_STATUS_ERROR;
    }
    am_hal_iom_enable(cfg->iomHandle);
    // Store config in global
    memcpy(&nsx_spi_config, cfg, sizeof(nsx_spi_config_t));

    // Enable the IOM interrupt
    // nsx_printf("IOM %d IRQ %d\n", cfg->iom, gc_iomIrq);
    NVIC_ClearPendingIRQ(gc_iomIrq);
    NVIC_EnableIRQ(gc_iomIrq);

    return NSX_SPI_STATUS_SUCCESS;
}

uint32_t nsx_spi_read(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin) {
    uint32_t err;
    am_hal_iom_transfer_t Transaction;
    Transaction.ui8Priority = 1;
    Transaction.ui32InstrLen = regLen;
    Transaction.ui64Instr = reg;
    Transaction.eDirection = AM_HAL_IOM_RX;
    Transaction.ui32NumBytes = bufLen;
    Transaction.pui32RxBuffer = (uint32_t *)buf;
    Transaction.bContinue = false;
    Transaction.ui8RepeatCount = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.uPeerInfo.ui32SpiChipSelect = csPin;
    // nsx_printf("SPI read of address 0x%llx, len %d\n", reg, regLen);
    err = am_hal_iom_blocking_transfer(cfg->iomHandle, &Transaction);
    if (err) {
        nsx_printf("SPI Read reg 0x%llx done err %d\n",reg, err);
        return err;
    }
    return NSX_SPI_STATUS_SUCCESS;
}

static void nsx_spi_dma_read_complete_cb(void *pCallbackCtxt, uint32_t ui32TransactionStatus) {
    (void)pCallbackCtxt;
    (void)ui32TransactionStatus;
    // nsx_printf("SPI DMA done\n");
    if (nsx_spi_config.cb) {
        nsx_spi_config.cb(&nsx_spi_config);
    }
}

uint32_t nsx_spi_read_dma(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin) {
    uint32_t err;
    am_hal_iom_transfer_t Transaction;
    Transaction.ui8Priority = 1;
    Transaction.ui32InstrLen = regLen;
    Transaction.ui64Instr = reg;
    Transaction.eDirection = AM_HAL_IOM_RX;
    Transaction.ui32NumBytes = bufLen;
    Transaction.pui32RxBuffer = (uint32_t *)buf;
    Transaction.bContinue = false;
    Transaction.ui8RepeatCount = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.uPeerInfo.ui32SpiChipSelect = csPin;
    // nsx_printf("SPI Read DMA len of %d to 0x%x IOM %d\n", bufLen, (uint32_t)buf, cfg->iom);
    err = am_hal_iom_nonblocking_transfer(
        cfg->iomHandle, &Transaction, nsx_spi_dma_read_complete_cb, 0);

    if (err) {
        nsx_printf("SPI Read DMA Error %d\n", err);
        return err;
    }

    return NSX_SPI_STATUS_SUCCESS;
}

uint32_t nsx_spi_write(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin) {
    am_hal_iom_transfer_t Transaction;

    Transaction.ui8Priority = 1;

    Transaction.ui32InstrLen = regLen;
    Transaction.ui64Instr = reg;
    Transaction.eDirection = AM_HAL_IOM_TX;
    Transaction.ui32NumBytes = bufLen;
    Transaction.pui32TxBuffer = (uint32_t *)buf;

    Transaction.bContinue = false;
    Transaction.ui8RepeatCount = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.uPeerInfo.ui32SpiChipSelect = csPin;
    // nsx_printf("SPI write of address 0x%llx\n", reg);

    if (am_hal_iom_blocking_transfer(cfg->iomHandle, &Transaction)) {
        return NSX_SPI_STATUS_ERROR;
    }

    return NSX_SPI_STATUS_SUCCESS;
}

uint32_t nsx_spi_transfer(
    nsx_spi_config_t *cfg, const void *txBuf, const void *rxBuf, uint32_t size, uint32_t csPin) {
    /**
     * @brief Full-duplex transfer !NOTE: Do not use for now
     *
     */
    am_hal_iom_transfer_t Transaction;
    Transaction.ui32InstrLen = 1;
    Transaction.ui64Instr = 0;
    Transaction.eDirection = AM_HAL_IOM_FULLDUPLEX;
    Transaction.ui32NumBytes = size;
    Transaction.pui32TxBuffer = (uint32_t *)txBuf;
    Transaction.pui32RxBuffer = (uint32_t *)rxBuf;
    Transaction.bContinue = false;
    Transaction.ui8RepeatCount = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.uPeerInfo.ui32SpiChipSelect = csPin;
    if (am_hal_iom_spi_blocking_fullduplex(cfg->iomHandle, &Transaction)) {
        return NSX_SPI_STATUS_ERROR;
    }
    return NSX_SPI_STATUS_SUCCESS;
}
