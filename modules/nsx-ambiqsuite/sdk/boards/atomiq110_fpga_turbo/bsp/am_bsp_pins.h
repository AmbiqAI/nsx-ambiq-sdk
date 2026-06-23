//*****************************************************************************
//
//  am_bsp_pins.h
//! @file
//!
//! @brief BSP pin configuration definitions.
//!
//! @addtogroup BSP Board Support Package (BSP)
//! @ingroup BSP
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_BSP_PINS_H
#define AM_BSP_PINS_H

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// COM_UART_TX pin: This pin is COM_UART transmit.
//
//*****************************************************************************
#define AM_BSP_GPIO_COM_UART_TX             66
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_COM_UART_TX;

//*****************************************************************************
//
// COM_UART_RX pin: This pin is COM_UART receive.
//
//*****************************************************************************
#define AM_BSP_GPIO_COM_UART_RX             67
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_COM_UART_RX;

//*****************************************************************************
//
// COM_UART_CTS pin: This pin is the COM_UART CTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_COM_UART_CTS            68
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_COM_UART_CTS;

//*****************************************************************************
//
// COM_UART_RTS pin: This pin is the COM_UART RTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_COM_UART_RTS            65
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_COM_UART_RTS;

//*****************************************************************************
//
// UART0_TX pin: This pin is UART0 transmit.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART0_TX                66
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART0_TX;

//*****************************************************************************
//
// UART0_RX pin: This pin is UART0 receive.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART0_RX                67
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART0_RX;

//*****************************************************************************
//
// UART0_CTS pin: This pin is the UART0 CTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART0_CTS               68
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART0_CTS;

//*****************************************************************************
//
// UART0_RTS pin: This pin is the UART0 RTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART0_RTS               65
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART0_RTS;

//*****************************************************************************
//
// UART1_TX pin: This pin is UART1 transmit.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART1_TX                41
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART1_TX;

//*****************************************************************************
//
// UART1_RX pin: This pin is UART1 receive.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART1_RX                42
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART1_RX;

//*****************************************************************************
//
// UART1_CTS pin: This pin is the UART1 CTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART1_CTS               43
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART1_CTS;

//*****************************************************************************
//
// UART1_RTS pin: This pin is the UART1 RTS.
//
//*****************************************************************************
#define AM_BSP_GPIO_UART1_RTS               44
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_UART1_RTS;

//*****************************************************************************
//
// IOM0_CS pin: I/O Master 0 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_CS                 14
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_CS;
#define AM_BSP_IOM0_CS_CHNL                 0

//*****************************************************************************
//
// IOM0_MISO pin: I/O Master 0 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_MISO               7
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_MISO;

//*****************************************************************************
//
// IOM0_SPI_DATA pin: I/O Master 0 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_SPI_DATA           6
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_SPI_DATA;

//*****************************************************************************
//
// IOM0_MOSI pin: I/O Master 0 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_MOSI               6
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_MOSI;

//*****************************************************************************
//
// IOM0_SCK pin: I/O Master 0 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_SCK                5
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_SCK;

//*****************************************************************************
//
// IOM0_SCL pin: I/O Master 0 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_SCL                5
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_SCL;

//*****************************************************************************
//
// IOM0_SDA pin: I/O Master 0 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM0_SDA                6
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM0_SDA;

//*****************************************************************************
//
// IOM1_CS pin: I/O Master 1 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_CS                 12
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_CS;
#define AM_BSP_IOM1_CS_CHNL                 0

//*****************************************************************************
//
// IOM1_MISO pin: I/O Master 1 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_MISO               24
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_MISO;

//*****************************************************************************
//
// IOM1_SPI_DATA pin: I/O Master 1 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_SPI_DATA           23
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_SPI_DATA;

//*****************************************************************************
//
// IOM1_MOSI pin: I/O Master 1 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_MOSI               23
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_MOSI;

//*****************************************************************************
//
// IOM1_SCK pin: I/O Master 1 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_SCK                22
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_SCK;

//*****************************************************************************
//
// IOM1_SCL pin: I/O Master 1 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_SCL                22
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_SCL;

