#ifndef __CHECKSUM_H
#define __CHECKSUM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool validate_checksum(const uint32_t * const image, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // __CHECKSUM_H
