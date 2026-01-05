#ifndef __FLASH_H
#define __FLASH_H

// Flashing routines //

#include <stdint.h>

/* Function prototypes (implementations moved to Core/Src/flash.c) */
void _flash_lock(void);
void _flash_unlock(void);
void _flash_erase_page(uint32_t page_address);
int  _flash_page_is_erased(uint32_t addr);
void _flash_program_buffer(uint32_t address, uint16_t *data, unsigned len);

#if defined(ENABLE_PROTECTIONS) || defined(ENABLE_WRITEPROT)
void _flash_erase_option_bytes(void);
void _flash_program_option_bytes(uint32_t address, uint16_t data);
void _optbytes_unlock(void);
#endif

#ifdef ENABLE_SAFEWRITE
void check_do_erase(void);
#endif

#endif // __FLASH_H
