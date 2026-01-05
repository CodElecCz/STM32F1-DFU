#include "spi_sram_image.h"
#include "spi_sram.h"
#include "flash_config.h" // for FLASH_BOOTLDR_PAYLOAD_SIZE_KB
#include "checksum.h"
#include <stdint.h>
#include <stddef.h>

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
// 0-3:  magic (uint32_t, little endian)
// 4-7:  img_offset (little endian)
// 8-11: img_size (little endian)

struct __attribute__((packed)) sram_image_header {
    uint32_t magic_le;
    uint32_t img_offset_le;
    uint32_t img_size_le;
};

#ifndef _Static_assert
/* Fallback for compilers that don't support C11 _Static_assert.
 * Creates a typedef for an array with negative size on failure, causing a
 * compile-time error. The message parameter is unused in this fallback. */
#define _SA_JOIN(a,b) a##b
#define _SA_MAKE_NAME(a,b) _SA_JOIN(a,b)
#define _Static_assert(cond, msg) typedef char _SA_MAKE_NAME(_static_assertion_, __LINE__)[(cond) ? 1 : -1]
#endif

_Static_assert(sizeof(struct sram_image_header) == 12, "sram header must be 12 bytes");

static uint32_t le32_from_bytes(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline uint32_t le32_from_u32(uint32_t v_le)
{
    // The field is stored little-endian in SRAM. On little-endian targets this is a no-op.
    // To be explicit and portable, reinterpret as bytes and reconstruct.
    uint8_t *p = (uint8_t*)&v_le;
    return le32_from_bytes(p);
}

bool sram_has_valid_image(void)
{
    struct sram_image_header hdr;
    int res = spi_sram_read(SRAM_IMAGE_HEADER_OFFSET, &hdr, sizeof(hdr));
    if (res != 0)
        return false;

    // Validate magic (stored little-endian in hdr.magic_le)
    uint32_t magic = le32_from_u32(hdr.magic_le);
    if (magic != (uint32_t)SRAM_IMAGE_MAGIC)
        return false;

    // Parse header fields (explicit little-endian conversion)
    uint32_t img_offset = le32_from_u32(hdr.img_offset_le);
    uint32_t img_size = le32_from_u32(hdr.img_size_le);

    // Basic sanity checks
    if (img_size == 0)
        return false;

    // img_size must be multiple of 4 (checksum operates on 32-bit words)
    if (img_size & 3U)
        return false;

    // Ensure image doesn't overlap header
    const uint32_t header_end = SRAM_IMAGE_HEADER_OFFSET + (uint32_t)sizeof(hdr);
    if (img_offset < header_end)
        return false;

    // Enforce maximum size capped to payload size configured for flash
    uint32_t max_bytes = FLASH_BOOTLDR_PAYLOAD_SIZE_KB * 1024U;
    if (img_size > max_bytes)
        return false;

    // Ensure image fits within allowed SRAM region (header_end .. header_end + max_bytes)
    if (img_offset > header_end + max_bytes)
        return false;
    if (img_offset > UINT32_MAX - img_size)
        return false;
    if (img_offset + img_size > header_end + max_bytes)
        return false;

    // Validate checksum by streaming reads via spi_sram_read.
    // validate_checksum_stream returns true if XOR checksum matches.
    return validate_checksum_stream(spi_sram_read, img_offset, img_size);
}