/*
 * This file is part of the libopenstm32 project.
 *
 * Copyright (C) 2010 Thomas Otto <tommi@viadmin.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Define memory regions. */
MEMORY
{
	/* Keep ROM length small (bootloader) — 8KB = 0x2000 */
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 0x2000
	/* Reserve the last 8 bytes of RAM to save info across reboots: 20KB RAM = 0x5000
	 * RAM available to the program = 0x5000 - 8 = 0x4FF8 */
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 0x4FF8
}

/* Enforce emission of the vector table. */
EXTERN (vector_table)

/* Define the entry point of the output file. */
ENTRY(reset_handler)

/* Define sections. */
SECTIONS
{
	.text : {
		KEEP(*(.vectors)) /* Vector table - ensure it's kept */
		*(.text*)
		. = ALIGN(4);
		*(.rodata*)
		. = ALIGN(4);
	} >rom

	_etext = .;

	.data : {
		_data = .;
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} >ram AT >rom
	_data_loadaddr = LOADADDR(.data);

	.bss : {
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} >ram

	/* Discard exception frame metadata (C++ not used in this project) */
	/DISCARD/ : { *(.eh_frame) }

	. = ALIGN(4);
	end = .;
}

PROVIDE(_stack = ORIGIN(ram) + LENGTH(ram));
PROVIDE(_estack = ORIGIN(ram) + LENGTH(ram));