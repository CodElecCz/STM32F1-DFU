#ifndef __SPI_SRAM_IMAGE_H
#define __SPI_SRAM_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Header location (same base as previous implementation)
#define SRAM_IMAGE_HEADER_OFFSET 0x00
// Expected magic value in header (uint32_t, little-endian in storage)
#ifndef SRAM_IMAGE_MAGIC
// Use a repeated pattern as a distinctive 32-bit magic. You can override this
// in a board header if you prefer a different magic value.
#define SRAM_IMAGE_MAGIC 0xA5A5A5A5U
#endif

// Packed header layout stored in SPI SRAM starting at SRAM_IMAGE_HEADER_OFFSET
// Layout (total 12 bytes):
// 0-3:  magic
// 4-7:  offset
// 8-11: size

typedef struct __attribute__((packed)) sram_image_header {
    uint32_t magic;
    uint32_t offset;
    uint32_t size;
} sram_image_header;

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
