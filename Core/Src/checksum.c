#include "checksum.h"
#include "flash_config.h"
#include <stdint.h>

bool validate_checksum(const uint32_t * const image, unsigned size)
{
	// Do some simple XOR checking
	uint32_t xorv = CHECKSUM_XOR_START;
	for (unsigned i = 0; i < size; i++)
		xorv ^= image[i];

	return xorv == 0;
}

bool validate_checksum_stream(int (*read_cb)(uint32_t addr, void *buf, uint32_t len), uint32_t start_addr, uint32_t size_bytes)
{
	if (!read_cb)
		return false;
	if ((size_bytes & 3U) != 0)
		return false; // must be multiple of 4

	uint32_t xorv = CHECKSUM_XOR_START;
	const uint32_t chunk = SRAM_IMAGE_CHUNK_BYTES;
	uint8_t buf[SRAM_IMAGE_CHUNK_BYTES];
	uint32_t remaining = size_bytes;
	uint32_t addr = start_addr;

	while (remaining)
	{
		uint32_t toread = (remaining > chunk) ? chunk : remaining;
		int r = read_cb(addr, buf, toread);
		if (r != 0)
			return false;
		uint32_t words = toread / 4U;
		for (uint32_t i = 0; i < words; ++i)
		{
			uint32_t w = (uint32_t)buf[i*4] | ((uint32_t)buf[i*4+1] << 8) | ((uint32_t)buf[i*4+2] << 16) | ((uint32_t)buf[i*4+3] << 24);
			xorv ^= w;
		}
		addr += toread;
		remaining -= toread;
	}

	return xorv == 0U;
}
