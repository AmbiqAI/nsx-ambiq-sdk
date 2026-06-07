#ifndef NSX_USB_HOOKS_H
#define NSX_USB_HOOKS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t *nsx_usb_get_rx_buffer(void);
uint8_t *nsx_usb_get_tx_buffer(void);
uint32_t nsx_usb_get_cdc_rx_buffer_length(void);
uint32_t nsx_usb_get_cdc_tx_buffer_length(void);

#ifdef __cplusplus
}
#endif

#endif /* NSX_USB_HOOKS_H */
