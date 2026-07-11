/**
 * @file nsx_usb.c
 * @brief USB CDC + Vendor implementation for Ambiq SoCs using TinyUSB.
 */

#include "nsx_usb.h"
#include "nsx_core.h"
#include "nsx_timer.h"
#include "tusb.h"

nsx_usb_config_t *g_usb_cfg = NULL;

/*
 * Task-context vs timer-ISR mutual exclusion for the TinyUSB core.
 *
 * usb_timer_callback() below runs in the NSX_TIMER_USB timer ISR and calls
 * tud_task(), which is not reentrant. The send/receive paths (task context)
 * also call tud_* functions, so they must not be interleaved with the ISR.
 *
 * Previous implementation wrapped every task-context chunk in
 * nsx_interrupt_master_disable()/_enable() (global PRIMASK). That blacks
 * out ALL interrupts -- including latency-sensitive sensor GPIO IRQs (e.g.
 * AS7058 bio-sensors, whose driver requires its INT be serviced within a
 * bounded window) -- many times per packet during sustained USB streaming,
 * which was observed on hardware starving such sensors.
 *
 * Since these SoCs are single-core, a volatile flag is sufficient: the task
 * side sets g_usb_task_lock around its tud_* critical section, and the
 * timer ISR skips its tick when the flag is set (the task side calls
 * tud_task() itself inside the section, so no servicing is lost -- at worst
 * one poll tick is skipped). Compiler barriers order the flag accesses
 * against the guarded calls. No interrupts are ever masked.
 *
 * Note: like the previous PRIMASK approach, this guards ISR-vs-task
 * reentrancy only -- concurrent nsx_usb_* calls from multiple tasks remain
 * the application's responsibility, unchanged from before.
 */
static volatile uint8_t g_usb_task_lock = 0;

static inline void nsx_usb_guard_enter(void) {
    g_usb_task_lock = 1;
    __asm volatile("" ::: "memory");
}

static inline void nsx_usb_guard_exit(void) {
    __asm volatile("" ::: "memory");
    g_usb_task_lock = 0;
}

static void usb_timer_callback(nsx_timer_config_t *tc) {
    (void)tc;
    if (g_usb_task_lock) {
        /* Task-context code is inside a tud_* critical section; it will run
         * tud_task() itself. Skip this tick instead of reentering TinyUSB. */
        return;
    }
    tud_task();
    if (g_usb_cfg != NULL) {
        if (g_usb_cfg->_rx_ready == 0 && tud_cdc_available()) {
            g_usb_cfg->_rx_ready = 1;
            if (g_usb_cfg->rx_cb != NULL) {
                g_usb_cfg->rx_cb(g_usb_cfg);
            }
        }
        if (g_usb_cfg->_vendor_rx_ready == 0 && tud_vendor_available()) {
            g_usb_cfg->_vendor_rx_ready = 1;
            if (g_usb_cfg->vendor_rx_cb != NULL) {
                g_usb_cfg->vendor_rx_cb(g_usb_cfg);
            }
        }
    }
}

static nsx_timer_config_t g_usb_timer = {
    .api                  = &nsx_timer_V1_0_0,
    .timer                = NSX_TIMER_USB,
    .enableInterrupt      = true,
    .periodInMicroseconds = NSX_USB_DEFAULT_POLL_US,
    .callback             = usb_timer_callback,
};

