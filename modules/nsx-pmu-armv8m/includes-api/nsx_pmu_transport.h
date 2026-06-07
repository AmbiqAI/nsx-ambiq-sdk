/**
 * @file nsx_pmu_transport.h
 * @author Ambiq
 * @brief Transport abstraction for PMU capture serialization.
 */

#ifndef NSX_PMU_TRANSPORT_H
#define NSX_PMU_TRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NSX_PMU_TRANSPORT_PRINTF_BUFFER_BYTES
#define NSX_PMU_TRANSPORT_PRINTF_BUFFER_BYTES 256
#endif

typedef uint32_t (*nsx_pmu_transport_write_fn)(void *context,
                                             const uint8_t *data,
                                             uint32_t size);
typedef uint32_t (*nsx_pmu_transport_flush_fn)(void *context);

typedef struct {
    void *context;
    nsx_pmu_transport_write_fn write;
    nsx_pmu_transport_flush_fn flush;
} nsx_pmu_transport_t;

uint32_t nsx_pmu_transport_write(nsx_pmu_transport_t *transport,
                                const void *data,
                                uint32_t size);
uint32_t nsx_pmu_transport_write_cstr(nsx_pmu_transport_t *transport,
                                     const char *text);
uint32_t nsx_pmu_transport_printf(nsx_pmu_transport_t *transport,
                                 const char *fmt,
                                 ...);
uint32_t nsx_pmu_transport_flush(nsx_pmu_transport_t *transport);
void nsx_pmu_transport_log_sink_init(nsx_pmu_transport_t *transport);

#ifdef __cplusplus
}
#endif

#endif // NSX_PMU_TRANSPORT_H
