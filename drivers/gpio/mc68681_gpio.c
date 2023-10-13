// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021, Collabora Ltd.
 * Copyright (C) 2021, General Electric Company
 * Author(s): Sebastian Reichel <sebastian.reichel@collabora.com>
 */

#define LOG_CATEGORY UCLASS_GPIO

#include <common.h>
#include <errno.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/uart.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dt-bindings/gpio/gpio.h>
#include <dm/platform_data/mc68681.h>

static int mc68681_gpio_get_value(struct udevice *dev, uint offset)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	uart_t *uart = (uart_t *)dev_read_addr(dev);
	int val = 0;

	if (uc_priv->gpio_count == 8)
	{
		val = (plat->outreg >> offset) & 1;
	}
	else
	{
		val = (readb(&uart->uip) >> offset) & 1;
	}

	dev_dbg(dev, "%s() offset=%u, val=%u, outreg=0x%x\n", __func__, offset, val, plat->outreg);

	return val;
}

static int mc68681_gpio_direction_output(struct udevice *dev, uint offset, int val)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	uart_t *uart = (uart_t *)dev_read_addr(dev);

	// XXX check that offset is less than gpio_count 
	//struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	// unsigned gpio_count = uc_priv->gpio_count;

	if (val)
	{
		plat->outreg |= BIT(offset);
		writeb(BIT(offset), &uart->uopc);
	}
	else 
	{
		plat->outreg &= ~BIT(offset);
		writeb(BIT(offset), &uart->uops);
	}

	dev_dbg(dev, "%s() offset=%u, val=%u, outreg=0x%x\n", __func__, offset, val, plat->outreg);

	return 0;
}

static int mc68681_gpio_set_value(struct udevice *dev, uint offset, int val)
{
	dev_dbg(dev, "%s() offset=%u, val=%u\n", __func__, offset, val);
	return mc68681_gpio_direction_output(dev, offset, val);
}

static int mc68681_gpio_direction_input(struct udevice *dev, uint offset)
{
	int val = mc68681_gpio_get_value(dev, offset);
	dev_dbg(dev, "%s() offset=%u, val=%d\n", __func__, offset, val);
	return val;
}

static int mc68681_gpio_get_function(struct udevice *dev, uint offset)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	int func = uc_priv->gpio_count == 8 ? GPIOF_OUTPUT : GPIOF_INPUT;
	dev_dbg(dev, "%s() offset=%u, func=%d\n", __func__, offset, func);
	return func;
}

static const struct dm_gpio_ops mc68681_gpio_ops = {
	.direction_input	= mc68681_gpio_direction_input,
	.direction_output	= mc68681_gpio_direction_output,
	.get_value		= mc68681_gpio_get_value,
	.set_value		= mc68681_gpio_set_value,
	.get_function		= mc68681_gpio_get_function,
};

static int mc68681_gpio_probe(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	uart_t *uart = (uart_t *)dev_read_addr(dev);
	char name[32], label[8], *str;
	int size;
	const u8 *tmp;

	plat->outreg = 0;

	/* set all output port pins as gpio pins */
	writeb(0, &uart->opcr);

	/* clear all output gpios */
	writeb(0xff, &uart->uops);

	tmp = dev_read_prop(dev, "gpio-bank-name", &size);

	if (tmp) 
	{
		memcpy(label, tmp, sizeof(label) - 1);
		label[sizeof(label) - 1] = '\0';
		snprintf(name, sizeof(name), "%s", label);
	} 
	else 
	{
		snprintf(name, sizeof(name), "gpio");
	}

	str = strdup(name);

	if (!str)
		return -ENOMEM;

	uc_priv->bank_name = str;
	uc_priv->gpio_count = dev_read_u32_default(dev, "ngpios", 8);

	dev_dbg(dev, "%s() addr=0x%x, bank_name=%s, gpio_count=%d\n", __func__,  (unsigned int) uart, str, uc_priv->gpio_count);

	return 0;
}

static const struct udevice_id mc68681_gpio_ids[] = {
	{ .compatible = "motorola,mc68681_gpio"},
	{ }
};

U_BOOT_DRIVER(mc68681_gpio) = {
	.name		= "mc68681_gpio",
	.id			= UCLASS_GPIO,
	.ops		= &mc68681_gpio_ops,
	.probe		= mc68681_gpio_probe,
	.of_match	= mc68681_gpio_ids,
	.plat_auto	= sizeof(struct mc68681_plat),
};
