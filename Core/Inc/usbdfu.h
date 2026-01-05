#ifndef __USBDFU_H
#define __USBDFU_H

#include <stdint.h>
#include "usb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DFU control request handler implemented in usbdfu.c */
enum usbd_request_return_codes usbdfu_control_request(struct usb_setup_data *req, uint16_t *len, void (**complete)(struct usb_setup_data *req));

#ifdef __cplusplus
}
#endif

#endif // __USBDFU_H
