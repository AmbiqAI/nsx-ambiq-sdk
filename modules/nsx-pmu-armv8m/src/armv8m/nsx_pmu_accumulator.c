/**
 * @file    nsx_pmu_acc_matrix.c
 * @author Ambiq
 * @brief   Implementation of 2-D PMU accumulator.
 */

#include "nsx_pmu_utils.h"
#include "nsx_pmu_map.h"
#include <string.h>
#include "nsx_pmu_accumulator.h"

#ifndef EVENTS_PER_RUN
#define EVENTS_PER_RUN (NSX_PMU_MAX_COUNTERS / 2)
#endif

typedef struct {
    const char *tag;
    uint16_t row;
} tag_map_t;

typedef struct {
    bool in_use;
    bool complete;
    uint16_t ops;
    uint16_t events;
    uint16_t next_event;
    uint16_t next_row;
    uint32_t *sum;
    tag_map_t tag_map[NSX_PMU_MAX_OPS];
    nsx_pmu_config_t cfg;
} acc_t;

static acc_t g_acc[NSX_PMU_MAX_ACTIVE_MATRICES] = {0};

static acc_t *handle_to_acc(nsx_pmu_accm_t h)
{
    return (h.id < NSX_PMU_MAX_ACTIVE_MATRICES && g_acc[h.id].in_use) ? &g_acc[h.id] : NULL;
}

static void cfg_select_events(acc_t *a)
{
    nsx_pmu_reset_config(&a->cfg);
    a->cfg.api = &nsx_pmu_V1_0_0;

    for (uint8_t i = 0; i < EVENTS_PER_RUN; ++i) {
        uint16_t ev = a->next_event + i;
        if (ev >= a->events) {
            break;
        }
        nsx_pmu_event_create(&a->cfg.events[i],
                            nsx_pmu_map[ev].eventId,
                            NSX_PMU_EVENT_COUNTER_SIZE_32);
    }
    nsx_pmu_init(&a->cfg);
}

nsx_pmu_accm_t nsx_pmu_accm_create(uint16_t ops, uint16_t events, void *backing_buf)
{
    nsx_pmu_accm_t h = {.id = 0xFF};

    if (!backing_buf || ops == 0 || ops > NSX_PMU_MAX_OPS || events == 0 ||
        events > NSX_PMU_MAP_SIZE) {
        return h;
    }

    uint8_t idx;
    for (idx = 0; idx < NSX_PMU_MAX_ACTIVE_MATRICES; ++idx) {
        if (!g_acc[idx].in_use) {
            break;
        }
    }
    if (idx == NSX_PMU_MAX_ACTIVE_MATRICES) {
        return h;
    }

    acc_t *a = &g_acc[idx];
    memset(a, 0, sizeof(*a));

    a->in_use = true;
    a->ops = ops;
    a->events = events;
    a->sum = (uint32_t *) backing_buf;
    a->next_event = 0;

    memset(a->sum, 0, NSX_PMU_MATRIX_BYTES(ops, events));
    for (uint16_t i = 0; i < NSX_PMU_MAX_OPS; ++i) {
        a->tag_map[i].tag = NULL;
    }

    cfg_select_events(a);

    h.id = idx;
    return h;
}

void nsx_pmu_accm_destroy(nsx_pmu_accm_t h)
{
    acc_t *a = handle_to_acc(h);
    if (a) {
        memset(a, 0, sizeof(*a));
    }
}

void nsx_pmu_accm_inference_begin(nsx_pmu_accm_t h)
{
    (void) h;
}

void nsx_pmu_accm_inference_end(nsx_pmu_accm_t h)
{
    acc_t *a = handle_to_acc(h);
    if (!a || a->complete) {
        return;
    }

    a->next_event += EVENTS_PER_RUN;
    if (a->next_event >= a->events) {
        a->complete = true;
        return;
    }

    cfg_select_events(a);
}

void nsx_pmu_accm_op_begin(nsx_pmu_accm_t h, uint16_t op_idx)
{
    acc_t *a = handle_to_acc(h);
    if (!a || op_idx >= a->ops) {
        return;
    }

    nsx_pmu_reset_counters();
}

void nsx_pmu_accm_op_end(nsx_pmu_accm_t h, uint16_t op_idx)
{
    acc_t *a = handle_to_acc(h);
    if (!a || op_idx >= a->ops) {
        return;
    }

    nsx_pmu_get_counters(&a->cfg);

    for (uint8_t i = 0; i < EVENTS_PER_RUN; ++i) {
        uint16_t ev = a->next_event + i;
        if (ev >= a->events) {
            break;
        }
        a->sum[op_idx * a->events + ev] += a->cfg.counter[i].counterValue;
    }
}

static uint16_t tag_lookup(acc_t *a, const char *tag, bool create_if_missing)
{
    for (uint16_t i = 0; i < a->next_row; ++i) {
        if (a->tag_map[i].tag == tag) {
            return a->tag_map[i].row;
        }
    }

    for (uint16_t i = 0; i < a->next_row; ++i) {
        if (strcmp(a->tag_map[i].tag, tag) == 0) {
            return a->tag_map[i].row;
        }
    }

    if (!create_if_missing || a->next_row >= a->ops) {
        return UINT16_MAX;
    }

    uint16_t row = a->next_row++;
    a->tag_map[row].tag = tag;
    a->tag_map[row].row = row;
    return row;
}

uint16_t nsx_pmu_accm_resolve_tag(nsx_pmu_accm_t h, const char *tag)
{
    acc_t *a = handle_to_acc(h);
    if (!a || !tag) {
        return UINT16_MAX;
    }
    return tag_lookup(a, tag, true);
}

uint16_t nsx_pmu_accm_find_tag(nsx_pmu_accm_t h, const char *tag)
{
    acc_t *a = handle_to_acc(h);
    if (!a || !tag) {
        return UINT16_MAX;
    }
    return tag_lookup(a, tag, false);
}

bool nsx_pmu_accm_complete(nsx_pmu_accm_t h)
{
    acc_t *a = handle_to_acc(h);
    return a ? a->complete : false;
}

void nsx_pmu_accm_get(nsx_pmu_accm_t h, uint32_t **matrix)
{
    acc_t *a = handle_to_acc(h);
    if (a && matrix) {
        *matrix = a->sum;
    }
}

void nsx_pmu_accmprint_matrix(nsx_pmu_accm_t h, uint32_t *matrix, uint16_t ops, uint16_t events)
{
    acc_t *a = handle_to_acc(h);

    nsx_printf("Op: ");
    for (uint16_t j = 0; j < events; ++j) {
        nsx_printf("%s ", nsx_pmu_map[j].regname);
    }
    nsx_printf("\n");

    if (a && matrix) {
        for (uint16_t i = 0; i < ops; ++i) {
            nsx_printf("%u, ", i);
            for (uint16_t j = 0; j < events; ++j) {
                nsx_printf("%u, ", matrix[i * events + j]);
            }
            nsx_printf("\n");
        }
    }
}

void nsx_pmu_accm_get_layer(nsx_pmu_accm_t h,
                           uint16_t layer,
                           uint32_t *out,
                           uint16_t caller_capacity)
{
    acc_t *a = handle_to_acc(h);
    if (!a || !out || layer >= a->ops) {
        return;
    }
    uint16_t n = (caller_capacity < a->events) ? caller_capacity : a->events;
    memcpy(out, &a->sum[layer * a->events], n * sizeof(uint32_t));
}
