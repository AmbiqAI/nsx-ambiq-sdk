/**
 * @file nsx_usb.h
 * @brief USB CDC + Vendor (WebUSB) driver for Ambiq SoCs.
 *
 * Provides a CDC serial interface and an optional Vendor class interface
 * (for WebUSB / WinUSB) backed by TinyUSB.  All device strings, VID/PID,
 * and the WebUSB landing-page URL are configurable at init time. No
 * values are hardcoded into the driver.
 *
 * ## DTR (Data Terminal Ready)
 * The CDC `nsx_usb_connected()` check relies on the host asserting DTR.
 * Most terminal emulators (minicom, screen, PuTTY) assert DTR on open,
 * but **pyserial does NOT by default**.  When opening a port with pyserial
 * you must explicitly set `ser.dtr = True` (or pass `dsrdtr=True`) before
 * the device will report as connected.  Without DTR, `tud_cdc_connected()`
 * returns false and send/receive will return NSX_USB_STATUS_NOT_CONNECTED.
 */
#ifndef NSX_USB_H
#define NSX_USB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_USB_STATUS_TIMEOUT       0x100
#define NSX_USB_STATUS_NOT_CONNECTED 0x101
#define NSX_USB_STATUS_PARTIAL       0x102

#define NSX_USB_DEFAULT_POLL_US  1000
#define NSX_USB_DEFAULT_TIMEOUT_MS  5000
#define NSX_USB_MIN_CDC_RX_BUFSIZE  1024

typedef struct {
    uint16_t    vid;
    uint16_t    pid;
    const char *manufacturer;
    const char *product;
    const char *serial;
    const char *cdc_interface;
    const char *vendor_interface;
    const char *webusb_url;
} nsx_usb_device_desc_t;

typedef struct nsx_usb_config nsx_usb_config_t;
typedef void (*nsx_usb_rx_cb_t)(nsx_usb_config_t *cfg);
typedef void (*nsx_usb_vendor_rx_cb_t)(nsx_usb_config_t *cfg);

struct nsx_usb_config {
    uint8_t            *tx_buffer;
    uint32_t            tx_buffer_len;
    uint8_t            *rx_buffer;
    uint32_t            rx_buffer_len;
    uint32_t            poll_interval_us;
    uint32_t            timeout_ms;
    nsx_usb_rx_cb_t     rx_cb;
    nsx_usb_vendor_rx_cb_t vendor_rx_cb;
    const nsx_usb_device_desc_t *device_desc;
    void               *user_ctx;
    uint8_t             _initialized;
    volatile uint8_t    _rx_ready;
    volatile uint8_t    _vendor_rx_ready;
    volatile uint8_t    _vendor_connected;
};

uint32_t nsx_usb_init(nsx_usb_config_t *cfg);
uint32_t nsx_usb_send(nsx_usb_config_t *cfg, const void *data, uint32_t len,
                       uint32_t *bytes_sent);
uint32_t nsx_usb_receive(nsx_usb_config_t *cfg, void *data, uint32_t len,
                          uint32_t *bytes_received);
bool nsx_usb_connected(nsx_usb_config_t *cfg);
bool nsx_usb_data_available(nsx_usb_config_t *cfg);
uint32_t nsx_usb_read_nb(nsx_usb_config_t *cfg, void *data, uint32_t max_len,
                          uint32_t *bytes_read);
void nsx_usb_flush_rx(nsx_usb_config_t *cfg);
uint32_t nsx_usb_vendor_send(nsx_usb_config_t *cfg, const void *data,
                              uint32_t len, uint32_t *bytes_sent);
uint32_t nsx_usb_vendor_read_nb(nsx_usb_config_t *cfg, void *data,
                                 uint32_t max_len, uint32_t *bytes_read);
bool nsx_usb_vendor_connected(nsx_usb_config_t *cfg);
bool nsx_usb_vendor_data_available(nsx_usb_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
