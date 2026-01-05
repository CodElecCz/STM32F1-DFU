/*
 * USB Device Firmware Upgrade (DFU) class driver
 *
 * Copyright (C) 2011-2014  Antonin B. (Hacker Noon)
 * Copyright (C) 2019  STMicroelectronics
 *
 * This file is part of the libopencm3 project.
 *
 * The libopencm3 project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * The libopencm3 project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
 * Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with the libopencm3 project. If not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>.
 */
#include <string.h>

#include "flash_config.h"
#include "usb.h"

// Defined in main
extern uint8_t usbd_control_buffer[DFU_TRANSFER_SIZE];
extern const char * const _usb_strings[5];
extern enum usbd_request_return_codes
usbdfu_control_request(struct usb_setup_data *req,
		uint16_t *len, void (**complete)(struct usb_setup_data *req));

// Simple builtin fns
size_t strlen(const char *s) {
	size_t ret = 0;
	while (*s++)
		ret++;
	return ret;
}

const struct usb_device_descriptor dev_desc = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = USB_VID,
	.idProduct = USB_PID,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

const struct {
	struct usb_config_descriptor config;
	struct usb_interface_descriptor iface;
	struct usb_dfu_descriptor dfu_function;
} config_desc = {
	.config = {
		.bLength = USB_DT_CONFIGURATION_SIZE,
		.bDescriptorType = USB_DT_CONFIGURATION,
		.wTotalLength = sizeof(config_desc),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 5,
		.bmAttributes = 0xC0,
		.bMaxPower = 0x32,
	},
	.iface = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0,
		.bInterfaceClass = 0xFE, /* Device Firmware Upgrade */
		.bInterfaceSubClass = 1,
		.bInterfaceProtocol = 2,
		.iInterface = 4,
	},
	.dfu_function = {
		.bLength = sizeof(struct usb_dfu_descriptor),
		.bDescriptorType = DFU_FUNCTIONAL,
		.bmAttributes =
			#ifdef ENABLE_DFU_UPLOAD
			USB_DFU_CAN_UPLOAD |
			#endif
			USB_DFU_CAN_DOWNLOAD |
			USB_DFU_WILL_DETACH,
		.wDetachTimeout = 255,
		.wTransferSize = DFU_TRANSFER_SIZE,
		.bcdDFUVersion = 0x011A,
	},
};

// USB FSM state
enum {
	IDLE, STALLED,
	DATA_IN, LAST_DATA_IN, STATUS_IN,
	DATA_OUT, LAST_DATA_OUT, STATUS_OUT,
} usb_fsm_state = IDLE;
uint16_t datasize = 0;
uint16_t dataoff = 0;
uint16_t usb_pm_top = 0;
uint8_t  usb_needs_zlp = 0;
struct usb_setup_data usb_req;
uint8_t usb_force_nak[8] = {0};
void (*usb_complete_cb)(struct usb_setup_data *req) = 0;

#define RCC_APB1ENR  (*(volatile uint32_t*)0x4002101CU)
#define RCC_USB   23

#define rcc_periph_enable(pn) RCC_APB1ENR |= (1 << (pn));

