
STM32F103 DFU bootloader
========================

## Project Description

This repository contains a **DFU (Device Firmware Update) bootloader** implementation for the **STM32F103** microcontroller. The bootloader enables firmware updates over USB without requiring an external programmer (ST-Link, J-Link, etc.), making it ideal for field updates and development iterations.

### Overview

The DFU bootloader is a compact, standalone application that resides in the lower portion of flash memory. When the device boots, the bootloader checks for update conditions and either:
- **Enters DFU mode** - Allowing firmware upload via USB using standard DFU tools
- **Jumps to application** - Transfers control to the main application firmware

This implementation follows the USB DFU Class Specification 1.1, making it compatible with industry-standard DFU tools like `dfu-util` (Linux/macOS/Windows) and ST's DfuSeDemo (Windows).

### Key Features

- **USB DFU Class 1.1 Compliant** - Works with standard DFU host tools
- **Compact Size** - 8KB bootloader footprint to minimize flash usage
- **Non-Volatile Configuration** - Uses last 4 bytes of RAM to preserve state across reboots
- **Application Protection** - Bootloader resides in protected flash area
- **No External Tools Required** - Standard USB connection for firmware updates
- **Fast Updates** - Direct USB transfer without intermediate programmers

### Use Cases

- **Field Firmware Updates** - Update devices deployed in the field without physical access to debug headers
- **Product Manufacturing** - Streamline production programming via USB
- **Development Workflow** - Faster iteration cycles without connecting/disconnecting programmers
- **Custom Bootloader Logic** - Foundation for implementing custom update policies and security features

## Memory Configuration

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

## How DFU Bootloader Works

### Boot Sequence

1. **Power-On/Reset** - MCU starts executing from 0x08000000 (bootloader entry point)
2. **RAM Check** - Bootloader reads last 4 bytes of RAM for DFU mode flag
3. **Decision Point**:
   - If DFU flag is set → Enter DFU mode and wait for firmware upload
   - If application valid → Jump to application at 8KB offset (0x08002000)
4. **Application Execution** - Application runs normally from its offset address

### Entering DFU Mode

The bootloader can be triggered to enter DFU mode by:
- **Software Command** - Application writes magic value to RAM and resets
- **Boot Condition** - Custom GPIO pin check or button press at startup
- **Invalid Application** - If application flash appears corrupted or empty

### Exiting DFU Mode

After successful firmware upload:
- DFU tool sends detach command
- Bootloader clears DFU flag in RAM
- System resets and boots into new application

## Repository Contents

```
STM32F1-DFU/
├── Core/
│   ├── Inc/          - Header files for bootloader logic
│   ├── Src/          - Source files including main.c, USB handlers
│   └── Startup/      - Startup assembly for STM32F103
├── Debug/            - Debug build output directory
├── Release/          - Release build output (optimized)
├── Release-PRG/      - Production release builds
├── documents/        - Additional documentation and references
├── stm32f103.ld      - Linker script defining memory layout
├── LICENSE           - License information
└── README.md         - This file
```

## Building the Bootloader

### Prerequisites

- **STM32CubeIDE** or compatible ARM GCC toolchain
- **STM32F103** development board with USB connection
- **dfu-util** or DfuSeDemo for firmware upload

### Build Steps

1. Open project in STM32CubeIDE
2. Build the project (Debug or Release configuration)
3. Flash bootloader using ST-Link programmer (first time only)
4. Subsequent updates can use DFU over USB

### Configuring Application Firmware

Your application firmware must be configured to work with the 8KB bootloader:
```c
// In application linker script
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08002000, LENGTH = 120K  // Start at 8KB offset
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 20K
}

// In application startup/main.c
#define VECT_TAB_OFFSET  0x2000  // Vector table offset
SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
```

## Using the Bootloader

### Uploading Firmware with dfu-util (Linux/macOS/Windows)

1. **Enter DFU Mode** - Power on device in bootloader mode
2. **List DFU Devices**:
   ```bash
   dfu-util -l
   ```
3. **Upload Firmware**:
   ```bash
   dfu-util -a 0 -s 0x08002000 -D application.bin
   ```
4. **Reset Device** - Application starts automatically after upload

### Uploading Firmware with DfuSeDemo (Windows)

1. Install ST DfuSe tools from STMicroelectronics website
2. Run DfuSeDemo application
3. Click "Choose" and select your .dfu or .bin file
4. Ensure address matches bootloader offset (0x08002000)
5. Click "Upgrade" to flash firmware
6. Click "Leave DFU mode" to boot application

