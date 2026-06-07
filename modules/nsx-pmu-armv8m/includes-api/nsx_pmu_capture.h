/**
 * @file nsx_pmu_capture.h
 * @author Ambiq
 * @brief Chunked PMU capture helpers for large-model profiling.
 */

#ifndef NSX_PMU_CAPTURE_H
#define NSX_PMU_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "nsx_pmu_transport.h"
#include "nsx_pmu_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_PMU_CAPTURE_MAX_EVENTS_PER_PASS (NSX_PMU_MAX_COUNTERS / 2)

typedef struct {
    const nsx_core_api_t *api;
    uint16_t total_ops;
    uint16_t ops_per_chunk;
    uint16_t event_count;
    uint8_t events_per_pass;
    const uint16_t *event_map_indices;
    uint32_t *matrix_storage;
    uint32_t matrix_storage_count;
} nsx_pmu_capture_cfg_t;

typedef struct {
    uint16_t chunk_start;
    uint16_t chunk_ops;
    uint16_t total_ops;
    uint16_t event_count;
    uint8_t events_per_pass;
    bool chunk_ready;
    bool complete;
} nsx_pmu_capture_chunk_info_t;

typedef struct {
    const nsx_core_api_t *api;
    const uint16_t *event_map_indices;
    uint32_t *matrix_storage;
    uint32_t matrix_storage_count;
    uint16_t total_ops;
    uint16_t ops_per_chunk;
    uint16_t total_event_count;
    uint16_t current_chunk_start;
    uint16_t current_chunk_ops;
    uint16_t current_event_base;
    uint8_t events_per_pass;
    uint8_t current_pass_events;
    bool chunk_ready;
    bool complete;
    nsx_pmu_config_t pmu_cfg;
} nsx_pmu_capture_t;

uint32_t nsx_pmu_capture_init(nsx_pmu_capture_t *capture,
                             const nsx_pmu_capture_cfg_t *cfg);
uint32_t nsx_pmu_capture_reset(nsx_pmu_capture_t *capture);

void nsx_pmu_capture_model_begin(nsx_pmu_capture_t *capture);
uint32_t nsx_pmu_capture_model_end(nsx_pmu_capture_t *capture);

bool nsx_pmu_capture_should_sample_layer(const nsx_pmu_capture_t *capture,
                                        uint16_t global_op_index,
                                        uint16_t *chunk_op_index);
void nsx_pmu_capture_layer_begin(nsx_pmu_capture_t *capture,
                                uint16_t global_op_index);
void nsx_pmu_capture_layer_end(nsx_pmu_capture_t *capture,
                              uint16_t global_op_index);

bool nsx_pmu_capture_chunk_ready(const nsx_pmu_capture_t *capture);
bool nsx_pmu_capture_complete(const nsx_pmu_capture_t *capture);
uint32_t nsx_pmu_capture_advance_chunk(nsx_pmu_capture_t *capture);

uint32_t nsx_pmu_capture_get_chunk_info(const nsx_pmu_capture_t *capture,
                                       nsx_pmu_capture_chunk_info_t *info);
uint32_t nsx_pmu_capture_get_chunk_matrix(const nsx_pmu_capture_t *capture,
                                         const uint32_t **matrix);
uint32_t nsx_pmu_capture_get_event_map_index(const nsx_pmu_capture_t *capture,
                                            uint16_t logical_event_index,
                                            uint16_t *map_index);
uint32_t nsx_pmu_capture_serialize_csv(const nsx_pmu_capture_t *capture,
                                      nsx_pmu_transport_t *transport,
                                      bool include_header);

#ifdef __cplusplus
}
#endif

#endif // NSX_PMU_CAPTURE_H
