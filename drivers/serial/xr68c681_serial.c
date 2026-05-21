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

#include <dm.h>
#include <asm/global_data.h>
#include <dm/platform_data/mc68681.h>
#include <serial.h>
#include <linux/compiler.h>
#include <asm/immap.h>
#include <xr68c681.h>

DECLARE_GLOBAL_DATA_PTR;

extern void uart_port_conf(int port);

/*
 * XR68C681 BRG table for BRG Set 2 (ACR[7]=1), 3.6864 MHz crystal.
 * Rates from Table 8 of the XR68C681 datasheet.  The 'extend' flag
 * controls the per-channel X bit (Table 10): X=1 selects the extended
 * column, which provides higher bit rates for the same CSR code.
 */
struct brg_entry {
	int  baudrate;
	u8   csr;
	bool extend;
};

static const struct brg_entry brg_table[] = {
	{    75, 0x00, false },
	{   110, 0x11, false },
	{   150, 0x33, false },
	{   300, 0x44, false },
	{   600, 0x55, false },
	{  1200, 0x66, false },
	{  2000, 0x77, false },
	{  2400, 0x88, false },
	{  4800, 0x99, false },
	{  1800, 0xAA, false },
	{  9600, 0xBB, false },
	{ 19200, 0xCC, false },
	{ 28800, 0x66,  true },
	{ 57600, 0x77,  true },
	{115200, 0x88,  true },
	{ 38400, 0xCC,  true },
};

static void xr68c681_serial_setbrg_common(xr68c681_t *uart, int baudrate)
{
	int i;

	debug("%s: uart=%p, baudrate=%d\n", __func__, uart, baudrate);

	writeb(UART_UCR_RESET_RX, &uart->ucr);
	writeb(UART_UCR_RESET_TX, &uart->ucr);

	for (i = 0; i < ARRAY_SIZE(brg_table); i++) {
		if (brg_table[i].baudrate == baudrate) {
			if (brg_table[i].extend) {
				writeb(UART_UCR_SET_RX_EXTEND, &uart->ucr);
				writeb(UART_UCR_SET_TX_EXTEND, &uart->ucr);
			} else {
				writeb(UART_UCR_CLR_RX_EXTEND, &uart->ucr);
				writeb(UART_UCR_CLR_TX_EXTEND, &uart->ucr);
			}
			writeb(brg_table[i].csr, &uart->ucsr);
			break;
		}
	}

	writeb(UART_UCR_RX_ENABLED | UART_UCR_TX_ENABLED, &uart->ucr);
}

static int xr68c681_serial_init_common(xr68c681_t *uart, int port_idx, int baudrate)
{
	uart_port_conf(port_idx);

	writeb(UART_UCR_RESET_RX, &uart->ucr);
	writeb(UART_UCR_RESET_TX, &uart->ucr);
	writeb(UART_UCR_RESET_ERROR, &uart->ucr);
	writeb(UART_UCR_RESET_MR, &uart->ucr);
	__asm__("nop");

	writeb(0, &uart->uimr);
	writeb(UART_UACR_CLK, &uart->uacr);

	/* MR1: 8 data bits, no parity; MR2: 1 stop bit */
	writeb(UART_UMR_BC_8 | UART_UMR_PM_NONE, &uart->umr);
	writeb(UART_UMR_SB_STOP_BITS_1, &uart->umr);

	xr68c681_serial_setbrg_common(uart, baudrate);

	return 0;
}

static int xr68c681_serial_probe(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);

	plat->port = dev_seq(dev);

	debug("%s: port=%d)\n", __func__, plat->port);

	return xr68c681_serial_init_common((xr68c681_t *)plat->base,
						plat->port, plat->baudrate);
}

static int xr68c681_serial_putc(struct udevice *dev, const char ch)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	xr68c681_t *uart = (xr68c681_t *)plat->base;

	/* Wait for last character to go. */
	if (!(readb(&uart->usr) & UART_USR_TXRDY))
		return -EAGAIN;

	writeb(ch, &uart->utb);

	return 0;
}

static int xr68c681_serial_getc(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	xr68c681_t *uart = (xr68c681_t *)(plat->base);

	/* Wait for a character to arrive. */
	if (!(readb(&uart->usr) & UART_USR_RXRDY))
		return -EAGAIN;

	return readb(&uart->urb);
}

int xr68c681_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	xr68c681_t *uart = (xr68c681_t *)(plat->base);

	xr68c681_serial_setbrg_common(uart, baudrate);

	return 0;
}

static int xr68c681_serial_pending(struct udevice *dev, bool input)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	xr68c681_t *uart = (xr68c681_t *)(plat->base);

	if (input)
		return readb(&uart->usr) & UART_USR_RXRDY ? 1 : 0;
	else
		return readb(&uart->usr) & UART_USR_TXRDY ? 0 : 1;

	return 0;
}

static int xr68c681_serial_of_to_plat(struct udevice *dev)
{
	struct mc68681_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr_base;

	addr_base = dev_read_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -ENODEV;

	plat->base = (uint32_t)addr_base;
	plat->baudrate = gd->baudrate;

	return 0;
}

static const struct dm_serial_ops xr68c681_serial_ops = {
	.putc = xr68c681_serial_putc,
	.pending = xr68c681_serial_pending,
	.getc = xr68c681_serial_getc,
	.setbrg = xr68c681_serial_setbrg,
};

static const struct udevice_id xr68c681_serial_ids[] = {
	{ .compatible = "exar,xr68c681" },
	{ }
};

U_BOOT_DRIVER(xr68c681_serial) = {
	.name = "xr68c681_serial",
	.id = UCLASS_SERIAL,
	.of_match = xr68c681_serial_ids,
	.of_to_plat = xr68c681_serial_of_to_plat,
	.plat_auto	= sizeof(struct mc68681_plat),
	.probe = xr68c681_serial_probe,
	.ops = &xr68c681_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
