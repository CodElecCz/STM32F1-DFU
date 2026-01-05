#ifndef __SPI_SRAM_IMAGE_H
#define __SPI_SRAM_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "sram_image_header.h"

/**
 * Check whether a valid firmware image (with XOR checksum) is present in
 * external SPI SRAM.
 *
 * @return true if a valid image is detected, false otherwise.
 */
bool sram_has_valid_image(void);

#ifdef __cplusplus
}
#endif

#endif // __SPI_SRAM_IMAGE_H