void usb_init() {
	rcc_periph_enable(RCC_USB);
	SET_REG(USB_CNTR_REG, 0);
	SET_REG(USB_BTABLE_REG, 0);
	SET_REG(USB_ISTR_REG, 0);

	/* Enable RESET, SUSPEND, RESUME and CTR interrupts. */
	SET_REG(USB_CNTR_REG, USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM);
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define USBD_PM_TOP 0x40

/* Packet Memory Area (PMA) layout and access notes
 * -------------------------------------------------
 * The STM32 USB FS peripheral exposes a dedicated Packet Memory Area
 * (PMA) which is accessed with 16-bit reads/writes. This driver follows
 * the same PMA addressing convention used by many STM32 examples:
 *
 * - Logical 16-bit data words live in every other 16-bit slot when the
 *   PMA is viewed as a uint16_t array. Concretely, logical words are at
 *   PM[0], PM[2], PM[4], ... (when PM is a `uint16_t *`).
 * - To advance to the next logical PMA word you must add 2 to the
 *   uint16_t pointer (e.g. PM += 2). The driver therefore uses steps of
 *   two 16-bit elements for each logical word.
 * - Odd trailing bytes are handled by reading/writing the low byte of
 *   the subsequent PMA word. That is, for an odd-length transfer the
 *   final leftover byte is stored in the low 8 bits of the next PMA
 *   16-bit location.
 * - Accesses to PMA must use volatile-qualified pointers so the
 *   compiler does not optimize or reorder the hardware memory accesses.
 * - The caller should pass PMA buffer pointers obtained via the
 *   USB_GET_EP_*_BUFF macros (these yield the correct PMA addresses
 *   used throughout this driver).
 *
 * These conventions are preserved by `st_usbfs_copy_to_pm` and
 * `st_usbfs_copy_from_pm` below; do not change the pointer arithmetic
 * unless you also update all PMA buffer setup and endpoint code.
 */

/* Helper: compute pointer to logical PMA word index
 * PMA stores logical words every other uint16_t slot. Use index-based
 * access where `index` is the logical word number (0..N-1).
 */
#define PMA_WORD(base, index) (*((volatile uint16_t *)(base) + ((index) << 1)))
#define PMA_WORD_CONST(base, index) (*((const volatile uint16_t *)(base) + ((index) << 1)))

static void st_usbfs_copy_to_pm(volatile void *vPM, const void *buf, uint16_t len)
{
    const uint8_t *src = (const uint8_t *)buf;
    volatile void *pma_base = vPM;

    if (!len)
        return;

    uint16_t words = len >> 1; /* number of full 16-bit words */
    uint16_t idx = 0; /* logical PMA word index */

    /* Copy full 16-bit words. Unroll loop to copy two words per iteration
     * to reduce loop overhead and improve throughput on small embedded
     * CPUs.
     */
    while (words >= 2) {
        uint16_t w0 = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
        uint16_t w1 = (uint16_t)src[2] | ((uint16_t)src[3] << 8);

        PMA_WORD(pma_base, idx) = w0;
        PMA_WORD(pma_base, idx + 1) = w1;

        src += 4;
        idx += 2;
        words -= 2;
    }

    /* Copy remaining single 16-bit word if present */
    if (words) {
        uint16_t w = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
        PMA_WORD(pma_base, idx) = w;
        idx += 1;
        src += 2;
    }

    /* If there's an odd trailing byte, write it as a 16-bit word with the
     * high byte zeroed. This matches the behaviour expected by the PMA
     * layout and other code in this file which reads an odd trailing byte
     * using a byte read from the PMA.
     */
    if (len & 1) {
        uint16_t w = (uint16_t)src[0];
        PMA_WORD(pma_base, idx) = w;
    }
}

static void st_usbfs_copy_from_pm(void *buf, const volatile void *vPM, uint16_t len)
{
    uint16_t *dst = (uint16_t *)buf;
    const volatile void *pma_base = vPM;
    uint16_t words = len >> 1;
    uint16_t idx = 0; /* logical PMA word index */

    /* Copy two 16-bit words per iteration to reduce loop overhead and
     * minimize volatile memory accesses.
     */
    while (words >= 2) {
        uint16_t w0 = PMA_WORD_CONST(pma_base, idx);
        uint16_t w1 = PMA_WORD_CONST(pma_base, idx + 1);

        dst[0] = w0;
        dst[1] = w1;

        dst += 2;
        idx += 2;
        words -= 2;
    }

    /* Handle remaining single 16-bit word, if any. */
    if (words) {
        *dst++ = PMA_WORD_CONST(pma_base, idx);
        idx += 1;
    }

    /* If there's an odd trailing byte, read the low byte from current PM address. */
    if (len & 1) {
        /* Read the low byte of the next logical PMA word. Cast via byte
         * pointer to ensure we read the low 8 bits only.
         */
        const volatile uint8_t *bytep = (const volatile uint8_t *)(((const volatile uint16_t *)pma_base) + (idx << 1));
        *(uint8_t *)dst = *bytep;
    }
}

static uint16_t _usbd_ep_write_packet(uint8_t addr, const void *buf, uint16_t len)
{
	addr &= 0x7F;

	if ((*USB_EP_REG(addr) & USB_EP_TX_STAT) == USB_EP_TX_STAT_VALID)
		return 0;

	st_usbfs_copy_to_pm(USB_GET_EP_TX_BUFF(addr), buf, len);
	USB_SET_EP_TX_COUNT(addr, len);
	USB_SET_EP_TX_STAT(addr, USB_EP_TX_STAT_VALID);

	return len;
}

static uint16_t _usbd_ep_read_packet(uint8_t addr, void *buf, uint16_t len)
{
	if ((*USB_EP_REG(addr) & USB_EP_RX_STAT) == USB_EP_RX_STAT_VALID)
		return 0;

	len = MIN(USB_GET_EP_RX_COUNT(addr) & 0x3ff, len);
	st_usbfs_copy_from_pm(buf, USB_GET_EP_RX_BUFF(addr), len);
	USB_CLR_EP_RX_CTR(addr);

	if (!usb_force_nak[addr])
	{
		USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
	}
	return len;
}

static void _usbd_ep_nak_set(uint8_t addr, uint8_t nak)
{
	// It does not make sense to force NAK on IN endpoints.
	if (addr & 0x80)
		return;

	usb_force_nak[addr] = nak;
	if (nak)
		USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_NAK);
	else
		USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
}

