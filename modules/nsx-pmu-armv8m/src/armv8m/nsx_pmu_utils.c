/**
 * @file nsx_pmu_utils.c
 * @author Ambiq
 * @brief A collection of functions to collect and analyze performance data
 * @version 0.1
 * @date 2024-08-15
 */

#include "am_mcu_apollo.h"

#include "m-profile/armv8m_pmu.h"
#include "nsx_core.h"
#include "nsx_ambiq_pmu.h"
#include "nsx_pmu_map.h"
#include "nsx_pmu_utils.h"

#include <string.h>

#define NSX_PMU_COUNTER_MASK(index) (1UL << (index))
#define NSX_PMU_CHARACTERIZATION_BATCH_SIZE (NSX_PMU_MAX_COUNTERS / 2)
#define NSX_PMU_VALID_COUNTERS                                                  \
    (PMU_CNTENSET_CNT0_ENABLE_Msk | PMU_CNTENSET_CNT1_ENABLE_Msk |             \
     PMU_CNTENSET_CNT2_ENABLE_Msk | PMU_CNTENSET_CNT3_ENABLE_Msk |             \
     PMU_CNTENSET_CNT4_ENABLE_Msk | PMU_CNTENSET_CNT5_ENABLE_Msk |             \
     PMU_CNTENSET_CNT6_ENABLE_Msk | PMU_CNTENSET_CNT7_ENABLE_Msk |             \
     PMU_CNTENSET_CCNTR_ENABLE_Msk)
#define NSX_PMU_VALID_OVSCLRS                                                   \
    (PMU_OVSCLR_CNT0_STATUS_Msk | PMU_OVSCLR_CNT1_STATUS_Msk |                 \
     PMU_OVSCLR_CNT2_STATUS_Msk | PMU_OVSCLR_CNT3_STATUS_Msk |                 \
     PMU_OVSCLR_CNT4_STATUS_Msk | PMU_OVSCLR_CNT5_STATUS_Msk |                 \
     PMU_OVSCLR_CNT6_STATUS_Msk | PMU_OVSCLR_CNT7_STATUS_Msk |                 \
     PMU_OVSCLR_CYCCNT_STATUS_Msk)

const nsx_core_api_t nsx_pmu_V0_0_1 = {.apiId = NSX_PMU_API_ID, .version = NSX_PMU_V0_0_1};
const nsx_core_api_t nsx_pmu_V1_0_0 = {.apiId = NSX_PMU_API_ID, .version = NSX_PMU_V1_0_0};
const nsx_core_api_t nsx_pmu_oldest_supported_version = {
    .apiId = NSX_PMU_API_ID, .version = NSX_PMU_V0_0_1};
const nsx_core_api_t nsx_pmu_current_version = {
    .apiId = NSX_PMU_API_ID, .version = NSX_PMU_V1_0_0};

static nsx_ambiq_pmu_config_t ns_ambiq_pmu_config;
static bool nsx_pmu_initialized = false;
static bool nsx_pmu_profiling = false;
static uint32_t nsx_pmu_config_index[NSX_PMU_MAX_COUNTERS];
uint32_t g_nsx_pmu_map_length;

