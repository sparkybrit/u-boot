// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023  Graeme Harker <graeme.harker@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/platform_data/serial_mc68681.h>
#include <serial.h>
#include <timer.h>
#include <linux/compiler.h>
#include <asm/immap.h>
#include <asm/uart.h>
#include <irq_func.h>

#define	IRQ_TIMER_VECTOR 0
#define	IRQ_TIMER_MASK 0b1000

DECLARE_GLOBAL_DATA_PTR;

static volatile u64 counter;

static void timer_interrupt_handler(void *arg)
{
	uart_t *base = (uart_t *)arg;

	/* reset the timer interrupt */
	readb(&base->uopc);

	/* increment the timer counter */
	counter++;
}

static u64 mc68681_timer_get_count(struct udevice *dev)
{
	debug("%s: %llu\n", __func__, counter);
	return counter;
}

static int mc68681_timer_probe(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *base = (uart_t *)(plat->base);
	u8 uacr = readb(&base->uacr);

	/* generate an interrupt every 10ms (100Hz) */
	writeb(0x04, &base->uctu);
	writeb(0x80, &base->uctl);

	/* mask out the timer/counter mode control bits */
	uacr &= 0b10001111;
	/* set mode to timer with uart clk divided by 16 */
	uacr |= 0b01110000; 
	/* write to ACR: set Counter/Timer in time mode with external uart clk divided by 16 */
	writeb(uacr, &base->uacr);

	debug("%s: base=0x%p, uacr=0x%x\n", __func__, base, uacr);

	/* set the interrupt vector */
	writeb(IRQ_TIMER_VECTOR + 0x40, &base->ivr);

	irq_install_handler(IRQ_TIMER_VECTOR, (interrupt_handler_t *)timer_interrupt_handler, base);

	/* enable counter/timer interrupts */
	writeb(IRQ_TIMER_MASK, &base->uimr);	

	return (0);
}

static int mc68681_of_to_plat(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	struct timer_dev_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr_base;

	addr_base = dev_read_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -ENODEV;

	plat->base = (uint32_t)addr_base;
	plat->baudrate = gd->baudrate;
	priv->clock_rate = 9600;

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