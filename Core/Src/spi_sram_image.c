#include "spi_sram_image.h"
#include "spi_sram.h"
#include "flash_config.h" // for FLASH_BOOTLDR_PAYLOAD_SIZE_KB
#include <stdint.h>

// Read buffer size in bytes (must be multiple of 4)
#ifndef SRAM_IMAGE_CHUNK_BYTES
#define SRAM_IMAGE_CHUNK_BYTES 256
#endif

bool sram_has_valid_image(void)
{
    // Read uint32 image length at offset 0x20 (in bytes offset) where image length
    // is stored as number of 32-bit words (same layout as flash image).
    uint32_t size_words = 0;
    int res = spi_sram_read(0x20, &size_words, sizeof(size_words));
    if (res != 0)
        return false;

    // Basic sanity checks
    if (size_words == 0)
        return false;

    // Maximum words allowed by payload size in flash
    uint32_t max_words = (FLASH_BOOTLDR_PAYLOAD_SIZE_KB * 1024U) / 4U;
    if (size_words > max_words)
        return false;

    uint32_t xorv = 0xB4DC0FEEU;
    uint32_t remaining_words = size_words;
    uint8_t buf[SRAM_IMAGE_CHUNK_BYTES];

    // Begin reading the image data which starts immediately after the size word
    // in this SRAM layout (i.e., offset 0x20 + 4 = 0x24 is first data byte).
    uint32_t read_addr = 0x20 + 4;

    while (remaining_words)
    {
        uint32_t toread_words = remaining_words;
        uint32_t toread_bytes = toread_words * 4U;
        if (toread_bytes > SRAM_IMAGE_CHUNK_BYTES)
            toread_bytes = SRAM_IMAGE_CHUNK_BYTES;

        int r = spi_sram_read(read_addr, buf, toread_bytes);
        if (r != 0)
            return false;

        // XOR words
        uint32_t count = toread_bytes / 4U;
        uint32_t *words = (uint32_t*)buf;
        for (uint32_t i = 0; i < count; ++i)
            xorv ^= words[i];

        remaining_words -= count;
        read_addr += toread_bytes;
    }

    return xorv == 0;
}
