//*****************************************************************************
//
//! @file system_apollo510L.c
//!
//! @brief Ambiq Micro Apollo5 MCU specific functions.
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include "system_apollo510L.h"
#include "apollo510L.h"

//*****************************************************************************
//
// Defines
//
//*****************************************************************************

//
// Clocks
//
#define __HSI             (6000000UL)
#define __XTAL            (32768UL)         // Crystal Oscillator frequency
#define __SYS_OSC_CLK     (96000000)        // Main oscillator frequency
#define __SYSTEM_CLOCK    (1 * __SYS_OSC_CLK)

//
// Initialize SystemCoreClock with the system core clock frequency value
// achieved after system intitialization.
// This means system core clock frequency after call to SystemInit()
//
uint32_t SystemCoreClock = __SYSTEM_CLOCK;  // System Clock Frequency (Core Clock)

//---------------------------------------------------------------------------
//  Exception / Interrupt Vector table
//---------------------------------------------------------------------------*/
extern const VECTOR_TABLE_Type __VECTOR_TABLE[496];

//*****************************************************************************
//
//! @brief Set the global clock frequncy.
//!
//! This function sets the global clock frequency.
//!
//! @return None.
//
//*****************************************************************************
void
SystemCoreClockUpdate(void)
{
    //
    // Calculate the system frequency based upon the current register settings.
    // This function can be used to retrieve the system core clock frequeny
    // after user changed register sittings.
    //
    SystemCoreClock = __SYS_OSC_CLK;
}

//*****************************************************************************
//
//! @brief Initialize the system.
//!
//! This function sets up the microcontroller system.
//!
//! @return None.
//
//*****************************************************************************
void
SystemInit(void)
{

#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  SCB->VTOR = (uint32_t)(&__VECTOR_TABLE[0]);
#endif

#if (defined (__FPU_USED) && (__FPU_USED == 1U)) || \
    (defined (__ARM_FEATURE_MVE) && (__ARM_FEATURE_MVE > 0U))
  SCB->CPACR |= ((3U << 10U*2U) |           /* enable CP10 Full Access */
                 (3U << 11U*2U)  );         /* enable CP11 Full Access */

  /* Set low-power state for PDEPU                */
  /*  0b00  | ON, PDEPU is not in low-power state */
  /*  0b01  | ON, but the clock is off            */
  /*  0b10  | RET(ention)                         */
  /*  0b11  | OFF                                 */

  /* Clear ELPSTATE, value is 0b11 on Cold reset */
  PWRMODCTL->CPDLPSTATE &= ~(PWRMODCTL_CPDLPSTATE_ELPSTATE_Msk);

  /* Favor best FP/MVE performance by default, avoid EPU switch-ON delays */
  /* PDEPU ON, Clock OFF */
  PWRMODCTL->CPDLPSTATE |= 0x1 << PWRMODCTL_CPDLPSTATE_ELPSTATE_Pos;
#endif

#ifdef UNALIGNED_SUPPORT_DISABLE
  SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
#endif

  /* Enable Loop and branch info cache */
  SCB->CCR |= SCB_CCR_LOB_Msk;
  __DSB();
  __ISB();

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  TZ_SAU_Setup();
#endif

  SystemCoreClock = __SYSTEM_CLOCK;

//
// Set the SSRAM non-cacheable for application explicitly requested SSRAM_NON_CACHEABLE
//
#ifdef SSRAM_NON_CACHEABLE
  __DSB();
  //
  // Set up non-cachable MPU region attributes.
  //
  ARM_MPU_SetMemAttr (
    7, // use the last MPU attribute slot
    ARM_MPU_ATTR (
      ARM_MPU_ATTR_MEMORY_ (0, 1, 0, 0),
      ARM_MPU_ATTR_MEMORY_ (0, 1, 0, 0)
    )
  );

  //
  // Set the whole SSRAM non-cacheable
  //
  ARM_MPU_Region_t region;
  region.RBAR = ((0x20080000 & MPU_RBAR_BASE_Msk) |
                 (ARM_MPU_SH_NON << MPU_RBAR_SH_Pos) |
                 (ARM_MPU_AP_(0, 1) << MPU_RBAR_AP_Pos) |
                 (1 << MPU_RBAR_XN_Pos));
  region.RLAR = ((0x2037FFFF & MPU_RLAR_LIMIT_Msk) |
                 (7 << MPU_RLAR_AttrIndx_Pos) |
                 (1));
  ARM_MPU_Load (
    15, // use the last MPU region
    (ARM_MPU_Region_t const*)&region, 1);

  //
  // Enable MPU
  //
  SCB_CleanInvalidateDCache();
  ARM_MPU_Enable((1 << MPU_CTRL_HFNMIENA_Pos) |
                 (1 << MPU_CTRL_PRIVDEFENA_Pos));
#endif  // SSRAM_NON_CACHEABLE
}