//*****************************************************************************
//
// IOM1_SDA pin: I/O Master 1 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM1_SDA                23
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM1_SDA;

//*****************************************************************************
//
// IOM2_CS pin: I/O Master 2 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_CS                 58
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_CS;
#define AM_BSP_IOM2_CS_CHNL                 0

//*****************************************************************************
//
// IOM2_MISO pin: I/O Master 2 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_MISO               27
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_MISO;

//*****************************************************************************
//
// IOM2_SPI_DATA pin: I/O Master 2 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_SPI_DATA           26
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_SPI_DATA;

//*****************************************************************************
//
// IOM2_MOSI pin: I/O Master 2 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_MOSI               26
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_MOSI;

//*****************************************************************************
//
// IOM2_SCK pin: I/O Master 2 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_SCK                25
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_SCK;

//*****************************************************************************
//
// IOM2_SCL pin: I/O Master 2 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_SCL                25
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_SCL;

//*****************************************************************************
//
// IOM2_SDA pin: I/O Master 2 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM2_SDA                26
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM2_SDA;

//*****************************************************************************
//
// IOM3_CS pin: I/O Master 3 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_CS                 44
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_CS;
#define AM_BSP_IOM3_CS_CHNL                 0

//*****************************************************************************
//
// IOM3_MISO pin: I/O Master 3 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_MISO               33
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_MISO;

//*****************************************************************************
//
// IOM3_MOSI pin: I/O Master 3 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_MOSI               32
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_MOSI;

//*****************************************************************************
//
// IOM3_SPI_DATA pin: I/O Master 3 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_SPI_DATA           32
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_SPI_DATA;

//*****************************************************************************
//
// IOM3_SCK pin: I/O Master 3 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_SCK                31
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_SCK;

//*****************************************************************************
//
// IOM3_SCL pin: I/O Master 3 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_SCL                31
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_SCL;

//*****************************************************************************
//
// IOM3_SDA pin: I/O Master 3 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM3_SDA                32
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM3_SDA;

//*****************************************************************************
//
// IOM4_CS pin: I/O Master 4 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_CS                 13
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_CS;
#define AM_BSP_IOM4_CS_CHNL                 0

//*****************************************************************************
//
// IOM4_MISO pin: I/O Master 4 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_MISO               36
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_MISO;

//*****************************************************************************
//
// IOM4_SPI_DATA pin: I/O Master 4 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_SPI_DATA           35
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_SPI_DATA;

//*****************************************************************************
//
// IOM4_MOSI pin: I/O Master 4 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_MOSI               35
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_MOSI;

//*****************************************************************************
//
// IOM4_SCK pin: I/O Master 4 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_SCK                34
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_SCK;

//*****************************************************************************
//
// IOM4_SCL pin: I/O Master 4 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_SCL                34
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_SCL;

//*****************************************************************************
//
// IOM4_SDA pin: I/O Master 4 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM4_SDA                35
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM4_SDA;

//*****************************************************************************
//
// IOM5_CS pin: I/O Master 5 chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_CS                 60
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_CS;
#define AM_BSP_IOM5_CS_CHNL                 0

//*****************************************************************************
//
// IOM5_MISO pin: I/O Master 5 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_MISO               49
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_MISO;

//*****************************************************************************
//
// IOM5_SPI_DATA pin: I/O Master 5 SPI Data I/O(3 wire) signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_SPI_DATA           48
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_SPI_DATA;

//*****************************************************************************
//
// IOM5_MOSI pin: I/O Master 5 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_MOSI               48
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_MOSI;

//*****************************************************************************
//
// IOM5_SCK pin: I/O Master 5 SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_SCK                47
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_SCK;

//*****************************************************************************
//
// IOM5_SCL pin: I/O Master 5 I2C clock signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_SCL                47
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_SCL;

//*****************************************************************************
//
// IOM5_SDA pin: I/O Master 5 I2C data signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOM5_SDA                48
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOM5_SDA;

//*****************************************************************************
//
// ACCEL_CS pin: Accelerometer I/O Master 5 SPI chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_CS                98
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_CS;
#define AM_BSP_ACCEL_CS_CHNL                0

