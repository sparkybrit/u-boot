// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023  Graeme Harker <graeme.harker@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/platform_data/mc68681.h>
#include <serial.h>
#include <timer.h>
#include <linux/compiler.h>
#include <asm/immap.h>
#include <xr68c681.h>
#include <irq_func.h>

#define	IRQ_TIMER_VECTOR 0
#define	IRQ_TIMER_MASK 0b1000

DECLARE_GLOBAL_DATA_PTR;

static volatile u64 counter;

static void timer_interrupt_handler(void *arg)
{
	xr68c681_t *base = (xr68c681_t *)arg;

	/* reset the timer interrupt */
	readb(&base->uopc);

	/* increment the timer counter */
	counter++;
}

static u64 xr68c681_timer_get_count(struct udevice *dev)
{
	debug("%s: %llu\n", __func__, counter);
	return counter;
}

static int xr68c681_timer_probe(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	xr68c681_t *base = (xr68c681_t *)(plat->base);

	/* generate an interrupt every 10ms (100Hz): N=0x0480=1152,
	 * f = (3.6864MHz/16) / (2*1152) = 100Hz */
	writeb(0x04, &base->uctu);
	writeb(0x80, &base->uctl);

	/*
	 * ACR is write-only (reads return IPCR, not ACR).  Write the full
	 * value directly: BRG Set 2 (bit 7) + Timer mode X1/CLK÷16 (bits 6:4=111).
	 */
	writeb(UART_UACR_CLK, &base->uacr);

	debug("%s: base=0x%p\n", __func__, base);

	/* set the interrupt vector */
	writeb(IRQ_TIMER_VECTOR + 0x40, &base->ivr);

	irq_install_handler(IRQ_TIMER_VECTOR, (interrupt_handler_t *)timer_interrupt_handler, base);

	/* enable counter/timer interrupts */
	writeb(IRQ_TIMER_MASK, &base->uimr);	

	return (0);
}

static int xr68c681_timer_of_to_plat(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	struct timer_dev_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr_base;

	addr_base = dev_read_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -ENODEV;

	plat->base = (uint32_t)addr_base;
	plat->baudrate = gd->baudrate;
	priv->clock_rate = dev_read_u32_default(dev, "clock-frequency", 100);

	return 0;
}

static const struct timer_ops xr68c681_timer_ops = {
	.get_count = xr68c681_timer_get_count,
};

static const struct udevice_id xr68c681_timer_ids[] = {
	{ .compatible = "exar,xr68c681_timer" },
	{}
};

U_BOOT_DRIVER(xr68c681_timer) = {
	.name = "xr68c681_timer",
	.id = UCLASS_TIMER,
	.of_match = xr68c681_timer_ids,
	.of_to_plat = xr68c681_timer_of_to_plat,
	.plat_auto = sizeof(struct mc68681_plat),
	.probe = xr68c681_timer_probe,
	.ops = &xr68c681_timer_ops,
};