uint32_t nsx_usb_init(nsx_usb_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (cfg->tx_buffer == NULL || cfg->rx_buffer == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->tx_buffer_len == 0 || cfg->rx_buffer_len == 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->rx_buffer_len < NSX_USB_MIN_CDC_RX_BUFSIZE) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    g_usb_cfg = cfg;
    cfg->_rx_ready = 0;
    cfg->_vendor_rx_ready = 0;
    cfg->_vendor_connected = 0;
    cfg->_initialized = 0;

    tusb_init();

    uint32_t poll_us = cfg->poll_interval_us;
    if (poll_us == 0) {
        poll_us = NSX_USB_DEFAULT_POLL_US;
    }
    g_usb_timer.periodInMicroseconds = poll_us;

    uint32_t rc = nsx_timer_init(&g_usb_timer);
    if (rc != NSX_STATUS_SUCCESS) {
        return NSX_STATUS_INIT_FAILED;
    }

    cfg->_initialized = 1;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_usb_send(nsx_usb_config_t *cfg, const void *data, uint32_t len,
                       uint32_t *bytes_sent) {
    if (cfg == NULL || !cfg->_initialized) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (data == NULL || len == 0) {
        if (bytes_sent != NULL) {
            *bytes_sent = 0;
        }
        return NSX_STATUS_SUCCESS;
    }

    uint32_t timeout_ms = cfg->timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = NSX_USB_DEFAULT_TIMEOUT_MS;
    }

    const uint8_t *src = (const uint8_t *)data;
    uint32_t remaining = len;
    uint32_t sent = 0;
    uint32_t elapsed_ms = 0;

    while (tud_cdc_write_available() < len && elapsed_ms < timeout_ms) {
        nsx_usb_guard_enter();
        tud_cdc_write_flush();
        tud_task();
        nsx_usb_guard_exit();
        am_util_delay_ms(1);
        elapsed_ms++;
    }

    while (remaining > 0 && elapsed_ms < timeout_ms) {
        nsx_usb_guard_enter();
        uint32_t chunk = tud_cdc_write(src + sent, remaining);
        tud_cdc_write_flush();
        tud_task();
        nsx_usb_guard_exit();

        if (chunk > 0) {
            sent += chunk;
            remaining -= chunk;
        } else {
            am_util_delay_ms(1);
            elapsed_ms++;
        }
    }

#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    tud_cdc_write_flush();
#endif

    if (bytes_sent != NULL) {
        *bytes_sent = sent;
    }
    if (remaining > 0) {
        return NSX_USB_STATUS_TIMEOUT;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_usb_receive(nsx_usb_config_t *cfg, void *data, uint32_t len,
                          uint32_t *bytes_received) {
    if (cfg == NULL || !cfg->_initialized) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (data == NULL || len == 0) {
        if (bytes_received != NULL) {
            *bytes_received = 0;
        }
        return NSX_STATUS_SUCCESS;
    }

    uint32_t timeout_ms = cfg->timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = NSX_USB_DEFAULT_TIMEOUT_MS;
    }

    uint32_t total_rx = 0;
    uint32_t elapsed_ms = 0;

    while (total_rx < len && elapsed_ms < timeout_ms) {
        uint32_t avail = tud_cdc_available();
        if (avail > 0) {
            uint32_t want = len - total_rx;
            if (want > avail) {
                want = avail;
            }

            nsx_usb_guard_enter();
            tud_task();
            uint32_t got = tud_cdc_read((uint8_t *)data + total_rx, want);
            nsx_usb_guard_exit();

            total_rx += got;
            cfg->_rx_ready = 0;
        } else {
            am_util_delay_ms(1);
            elapsed_ms++;
        }
    }

    if (bytes_received != NULL) {
        *bytes_received = total_rx;
    }
    if (total_rx == 0 && elapsed_ms >= timeout_ms) {
        return NSX_USB_STATUS_TIMEOUT;
    }
    if (total_rx < len) {
        return NSX_USB_STATUS_PARTIAL;
    }
    return NSX_STATUS_SUCCESS;
}

bool nsx_usb_connected(nsx_usb_config_t *cfg) {
    (void)cfg;
    return tud_cdc_connected();
}

bool nsx_usb_data_available(nsx_usb_config_t *cfg) {
    if (cfg == NULL) {
        return false;
    }
    return (cfg->_rx_ready != 0) || (tud_cdc_available() > 0);
}

void nsx_usb_flush_rx(nsx_usb_config_t *cfg) {
    if (cfg == NULL) {
        return;
    }
    tud_cdc_read_flush();
    cfg->_rx_ready = 0;
}

uint32_t nsx_usb_read_nb(nsx_usb_config_t *cfg, void *data, uint32_t max_len,
                          uint32_t *bytes_read) {
    if (bytes_read != NULL) {
        *bytes_read = 0;
    }
    if (cfg == NULL || !cfg->_initialized || data == NULL || max_len == 0) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    nsx_usb_guard_enter();
    tud_task();
    uint32_t avail = tud_cdc_available();
    uint32_t got = 0;
    if (avail > 0) {
        uint32_t want = (avail < max_len) ? avail : max_len;
        got = tud_cdc_read(data, want);
    }
    nsx_usb_guard_exit();

    if (bytes_read != NULL) {
        *bytes_read = got;
    }
    cfg->_rx_ready = 0;
    return NSX_STATUS_SUCCESS;
}

uint8_t *nsx_usb_get_rx_buffer(void) {
    return (g_usb_cfg != NULL) ? g_usb_cfg->rx_buffer : NULL;
}

uint8_t *nsx_usb_get_tx_buffer(void) {
    return (g_usb_cfg != NULL) ? g_usb_cfg->tx_buffer : NULL;
}

uint32_t nsx_usb_get_cdc_rx_buffer_length(void) {
    return (g_usb_cfg != NULL) ? g_usb_cfg->rx_buffer_len : 0;
}

uint32_t nsx_usb_get_cdc_tx_buffer_length(void) {
    return (g_usb_cfg != NULL) ? g_usb_cfg->tx_buffer_len : 0;
}

uint32_t nsx_usb_vendor_send(nsx_usb_config_t *cfg, const void *data,
                              uint32_t len, uint32_t *bytes_sent) {
    if (cfg == NULL || !cfg->_initialized) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (data == NULL || len == 0) {
        if (bytes_sent != NULL) {
            *bytes_sent = 0;
        }
        return NSX_STATUS_SUCCESS;
    }

    uint32_t timeout_ms = cfg->timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = NSX_USB_DEFAULT_TIMEOUT_MS;
    }

    const uint8_t *src = (const uint8_t *)data;
    uint32_t remaining = len;
    uint32_t sent = 0;
    uint32_t elapsed_ms = 0;

    while (remaining > 0 && elapsed_ms < timeout_ms) {
        nsx_usb_guard_enter();
        uint32_t avail = tud_vendor_write_available();
        uint32_t chunk = 0;
        if (avail > 0) {
            uint32_t want = (remaining < avail) ? remaining : avail;
            chunk = tud_vendor_write(src + sent, want);
            tud_vendor_flush();
        }
        tud_task();
        nsx_usb_guard_exit();

        if (chunk > 0) {
            sent += chunk;
            remaining -= chunk;
        } else {
            am_util_delay_ms(1);
            elapsed_ms++;
        }
    }

    if (bytes_sent != NULL) {
        *bytes_sent = sent;
    }
    if (remaining > 0) {
        return NSX_USB_STATUS_TIMEOUT;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_usb_vendor_write_available(nsx_usb_config_t *cfg) {
    uint32_t available;

    if (cfg == NULL || !cfg->_initialized || !cfg->_vendor_connected) {
        return 0u;
    }
    nsx_usb_guard_enter();
    tud_task();
    available = tud_vendor_write_available();
    nsx_usb_guard_exit();
    return available;
}

uint32_t nsx_usb_vendor_read_nb(nsx_usb_config_t *cfg, void *data,
                                 uint32_t max_len, uint32_t *bytes_read) {
    if (bytes_read != NULL) {
        *bytes_read = 0;
    }
    if (cfg == NULL || !cfg->_initialized || data == NULL || max_len == 0) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    nsx_usb_guard_enter();
    tud_task();
    uint32_t avail = tud_vendor_available();
    uint32_t got = 0;
    if (avail > 0) {
        uint32_t want = (avail < max_len) ? avail : max_len;
        got = tud_vendor_read(data, want);
    }
    nsx_usb_guard_exit();

    if (bytes_read != NULL) {
        *bytes_read = got;
    }
    cfg->_vendor_rx_ready = 0;
    return NSX_STATUS_SUCCESS;
}

bool nsx_usb_vendor_connected(nsx_usb_config_t *cfg) {
    if (cfg == NULL) {
        return false;
    }
    return cfg->_vendor_connected != 0;
}

bool nsx_usb_vendor_data_available(nsx_usb_config_t *cfg) {
    if (cfg == NULL) {
        return false;
    }
    return (cfg->_vendor_rx_ready != 0) || (tud_vendor_available() > 0);
}