void _ep_stall_set(uint8_t addr, uint8_t stall)
{
	if (addr == 0)
		USB_SET_EP_TX_STAT(addr, stall ? USB_EP_TX_STAT_STALL : USB_EP_TX_STAT_NAK);

	if (addr & 0x80)
	{
		addr &= 0x7F;

		USB_SET_EP_TX_STAT(addr, stall ? USB_EP_TX_STAT_STALL : USB_EP_TX_STAT_NAK);

		// Reset to DATA0 if clearing stall condition.
		if (!stall)
			USB_CLR_EP_TX_DTOG(addr);
	}
	else
	{
		// Reset to DATA0 if clearing stall condition.
		if (!stall)
			USB_CLR_EP_RX_DTOG(addr);

		USB_SET_EP_RX_STAT(addr, stall ? USB_EP_RX_STAT_STALL : USB_EP_RX_STAT_VALID);
	}
}

uint8_t _ep_stall_get(uint8_t addr)
{
	if (addr & 0x80)
	{
		if ((*USB_EP_REG(addr & 0x7F) & USB_EP_TX_STAT) == USB_EP_TX_STAT_STALL)
			return 1;
	}
	else
	{
		if ((*USB_EP_REG(addr) & USB_EP_RX_STAT) == USB_EP_RX_STAT_STALL)
			return 1;
	}
	return 0;
}

static inline void _stall_transaction()
{
	_ep_stall_set(0, 1);
	usb_fsm_state = IDLE;
}


// Sends or keeps sending data to host
static void usb_control_send_chunk()
{
	if (dev_desc.bMaxPacketSize0 < datasize)
	{
		/* Data stage, normal transmission */
		_usbd_ep_write_packet(0, &usbd_control_buffer[dataoff], dev_desc.bMaxPacketSize0);
		usb_fsm_state = DATA_IN;
		dataoff += dev_desc.bMaxPacketSize0;
		datasize -= dev_desc.bMaxPacketSize0;
	}
	else
	{
		/* Data stage, end of transmission */
		_usbd_ep_write_packet(0, &usbd_control_buffer[dataoff], datasize);

		usb_fsm_state = usb_needs_zlp ? DATA_IN : LAST_DATA_IN;
		usb_needs_zlp = 0;
		datasize = 0;
	}
}