//*****************************************************************************
//
// ACCEL_MISO pin: Accelerometer I/O Master 5 SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_MISO              49
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_MISO;

//*****************************************************************************
//
// ACCEL_MOSI pin: Accelerometer I/O Master 5 SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_MOSI              48
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_MOSI;

//*****************************************************************************
//
// ACCEL_SCK pin: Accelerometer I/O Master 5 SPI CLK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_SCK               47
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_SCK;

//*****************************************************************************
//
// ACCEL_INT1 pin: Accelerometer interrupt 1.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_INT1              99
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_INT1;

//*****************************************************************************
//
// ACCEL_INT2 pin: Accelerometer interrupt 2.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_INT2              100
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_INT2;

//*****************************************************************************
//
// ACCEL_INT2_PWM pin: Accelerometer interrupt 2 PWM.
//
//*****************************************************************************
#define AM_BSP_GPIO_ACCEL_INT2_PWM          100
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ACCEL_INT2_PWM;

//*****************************************************************************
//
// IOSFD0_CE pin: I/O Slave Full Duplex chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD0_CE               3
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD0_CE;

//*****************************************************************************
//
// IOSFD0_MOSI pin: I/O Slave Full Duplex SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD0_MOSI             1
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD0_MOSI;

//*****************************************************************************
//
// IOSFD0_MISO pin: I/O Slave Full Duplex SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD0_MISO             2
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD0_MISO;

//*****************************************************************************
//
// IOSFD0_SCK pin: I/O Slave Full Duplex SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD0_SCK              0
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD0_SCK;

//*****************************************************************************
//
// IOSFD0_INT pin: I/O Slave Full Duplex interrupt.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD0_INT              4
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD0_INT;

//*****************************************************************************
//
// IOSFD1_CE pin: I/O Slave Full Duplex chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD1_CE               28
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD1_CE;

//*****************************************************************************
//
// IOSFD1_MOSI pin: I/O Slave Full Duplex SPI MOSI signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD1_MOSI             23
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD1_MOSI;

//*****************************************************************************
//
// IOSFD1_MISO pin: I/O Slave Full Duplex SPI MISO signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD1_MISO             24
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD1_MISO;

//*****************************************************************************
//
// IOSFD1_SCK pin: I/O Slave Full Duplex SPI SCK signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD1_SCK              22
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD1_SCK;

//*****************************************************************************
//
// IOSFD1_INT pin: I/O Slave Full Duplex interrupt.
//
//*****************************************************************************
#define AM_BSP_GPIO_IOSFD1_INT              29
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_IOSFD1_INT;

//*****************************************************************************
//
// ITM_SWO pin: ITM Serial Wire Output.
//
//*****************************************************************************
#define AM_BSP_GPIO_ITM_SWO                 28
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ITM_SWO;

//*****************************************************************************
//
// I2S0_DATA pin: I2S0 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S0_DATA               12
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S0_DATA;

//*****************************************************************************
//
// I2S0_SDOUT pin: I2S0 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S0_SDOUT              12
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S0_SDOUT;

//*****************************************************************************
//
// I2S0_SDIN pin: I2S0 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S0_SDIN               14
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S0_SDIN;

//*****************************************************************************
//
// I2S0_CLK pin: I2S0 Bit Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S0_CLK                11
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S0_CLK;

//*****************************************************************************
//
// I2S0_WS pin: I2S0 L/R Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S0_WS                 13
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S0_WS;

//*****************************************************************************
//
// I2S1_DATA pin: I2S1 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S1_DATA               17
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S1_DATA;

//*****************************************************************************
//
// I2S1_SDOUT pin: I2S1 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S1_SDOUT              17
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S1_SDOUT;

//*****************************************************************************
//
// I2S1_SDIN pin: I2S1 Bidirectional Data.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S1_SDIN               19
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S1_SDIN;

//*****************************************************************************
//
// I2S1_CLK pin: I2S1 Bit Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S1_CLK                16
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S1_CLK;

//*****************************************************************************
//
// I2S1_WS pin: I2S1 L/R Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_I2S1_WS                 18
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_I2S1_WS;

//*****************************************************************************
//
// PDM0_CLK pin: PDM 0 Clock Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM0_CLK                50
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM0_CLK;

