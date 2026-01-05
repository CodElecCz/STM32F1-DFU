#include <string.h>
#include <stdint.h>
#include "flash_config.h"
#include "usb.h"
#include "usbdfu.h"
#include "flash.h"
#include "reboot.h"
#include "checksum.h"

/* Commands sent with wBlockNum == 0 as per ST implementation. */
#define CMD_SETADDR	0x21
#define CMD_ERASE	0x41

// USB control data buffer
uint8_t usbd_control_buffer[DFU_TRANSFER_SIZE];

// DFU state and program buffer
static enum dfu_state usbdfu_state = STATE_DFU_IDLE;
static struct {
    uint8_t buf[DFU_TRANSFER_SIZE];
    uint16_t len;
    uint32_t addr;
    uint16_t blocknum;
} prog;

// Forward declaration for complete callback
static void usbdfu_getstatus_complete(struct usb_setup_data *req);

static uint8_t usbdfu_getstatus(uint32_t *bwPollTimeout)
{
    switch (usbdfu_state)
    {
    case STATE_DFU_DNLOAD_SYNC:
        usbdfu_state = STATE_DFU_DNBUSY;
        *bwPollTimeout = 100;
        return DFU_STATUS_OK;
    case STATE_DFU_MANIFEST_SYNC:
        // Device will reset when read is complete.
        usbdfu_state = STATE_DFU_MANIFEST;
        return DFU_STATUS_OK;
    case STATE_DFU_ERROR:
        return STATE_DFU_ERROR;
    default:
        return DFU_STATUS_OK;
    }
}

static void _full_system_reset()
{
    volatile uint32_t *_scb_aircr = (uint32_t*)0xE000ED0CU;
    *_scb_aircr = 0x05FA0000 | 0x4;
    while(1);
    __builtin_unreachable();
}

static void usbdfu_getstatus_complete(struct usb_setup_data *req)
{
    (void)req;

    const uint32_t start_addr = FLASH_BASE_ADDR + (FLASH_BOOTLDR_SIZE_KB*1024);
    const uint32_t end_addr   = FLASH_BASE_ADDR + (        FLASH_SIZE_KB*1024);

    switch (usbdfu_state)
    {
    case STATE_DFU_DNBUSY:
        _flash_unlock();
        if (prog.blocknum == 0)
        {
            switch (prog.buf[0])
            {
            case CMD_ERASE:
                {
#ifdef ENABLE_SAFEWRITE
                    check_do_erase();
#endif
                    uint32_t baseaddr = *(uint32_t *)(prog.buf + 1);
                    if (baseaddr >= start_addr && baseaddr + DFU_TRANSFER_SIZE <= end_addr)
                    {
                        if (!_flash_page_is_erased(baseaddr))
                            _flash_erase_page(baseaddr);
                    }
                }
                break;
            case CMD_SETADDR:
                prog.addr = *(uint32_t *)(prog.buf + 1);
                break;
            }
        }
        else
        {
#ifdef ENABLE_SAFEWRITE
            check_do_erase();
#endif
            uint32_t baseaddr = prog.addr + ((prog.blocknum - 2) * DFU_TRANSFER_SIZE);

            if (baseaddr >= start_addr && baseaddr + prog.len <= end_addr)
            {
                if (!_flash_page_is_erased(baseaddr))
                    _flash_erase_page(baseaddr);
                _flash_program_buffer(baseaddr, (uint16_t*)prog.buf, prog.len);
            }
        }
        _flash_lock();
        usbdfu_state = STATE_DFU_DNLOAD_IDLE;
        return;
    case STATE_DFU_MANIFEST:
        clear_reboot_flags();
        _full_system_reset();
        return;
    default:
        return;
    }
}

enum usbd_request_return_codes usbdfu_control_request(struct usb_setup_data *req, uint16_t *len, void (**complete)(struct usb_setup_data *req))
{
    switch (req->bRequest)
    {
    case DFU_DNLOAD:
        if ((len == NULL) || (*len == 0))
        {
            usbdfu_state = STATE_DFU_MANIFEST_SYNC;
            *complete = usbdfu_getstatus_complete;
            return USBD_REQ_HANDLED;
        }
        else
        {
            prog.blocknum = req->wValue;
            prog.len = *len;
            if (prog.len > sizeof(prog.buf))
                prog.len = sizeof(prog.buf);
            memcpy(prog.buf, usbd_control_buffer, prog.len);
            usbdfu_state = STATE_DFU_DNLOAD_SYNC;
            return USBD_REQ_HANDLED;
        }
    case DFU_CLRSTATUS:
        if (usbdfu_state == STATE_DFU_ERROR)
            usbdfu_state = STATE_DFU_IDLE;
        return USBD_REQ_HANDLED;
    case DFU_ABORT:
        usbdfu_state = STATE_DFU_IDLE;
        return USBD_REQ_HANDLED;
    case DFU_DETACH:
        usbdfu_state = STATE_DFU_MANIFEST_SYNC;
        *complete = usbdfu_getstatus_complete;
        return USBD_REQ_HANDLED;
    case DFU_UPLOAD:
        usbdfu_state = STATE_DFU_UPLOAD_IDLE;
        if (!req->wValue)
        {
            usbd_control_buffer[0] = 0x00;
            usbd_control_buffer[1] = CMD_SETADDR;
            usbd_control_buffer[2] = CMD_ERASE;
            *len = 3;
            return USBD_REQ_HANDLED;
        }
        else
        {
#ifndef ENABLE_DFU_UPLOAD
            usbdfu_state = STATE_DFU_ERROR;
            *len = 0;
#else
            uint32_t baseaddr = prog.addr + ((req->wValue - 2) * DFU_TRANSFER_SIZE);
            const uint32_t start_addr = FLASH_BASE_ADDR + (FLASH_BOOTLDR_SIZE_KB*1024);
            const uint32_t end_addr   = FLASH_BASE_ADDR + (        FLASH_SIZE_KB*1024);
            if (baseaddr >= start_addr && baseaddr + DFU_TRANSFER_SIZE <= end_addr)
            {
                memcpy(usbd_control_buffer, (void*)baseaddr, DFU_TRANSFER_SIZE);
                *len = DFU_TRANSFER_SIZE;
            }
            else
            {
                usbdfu_state = STATE_DFU_ERROR;
                *len = 0;
            }
#endif
        }
        return USBD_REQ_HANDLED;
    case DFU_GETSTATUS:
        {
            uint32_t bwPollTimeout = 0;
            usbd_control_buffer[0] = usbdfu_getstatus(&bwPollTimeout);
            usbd_control_buffer[1] = bwPollTimeout & 0xFF;
            usbd_control_buffer[2] = (bwPollTimeout >> 8) & 0xFF;
            usbd_control_buffer[3] = (bwPollTimeout >> 16) & 0xFF;
            usbd_control_buffer[4] = usbdfu_state;
            usbd_control_buffer[5] = 0;
            *len = 6;
            *complete = usbdfu_getstatus_complete;
            return USBD_REQ_HANDLED;
        }
    case DFU_GETSTATE:
        usbd_control_buffer[0] = usbdfu_state;
        *len = 1;
        return USBD_REQ_HANDLED;
    }

    return USBD_REQ_NEXT_CALLBACK;
}