const nsx_pmu_map_t nsx_pmu_map[] = {
    {0x0000, "ARM_PMU_SW_INCR", "Software update to the PMU_SWINC register, architecturally executed and condition code check pass"},
    {0x0001, "ARM_PMU_L1I_CACHE_REFILL", "L1 I-Cache refill"},
    {0x0003, "ARM_PMU_L1D_CACHE_REFILL", "L1 D-Cache refill"},
    {0x0004, "ARM_PMU_L1D_CACHE", "L1 D-Cache access"},
    {0x0006, "ARM_PMU_LD_RETIRED", "Memory-reading instruction architecturally executed and condition code check pass"},
    {0x0007, "ARM_PMU_ST_RETIRED", "Memory-writing instruction architecturally executed and condition code check pass"},
    {0x0008, "ARM_PMU_INST_RETIRED", "Instruction architecturally executed"},
    {0x0009, "ARM_PMU_EXC_TAKEN", "Exception entry"},
    {0x000A, "ARM_PMU_EXC_RETURN", "Exception return instruction architecturally executed and the condition code check pass"},
    {0x000C, "ARM_PMU_PC_WRITE_RETIRED", "Software change to the Program Counter (PC). Instruction is architecturally executed and condition code check pass"},
    {0x000D, "ARM_PMU_BR_IMMED_RETIRED", "Immediate branch architecturally executed"},
    {0x000E, "ARM_PMU_BR_RETURN_RETIRED", "Function return instruction architecturally executed and the condition code check pass"},
    {0x000F, "ARM_PMU_UNALIGNED_LDST_RETIRED", "Unaligned memory memory-reading or memory-writing instruction architecturally executed and condition code check pass"},
    {0x0011, "ARM_PMU_CPU_CYCLES", "Cycle"},
    {0x0013, "ARM_PMU_MEM_ACCESS", "Data memory access"},
    {0x0014, "ARM_PMU_L1I_CACHE", "Level 1 instruction cache access"},
    {0x0015, "ARM_PMU_L1D_CACHE_WB", "Level 1 data cache write-back"},
    {0x0019, "ARM_PMU_BUS_ACCESS", "Bus access"},
    {0x001A, "ARM_PMU_MEMORY_ERROR", "Local memory error"},
    {0x001D, "ARM_PMU_BUS_CYCLES", "Bus cycles"},
    {0x001F, "ARM_PMU_L1D_CACHE_ALLOCATE", "Level 1 data cache allocation without refill"},
    {0x0021, "ARM_PMU_BR_RETIRED", "Branch instruction architecturally executed"},
    {0x0022, "ARM_PMU_BR_MIS_PRED_RETIRED", "Mispredicted branch instruction architecturally executed"},
    {0x0023, "ARM_PMU_STALL_FRONTEND", "No operation issued because of the frontend"},
    {0x0024, "ARM_PMU_STALL_BACKEND", "No operation issued because of the backend"},
    {0x0036, "ARM_PMU_LL_CACHE_RD", "Last level data cache read"},
    {0x0037, "ARM_PMU_LL_CACHE_MISS_RD", "Last level data cache read miss"},
    {0x0039, "ARM_PMU_L1D_CACHE_MISS_RD", "Level 1 data cache read miss"},
    {0x003C, "ARM_PMU_STALL", "Stall cycle for instruction or operation not sent for execution"},
    {0x0040, "ARM_PMU_L1D_CACHE_RD", "Level 1 data cache read"},
    {0x0100, "ARM_PMU_LE_RETIRED", "Loop end instruction executed"},
    {0x0108, "ARM_PMU_LE_CANCEL", "Loop end instruction not taken"},
    {0x0114, "ARM_PMU_SE_CALL_S", "Call to secure function, resulting in Security state change"},
    {0x0115, "ARM_PMU_SE_CALL_NS", "Call to non-secure function, resulting in Security state change"},
    {0x0200, "ARM_PMU_MVE_INST_RETIRED", "MVE instruction architecturally executed"},
    {0x0204, "ARM_PMU_MVE_FP_RETIRED", "MVE floating-point instruction architecturally executed"},
    {0x0208, "ARM_PMU_MVE_FP_HP_RETIRED", "MVE half-precision floating-point instruction architecturally executed"},
    {0x020C, "ARM_PMU_MVE_FP_SP_RETIRED", "MVE single-precision floating-point instruction architecturally executed"},
    {0x0214, "ARM_PMU_MVE_FP_MAC_RETIRED", "MVE floating-point multiply or multiply-accumulate instruction architecturally executed"},
    {0x0224, "ARM_PMU_MVE_INT_RETIRED", "MVE integer instruction architecturally executed"},
    {0x0228, "ARM_PMU_MVE_INT_MAC_RETIRED", "MVE multiply or multiply-accumulate instruction architecturally executed"},
    {0x0238, "ARM_PMU_MVE_LDST_RETIRED", "MVE load or store instruction architecturally executed"},
    {0x023C, "ARM_PMU_MVE_LD_RETIRED", "MVE load instruction architecturally executed"},
    {0x0240, "ARM_PMU_MVE_ST_RETIRED", "MVE store instruction architecturally executed"},
    {0x0244, "ARM_PMU_MVE_LDST_CONTIG_RETIRED", "MVE contiguous load or store instruction architecturally executed"},
    {0x0248, "ARM_PMU_MVE_LD_CONTIG_RETIRED", "MVE contiguous load instruction architecturally executed"},
    {0x024C, "ARM_PMU_MVE_ST_CONTIG_RETIRED", "MVE contiguous store instruction architecturally executed"},
    {0x0250, "ARM_PMU_MVE_LDST_NONCONTIG_RETIRED", "MVE non-contiguous load or store instruction architecturally executed"},
    {0x0254, "ARM_PMU_MVE_LD_NONCONTIG_RETIRED", "MVE non-contiguous load instruction architecturally executed"},
    {0x0258, "ARM_PMU_MVE_ST_NONCONTIG_RETIRED", "MVE non-contiguous store instruction architecturally executed"},
    {0x025C, "ARM_PMU_MVE_LDST_MULTI_RETIRED", "MVE memory instruction targeting multiple registers architecturally executed"},
    {0x0260, "ARM_PMU_MVE_LD_MULTI_RETIRED", "MVE memory load instruction targeting multiple registers architecturally executed"},
    {0x0264, "ARM_PMU_MVE_ST_MULTI_RETIRED", "MVE memory store instruction targeting multiple registers architecturally executed"},
    {0x028C, "ARM_PMU_MVE_LDST_UNALIGNED_RETIRED", "MVE unaligned memory load or store instruction architecturally executed"},
    {0x0290, "ARM_PMU_MVE_LD_UNALIGNED_RETIRED", "MVE unaligned load instruction architecturally executed"},
    {0x0294, "ARM_PMU_MVE_ST_UNALIGNED_RETIRED", "MVE unaligned store instruction architecturally executed"},
    {0x0298, "ARM_PMU_MVE_LDST_UNALIGNED_NONCONTIG_RETIRED", "MVE unaligned noncontiguous load or store instruction architecturally executed"},
    {0x02A0, "ARM_PMU_MVE_VREDUCE_RETIRED", "MVE vector reduction instruction architecturally executed"},
    {0x02A4, "ARM_PMU_MVE_VREDUCE_FP_RETIRED ", "MVE floating-point vector reduction instruction architecturally executed"},
    {0x02A8, "ARM_PMU_MVE_VREDUCE_INT_RETIRED", "MVE integer vector reduction instruction architecturally executed"},
    {0x02B8, "ARM_PMU_MVE_PRED", "Cycles where one or more predicated beats architecturally executed"},
    {0x02CC, "ARM_PMU_MVE_STALL", "Stall cycles caused by an MVE instruction"},
    {0x02CD, "ARM_PMU_MVE_STALL_RESOURCE", "Stall cycles caused by an MVE instruction because of resource conflicts"},
    {0x02CE, "ARM_PMU_MVE_STALL_RESOURCE_MEM ", "Stall cycles caused by an MVE instruction because of memory resource conflicts"},
    {0x02CF, "ARM_PMU_MVE_STALL_RESOURCE_FP", "Stall cycles caused by an MVE instruction because of floating-point resource conflicts"},
    {0x02D0, "ARM_PMU_MVE_STALL_RESOURCE_INT ", "Stall cycles caused by an MVE instruction because of integer resource conflicts"},
    {0x02D3, "ARM_PMU_MVE_STALL_BREAK", "Stall cycles caused by an MVE chain break"},
    {0x02D4, "ARM_PMU_MVE_STALL_DEPENDENCY", "Stall cycles caused by MVE register dependency"},
    {0x4007, "ARM_PMU_ITCM_ACCESS", "Instruction TCM access"},
    {0x4008, "ARM_PMU_DTCM_ACCESS", "Data TCM access"},
};