//*****************************************************************************
//
// PDM0_DATA pin: PDM 0 Data Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM0_DATA               51
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM0_DATA;

//*****************************************************************************
//
// PDMI2S0_SDOUT pin: PDMI2S 0 Data Out Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDMI2S0_SDOUT           19
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDMI2S0_SDOUT;

//*****************************************************************************
//
// PDMI2S0_WS pin: PDMI2S 0 L/R Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDMI2S0_WS              18
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDMI2S0_WS;

//*****************************************************************************
//
// PDMI2S0_CLK pin: PDMI2S 0 Bit Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDMI2S0_CLK             16
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDMI2S0_CLK;

//*****************************************************************************
//
// PDM1_CLK pin: PDM 1 Clock Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM1_CLK                99
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM1_CLK;

//*****************************************************************************
//
// PDM1_DATA pin: PDM 1 Data Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM1_DATA               100
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM1_DATA;

//*****************************************************************************
//
// PDM2_CLK pin: PDM 2 Clock Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM2_CLK                101
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM2_CLK;

//*****************************************************************************
//
// PDM2_DATA pin: PDM 2 Data Signal.
//
//*****************************************************************************
#define AM_BSP_GPIO_PDM2_DATA               102
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PDM2_DATA;

//*****************************************************************************
//
// PSRAM_RESET pin: Winbond PSRAM Reset.
//
//*****************************************************************************
#define AM_BSP_GPIO_PSRAM_RESET             58
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_PSRAM_RESET;

//*****************************************************************************
//
// MSPI1_CE0 pin: MSPI1 chip select 0
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_CE0               42
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_CE0;

//*****************************************************************************
//
// MSPI1_CE1 pin: MSPI1 chip select 1
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_CE1               43
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_CE1;

//*****************************************************************************
//
// MSPI1_D0 pin: NOR Flash I/F using MSPI1 data 0.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_D0                37
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_D0;

//*****************************************************************************
//
// MSPI1_D1 pin: NOR Flash I/F using MSPI1 data 1.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_D1                38
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_D1;

//*****************************************************************************
//
// MSPI1_D2 pin: NOR Flash I/F using MSPI1 data 2.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_D2                39
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_D2;

//*****************************************************************************
//
// MSPI1_D3 pin: NOR Flash I/F using MSPI1 data 3.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_D3                40
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_D3;

//*****************************************************************************
//
// MSPI1_SCK pin: NOR Flash I/F using MSPI1 clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_SCK               79
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_SCK;

//*****************************************************************************
//
// MSPI1_DQSDM pin: NOR Flash I/F using MSPI1 DQS.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_DQSDM             80
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_DQSDM;

//*****************************************************************************
//
// MSPI1_RST pin: NOR Flash Reset.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI1_RST               119
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI1_RST;

//*****************************************************************************
//
// MSPI2_CE0 pin: MSPI2 chip select 0
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_CE0               158
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_CE0;

//*****************************************************************************
//
// MSPI2_CE1 pin: MSPI2 chip select 1
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_CE1               157
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_CE1;

//*****************************************************************************
//
// MSPI2_D0 pin: NOR Flash I/F using MSPI2 data 0.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D0                56
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D0;

//*****************************************************************************
//
// MSPI2_D1 pin: NOR Flash I/F using MSPI2 data 1.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D1                57
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D1;

//*****************************************************************************
//
// MSPI2_D2 pin: NOR Flash I/F using MSPI2 data 2.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D2                58
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D2;

//*****************************************************************************
//
// MSPI2_D3 pin: NOR Flash I/F using MSPI2 data 3.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D3                59
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D3;

//*****************************************************************************
//
// XSPI2_D4 pin: NOR Flash I/F using MSPI2 data 4.
//
//*****************************************************************************
#define AM_BSP_GPIO_XSPI2_D4                94
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_XSPI2_D4;

//*****************************************************************************
//
// MSPI2_D5 pin: NOR Flash I/F using MSPI2 data 5.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D5                95
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D5;

//*****************************************************************************
//
// MSPI2_D6 pin: NOR Flash I/F using MSPI2 data 6.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D6                96
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D6;

