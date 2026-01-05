#ifndef __SPI_SRAM_H
#define __SPI_SRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
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
 * Chip-select control hooks. Provide board-specific implementations to drive
 * the SRAM CS pin low/high. Weak defaults do nothing.
 */
void spi_sram_cs_assert(void) __attribute__((weak));
void spi_sram_cs_deassert(void) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif // __SPI_SRAM_H