static void nsx_pmu_copy_name(char *dest, const char *src)
{
    strncpy(dest, src, NSX_PMU_EVENT_NAME_MAX_LEN - 1);
    dest[NSX_PMU_EVENT_NAME_MAX_LEN - 1] = '\0';
}

static uint32_t cntr_enable(uint32_t counters_enable)
{
    if (counters_enable & (~NSX_PMU_VALID_COUNTERS)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    ARM_PMU_CNTR_Enable(counters_enable);
    return NSX_STATUS_SUCCESS;
}

static uint32_t cntr_disable(uint32_t counters_disable)
{
    if (counters_disable & (~NSX_PMU_VALID_COUNTERS)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    ARM_PMU_CNTR_Disable(counters_disable);
    return NSX_STATUS_SUCCESS;
}

static int nsx_pmu_get_map_index(uint32_t event_id)
{
    for (uint32_t i = 0; i < NSX_PMU_MAP_SIZE; ++i) {
        if (nsx_pmu_map[i].eventId == event_id) {
            return (int) i;
        }
    }
    return -1;
}

static uint32_t nsx_pmu_get_map_length(void)
{
    return sizeof(nsx_pmu_map);
}

void nsx_pmu_reset_counters(void)
{
    ARM_PMU_CYCCNT_Reset();
    ARM_PMU_EVCNTR_ALL_Reset();
    ARM_PMU_Set_CNTR_OVS(NSX_PMU_VALID_OVSCLRS);
}

uint32_t nsx_pmu_init(nsx_pmu_config_t *cfg)
{
    uint8_t total_counters = 0;
    uint32_t counter_mask = PMU_CNTENSET_CCNTR_ENABLE_Msk;

#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        nsx_printf("Invalid handle\n");
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (cfg->api == NULL) {
        nsx_printf("Invalid PMU API version\n");
        return NSX_STATUS_INVALID_VERSION;
    }
    if (nsx_core_check_api(cfg->api, &nsx_pmu_oldest_supported_version, &nsx_pmu_current_version) !=
        NSX_STATUS_SUCCESS) {
        nsx_printf("Invalid PMU API version\n");
        return NSX_STATUS_INVALID_VERSION;
    }

    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        if (!cfg->events[i].enabled) {
            continue;
        }
        if (cfg->events[i].counterSize == NSX_PMU_EVENT_COUNTER_SIZE_32) {
            total_counters += 2;
        } else if (cfg->events[i].counterSize == NSX_PMU_EVENT_COUNTER_SIZE_16) {
            total_counters += 1;
        } else {
            return NSX_STATUS_INVALID_CONFIG;
        }
    }
    if (total_counters > NSX_PMU_MAX_COUNTERS) {
        nsx_printf("Too many counters enabled tc is %d\n", total_counters);
        return NSX_STATUS_INVALID_CONFIG;
    }
#endif

    g_nsx_pmu_map_length = nsx_pmu_get_map_length();
    ns_ambiq_pmu_config.counters = 0;
    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        cfg->counter[i].counterValue = 0;
        cfg->counter[i].added = false;
        ns_ambiq_pmu_config.event_types[i] = 0xFFFF;

        if (!cfg->events[i].enabled) {
            continue;
        }

        int map_index = nsx_pmu_get_map_index(cfg->events[i].eventId);
        if (map_index < 0) {
            nsx_printf("Invalid event id %d\n", cfg->events[i].eventId);
            return NSX_STATUS_INVALID_CONFIG;
        }
        cfg->counter[i].mapIndex = (uint32_t) map_index;
    }

    total_counters = 0;
    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        if (!(cfg->events[i].enabled) || cfg->counter[i].added ||
            cfg->events[i].counterSize != NSX_PMU_EVENT_COUNTER_SIZE_32) {
            continue;
        }

        counter_mask |= NSX_PMU_COUNTER_MASK(total_counters);
        nsx_pmu_config_index[total_counters] = i;
        ns_ambiq_pmu_config.event_types[total_counters++] = cfg->events[i].eventId;

        counter_mask |= NSX_PMU_COUNTER_MASK(total_counters);
        ns_ambiq_pmu_config.event_types[total_counters++] = ARM_PMU_CHAIN;
        cfg->counter[i].added = true;
    }

    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        if (!(cfg->events[i].enabled) || cfg->counter[i].added ||
            cfg->events[i].counterSize != NSX_PMU_EVENT_COUNTER_SIZE_16) {
            continue;
        }

        counter_mask |= NSX_PMU_COUNTER_MASK(total_counters);
        nsx_pmu_config_index[total_counters] = i;
        ns_ambiq_pmu_config.event_types[total_counters++] = cfg->events[i].eventId;
        cfg->counter[i].added = true;
    }

    ns_ambiq_pmu_config.counters = counter_mask;
    if (nsx_ambiq_pmu_enable() != 0U) {
        return NSX_STATUS_INIT_FAILED;
    }
    if (nsx_ambiq_pmu_init(&ns_ambiq_pmu_config) != 0U) {
        return NSX_STATUS_INIT_FAILED;
    }
    nsx_pmu_initialized = true;
    nsx_pmu_profiling = true;

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_pmu_get_counters(nsx_pmu_config_t *cfg)
{
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (!nsx_pmu_initialized || !nsx_pmu_profiling) {
        return NSX_STATUS_INIT_FAILED;
    }

    cntr_disable(ns_ambiq_pmu_config.counters);

    for (uint32_t pmu_index = 0; pmu_index < NSX_PMU_MAX_COUNTERS; ++pmu_index) {
        if (!(ns_ambiq_pmu_config.counters & NSX_PMU_COUNTER_MASK(pmu_index))) {
            continue;
        }

        uint32_t cfg_index = nsx_pmu_config_index[pmu_index];
        if (cfg->events[cfg_index].counterSize == NSX_PMU_EVENT_COUNTER_SIZE_32) {
            uint32_t base_value = ARM_PMU_Get_EVCNTR(pmu_index);
            uint32_t chain_value = ARM_PMU_Get_EVCNTR(pmu_index + 1);
            cfg->counter[cfg_index].counterValue = base_value + (chain_value << 16);
            ++pmu_index;
            continue;
        }

        cfg->counter[cfg_index].counterValue = ARM_PMU_Get_EVCNTR(pmu_index);
    }

    nsx_pmu_reset_counters();
    cntr_enable(ns_ambiq_pmu_config.counters);
    return NSX_STATUS_SUCCESS;
}

