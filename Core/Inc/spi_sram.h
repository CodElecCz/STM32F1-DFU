#ifndef __SPI_SRAM_H
#define __SPI_SRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default chunk size (in bytes) used by streaming/read-in-chunks helpers.
 * Must be a multiple of 4. Can be overridden in a board header or via
 * compiler -D option if you need a different value for memory/stack limits.
 */
#ifndef SRAM_IMAGE_CHUNK_BYTES
#define SRAM_IMAGE_CHUNK_BYTES 256
#endif

/**
 * Read data from external SPI SRAM.
 * @param addr Byte address inside the SRAM to read from.
 * @param buf Destination buffer to store read data.
 * @param len Number of bytes to read.
 * @return 0 on success, non-zero on error.
 *
 * NOTE: This default implementation (in `spi_sram.c`) expects a HAL SPI
 * peripheral to be available (guarded by HAL_SPI_MODULE_ENABLED). The
 * project/board should provide proper chip-select control by overriding the
 * weak functions `spi_sram_cs_assert()` and `spi_sram_cs_deassert()` (or
 * provide an alternative `spi_sram_read()` implementation).
 */
int spi_sram_read(uint32_t addr, void *buf, uint32_t len);

/**
 * Write data to external SPI SRAM.
 * @param addr Byte address inside the SRAM to write to.
 * @param buf Source buffer containing data to write.
 * @param len Number of bytes to write.
 * @return 0 on success, non-zero on error.
 *
 * Implemented as a simple sequential write (23LCV mode 0) using bit-banged
 * SPI in `Core/Src/spi_sram.c`. The board may provide `spi_sram_hw_init()`
 * and CS hook overrides for different configurations.
 */
int spi_sram_write(uint32_t addr, const void *buf, uint32_t len);

/**
 * Chip-select control hooks. Provide board-specific implementations to drive
 * the SRAM CS pin low/high. Weak defaults do nothing.
 */
void spi_sram_cs_assert(void) __attribute__((weak));
void spi_sram_cs_deassert(void) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif // __SPI_SRAM_H
