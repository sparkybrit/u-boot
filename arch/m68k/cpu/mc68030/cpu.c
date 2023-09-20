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

#include <common.h>
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
	/* we don't return! */
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
