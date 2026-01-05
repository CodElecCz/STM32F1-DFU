#ifndef __SRAM_IMAGE_HEADER_H
#define __SRAM_IMAGE_HEADER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packed SRAM image header layout stored in external SPI SRAM.
 * Keep this header minimal so other modules can include it without
 * bringing additional dependencies.
 *
 * Layout (12 bytes):
 *  - 0..3:  magic (little-endian uint32)
 *  - 4..7:  offset (little-endian uint32)
 *  - 8..11: size (little-endian uint32)
 */
typedef struct __attribute__((packed)) sram_image_header {
    uint32_t magic;
    uint32_t offset;
    uint32_t size;
} sram_image_header;

/* Convert a 32-bit value read directly from SRAM (little-endian) to host-endian */
static inline uint32_t sram_le32(uint32_t v_le)
{
    uint8_t *p = (uint8_t*)&v_le;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Header location (same base as previous implementation)
#define SRAM_IMAGE_HEADER_OFFSET 0x00
// Expected magic value in header (uint32_t, little-endian in storage)
#ifndef SRAM_IMAGE_MAGIC
// Default magic uses the ASCII bytes 'S','R','A','M' stored little-endian.
// Value: 0x4D414253
#define SRAM_IMAGE_MAGIC 0x4D414253U
#endif

#ifdef __cplusplus
}
#endif

#endif // __SRAM_IMAGE_HEADER_H
