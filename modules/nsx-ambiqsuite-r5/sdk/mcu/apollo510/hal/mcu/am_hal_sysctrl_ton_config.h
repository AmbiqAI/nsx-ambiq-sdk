//*****************************************************************************
//
//! @file am_hal_sysctrl_ton_config.h
//!
//! @brief Internal api definition for Ton Control
//!
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
//! @cond SYSCTRL_TON_CONFIG
#ifndef AM_HAL_SYSCTRL_TON_CONFIG_H
#define AM_HAL_SYSCTRL_TON_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! Selection of SIMOBUCK scheme for TON configuration
//
//*****************************************************************************
#define AM_SIMOBUCK_SCHEME_HIGH_EFFICIENCY 0

//*****************************************************************************
//
//! Definition of TON Value Levels
//
//*****************************************************************************
typedef enum
{
    SYSCTRL_TON_LEVEL_NA = 0,
    SYSCTRL_TON_LEVEL_LOW,
    SYSCTRL_TON_LEVEL_HIGH,
}
am_hal_sysctrl_ton_levels_t;

//*****************************************************************************
//
//! Definition of GPU Power State enumeration for SIMOBUCK TON control
//
//*****************************************************************************
typedef enum
{
    SYSCTRL_GPU_TON_POWER_STATE_OFF,
    SYSCTRL_GPU_TON_POWER_STATE_LP,
    SYSCTRL_GPU_TON_POWER_STATE_HP,
    SYSCTRL_GPU_TON_POWER_STATE_MAX,
}
am_hal_sysctrl_gpu_ton_power_state;

//*****************************************************************************
//
//! Definition of CPU Power State enumeration for SIMOBUCK TON control
//
//*****************************************************************************
typedef enum
{
    SYSCTRL_CPU_TON_POWER_STATE_OFF = 0,
    SYSCTRL_CPU_TON_POWER_STATE_LP,
    SYSCTRL_CPU_TON_POWER_STATE_HP,
    SYSCTRL_CPU_TON_POWER_STATE_MAX,
}
am_hal_sysctrl_cpu_ton_power_state;

#ifdef __cplusplus
}
#endif

#endif //AM_HAL_SYSCTRL_TON_CONFIG_H

//! @endcond SYSCTRL_TON_CONFIG
