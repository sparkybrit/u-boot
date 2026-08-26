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
#define CFG_SYS_INIT_RAM_ADDR	0x40000000
#define CFG_SYS_INIT_RAM_SIZE	0x10000	

/*-----------------------------------------------------------------------
 * Start addresses for the final memory configuration
 * (Set up by the startup code)
 */
#define CFG_SYS_SDRAM_BASE		0x40000000
#define CFG_SYS_SDRAM_SIZE		0x00400000

/*
 * Boot image store: a Dallas DS1250Y NVSRAM at 0x00000000, holding the
 * exception vector table and U-Boot itself.  Not 0xC0000000 - that quadrant
 * is /DISKCS, the CompactFlash interface (see CONFIG_SYS_ATA_BASE_ADDR).
 */
#define CFG_SYS_FLASH_BASE		0x00000000

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
