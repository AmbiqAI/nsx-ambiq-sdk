#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nsx_core.h"
#include "nsx_timer.h"
#include "nsx_usb.h"
#include "tusb.h"

/* Model the SDK's finite FIFO, short writes and asynchronous host draining. */
#define CAPACITY 16u
#define MAX_PAYLOAD 65537u
const int nsx_timer_V1_0_0 = 0;
static uint8_t fifo[CAPACITY], wire[MAX_PAYLOAD + CAPACITY];
static unsigned queued, delivered, serviced, delayed, flushed;
static unsigned drain_budget = MAX_PAYLOAD + CAPACITY, write_limit = CAPACITY;
static unsigned wait_before_drain;
uint32_t nsx_timer_init(nsx_timer_config_t *cfg) {(void)cfg; return 0;}
bool tud_ready(void) {return true;}
bool tusb_init(void) {return true;}
void am_util_delay_ms(uint32_t ms) {delayed += ms;}
void tud_task(void) {
    ++serviced;
    if (delayed < wait_before_drain) return;
    unsigned count = queued < 8 ? queued : 8;
    if (count > drain_budget) count = drain_budget;
    assert(delivered + count <= sizeof(wire));
    memcpy(wire + delivered, fifo, count);
    delivered += count;
    queued -= count;
    drain_budget -= count;
    memmove(fifo, fifo + count, queued);
}
uint32_t tud_cdc_write_available(void) {return CAPACITY - queued;}
uint32_t tud_cdc_write(const void *data, uint32_t len) {
    /* Actual SDK FIFO API narrows the CDC uint32_t length to uint16_t. */
    unsigned count = (uint16_t)len;
    if (count > CAPACITY - queued) count = CAPACITY - queued;
    if (count > write_limit) count = write_limit;
    memcpy(fifo + queued, data, count);
    queued += count;
    return count;
}
uint32_t tud_cdc_write_flush(void) {++flushed; return 0;}
uint32_t tud_cdc_available(void) {return 0;}
uint32_t tud_cdc_read(void *data, uint32_t len) {(void)data; (void)len; return 0;}
void tud_cdc_read_flush(void) {}
bool tud_cdc_connected(void) {return true;}
uint32_t tud_vendor_available(void) {return 0;}
uint32_t tud_vendor_write_available(void) {return 0;}
uint32_t tud_vendor_write(const void *data, uint32_t len) {(void)data; (void)len; return 0;}
uint32_t tud_vendor_flush(void) {return 0;}
uint32_t tud_vendor_read(void *data, uint32_t len) {(void)data; (void)len; return 0;}

int main(int argc, char **argv) {
    assert(argc == 2);
    uint8_t rx[1024], tx[CAPACITY];
    static uint8_t payload[MAX_PAYLOAD];
    for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = (uint8_t)(i * 17u + i / 251u);
    nsx_usb_config_t cfg = {.rx_buffer=rx, .tx_buffer=tx,
        .rx_buffer_len=sizeof(rx), .tx_buffer_len=sizeof(tx), .timeout_ms=5};
    assert(nsx_usb_init(&cfg) == 0);
    unsigned len = 7, prefix = 0, expected_wait = 0;
    uint32_t expected_status = NSX_STATUS_SUCCESS;
    if (!strcmp(argv[1], "boundary")) len = CAPACITY;
    else if (!strcmp(argv[1], "large")) len = 97;
    else if (!strcmp(argv[1], "wide-length")) len = MAX_PAYLOAD;
    else if (!strcmp(argv[1], "initial-full")) {
        queued = prefix = CAPACITY;
        memset(fifo, 0xa5, sizeof(fifo));
        wait_before_drain = 2;
    } else if (!strcmp(argv[1], "short-write")) {len = 97; write_limit = 3;}
    else if (!strcmp(argv[1], "partial")) {
        len = 97; drain_budget = 8; expected_wait = cfg.timeout_ms;
        expected_status = NSX_USB_STATUS_TIMEOUT;
    } else if (!strcmp(argv[1], "stalled") || !strcmp(argv[1], "default-timeout")) {
        queued = prefix = CAPACITY;
        memset(fifo, 0xa5, sizeof(fifo));
        drain_budget = 0;
        if (!strcmp(argv[1], "default-timeout")) cfg.timeout_ms = 0;
        expected_wait = cfg.timeout_ms ? cfg.timeout_ms : NSX_USB_DEFAULT_TIMEOUT_MS;
        expected_status = NSX_USB_STATUS_TIMEOUT;
    } else assert(!strcmp(argv[1], "fit"));
    uint32_t sent = UINT32_MAX;
    assert(nsx_usb_send(&cfg, payload, len, &sent) == expected_status);
    unsigned accepted = delivered + queued - prefix;
    assert(sent == accepted);
    assert(sent == (expected_status == NSX_STATUS_SUCCESS ? len : (prefix ? 0u : 24u)));
    /* Success means accepted into the transport, not acknowledged by the host. */
    for (unsigned i = 0; i < delivered + queued; ++i) {
        uint8_t byte = i < delivered ? wire[i] : fifo[i - delivered];
        assert(byte == (i < prefix ? 0xa5 : payload[i - prefix]));
    }
    if (expected_status == NSX_USB_STATUS_TIMEOUT) assert(delayed == expected_wait);
    else assert(delayed < cfg.timeout_ms);
    if (prefix && expected_status == NSX_STATUS_SUCCESS) assert(delayed >= wait_before_drain);
    assert(serviced > 0);
#if defined(AM_PART_APOLLO5B)
    assert(flushed == serviced + 1); /* Preserve the family-specific final flush. */
#else
    assert(flushed == serviced);
#endif
    puts("PASS");
}
