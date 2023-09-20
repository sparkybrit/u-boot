// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker, graeme.harker@gmai.com
 *
 */

#include <common.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/immap.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void) 
{
	puts ("Board: Sparky1\n");
	return 0;
};

int dram_init(void)
{
	gd->ram_size = CFG_SYS_SDRAM_SIZE;

	return 0;
};

int testdram(void)
{
	/* TODO: XXX XXX XXX */
	printf ("DRAM test not implemented!\n");

	return (0);
}