//*****************************************************************************
//
// MSPI2_D7 pin: NOR Flash I/F using MSPI2 data 7.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D7                97
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D7;

//*****************************************************************************
//
// MSPI2_SCK pin: NOR Flash I/F using MSPI2 clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_SCK               60
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_SCK;

//*****************************************************************************
//
// MSPI2_DQSDM pin: NOR Flash I/F using MSPI2 DQS.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_DQSDM             98
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_DQSDM;

//*****************************************************************************
//
// MSPI2_RST pin: NOR Flash Reset.
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_RST               119
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_RST;

//*****************************************************************************
//
// MSPI2_D4_CLK pin: MSPI2 data 4 replace MSPI2_8(CLK).
//
//*****************************************************************************
#define AM_BSP_GPIO_MSPI2_D4_CLK            94
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI2_D4_CLK;

//*****************************************************************************
//
// SDIO0_DAT0 pin: eMMC0 I/F using SDIO data 0.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT0              50
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT0;

//*****************************************************************************
//
// SDIO0_DAT1 pin: eMMC0 I/F using SDIO data 1.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT1              51
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT1;

//*****************************************************************************
//
// SDIO0_DAT2 pin: eMMC0 I/F using SDIO data 2.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT2              52
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT2;

//*****************************************************************************
//
// SDIO0_DAT3 pin: eMMC0 I/F using SDIO data 3.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT3              53
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT3;

//*****************************************************************************
//
// SDIO0_CLK pin: eMMC0 I/F using SDIO clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_CLK               54
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_CLK;

//*****************************************************************************
//
// SDIO0_CMD pin: eMMC0 I/F using SDIO CMD
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_CMD               55
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_CMD;

//*****************************************************************************
//
// SDIO0_DAT4 pin: eMMC0 I/F using SDIO data 4.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT4              145
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT4;

//*****************************************************************************
//
// SDIO0_DAT5 pin: eMMC0 I/F using SDIO data 5.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT5              146
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT5;

//*****************************************************************************
//
// SDIO0_DAT6 pin: eMMC0 I/F using SDIO data 6.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT6              147
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT6;

//*****************************************************************************
//
// SDIO0_DAT7 pin: eMMC0 I/F using SDIO data 7.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_DAT7              148
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_DAT7;

//*****************************************************************************
//
// SDIO0_RST pin: eMMC0 I/F using GPIO for Reset.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_RST               90
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SDIO0_RST;

//*****************************************************************************
//
// SD0_CD pin: SD Card using GPIO for card detection.
//
//*****************************************************************************
#define AM_BSP_GPIO_SD0_CD                  150
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SD0_CD;

//*****************************************************************************
//
// SD0_WP pin: SD Card using GPIO for write protection.
//
//*****************************************************************************
#define AM_BSP_GPIO_SD0_WP                  151
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SD0_WP;

//*****************************************************************************
//
// SD_LEVEL_SHIFT_SEL pin: SD Card using GPIO for switching supply voltage.
//
//*****************************************************************************
#define AM_BSP_GPIO_SD_LEVEL_SHIFT_SEL      16
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_SD_LEVEL_SHIFT_SEL;

//*****************************************************************************
//
// DISP_SPI_SD pin: Display SPI Data In/Out (3-wire).
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_SPI_SD             0
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_SPI_SD;

//*****************************************************************************
//
// DISP_SPI_DCX pin: Display SPI DCx.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_SPI_DCX            1
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_SPI_DCX;

//*****************************************************************************
//
// DISP_SPI_SCK pin: Display SPI Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_SPI_SCK            2
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_SPI_SCK;

//*****************************************************************************
//
// DISP_SPI_CS_N pin: Display SPI CS.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_SPI_CS_N           101
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_SPI_CS_N;

//*****************************************************************************
//
// DISP_QSPI_D0 pin: Display QSPI Data Out 0.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_D0            0
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_D0;

//*****************************************************************************
//
// DISP_QSPI_D1 pin: Display QSPI Data Out 1.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_D1            1
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_D1;

//*****************************************************************************
//
// DISP_QSPI_D2 pin: Display QSPI Data Out 2.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_D2            3
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_D2;

