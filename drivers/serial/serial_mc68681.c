// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2004-2007 Freescale Semiconductor, Inc.
 * TsiChung Liew, Tsi-Chung.Liew@freescale.com.
 *
 * Modified to add device model (DM) support
 * (C) Copyright 2015  Angelo Dureghello <angelo@sysam.it>
 *
 * Modified to add DM and fdt support, removed non DM code
 * (C) Copyright 2018  Angelo Dureghello <angelo@sysam.it>
 * 
 * Modified to support 68681
 * (C) Copyright 2023  Graeme Harker <graeme.harker@gmail.com>
 */

/*
 * Minimal serial functions needed to use one of the uart ports
 * as serial console interface.
 */

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/platform_data/serial_mc68681.h>
#include <serial.h>
#include <linux/compiler.h>
#include <asm/immap.h>
#include <asm/uart.h>

DECLARE_GLOBAL_DATA_PTR;

extern void uart_port_conf(int port);

static int mc68681_serial_init_common(uart_t *uart, int port_idx, int baudrate)
{
	uart_port_conf(port_idx);

	writeb(UART_UCR_RESET_RX, &uart->ucr);
	writeb(UART_UCR_RESET_TX, &uart->ucr);
	writeb(UART_UCR_RESET_ERROR, &uart->ucr);
	writeb(UART_UCR_RESET_MR, &uart->ucr);
	__asm__("nop");

	/* write to IMR Interrupt Mask Register */
	/* disable uart interrupts */
	writeb(0, &uart->uimr);

	/* write to ACR: */
	/* set baudrate set to 2 */
	/* set counter/timer source to CLK/16 */
	/* disable gpio interrupts */
	writeb(UART_UACR_CLK, &uart->uacr);

	/* read from CR: set test baud rates */
	/* readb(&uart->ucr);*/

	/* write to CSR: set RX/TX baud rate */
	writeb(UART_UCSR_RCS(0b1011) | UART_UCSR_TCS(0b1011), &uart->ucsr);

	/* write to MR: set 8N1 */
	writeb(UART_UMR_BC_8 | UART_UMR_PM_NONE, &uart->umr);
	writeb(UART_UMR_SB_STOP_BITS_1, &uart->umr);

	writeb(UART_UCR_RX_ENABLED | UART_UCR_TX_ENABLED, &uart->ucr);

	return (0);
}

static void mc68681_serial_setbrg_common(uart_t *uart, int baudrate)
{
	debug("mc68681_serial_setbrg_common(uart=%p, baudrate=%d)\n", uart, baudrate);

	writeb(UART_UCR_RESET_RX, &uart->ucr);
	writeb(UART_UCR_RESET_TX, &uart->ucr);

	writeb(UART_UCR_RX_ENABLED | UART_UCR_TX_ENABLED, &uart->ucr);
}

static int mc68681_serial_probe(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);

	plat->port = dev_seq(dev);

	debug("serial_68681.c:coldfire_serial_probe(port=%d)\n", plat->port);

	return mc68681_serial_init_common((uart_t *)plat->base,
						plat->port, plat->baudrate);
}

static int mc68681_serial_putc(struct udevice *dev, const char ch)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *uart = (uart_t *)plat->base;

	/* Wait for last character to go. */
	if (!(readb(&uart->usr) & UART_USR_TXRDY))
		return -EAGAIN;

	writeb(ch, &uart->utb);

	return 0;
}

static int mc68681_serial_getc(struct udevice *dev)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *uart = (uart_t *)(plat->base);

	/* Wait for a character to arrive. */
	if (!(readb(&uart->usr) & UART_USR_RXRDY))
		return -EAGAIN;

	return readb(&uart->urb);
}

int mc68681_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *uart = (uart_t *)(plat->base);

	mc68681_serial_setbrg_common(uart, baudrate);

	return 0;
}

static int mc68681_serial_pending(struct udevice *dev, bool input)
{
	struct mc68681_serial_plat *plat = dev_get_plat(dev);
	uart_t *uart = (uart_t *)(plat->base);

	if (input)
		return readb(&uart->usr) & UART_USR_RXRDY ? 1 : 0;
	else
		return readb(&uart->usr) & UART_USR_TXRDY ? 0 : 1;

	return 0;
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

static const struct dm_serial_ops mc68681_serial_ops = {
	.putc = mc68681_serial_putc,
	.pending = mc68681_serial_pending,
	.getc = mc68681_serial_getc,
	.setbrg = mc68681_serial_setbrg,
};

static const struct udevice_id mc68681_serial_ids[] = {
	{ .compatible = "motorola,uart68681" },
	{ }
};

U_BOOT_DRIVER(serial_mc68681) = {
	.name = "serial_mc68681",
	.id = UCLASS_SERIAL,
	.of_match = mc68681_serial_ids,
	.of_to_plat = mc68681_of_to_plat,
	.plat_auto	= sizeof(struct mc68681_serial_plat),
	.probe = mc68681_serial_probe,
	.ops = &mc68681_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
