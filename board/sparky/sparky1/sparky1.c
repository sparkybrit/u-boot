// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker, graeme.harker@gmai.com
 *
 */

#include <init.h>
#include <dm.h>
#include <errno.h>
#include <timer.h>
#include <xr68c681.h>
#include <asm/global_data.h>
#include <asm/immap.h>
#include <asm/io.h>
#include <dm/platform_data/mc68681.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

extern U_BOOT_DRIVER(xr68c681_timer);

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

/*
 * A finer time source for udelay().
 *
 * get_ticks() only advances on the XR68C681's 100 Hz timer interrupt, so the
 * generic __udelay() in lib/time.c cannot resolve anything below 10 ms - and
 * because usec_to_tick() rounds sub-tick requests to zero and its loop then
 * waits for the next tick edge anyway, *every* udelay() on this board costs a
 * full 10 ms whatever it asked for.  That is what caps CompactFlash reads at
 * ~50 KB/s: ide_read() spends one tick per sector on an unconditional
 * udelay(50).
 *
 * The same counter can be read live, though, and it decrements 2300x faster
 * than the tick.  Spinning on it here leaves the tick counter, the DT's
 * clock-frequency and get_timer() completely alone - this overrides only the
 * __weak __udelay(), exactly as board/armltd/integrator and the ColdFire
 * arch/m68k/lib/time.c do.
 */

/*
 * Counter/timer input clock: the 3.6864 MHz crystal divided by 16, which is
 * the timer mode xr68c681_timer_probe() selects with UART_UACR_CLK.  One
 * count is ~4.34 us.
 */
#define CT_CLK_HZ		230400

/* Counts per microsecond as an exact fraction: 230400 / 1000000. */
#define CT_PER_USEC_NUM		144
#define CT_PER_USEC_DEN		625

/* Cap per pass so chunk * CT_PER_USEC_NUM cannot overflow 32 bits. */
#define CT_MAX_USEC		1000000UL

/*
 * Fallback for the window before xr68c681_timer_probe() starts the counter,
 * i.e. any udelay() from board_init_f().  Without it those callers reach
 * get_ticks(), find gd->timer unset and panic in dm_timer_init().  A NOP loop
 * is good to no better than a factor of two, which is all early code needs.
 */
static void sparky1_udelay_spin(unsigned long usec)
{
	unsigned long loops = usec * (CFG_SYS_CLK / 1000000) / 8;

	while (loops--)
		asm volatile ("nop");
}

/*
 * Read the live counter.  The two halves are separate byte reads of a running
 * counter, so re-read the upper half and retry if it moved underneath us; it
 * only changes once every 256 counts (~1.1 ms), so this settles immediately.
 */
static u16 sparky1_ct_read(xr68c681_t *base)
{
	u8 hi, lo, hi2;

	do {
		hi = readb(&base->uctu);
		lo = readb(&base->uctl);
		hi2 = readb(&base->uctu);
	} while (hi != hi2);

	return ((u16)hi << 8) | lo;
}

void __udelay(unsigned long usec)
{
	struct timer_dev_priv *priv;
	struct mc68681_plat *plat;
	xr68c681_t *base;
	unsigned int reload;

	if (!gd->timer || gd->timer->driver != DM_DRIVER_REF(xr68c681_timer)) {
		sparky1_udelay_spin(usec);
		return;
	}

	plat = dev_get_plat(gd->timer);
	priv = dev_get_uclass_priv(gd->timer);
	base = (xr68c681_t *)plat->base;

	/*
	 * In timer mode the counter free-runs down from its reload value N to
	 * zero and starts again - twice per interrupt period, since
	 * f = CT_CLK_HZ / 2N.  Recover N from the tick rate rather than
	 * duplicating the driver's constant.  Only elapsed counts are ever
	 * used, never the counter's absolute phase, which those two passes
	 * per period make ambiguous.
	 */
	reload = CT_CLK_HZ / (2 * priv->clock_rate);
	if (!reload) {
		sparky1_udelay_spin(usec);
		return;
	}

	while (usec) {
		unsigned long chunk = usec > CT_MAX_USEC ? CT_MAX_USEC : usec;
		u32 target, elapsed = 0;
		u16 prev, now;

		/* round up, so a short delay is never shorter than asked for */
		target = (chunk * CT_PER_USEC_NUM + CT_PER_USEC_DEN - 1) /
			 CT_PER_USEC_DEN;

		/*
		 * Sample far faster than the ~5 ms it takes the counter to
		 * wrap, so each step is a small difference and no wrap can be
		 * missed.  Whether the reload costs a clock - making the cycle
		 * N + 1 counts rather than N - is not worth pinning down: the
		 * two differ by 0.09%, and counting the longer of the two errs
		 * towards delaying slightly too long.
		 */
		prev = sparky1_ct_read(base);
		while (elapsed < target) {
			now = sparky1_ct_read(base);
			elapsed += (now <= prev) ? prev - now
						 : prev + reload + 1 - now;
			prev = now;
		}

		usec -= chunk;
	}
}
