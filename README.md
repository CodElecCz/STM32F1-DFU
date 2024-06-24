
STM32F103 DFU bootloader
========================

4kb:
```
#define FLASH_BASE_ADDR 				0x08000000
#define FLASH_SIZE_KB 					128
#define FLASH_BOOTLDR_PAYLOAD_SIZE_KB 	124
#define FLASH_BOOTLDR_SIZE_KB 			4

MEMORY
{
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 4K
	/* Reserve the last 4 bytes of RAM to save info across reboots */
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20k - 4
}
```
8kb:
```
#define FLASH_BASE_ADDR 				0x08000000
#define FLASH_SIZE_KB 					128
#define FLASH_BOOTLDR_PAYLOAD_SIZE_KB 	120
#define FLASH_BOOTLDR_SIZE_KB 			8

MEMORY
{
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 8K
	/* Reserve the last 4 bytes of RAM to save info across reboots */
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20k - 4
}
```
