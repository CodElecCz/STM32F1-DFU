#include "spi_sram_image.h"
#include "spi_sram.h"
#include "flash_config.h" // for FLASH_BOOTLDR_PAYLOAD_SIZE_KB
#include "checksum.h"
#include <stdint.h>
#include <stddef.h>

bool sram_has_valid_image(void)
{
    sram_image_header hdr;
    int res = spi_sram_read(SRAM_IMAGE_HEADER_OFFSET, &hdr, sizeof(hdr));
    if (res != 0)
        return false;

    // Validate magic (stored little-endian in hdr.magic)
    uint32_t magic = sram_le32(hdr.magic);
    if (magic != (uint32_t)SRAM_IMAGE_MAGIC)
        return false;

    // Parse header fields (explicit little-endian conversion)
    uint32_t img_offset = sram_le32(hdr.offset);
    uint32_t img_size = sram_le32(hdr.size);

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
