#ifndef __FLASH_CONFIG_H
#define __FLASH_CONFIG_H

#define FLASH_BASE_ADDR 				0x08000000
#define FLASH_SIZE_KB 					128
#define FLASH_BOOTLDR_PAYLOAD_SIZE_KB 	120
#define FLASH_BOOTLDR_SIZE_KB 			8

#define USB_VID 						0x0483
#define USB_PID							0xDF11

#define WINUSB_SUPPORT					1
#define ENABLE_DFU_UPLOAD				1
#define ENABLE_CHECKSUM					1
#define ENABLE_SPI_SRAM_IMAGE			1

#define VERSION		"01.00"

/* Default chunk size (in bytes) used by streaming/read-in-chunks helpers.
 * Must be a multiple of 4. Can be overridden in a board header or via
 * compiler -D option if you need a different value for memory/stack limits.
 */
#ifndef SRAM_IMAGE_CHUNK_BYTES
#define SRAM_IMAGE_CHUNK_BYTES 256
#endif

#endif // __FLASH_CONFIG_H
