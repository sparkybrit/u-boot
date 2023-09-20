// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003
 * Josef Baumgartner <josef.baumgartner@telex.de>
 *
 * Copyright (C) 2004-2007, 2012 Freescale Semiconductor, Inc.
 * Hayden Fraser (Hayden.Fraser@freescale.com)
 */

#include <common.h>
#include <clock_legacy.h>
#include <asm/global_data.h>
#include <asm/processor.h>
#include <asm/immap.h>
#include <asm/io.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

/* get_clocks() fills in gd->cpu_clock and gd->bus_clk */
int get_clocks(void)
{
	gd->cpu_clk = CFG_SYS_CLK;
	return (0);
}