## Practical Usage Example

### Firmware Preparation

Before uploading firmware, you can verify the binary using the checksum utility:

```bash
py checksum.py SW_v0101_DFU.bin
```

Expected output:
```
Firmware size 45900
Firmware size after padding 45900
Firmware size for checksum purposes 45900
```

### Detecting DFU Device

Verify your device is in DFU mode and detected by the system:

```bash
dfu-util -l
```

Expected output shows the device with internal flash layout:
```
Found DFU: [0483:df11] ver=0200, devnum=29, cfg=1, intf=0, path="1-1.3", 
alt=0, name="@Internal Flash /0x08000000/8*001Ka,120*001Kg", 
serial="54FF6C068687545516541567"
```

The flash layout indicates:
- **8*001Ka** - 8 pages of 1KB (bootloader area, write-protected)
- **120*001Kg** - 120 pages of 1KB (application area, writable)

### Production Firmware Upload

Upload firmware and automatically exit DFU mode using the `:leave` suffix:

```bash
dfu-util -d 0x0483:0xDF11 -D SW_v0101_DFU.bin -s0x08002000:leave
```

This command will:
1. Upload firmware to address 0x08002000
2. Erase necessary flash pages (progress shown)
3. Write firmware data (progress shown)
4. Automatically exit DFU mode and boot application

Expected output:
```
Downloading element to address = 0x08002000, size = 45900
Erase           [=========================] 100%        45900 bytes
Erase    done.
Download        [=========================] 100%        45900 bytes
Download done.
File downloaded successfully
Submitting leave request...
Transitioning to dfuMANIFEST state
```

**Note**: The warning "Invalid DFU suffix signature" can be safely ignored for raw binary files. Use `.dfu` format files to avoid this warning.

## Customization

### Modifying Entry Conditions

Edit the bootloader logic in `Core/Src/main.c` to customize when DFU mode is entered:

```c
// Example: Enter DFU if button pressed at boot
if (HAL_GPIO_ReadPin(BOOT_BUTTON_GPIO_Port, BOOT_BUTTON_Pin) == GPIO_PIN_RESET) {
    enter_dfu_mode();
}
```

### Adjusting Memory Layout

Modify `stm32f103.ld` to change bootloader size or RAM allocation:
- Adjust `LENGTH` in MEMORY section
- Update corresponding defines in source code
- Rebuild bootloader and update application offset accordingly

## Troubleshooting

### Device Not Detected in DFU Mode

- Verify USB cable supports data (not charge-only)
- Check USB device enumeration in Device Manager (Windows) or `lsusb` (Linux)
- Ensure bootloader is properly flashed at 0x08000000
- Install STM32 DFU drivers if using Windows

### Application Not Starting After Upload

- Verify application linker script has correct flash origin offset
- Ensure vector table offset is set in application startup code
- Check that uploaded firmware size doesn't exceed available space
- Confirm application binary is valid (not corrupted)

### DFU Mode Won't Exit

- Power cycle the device
- Re-flash bootloader using ST-Link
- Check RAM preservation across resets (voltage stability)

## Technical Details

### USB Configuration

- **VID/PID**: Configured for STM32 DFU (0x0483:0xDF11)
- **Class**: USB DFU Class 1.1
- **Speed**: Full Speed USB 2.0 (12 Mbps)
- **Endpoints**: Control endpoint only (EP0)

### Flash Programming

- **Page Size**: 1KB (STM32F103 medium density)
- **Erase Granularity**: Per page
- **Write Alignment**: 16-bit half-word
- **Verification**: CRC checked after upload

## Security Considerations

⚠️ **Important**: This is a basic bootloader implementation intended for development use.

For production deployments, consider adding:
- **Firmware Signature Verification** - Cryptographic signature checking before boot
- **Secure Boot** - Chain of trust from bootloader to application
- **Encrypted Firmware** - AES encryption for firmware images
- **Rollback Protection** - Version checking to prevent downgrade attacks
- **Debug Lock** - Disable debug interfaces in production

## License

Refer to the LICENSE file for licensing information.

## Contributing

Contributions and improvements are welcome! Please submit issues or pull requests via the repository.

## Keywords

STM32F103, DFU, bootloader, USB, firmware update, Device Firmware Update, STM32CubeIDE, embedded systems, ARM Cortex-M3, flash programming, OTA update, USB bootloader, dfu-util, firmware upgrade.
