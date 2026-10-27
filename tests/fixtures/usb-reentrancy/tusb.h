#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CONTROL_STAGE_SETUP 1
#define TUSB_REQ_TYPE_VENDOR 2
#define TUSB_REQ_TYPE_CLASS 1
typedef struct { struct {uint8_t type;} bmRequestType_bit; uint8_t bRequest; uint16_t wIndex,wValue; } tusb_control_request_t;
bool tusb_init(void);
void tud_task(void);
uint32_t tud_cdc_available(void);
uint32_t tud_cdc_write_available(void);
uint32_t tud_cdc_write(const void *, uint32_t);
uint32_t tud_cdc_write_flush(void);
uint32_t tud_cdc_read(void *, uint32_t);
void tud_cdc_read_flush(void);
bool tud_cdc_connected(void);
uint32_t tud_vendor_available(void);
uint32_t tud_vendor_write_available(void);
uint32_t tud_vendor_write(const void *, uint32_t);
uint32_t tud_vendor_flush(void);
uint32_t tud_vendor_read(void *, uint32_t);
bool tud_control_xfer(uint8_t, const tusb_control_request_t *, void *, uint16_t);
bool tud_control_status(uint8_t, const tusb_control_request_t *);
void tud_cdc_rx_cb(uint8_t);
void tud_vendor_rx_cb(uint8_t, const uint8_t *, uint16_t);
