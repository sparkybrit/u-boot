// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003
 * Josef Baumgartner <josef.baumgartner@telex.de>
 *
 * MCF5282 additionals
 * (C) Copyright 2005
 * BuS Elektronik GmbH & Co. KG <esw@bus-elektronik.de>
 *
 * MCF5275 additions
 * Copyright (C) 2008 Arthur Shipkowski (art@videon-central.com)
 *
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 */

#include <init.h>
#include <net.h>
#include <vsprintf.h>
#include <watchdog.h>
#include <command.h>
#include <asm/global_data.h>
#include <asm/immap.h>
#include <asm/io.h>
#include <netdev.h>
#include <linux/delay.h>
#include "cpu.h"

DECLARE_GLOBAL_DATA_PTR;

int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	void (*rom_reset)(void);

	/*
	 * A 68030 is not meant to reset itself.  The RESET instruction
	 * drives the /RESET pin to reset external devices and the CPU should
	 * ignore its own assertion, and this board has no reset line under
	 * software control.  So undo the state board_init_r set up behind
	 * the boot code's back -- vector base, caches, interrupt mask -- and
	 * hand over to reset_from_rom(), which pulses /RESET and re-enters
	 * at _start, exactly where a power-on reset would.
	 *
	 * The RESET instruction does not reset the CPU itself, but the 68030
	 * does reset when /RESET stays asserted at least eight clocks past the
	 * instruction's own pulse (MC68030UM 7.8).  On sparky1 it did, while
	 * /RESET rose slowly through a 5k pull-up; a 1k pull-up stopped it.
	 * This works either way: by the jump to _start, or through the reset
	 * vectors.
	 *
	 * It must run from the boot image, not from this relocated copy:
	 * see reset_from_rom() in start.S.  Its link-time address is the
	 * relocated one less the relocation offset.
	 */
	rom_reset = (void (*)(void))((ulong)reset_from_rom - gd->reloc_off);

#if CFG_SYS_SDRAM_BASE != CONFIG_TEXT_BASE
	/*
	 * The CPU may be reset as the pulse ends: on sparky1 it then fetches
	 * its reset vectors again, and the boot logic that sends those
	 * fetches to the boot image can count the bus cycle in flight as
	 * /RESET releases as one of them, leaving the last byte of the reset
	 * PC to come from RAM at 0.  trap_init() leaves vectors 0 and 1 of
	 * the RAM table unused, so give them the boot image's values: then
	 * every byte of the vector fetch is right, wherever it comes from.
	 */
	((volatile ulong *)CFG_SYS_SDRAM_BASE)[0] = ((ulong *)CONFIG_TEXT_BASE)[0];
	((volatile ulong *)CFG_SYS_SDRAM_BASE)[1] = ((ulong *)CONFIG_TEXT_BASE)[1];
#endif

	puts("resetting ...\n");
	udelay(50000);			/* let the console drain first */

	__asm__ __volatile__(
		"move.w	#0x2700,%%sr\n\t"	/* mask all interrupts        */
		"movec	%1,%%vbr\n\t"		/* the boot image's vectors   */
		"moveq	#0,%%d0\n\t"
		"movec	%%d0,%%cacr\n\t"	/* caches off                 */
		"jmp	(%0)\n"
		: : "a" (rom_reset), "d" (CONFIG_TEXT_BASE) : "d0", "memory");

	/* not reached */
	return 0;
};

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	puts("CPU:   Motorola MC68030\n");
	return 0;
};
#endif /* CONFIG_DISPLAY_CPUINFO */

#if defined(CONFIG_WATCHDOG)
/* Called by macro WATCHDOG_RESET */
void watchdog_reset(void)
{
	wdog_t *wdt = (wdog_t *)(MMAP_WDOG);

	out_be16(&wdt->wdog_wcr, 0);
}

int watchdog_disable(void)
{
	wdog_t *wdt = (wdog_t *)(MMAP_WDOG);

	/* reset watchdog counter */
	out_be16(&wdt->wdog_wcr, 0);
	/* disable watchdog interrupt */
	out_be16(&wdt->wdog_wirr, 0);
	/* disable watchdog timer */
	out_be16(&wdt->wdog_wrrr, 0);

	puts("WATCHDOG:disabled\n");
	return (0);
}

int watchdog_init(void)
{
	wdog_t *wdt = (wdog_t *)(MMAP_WDOG);

	/* disable watchdog interrupt */
	out_be16(&wdt->wdog_wirr, 0);

	/* set timeout and enable watchdog */
	out_be16(&wdt->wdog_wrrr,
		(CONFIG_WATCHDOG_TIMEOUT_MSECS * CONFIG_SYS_HZ) / (32768 * 1000) - 1);

	/* reset watchdog counter */
	out_be16(&wdt->wdog_wcr, 0);

	puts("WATCHDOG:enabled\n");
	return (0);
}
#endif				/* #ifdef CONFIG_WATCHDOG */


#if defined(CONFIG_MCFFEC)
/* Default initializations for MCFFEC controllers.  To override,
 * create a board-specific function called:
 *	int board_eth_init(struct bd_info *bis)
 */

int cpu_eth_init(struct bd_info *bis)
{
	return mcffec_initialize(bis);
}
#endif