// Receives data from host
static int usb_control_recv_chunk()
{
	uint16_t packetsize = MIN(dev_desc.bMaxPacketSize0, usb_req.wLength - datasize);
	uint16_t size = _usbd_ep_read_packet(0, &usbd_control_buffer[datasize], packetsize);

	if (size != packetsize)
	{
		_stall_transaction();
		return -1;
	}

	datasize += size;
	return packetsize;
}

static enum usbd_request_return_codes usb_standard_get_descriptor()
{
	int array_idx, descr_idx, descr_type;
	struct usb_string_descriptor *sd = (struct usb_string_descriptor *)usbd_control_buffer;

	descr_idx = usb_req.wValue & 0xFF;
	descr_type = usb_req.wValue >> 8;

	switch (descr_type)
	{
	case USB_DT_DEVICE:
		memcpy(usbd_control_buffer, &dev_desc, sizeof(dev_desc));
		datasize = sizeof(dev_desc);
		return USBD_REQ_HANDLED;
	case USB_DT_CONFIGURATION:
		memcpy(usbd_control_buffer, &config_desc, sizeof(config_desc));
		datasize = sizeof(config_desc);
		return USBD_REQ_HANDLED;
	case USB_DT_STRING:
		sd->bDescriptorType = USB_DT_STRING;
		if (descr_idx == 0) {
			/* Send sane Language ID descriptor... */
			sd->wData[0] = USB_LANGID_ENGLISH_US;
			datasize = sd->bLength = sizeof(sd->bLength) + sizeof(sd->bDescriptorType) + sizeof(sd->wData[0]);
#ifdef WINUSB_SUPPORT
		}
		else if (descr_idx == 0xEE)
		{
			const char winusbstr[] = {'M','S','F','T','1','0','0','A','\0'};
			for (int i = 0; i < sizeof(winusbstr); i++)
				sd->wData[i] = winusbstr[i];
			datasize = sd->bLength = sizeof(sd->bLength) + sizeof(sd->bDescriptorType) + sizeof(winusbstr)*2;
#endif
		}
		else
		{
			array_idx = descr_idx - 1;

			/* Check that string index is in range. */
			if (array_idx >= sizeof(_usb_strings) / sizeof(_usb_strings[0]))
				return USBD_REQ_NOTSUPP;

			/* Strings with Language ID different from
			 * USB_LANGID_ENGLISH_US are not supported */
			if (usb_req.wIndex != USB_LANGID_ENGLISH_US)
				return USBD_REQ_NOTSUPP;

			/* This string is returned as UTF16, hence the
			 * multiplication
			 */
			unsigned numchars = strlen(_usb_strings[array_idx]);
			datasize = sd->bLength = numchars * 2 +
			          sizeof(sd->bLength) + sizeof(sd->bDescriptorType);

			for (int i = 0; i < numchars; i++)
				sd->wData[i] = _usb_strings[array_idx][i];
		}
		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

enum usbd_request_return_codes _usbd_standard_request_device()
{
	switch (usb_req.bRequest)
	{
	case USB_REQ_SET_ADDRESS:
		/* The actual address is only latched at the STATUS IN stage. */
		if ((usb_req.bmRequestType != 0) || (usb_req.wValue >= 128))
			return USBD_REQ_NOTSUPP;

		// Do not set addr here, wait for status IN
		// SET_REG(USB_DADDR_REG, (usb_req.wValue & USB_DADDR_ADDR) | USB_DADDR_EF);

		return USBD_REQ_HANDLED;
	case USB_REQ_SET_CONFIGURATION:
		// Reset all endpoints
		if (usb_req.wValue == config_desc.config.bConfigurationValue)
		{
			for (int i = 1; i < 8; i++) {
				USB_SET_EP_TX_STAT(i, USB_EP_TX_STAT_DISABLED);
				USB_SET_EP_RX_STAT(i, USB_EP_RX_STAT_DISABLED);
			}
			usb_pm_top = USBD_PM_TOP + (2 * dev_desc.bMaxPacketSize0);
			return USBD_REQ_HANDLED;
		}
		return USBD_REQ_NOTSUPP;
	case USB_REQ_GET_CONFIGURATION:
		usbd_control_buffer[0] = 1;  // FIXME?
		datasize = 1;
		return USBD_REQ_HANDLED;
	case USB_REQ_GET_DESCRIPTOR:
		return usb_standard_get_descriptor();
	case USB_REQ_GET_STATUS:
		// GET_STATUS always responds with zero reply.
		datasize = 2;
		usbd_control_buffer[0] = 0;
		usbd_control_buffer[1] = 0;
		return USBD_REQ_HANDLED;
	}

	return USBD_REQ_NOTSUPP;
}

enum usbd_request_return_codes _usbd_standard_request_interface()
{
	switch (usb_req.bRequest)
	{
	case USB_REQ_GET_INTERFACE:
		// command = usb_standard_get_interface;
		usbd_control_buffer[0] = 1;
		datasize = 1;
		return USBD_REQ_HANDLED;
	case USB_REQ_SET_INTERFACE:
		datasize = 0;
		return USBD_REQ_HANDLED;
	case USB_REQ_GET_STATUS:
		datasize = 2;
		usbd_control_buffer[0] = 0;
		usbd_control_buffer[1] = 0;
		break;
	}
	return USBD_REQ_NOTSUPP;
}

enum usbd_request_return_codes _usbd_standard_request_endpoint()
{
	switch (usb_req.bRequest)
	{
	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
		if (usb_req.wValue == USB_FEAT_ENDPOINT_HALT)
			_ep_stall_set(usb_req.wIndex, usb_req.bRequest == USB_REQ_SET_FEATURE);
		else
			return USBD_REQ_NOTSUPP;
		return USBD_REQ_HANDLED;
	case USB_REQ_GET_STATUS:
		usbd_control_buffer[0] = _ep_stall_get(usb_req.wIndex) ? 1 : 0;
		usbd_control_buffer[1] = 0;
		datasize = 2;
		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

enum usbd_request_return_codes _usbd_standard_request()
{
	if ((usb_req.bmRequestType & USB_REQ_TYPE_TYPE) != USB_REQ_TYPE_STANDARD)
		return USBD_REQ_NOTSUPP;

	switch (usb_req.bmRequestType & USB_REQ_TYPE_RECIPIENT)
	{
	case USB_REQ_TYPE_DEVICE:
		return _usbd_standard_request_device();
	case USB_REQ_TYPE_INTERFACE:
		return _usbd_standard_request_interface();
	case USB_REQ_TYPE_ENDPOINT:
		return _usbd_standard_request_endpoint();
	}
	return USBD_REQ_NOTSUPP;
}

static enum usbd_request_return_codes usb_control_request_dispatch()
{
	// Filter out
	const uint8_t type = USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE;
	const uint8_t mask = USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT;
	if ((usb_req.bmRequestType & mask) == type)
	{
		datasize = usb_req.wLength;
		int result = usbdfu_control_request(&usb_req, &datasize, &usb_complete_cb);
		if (result == USBD_REQ_HANDLED || result == USBD_REQ_NOTSUPP)
			return result;
	}

#ifdef WINUSB_SUPPORT
	const uint8_t wtype = USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE;
	const uint8_t wmask = USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT;
	if ((usb_req.bmRequestType & wmask) == wtype && usb_req.bRequest == 0x41 /* A */) {
		// From https://github.com/pbatard/libwdi/wiki/WCID-Devices
		const uint8_t winusb_desc[] = {
			0x28, 0x00, 0x00, 0x00,       // Descriptor length (32bit word) (40 bytes)
			0x00, 0x01,                   // bcdVersion (1.0)
			0x04, 0x00,                   // wIndex = 0x0004 (Compat ID descriptor Index)
			0x01,                         // Num of sections (1)
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Reserved (7bytes)
			0x00,                         // interface num (0)
			0x01,                         // Reserved
			0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, // compatibleID[8]    "WINUSB\0\0"
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // subCompatibleID[6] ""
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00  //Reserved
		};

		memcpy(usbd_control_buffer, winusb_desc, sizeof(winusb_desc));
		datasize = sizeof(winusb_desc);
		return USBD_REQ_HANDLED;
	}
#endif

	/* Try standard request if not already handled. */
	return _usbd_standard_request();
}

static uint8_t _needs_zlp(uint16_t len, uint16_t wLength, uint8_t ep_size) {
	if (len < wLength) {
		if (len && (len % ep_size == 0)) {
			return 1;
		}
	}
	return 0;
}

static void _usb_control_setup_read()
{
	unsigned maxdataout = usb_req.wLength;

	dataoff = 0; // Restart transmission counter
	if (usb_control_request_dispatch())
	{
		if (datasize > maxdataout)  // Truncate output
			datasize = maxdataout;

		if (maxdataout)
		{
			usb_needs_zlp = _needs_zlp(datasize, maxdataout, dev_desc.bMaxPacketSize0);
			/* Go to data out stage if handled. */
			usb_control_send_chunk();
		}
		else
		{
			/* Go to status stage if handled. */
			_usbd_ep_write_packet(0, 0, 0);
			usb_fsm_state = STATUS_IN;
		}
	}
	else
		_stall_transaction();  // Stall endpoint on failure.
}

static void _usb_control_setup_write()
{
	// Stall EP if we have too much data?
	if (usb_req.wLength > DFU_TRANSFER_SIZE)
	{
		_stall_transaction();
		return;
	}

	/* Buffer into which to write received data. */
	datasize = 0;
	/* Wait for DATA OUT stage. */
	if (usb_req.wLength > dev_desc.bMaxPacketSize0)
		usb_fsm_state = DATA_OUT;
	else
		usb_fsm_state = LAST_DATA_OUT;

	_usbd_ep_nak_set(0, 0);
}

static void _usbd_control_setup()
{
	usb_complete_cb = 0;
	_usbd_ep_nak_set(0, 1);

	if (_usbd_ep_read_packet(0, &usb_req, sizeof(usb_req)) != sizeof(usb_req))
	{
		_stall_transaction();
		return;
	}

	if ((usb_req.wLength == 0) || (usb_req.bmRequestType & 0x80))
		_usb_control_setup_read();
	else
		_usb_control_setup_write();
}

static void _usbd_control_out()
{
	switch (usb_fsm_state)
	{
	case DATA_OUT:
		if (usb_control_recv_chunk() < 0)
			break;

		// Check for last packet
		if ((usb_req.wLength - datasize) <= dev_desc.bMaxPacketSize0)
			usb_fsm_state = LAST_DATA_OUT;
		break;
	case LAST_DATA_OUT:
		if (usb_control_recv_chunk() < 0)
			break;
		/*
		 * We have now received the full data payload.
		 * Invoke callback to process.
		 */
		if (usb_control_request_dispatch()) {
			/* Go to status stage on success. */
			_usbd_ep_write_packet(0, 0, 0);
			usb_fsm_state = STATUS_IN;
		} else
			_stall_transaction();
		break;
	case STATUS_OUT:
		_usbd_ep_read_packet(0, 0, 0);
		usb_fsm_state = IDLE;
		if (usb_complete_cb)
			usb_complete_cb(&usb_req);

		usb_complete_cb = 0;
		break;
	default:
		_stall_transaction();
	}
}

static void _usbd_control_in()
{
	switch (usb_fsm_state)
	{
	case DATA_IN:
		usb_control_send_chunk();
		break;
	case LAST_DATA_IN:
		usb_fsm_state = STATUS_OUT;
		_usbd_ep_nak_set(0, 0);
		break;
	case STATUS_IN:
		if (usb_complete_cb)
			usb_complete_cb(&usb_req);

		/* Exception: Handle SET ADDRESS function here... */
		if ((usb_req.bmRequestType == 0) && (usb_req.bRequest == USB_REQ_SET_ADDRESS)) {
			/* Set device address and enable. */
			SET_REG(USB_DADDR_REG, (usb_req.wValue & USB_DADDR_ADDR) | USB_DADDR_EF);
		}
		usb_fsm_state = IDLE;
		break;
	default:
		_stall_transaction();
	}
}

void _set_ep_rx_bufsize(uint8_t ep, uint32_t size)
{
	if (size > 62) {
		if (size & 0x1f)
		{
			size -= 32;
		}
		USB_SET_EP_RX_COUNT(ep, (size << 5) | 0x8000);
	}
	else
	{
		if (size & 1)
		{
			size++;
		}
		USB_SET_EP_RX_COUNT(ep, size << 10);
	}
}

void _usbd_ep_setup(uint8_t addr, uint8_t type, uint16_t max_size)
{
	/* Translate USB standard type codes to STM32. */
	const uint16_t typelookup[] = {
		[USB_ENDPOINT_ATTR_CONTROL] = USB_EP_TYPE_CONTROL,
		[USB_ENDPOINT_ATTR_ISOCHRONOUS] = USB_EP_TYPE_ISO,
		[USB_ENDPOINT_ATTR_BULK] = USB_EP_TYPE_BULK,
		[USB_ENDPOINT_ATTR_INTERRUPT] = USB_EP_TYPE_INTERRUPT,
	};
	uint8_t dir = addr & 0x80;
	addr &= 0x7f;

	/* Assign address. */
	USB_SET_EP_ADDR(addr, addr);
	USB_SET_EP_TYPE(addr, typelookup[type]);

	if (dir || (addr == 0))
	{
		USB_SET_EP_TX_ADDR(addr, usb_pm_top);
		USB_CLR_EP_TX_DTOG(addr);
		USB_SET_EP_TX_STAT(addr, USB_EP_TX_STAT_NAK);
		usb_pm_top += max_size;
	}

	if (!dir)
	{
		USB_SET_EP_RX_ADDR(addr, usb_pm_top);
		_set_ep_rx_bufsize(addr, max_size);
		USB_CLR_EP_RX_DTOG(addr);
		USB_SET_EP_RX_STAT(addr, USB_EP_RX_STAT_VALID);
		usb_pm_top += max_size;
	}
}

void do_usb_poll()
{
	uint16_t istr = *USB_ISTR_REG;

	if (istr & USB_ISTR_RESET)
	{
		USB_CLR_ISTR_RESET();
		usb_pm_top = USBD_PM_TOP;

		_usbd_ep_setup(0, USB_ENDPOINT_ATTR_CONTROL, dev_desc.bMaxPacketSize0);
		// Set driver addr to zero
		SET_REG(USB_DADDR_REG, 0 | USB_DADDR_EF);

		return;
	}

	if (istr & USB_ISTR_CTR)
	{
		uint8_t ep = istr & USB_ISTR_EP_ID;
		if (istr & USB_ISTR_DIR)
		{
			if (*USB_EP_REG(ep) & USB_EP_SETUP)
				_usbd_control_setup();
			else
				_usbd_control_out();
		}
		else
		{
			USB_CLR_EP_TX_CTR(ep);
			_usbd_control_in();
		}
	}

	if (istr & USB_ISTR_SUSP)
		USB_CLR_ISTR_SUSP();

	if (istr & USB_ISTR_WKUP)
		USB_CLR_ISTR_WKUP();

	if (istr & USB_ISTR_SOF)
		USB_CLR_ISTR_SOF();

	*USB_CNTR_REG &= ~USB_CNTR_SOFM;
}