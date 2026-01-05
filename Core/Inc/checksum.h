#ifndef __CHECKSUM_H
#define __CHECKSUM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Seed value used by the XOR checksum. Override this in a board header if needed.
#ifndef CHECKSUM_XOR_START
#define CHECKSUM_XOR_START 0xB4DC0FEEU
#endif

bool validate_checksum(const uint32_t * const image, unsigned size);

/**
 * Validate checksum by reading the image in chunks using a provided read
 * callback. The callback should return 0 on success and non-zero on error.
 *
 * Parameters:
 *  - read_cb: function pointer int read_cb(uint32_t addr, void *buf, uint32_t len)
 *  - start_addr: starting byte address of the image in the storage being read
 *  - size_bytes: size of the image in bytes (must be multiple of 4)
 *
 * Returns true if the checksum (XOR with initial value CHECKSUM_XOR_START) equals 0.
 */
bool validate_checksum_stream(int (*read_cb)(uint32_t addr, void *buf, uint32_t len), uint32_t start_addr, uint32_t size_bytes);

#ifdef __cplusplus
}
#endif

#endif // __CHECKSUM_H
