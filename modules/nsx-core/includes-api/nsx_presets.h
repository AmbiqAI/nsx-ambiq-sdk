/**
 * @file nsx_presets.h
 * @brief Opinionated, opt-in system presets.
 *
 * These are convenience configurations for quick bring-up, demos, and tools.
 * They are intentionally NOT part of the core system API surface
 * (nsx_system.h) — include this header explicitly to opt in, and link the
 * `nsx::presets` target.
 *
 * @copyright Copyright (c) 2024-2026, Ambiq Micro, Inc.
 */
#ifndef NSX_PRESETS_H
#define NSX_PRESETS_H

#include "nsx_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Development preset: HP mode, caches, ITM, SRAM, SpotManager.
 *
 * Good for bring-up and debugging. Includes full BSP init.
 */
extern const nsx_system_config_t nsx_system_development;

/**
 * @brief Inference preset: HP mode, caches, no debug, SpotManager.
 *
 * Minimal footprint for production inference workloads.
 */
extern const nsx_system_config_t nsx_system_inference;

/**
 * @brief Minimal preset: zero-init safe defaults.
 *
 * Low-power mode, no caches, no debug. Use as a starting point
 * and enable only what you need.
 */
extern const nsx_system_config_t nsx_system_minimal;

#ifdef __cplusplus
}
#endif

#endif /* NSX_PRESETS_H */
