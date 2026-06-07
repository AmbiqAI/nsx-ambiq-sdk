/**
 * @file nsx_pmu_utils.h
 * @author Ambiq
 * @brief Utilities for setting up and using the Performance Monitoring Unit
 *
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef NSX_PMU_UTILS_H
#define NSX_PMU_UTILS_H

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_PMU_MAX_COUNTERS 8

#define NSX_PMU_V0_0_1                                                                        \
    { .major = 0, .minor = 0, .revision = 1 }
#define NSX_PMU_V1_0_0                                                                        \
    { .major = 1, .minor = 0, .revision = 0 }

#define NSX_PMU_OLDEST_SUPPORTED_VERSION NSX_PMU_V0_0_1
#define NSX_PMU_CURRENT_VERSION NSX_PMU_V1_0_0
#define NSX_PMU_API_ID 0xCA000B

extern const nsx_core_api_t nsx_pmu_V0_0_1;
extern const nsx_core_api_t nsx_pmu_V1_0_0;
extern const nsx_core_api_t nsx_pmu_oldest_supported_version;
extern const nsx_core_api_t nsx_pmu_current_version;

typedef enum {
    NSX_PMU_EVENT_COUNTER_SIZE_16 = 0,
    NSX_PMU_EVENT_COUNTER_SIZE_32 = 1
} nsx_pmu_event_counter_size_e;

typedef struct nsx_pmu_event {
    bool enabled;
    uint32_t eventId;
    nsx_pmu_event_counter_size_e counterSize;
} nsx_pmu_event_t;

typedef struct nsx_pmu_counter {
    bool added;            /*!< Used to track if the event has been added to
                                the am_util_pmu_config_t */
    uint32_t mapIndex;     /// pmu_map index for this event
    uint32_t counterValue; /// Value read from the PMU
} nsx_pmu_counter_t;

typedef struct {
    uint32_t counterValue[NSX_PMU_MAX_COUNTERS]; /// Value read from the PMU
} nsx_pmu_counters_t; // just the counter values, used for snapshots

typedef struct nsx_pmu_config {
    const nsx_core_api_t *api;
    nsx_pmu_event_t events[NSX_PMU_MAX_COUNTERS];
    nsx_pmu_counter_t counter[NSX_PMU_MAX_COUNTERS];
} nsx_pmu_config_t;

typedef enum {
    NSX_PMU_PRESET_BASIC_CPU = 0,
    NSX_PMU_PRESET_MEMORY = 1,
    NSX_PMU_PRESET_MVE = 2,
    NSX_PMU_PRESET_ML_DEFAULT = 3,
} nsx_pmu_preset_e;

uint32_t nsx_pmu_init(nsx_pmu_config_t *cfg);
uint32_t nsx_pmu_get_counters(nsx_pmu_config_t *cfg);
void ns_delta_pmu(nsx_pmu_counters_t *s, nsx_pmu_counters_t *e, nsx_pmu_counters_t *d);
void nsx_pmu_get_name(nsx_pmu_config_t *cfg, uint32_t i, char *name);
uint32_t nsx_pmu_print_counters(nsx_pmu_config_t *cfg);
void nsx_pmu_event_create(nsx_pmu_event_t *event, uint32_t eventId, nsx_pmu_event_counter_size_e counterSize);
uint32_t nsx_pmu_apply_preset(nsx_pmu_config_t *cfg, nsx_pmu_preset_e preset);
void nsx_pmu_reset_counters();
void nsx_pmu_reset_config(nsx_pmu_config_t *cfg);

// Callback for tensorflow invoke
typedef int (*invoke_fp)();
// typedef TfLiteStatus (tflite::MicroInterpreter::*)()
void nsx_pmu_characterize_function(invoke_fp func, nsx_pmu_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif // NSX_PMU_UTILS_H
