#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifndef TEST_APOLLO3
#define AM_PART_APOLLO510 1
#endif
#define PDM0_IRQn 0
#define PDM_IRQn 0
#define PDM_DMATOTCOUNT_TOTCOUNT_Msk 0xfffffu
#define PDM0_DMATOTCOUNT_TOTCOUNT_Msk 0xfffffu
#define NVIC_ClearPendingIRQ(a) ((void)0)
#define NVIC_SetPriority(a,b) ((void)0)
#define NVIC_EnableIRQ(a) ((void)0)
#define NVIC_DisableIRQ(a) ((void)0)
typedef unsigned IRQn_Type;
typedef unsigned am_hal_pdm_gain_e;
typedef struct { unsigned eStepSize,bHighPassEnable,ui32HighPassCutoff,eLeftGain,eRightGain,bDataPacking,bPDMSampleDelay,ui32GainChangeDelay,bSoftMute,bLRSwap,ePDMClkSpeed,eClkDivider,ePDMAClkOutDivder,ui32DecimationRate,ePCMChannels,bInvertI2SBCLK,ePDMClkSource,bI2SEnable,bDoNotStartPdm; } am_hal_pdm_config_t;
typedef struct { unsigned ui32TotalCount,ui32TargetAddr,ui32TargetAddrReverse,ui32BufferWithDataAddr; bool bRestartDMA; } am_hal_pdm_transfer_t;
unsigned am_hal_pdm_initialize(unsigned,void**);
unsigned am_hal_pdm_power_control(void*,unsigned,bool);
unsigned am_hal_pdm_configure(void*,am_hal_pdm_config_t*);
unsigned am_hal_pdm_fifo_flush(void*);
unsigned am_hal_pdm_fifo_threshold_setup(void*,unsigned);
unsigned am_hal_pdm_interrupt_enable(void*,unsigned);
unsigned am_hal_pdm_interrupt_disable(void*,unsigned);
unsigned am_hal_pdm_interrupt_clear(void*,unsigned);
unsigned am_hal_pdm_interrupt_status_get(void*,uint32_t*,bool);
unsigned am_hal_pdm_enable(void*);
unsigned am_hal_pdm_disable(void*);
unsigned am_hal_pdm_deinitialize(void*);
unsigned am_hal_pdm_dma_start(void*,am_hal_pdm_transfer_t*);
unsigned am_hal_clkmgr_clock_config(unsigned,unsigned,void*);
#define AM_HAL_CLKGEN_CONTROL_HF2ADJ_ENABLE 11u
#define AM_HAL_CLKMGR_CLK_ID_HFRC2 12u
#define AM_HAL_CLKMGR_CLK_ID_PLLPOSTDIV 13u
#define AM_HAL_CLKMGR_CLK_ID_PLLVCO 14u
#define AM_HAL_CLKMGR_CLK_ID_SYSPLL 15u
#define AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_196P608MHZ 16u
#define AM_HAL_CLKMGR_SYPLL_FREF_PRIORITY_SET 17u
#define AM_HAL_MCUCTRL_CONTROL_EXTCLK32M_KICK_START 18u
#define AM_HAL_PDM_CHANNEL_LEFT 19u
#define AM_HAL_PDM_CHANNEL_STEREO 20u
#define AM_HAL_PDM_CLKOUT_DELAY_NONE 21u
#define AM_HAL_PDM_CLKOUT_PHSDLY_NONE 22u
#define AM_HAL_PDM_CLK_HFRC2_31MHZ 23u
#define AM_HAL_PDM_CLK_HFRC_24MHZ 24u
#define AM_HAL_PDM_CLK_HFXTAL 25u
#define AM_HAL_PDM_CLK_PLL 26u
#define AM_HAL_PDM_GAIN_STEP_0_13DB 27u
#define AM_HAL_PDM_HIGH_PASS_ENABLE 28u
#define AM_HAL_PDM_INT_DCMP 1u
#define AM_HAL_PDM_INT_DERR 2u
#define AM_HAL_PDM_INT_OVF 8u
#define AM_HAL_PDM_INT_UNDFL 4u
#define AM_HAL_PDM_MCLKDIV_1 33u
#define AM_HAL_PDM_MCLKDIV_3 34u
#define AM_HAL_PDM_PDMA_CLKO_DIV14 35u
#define AM_HAL_PDM_PDMA_CLKO_DIV15 36u
#define AM_HAL_PDM_PDMA_CLKO_DIV3 37u
#define AM_HAL_PDM_PDMA_CLKO_DIV7 38u
#define AM_HAL_PDM_POWER_OFF 39u
#define AM_HAL_PDM_POWER_ON 40u
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_SYSPLL_FREFSEL_EXTREFCLK 42u
#define AM_HAL_SYSPLL_FREFSEL_HFRC192DIV4 43u
#define AM_IRQ_PRIORITY_DEFAULT 44u

typedef struct { unsigned isrStat; } am_hal_interupt_service_msg_t;
unsigned am_hal_pdm_interrupt_service(void*,am_hal_interupt_service_msg_t*,am_hal_pdm_transfer_t*);
unsigned am_hal_pdm_dma_get_buffer(void*);
#define AM_HAL_PDM_CLK_750KHZ 45u
#define AM_HAL_PDM_CLK_1_5MHZ 46u
#define AM_HAL_PDM_INTERNAL_CLK 47u
#define AM_HAL_PDM_DMA_THRESHOLD_MIN 16u

unsigned am_hal_pdm_dma_stop(void*);

#ifdef TEST_LIFECYCLE
void test_irq_enable(unsigned);
void test_irq_disable(unsigned);
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#define NVIC_EnableIRQ(n) test_irq_enable(n)
#define NVIC_DisableIRQ(n) test_irq_disable(n)
#endif
