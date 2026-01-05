#include "checksum.h"

bool validate_checksum(const uint32_t * const image, unsigned size)
{
	// Do some simple XOR checking
	uint32_t xorv = 0xB4DC0FEE;
	for (unsigned i = 0; i < size; i++)
		xorv ^= image[i];

	return xorv == 0;
}
