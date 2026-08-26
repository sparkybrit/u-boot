// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker, graeme.harker@gmai.com
 *
 */

#include <init.h>
#include <dm.h>
#include <errno.h>
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

/*
 * Scan the CompactFlash here rather than letting DM autoprobe it.
 *
 * The IDE driver paces itself with mdelay()/udelay(), which on this board
 * resolve to the XR68C681 tick counter - and that counter is only advanced by
 * the timer's 100 Hz interrupt handler. Both interrupt_init() and the M68K
 * timer_init() run late in board_init_r(), well after initr_dm() where
 * DM_FLAG_PROBE_AFTER_BIND devices are probed, so an autoprobed controller
 * would reach udelay() with the counter frozen at zero and spin forever.
 * board_late_init() is the first hook that runs after both.
 *
 * A missing card is not an error: the driver reports "not available" and the
 * board carries on booting.
 */
int board_late_init(void)
{
	struct udevice *dev;
	int ret;

	if (!IS_ENABLED(CONFIG_IDE))
		return 0;

	ret = uclass_first_device_err(UCLASS_IDE, &dev);
	if (ret && ret != -ENODEV)
		printf("IDE: controller init failed (%d)\n", ret);

	return 0;
}

int testdram(void)
{
	/* TODO: XXX XXX XXX */
	printf ("DRAM test not implemented!\n");

	return (0);
}
