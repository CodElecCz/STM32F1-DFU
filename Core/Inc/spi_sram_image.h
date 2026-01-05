#ifndef __SPI_SRAM_IMAGE_H
#define __SPI_SRAM_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check whether a valid firmware image (with XOR checksum) is present in
 * external SPI SRAM. The SRAM layout expected:
 *  - word at offset 0x20 contains the image size in 32-bit words
 *  - following that, the image contains `imagesize` 32-bit words whose XOR
 *    with initial value 0xB4DC0FEE must equal 0
 *
 * @return true if a valid image is detected, false otherwise.
 */
bool sram_has_valid_image(void);

#ifdef __cplusplus
}
#endif

#endif // __SPI_SRAM_IMAGE_H