void ns_delta_pmu(nsx_pmu_counters_t *s, nsx_pmu_counters_t *e, nsx_pmu_counters_t *d)
{
    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        uint32_t sv = s->counterValue[i];
        uint32_t ev = e->counterValue[i];
        d->counterValue[i] = (ev >= sv) ? (ev - sv) : ev;
    }
}

uint32_t nsx_pmu_print_counters(nsx_pmu_config_t *cfg)
{
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        if (!cfg->events[i].enabled) {
            continue;
        }
        uint32_t map_index = cfg->counter[i].mapIndex;
        nsx_printf("%d %d, \t%s, \t \"%s\"\n",
                     i,
                     cfg->counter[i].counterValue,
                     nsx_pmu_map[map_index].regname,
                     nsx_pmu_map[map_index].description);
    }
    return NSX_STATUS_SUCCESS;
}

void nsx_pmu_get_name(nsx_pmu_config_t *cfg, uint32_t i, char *name)
{
    if (name == NULL) {
        return;
    }
    if (cfg == NULL || i >= NSX_PMU_MAX_COUNTERS) {
        nsx_pmu_copy_name(name, "Invalid handle");
        return;
    }
    if (!cfg->events[i].enabled) {
        nsx_pmu_copy_name(name, "Not enabled");
        return;
    }

    nsx_pmu_copy_name(name, nsx_pmu_map[cfg->counter[i].mapIndex].regname);
}

