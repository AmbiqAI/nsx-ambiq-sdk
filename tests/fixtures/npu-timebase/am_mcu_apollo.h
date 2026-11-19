#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    STIMER_STCFG_CLKSEL_NOCLK,
    STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ,
    STIMER_STCFG_CLKSEL_HFRC_488_KHZ,
    STIMER_STCFG_CLKSEL_XTAL_32KHZ,
    STIMER_STCFG_CLKSEL_XTAL_8KHZ,
    STIMER_STCFG_CLKSEL_XTAL_2KHZ,
    STIMER_STCFG_CLKSEL_XTAL_1KHZ,
    STIMER_STCFG_CLKSEL_XTAL_512HZ,
    STIMER_STCFG_CLKSEL_LFRC_1KHZ
};

#define _FLD2VAL(field, value) field(value)
#define STIMER_STCFG_CLKSEL(value) ((value) & 0xFU)
#define STIMER_STCFG_FREEZE(value) (((value) >> 31) & 1U)
#define STIMER_STCFG_CLEAR(value) (((value) >> 30) & 1U)
#define STIMER_STCFG_FREEZE_THAW 0U
#define STIMER_STCFG_CLEAR_RUN 0U
#define AM_HAL_STIMER_CFG_RUN 0U
#define AM_HAL_STIMER_HFRC_488KHZ STIMER_STCFG_CLKSEL_HFRC_488_KHZ

typedef struct { uint32_t STCFG; } mock_stimer_t;
extern mock_stimer_t timer;
#define STIMER (&timer)

typedef enum { AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_CLKMGR_CLK_ID_XTAL_LS } am_hal_clkmgr_clock_id_e;
#define AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ 500000000U
#define AM_HAL_CLKMGR_DEFAULT_XTAL_LS_FREQ_HZ 32768U
#define AM_HAL_STATUS_SUCCESS 0U

bool am_hal_stimer_is_running(void);
uint32_t am_hal_stimer_counter_get(void);
uint32_t am_hal_stimer_config(uint32_t cfg);
void am_hal_delay_us(uint32_t us);
uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t level);
uint32_t am_hal_clkmgr_clock_config_get(am_hal_clkmgr_clock_id_e id, uint32_t *hz, void *source);
