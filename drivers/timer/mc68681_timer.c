// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023  Graeme Harker <graeme.harker@gmail.com>
 */

#define	DEBUG

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/platform_data/serial_mc68681.h>
#include <serial.h>
#include <timer.h>
#include <linux/compiler.h>
#include <asm/immap.h>
#include <asm/uart.h>

DECLARE_GLOBAL_DATA_PTR;

static u64 mc68681_timer_get_count(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *base = (uart_t *)(plat->base);
	unsigned int count = 0xffff - ((readb(&base->uctu) << 8) + readb(&base->uctl));
	debug("%s: %u\n", __func__, count);
	return count;
}

static int mc68681_timer_probe(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *base = (uart_t *)(plat->base);
	u8 uacr = readb(&base->uacr);

	uacr &= 0b10001111;
	uacr |= 0b00110000; 

	debug("%s: base=0x%p, uacr=0x%x\n", __func__, base, uacr);
	
	/* write to ACR: set Counter/Timer in counter mode to count external clock divided by 16 */
	writeb(uacr, &base->uacr);

	/* read from OPS: start the counter*/
	readb(&base->uops);

	return (0);
}

static int mc68681_of_to_plat(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr_base;

	addr_base = dev_read_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -ENODEV;

	plat->base = (uint32_t)addr_base;
	plat->baudrate = gd->baudrate;

	return 0;
}

static const struct timer_ops mc68681_timer_ops = {
	.get_count = mc68681_timer_get_count,
};

static const struct udevice_id mc68681_timer_ids[] = {
	{ .compatible = "motorola,mc68681_timer" },
	{}
};

U_BOOT_DRIVER(mc68681_timer) = {
	.name = "mc68681_timer",
	.id = UCLASS_TIMER,
	.of_match = mc68681_timer_ids,
	.of_to_plat = mc68681_of_to_plat,
	.plat_auto = sizeof(struct mc68681_serial_plat),
	.probe = mc68681_timer_probe,
	.ops = &mc68681_timer_ops,
};