void nsx_pmu_reset_config(nsx_pmu_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    for (uint32_t i = 0; i < NSX_PMU_MAX_COUNTERS; ++i) {
        cfg->events[i].enabled = false;
        cfg->events[i].eventId = 0;
        cfg->events[i].counterSize = NSX_PMU_EVENT_COUNTER_SIZE_16;
        cfg->counter[i].counterValue = 0;
        cfg->counter[i].mapIndex = 0;
        cfg->counter[i].added = false;
    }
}

uint32_t nsx_pmu_apply_preset(nsx_pmu_config_t *cfg, nsx_pmu_preset_e preset)
{
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    nsx_pmu_reset_config(cfg);

    switch (preset) {
    case NSX_PMU_PRESET_BASIC_CPU:
        nsx_pmu_event_create(&(cfg->events[0]), ARM_PMU_CPU_CYCLES, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[1]), ARM_PMU_INST_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[2]), ARM_PMU_STALL_FRONTEND, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[3]), ARM_PMU_STALL_BACKEND, NSX_PMU_EVENT_COUNTER_SIZE_32);
        return NSX_STATUS_SUCCESS;
    case NSX_PMU_PRESET_MEMORY:
        nsx_pmu_event_create(&(cfg->events[0]), ARM_PMU_MEM_ACCESS, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[1]), ARM_PMU_L1D_CACHE_REFILL, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[2]), ARM_PMU_BUS_ACCESS, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[3]), ARM_PMU_BUS_CYCLES, NSX_PMU_EVENT_COUNTER_SIZE_32);
        return NSX_STATUS_SUCCESS;
    case NSX_PMU_PRESET_MVE:
        nsx_pmu_event_create(&(cfg->events[0]), ARM_PMU_MVE_INST_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[1]), ARM_PMU_MVE_INT_MAC_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[2]), ARM_PMU_MVE_LDST_MULTI_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[3]), ARM_PMU_MVE_STALL, NSX_PMU_EVENT_COUNTER_SIZE_32);
        return NSX_STATUS_SUCCESS;
    case NSX_PMU_PRESET_ML_DEFAULT:
        nsx_pmu_event_create(&(cfg->events[0]), ARM_PMU_MVE_INST_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[1]), ARM_PMU_MVE_INT_MAC_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[2]), ARM_PMU_INST_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
        nsx_pmu_event_create(&(cfg->events[3]), ARM_PMU_BUS_CYCLES, NSX_PMU_EVENT_COUNTER_SIZE_32);
        return NSX_STATUS_SUCCESS;
    default:
        return NSX_STATUS_INVALID_CONFIG;
    }
}

