/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuation settings for the Sparky1 board.
 *
 * (C) Copyright 2023 Graeme Harker <graeme.harker@gmail.com>
 */

/*
 * board/config.h - configuration options, board specific
 */

#ifndef _SPARKY1_H
#define _SPARKY1_H

/*
 * High Level Configuration Options
 * (easy to change)
 */

#define CFG_SYS_UART_PORT		(0)

#define CFG_SYS_CLK			16000000

/*-----------------------------------------------------------------------
 * Definitions for pre-relocation global data, stack and heap (at the bottom of SRAM)
 */
#define CFG_SYS_INIT_RAM_ADDR	0x00000000
#define CFG_SYS_INIT_RAM_SIZE	0x10000	

/*-----------------------------------------------------------------------
 * Start addresses for the final memory configuration
 * (Set up by the startup code)
 */
#define CFG_SYS_SDRAM_BASE		0x00000000
#define CFG_SYS_SDRAM_SIZE		0x00800000

/*
 * Boot image store: a Dallas DS1250Y NVSRAM at 0x01000000, holding the
 * exception vector table and U-Boot itself (CONFIG_TEXT_BASE).
 *
 * The SRAM is at 0, where Linux wants it, so the reset vectors the 68030
 * fetches from 0x00000000 and 0x00000004 would be uninitialised SRAM.  The
 * board's /BOOT logic sends the first 8 bus cycles after /RESET - those two
 * longwords, a byte at a time - to the NVRAM instead, and the reset PC there
 * points into the NVRAM's own quadrant.  See busdecoder revision 03 and
 * plds/boot/ in the sparky1 hardware project.
 */
#define CFG_SYS_FLASH_BASE		0x01000000

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 8 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization ??
 */
#define CFG_SYS_BOOTMAPSZ		(CFG_SYS_SDRAM_BASE + CFG_SYS_SDRAM_SIZE)

/*
 * FLASH organization
 */
#ifdef CONFIG_SYS_FLASH_CFI
#	define CFG_SYS_FLASH_SIZE		0x40000	/* Max size that the board might have */
#endif

#endif				/* _SPARKY1_H */
