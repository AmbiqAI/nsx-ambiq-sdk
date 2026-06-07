/**
 * @file nsx_pmu_transport.c
 * @author Ambiq
 * @brief Transport helpers for PMU serialization.
 */

#include "nsx_pmu_transport.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "nsx_core.h"
#include "nsx_core.h"

typedef struct {
    char line_buffer[NSX_PMU_TRANSPORT_PRINTF_BUFFER_BYTES];
    uint32_t used;
} nsx_pmu_log_sink_context_t;

static nsx_pmu_log_sink_context_t g_nsx_pmu_log_sink_context;

static uint32_t nsx_pmu_log_sink_emit(nsx_pmu_log_sink_context_t *context)
{
    if (context == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (context->used == 0U) {
        return NSX_STATUS_SUCCESS;
    }

    context->line_buffer[context->used] = '\0';
    nsx_printf("%s", context->line_buffer);
    context->used = 0;
    return NSX_STATUS_SUCCESS;
}

static uint32_t nsx_pmu_log_sink_write(void *context, const uint8_t *data, uint32_t size)
{
    nsx_pmu_log_sink_context_t *log_context = (nsx_pmu_log_sink_context_t *) context;

    if (log_context == NULL) {
        log_context = &g_nsx_pmu_log_sink_context;
    }
    if (data == NULL && size != 0) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    for (uint32_t offset = 0; offset < size; ++offset) {
        if (log_context->used >= (sizeof(log_context->line_buffer) - 1U)) {
            uint32_t status = nsx_pmu_log_sink_emit(log_context);
            if (status != NSX_STATUS_SUCCESS) {
                return status;
            }
        }

        log_context->line_buffer[log_context->used++] = (char) data[offset];
        if (data[offset] == (uint8_t) '\n') {
            uint32_t status = nsx_pmu_log_sink_emit(log_context);
            if (status != NSX_STATUS_SUCCESS) {
                return status;
            }
        }
    }
    return NSX_STATUS_SUCCESS;
}

static uint32_t nsx_pmu_log_sink_flush(void *context)
{
    nsx_pmu_log_sink_context_t *log_context = (nsx_pmu_log_sink_context_t *) context;

    if (log_context == NULL) {
        log_context = &g_nsx_pmu_log_sink_context;
    }
    return nsx_pmu_log_sink_emit(log_context);
}

uint32_t nsx_pmu_transport_write(nsx_pmu_transport_t *transport,
                                const void *data,
                                uint32_t size)
{
    if (transport == NULL || transport->write == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return transport->write(transport->context, (const uint8_t *) data, size);
}

uint32_t nsx_pmu_transport_write_cstr(nsx_pmu_transport_t *transport, const char *text)
{
    if (text == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return nsx_pmu_transport_write(transport, text, (uint32_t) strlen(text));
}

uint32_t nsx_pmu_transport_printf(nsx_pmu_transport_t *transport, const char *fmt, ...)
{
    char buffer[NSX_PMU_TRANSPORT_PRINTF_BUFFER_BYTES];
    va_list args;
    int length;

    if (fmt == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    va_start(args, fmt);
    length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length < 0) {
        return NSX_STATUS_FAILURE;
    }

    if ((uint32_t) length >= sizeof(buffer)) {
        length = (int) sizeof(buffer) - 1;
    }
    return nsx_pmu_transport_write(transport, buffer, (uint32_t) length);
}

uint32_t nsx_pmu_transport_flush(nsx_pmu_transport_t *transport)
{
    if (transport == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (transport->flush == NULL) {
        return NSX_STATUS_SUCCESS;
    }
    return transport->flush(transport->context);
}

void nsx_pmu_transport_log_sink_init(nsx_pmu_transport_t *transport)
{
    if (transport == NULL) {
        return;
    }
    memset(&g_nsx_pmu_log_sink_context, 0, sizeof(g_nsx_pmu_log_sink_context));
    transport->context = &g_nsx_pmu_log_sink_context;
    transport->write = nsx_pmu_log_sink_write;
    transport->flush = nsx_pmu_log_sink_flush;
}