//*****************************************************************************
//
// DISP_QSPI_D3 pin: Display QSPI Data Out 3.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_D3            4
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_D3;

//*****************************************************************************
//
// DISP_QSPI_SCK pin: Display QSPI Clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_SCK           2
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_SCK;

//*****************************************************************************
//
// DISP_QSPI_CS_N pin: Display CSX chip select.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_QSPI_CS_N          101
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_QSPI_CS_N;

//*****************************************************************************
//
// DISP_GEN pin: GATE ENABLE SIGNAL
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_GEN                105
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_GEN;

//*****************************************************************************
//
// DISP_INTB pin: INITIAL SIGNAL FOR BINARY/GATE DRIVER
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_INTB               106
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_INTB;

//*****************************************************************************
//
// DISP_R1 pin: RED OF ODD PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_R1                 107
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_R1;

//*****************************************************************************
//
// DISP_R2 pin: RED OF EVEN PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_R2                 108
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_R2;

//*****************************************************************************
//
// DISP_G1 pin: GREEN OF ODD PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_G1                 109
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_G1;

//*****************************************************************************
//
// DISP_G2 pin: GREEN OF EVEN PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_G2                 110
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_G2;

//*****************************************************************************
//
// DISP_B1 pin: BLUE OF ODD PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_B1                 111
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_B1;

//*****************************************************************************
//
// DISP_B2 pin: BLUE OF EVEN PIXELS
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_B2                 112
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_B2;

//*****************************************************************************
//
// DISP_BSP pin: START SIGNAL OF BINARY DRIVER
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_BSP                113
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_BSP;

//*****************************************************************************
//
// DISP_GSP pin: START SIGNAL OF GATE DRIVER
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_GSP                114
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_GSP;

//*****************************************************************************
//
// DISP_BCK pin: CLOCK OF BINARY DRIVER
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_BCK                115
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_BCK;

//*****************************************************************************
//
// DISP_GCK pin: CLOCK OF GATE DRIVER
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_GCK                116
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_GCK;

//*****************************************************************************
//
// DISP_VA pin: OPPOSITE PHASE SIGNAL TO VCOM
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_VA                 117
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_VA;

//*****************************************************************************
//
// DISP_VB pin: INPHASE SIGNAL TO VCOM
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_VB                 118
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_VB;

//*****************************************************************************
//
// DISP_VCOM pin: PHASE SIGNAL TO VCOM
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_VCOM               119
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_VCOM;

//*****************************************************************************
//
// TOUCH_ALS_SDA pin: Touch ALS I/O Master I2C data.
//
//*****************************************************************************
#define AM_BSP_GPIO_TOUCH_ALS_SDA           48
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_TOUCH_ALS_SDA;

//*****************************************************************************
//
// TOUCH_ALS_SCL pin: Touch ALS I/O Master I2C clock.
//
//*****************************************************************************
#define AM_BSP_GPIO_TOUCH_ALS_SCL           47
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_TOUCH_ALS_SCL;

//*****************************************************************************
//
// TOUCH_INT pin: Touch panel interrupt.
//
//*****************************************************************************
#define AM_BSP_GPIO_TOUCH_INT               24
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_TOUCH_INT;

//*****************************************************************************
//
// DISP_DEVICE_TE pin: DISPLAY TE PIN
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_DEVICE_TE          10
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_DEVICE_TE;

//*****************************************************************************
//
// DISP_DEVICE_RST pin: Display Device Reset.
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_DEVICE_RST         90
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_DEVICE_RST;

//*****************************************************************************
//
// DISP_DSI_TE pin: defined it avoid error
//
//*****************************************************************************
#define AM_BSP_GPIO_DISP_DSI_TE             10
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_DISP_DSI_TE;

//*****************************************************************************
//
// CLKOUT pin: GPIO CLKOUT
//
//*****************************************************************************
#define AM_BSP_GPIO_CLKOUT                  33
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_CLKOUT;

//*****************************************************************************
//
// FLASH_RESET pin: external flash reset pin.
//
//*****************************************************************************
#define AM_BSP_GPIO_FLASH_RESET             119
extern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_FLASH_RESET;

#ifdef __cplusplus
}
#endif

#endif // AM_BSP_PINS_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