void nsx_pmu_event_create(nsx_pmu_event_t *event,
                         uint32_t eventId,
                         nsx_pmu_event_counter_size_e counterSize)
{
    event->enabled = true;
    event->eventId = eventId;
    event->counterSize = counterSize;
}

void nsx_pmu_characterize_function(invoke_fp func, nsx_pmu_config_t *cfg)
{
    if (func == NULL || cfg == NULL) {
        return;
    }

    for (uint32_t map_index = 0; map_index < NSX_PMU_MAP_SIZE;
         map_index += NSX_PMU_CHARACTERIZATION_BATCH_SIZE) {
        nsx_pmu_reset_config(cfg);
        cfg->api = &nsx_pmu_V1_0_0;

        for (uint32_t slot = 0; slot < NSX_PMU_CHARACTERIZATION_BATCH_SIZE; ++slot) {
            uint32_t event_index = map_index + slot;
            if (event_index >= NSX_PMU_MAP_SIZE) {
                break;
            }
            nsx_pmu_event_create(&(cfg->events[slot]),
                                nsx_pmu_map[event_index].eventId,
                                NSX_PMU_EVENT_COUNTER_SIZE_32);
        }

        if (nsx_pmu_init(cfg) != NSX_STATUS_SUCCESS) {
            return;
        }
        func();
        if (nsx_pmu_get_counters(cfg) != NSX_STATUS_SUCCESS) {
            return;
        }
        nsx_pmu_print_counters(cfg);
    }
}
