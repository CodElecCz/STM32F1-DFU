#ifndef CORE_INC_BOARD_H
#define CORE_INC_BOARD_H

/* Sample board configuration header.
 *
 * This file contains default pin/port assignments used by drivers in this
 * repository. Copy this file to a board-specific header or override the
 * macros in your project settings to match your hardware.
 *
 * Adjust the values below as needed for your board.
 */

// SPI-SRAM pin mapping (port: 0=A, 1=B, 2=C, ...)
#ifndef SPI_SRAM_PORT
#define SPI_SRAM_PORT        1  /* GPIO port number (0=A, 1=B, 2=C, etc.) */
#endif
#ifndef SPI_SRAM_SCK_PIN
#define SPI_SRAM_SCK_PIN     13 /* SCK pin number (0..15) */
#endif
#ifndef SPI_SRAM_MOSI_PIN
#define SPI_SRAM_MOSI_PIN    15 /* MOSI pin number (0..15) */
#endif
#ifndef SPI_SRAM_MISO_PIN
#define SPI_SRAM_MISO_PIN    14 /* MISO pin number (0..15) */
#endif
#ifndef SPI_SRAM_CS_PIN
#define SPI_SRAM_CS_PIN      12 /* CS pin number (0..15) */
#endif

// Optional: DFU force GPIO sample (used if ENABLE_GPIO_DFU_BOOT)
#ifndef GPIO_DFU_BOOT_PORT
#define GPIO_DFU_BOOT_PORT   0 /* GPIOA by default */
#endif
#ifndef GPIO_DFU_BOOT_PIN
#define GPIO_DFU_BOOT_PIN    0 /* pin 0 by default */
#endif

#endif // CORE_INC_BOARD_H
