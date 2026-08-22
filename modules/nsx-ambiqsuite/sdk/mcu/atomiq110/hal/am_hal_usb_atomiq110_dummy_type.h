//*****************************************************************************
//
//! @file am_hal_usb_atomiq110_dummy_type.h
//!
//! @brief Workaround: Local USB_Type for Atomiq110 until CMSIS is updated.
//!
//! The HAL (am_hal_usb.c) expects register names (USBIRQ, SETUPDAT, etc.)
//! that are not present in the current atomiq110_generic.h USB_Type (which
//! uses CFG0, CFG1, IDX0, etc.). This header provides a struct with the
//! expected member names so the HAL compiles.
//!
//! When the official USB_Type for Atomiq110 is updated in CMSIS to the
//! latest layout, remove the use of this header from am_mcu_apollo.h and
//! delete this file.
//!
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_HAL_USB_ATOMIQ110_DUMMY_TYPE_H_
#define AM_HAL_USB_ATOMIQ110_DUMMY_TYPE_H_

#ifdef AM_PART_ATOMIQ110_USB_DUMMY_TYPE

#include <stdint.h>

#ifndef __IOM
#define __IOM volatile
#endif

typedef struct
{
    __IOM uint8_t  USBIRQ;
    __IOM uint8_t  EXTIRQ;   /* Cadence extirq @ 0x18D (external/wakeup IRQ) */
    __IOM uint8_t  IN07IRQ;
    __IOM uint8_t  IN815IRQ;
    __IOM uint8_t  OUT07IRQ;
    __IOM uint8_t  OUT815IRQ;
    __IOM uint8_t  USBIEN;
    __IOM uint8_t  EXTIEN;   /* Cadence extien @ 0x194 (external interrupt enable) */
    uint8_t        _pad0[1];
    __IOM uint8_t  IN07IEN;
    __IOM uint8_t  IN815IEN;
    __IOM uint8_t  OUT07IEN;
    __IOM uint8_t  OUT815IEN;
    __IOM uint8_t  SETUPDAT[8];
    __IOM uint32_t FIFO1DAT;   /* FIFO1DAT, FIFO2DAT = FIFO1DAT+1, etc. */
    __IOM uint8_t  OUT0BC;
    __IOM uint8_t  IN0BC;
    uint8_t        _pad1[2];
    __IOM uint32_t IN1CS;      /* IN1CS, IN2CS, ... (array of 5) */
    __IOM uint32_t IN2CS;
    __IOM uint32_t IN3CS;
    __IOM uint32_t IN4CS;
    __IOM uint32_t IN5CS;
    __IOM uint8_t  OUT1CS;   /* OUT (HCIN) X endpoint control and status (Cadence outxcs) */
    __IOM uint8_t  OUT2CS;
    __IOM uint8_t  OUT3CS;
    __IOM uint8_t  OUT4CS;
    __IOM uint8_t  OUT5CS;
    __IOM uint32_t OUT1CON;
    __IOM uint32_t OUT2CON;
    __IOM uint32_t OUT3CON;
    __IOM uint32_t OUT4CON;
    __IOM uint32_t OUT5CON;
    __IOM uint32_t IN1CON;
    __IOM uint32_t IN2CON;
    __IOM uint32_t IN3CON;
    __IOM uint32_t IN4CON;
    __IOM uint32_t IN5CON;
    __IOM uint32_t IN1STADDR;
    __IOM uint32_t IN2STADDR;
    __IOM uint32_t IN3STADDR;
    __IOM uint32_t IN4STADDR;
    __IOM uint32_t IN5STADDR;
    __IOM uint32_t OUT1STADDR;
    __IOM uint32_t OUT2STADDR;
    __IOM uint32_t OUT3STADDR;
    __IOM uint32_t OUT4STADDR;
    __IOM uint32_t OUT5STADDR;
    __IOM uint8_t  EP0CS;
    uint8_t        _pad2[3];
    __IOM uint8_t  IN1MAXPCK[10];  /* 2 bytes per EP for EP1..EP5 */
    __IOM uint8_t  OUT1MAXPCK[10];
    __IOM uint8_t  OUT1BCL[5];     /* OUT byte count low for EP1..EP5 */
    __IOM uint8_t  OUT1BCH[5];     /* OUT byte count high for EP1..EP5 */
    __IOM uint16_t OUT0MAXPCK;
    __IOM uint8_t  FNADDR;   /* Function Address (Cadence fnaddr) */
    uint8_t        _pad4[3];
    __IOM uint32_t ENDPRST;
    __IOM uint8_t  USBCS;    /* USB Control/Status register (Cadence usbcs) */
    uint8_t        _pad5[3];
    __IOM uint8_t  FRMNRL;   /* Frame Number Low byte (Cadence frmnrl) */
    __IOM uint8_t  FRMNRH;   /* Frame Number High byte (Cadence frmnrh) */
    uint8_t        _pad6[1];
    __IOM uint8_t  CLKGATE;   /* 0x1A7 clock-gate control (write only in CUSB2 SFR) */
    __IOM uint8_t  FIFOCTRL;  /* 0x1A8 FIFO control (fifoauto/fifocmit/fifoacc) */
    __IOM uint8_t  SPEEDCTRL; /* Speed Control register (Cadence speedctrl) */
    uint8_t        _pad7[3];
    __IOM uint8_t  EP0INDAT[64];  /* EP0 IN data buffer (Cadence ep0indat, 64 bytes) */
    __IOM uint8_t  EP0OUTDAT[64]; /* EP0 OUT data buffer (Cadence ep0outdat, 64 bytes) */
    uint8_t        _pad_adma[691];   /* pad so ADMA block starts at 0x400 (appendix_ADMA_special_function_registers.pdf Table 34) */
    __IOM uint32_t DMA_CONF;        /* 0x400 ADMA global configuration */
    __IOM uint32_t DMA_STS;         /* 0x404 ADMA status */
    uint8_t        _pad_dma0[0x18]; /* 0x408-0x41B reserved */
    __IOM uint32_t DMA_EP_SEL;      /* 0x41C endpoint select (DIR, EPNO) */
    union
    {
        __IOM uint32_t DMA_EP_TRADDR; /* 0x420 selected endpoint transfer ring address */
        struct
        {
            __IOM uint32_t TRADDR0;
            __IOM uint32_t TRADDR1;
            __IOM uint32_t TRADDR2;
            __IOM uint32_t TRADDR3;
            __IOM uint32_t TRADDR4;
            __IOM uint32_t TRADDR5;
            __IOM uint32_t TRADDR6;
            __IOM uint32_t TRADDR7;
        };
    };
    __IOM uint32_t DMA_EP_CFG;      /* 0x424 endpoint config (ENABLE, DSING) */
    __IOM uint32_t DMA_EP_CMD;      /* 0x428 endpoint command (DRDY) */
    __IOM uint32_t DMA_EP_STS;      /* 0x42C endpoint status (IOC, ISP, TRBERR, OUTSMM) */
    uint8_t        _pad_dma1[4];    /* 0x430-0x433 reserved */
    __IOM uint32_t DMA_EP_STS_EN;   /* 0x434 endpoint status enable */
    __IOM uint32_t DMA_DRBL;        /* 0x438 doorbell (OUT [15:0], IN [31:16]) */
    __IOM uint32_t DMA_EP_IEN;      /* 0x43C endpoint interrupt enable */
    __IOM uint32_t DMA_EP_ISTS;     /* 0x440 endpoint interrupt status */
    uint8_t        _pad3[8130];     /* remainder; adjust when official layout is known */
} am_hal_usb_atomiq110_dummy_type_t;

#define USB_Type am_hal_usb_atomiq110_dummy_type_t

#endif /* AM_PART_ATOMIQ110_USB_DUMMY_TYPE */

#endif /* AM_HAL_USB_ATOMIQ110_DUMMY_TYPE_H_ */
