// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker <graeme.harker@gmail.com>
 */
#define DEBUG

#include <common.h>
#include <cpu_func.h>
#include <init.h>
#include <watchdog.h>
#include <asm/immap.h>
#include <asm/io.h>
#include <asm/uart.h>

#if defined(CONFIG_CMD_NET)
#include <config.h>
#include <net.h>
#include <asm/fec.h>
#endif


static int scc68681_serial_init_common(uart_t *uart, int port_idx, int baudrate)
{
	/* write to SICR: SIM2 = uart mode,dcd does not affect rx */
	writeb(UART_UCR_RESET_RX, &uart->ucr);
	writeb(UART_UCR_RESET_TX, &uart->ucr);
	writeb(UART_UCR_RESET_ERROR, &uart->ucr);
	writeb(UART_UCR_RESET_MR, &uart->ucr);
	__asm__("nop");

	/* write to IMR: disable all interrupts */
	writeb(0, &uart->uimr);

	/* write to UACR: set baudrate set to 1, set counter/timer source as CLK, disable port interrupts */
	writeb(UART_UACR_CLK, &uart->uacr);

	/* read from CR: set test baud rates */
	readb(&uart->ucr);

	/* write to CSR: RX/TX baud rate from timers */
	writeb(UART_UCSR_RCS(6) | UART_UCSR_TCS(6), &uart->ucsr);

	writeb(UART_UMR_BC_8 | UART_UMR_PM_NONE, &uart->umr);
	writeb(UART_UMR_SB_STOP_BITS_1, &uart->umr);

	writeb(UART_UCR_RX_ENABLED | UART_UCR_TX_ENABLED, &uart->ucr);

	return (0);
}

static int scc68681_serial_putc(const char ch)
{
	uart_t *uart = (uart_t *) 0x80000000;

	/* Wait for last character to go. */
	if (!(readb(&uart->usr) & UART_USR_TXRDY))
		return -EAGAIN;

	writeb(ch, &uart->utb);

	return 0;
}

static int scc68681_serial_puts(const char *s)
{
	while (*s)
	{
		while (scc68681_serial_putc(*s));
		s++;
	}
	return 0;
}

/*
 * Breath some life into the CPU...
 */
void cpu_init_f(void)
{
	icache_enable();
	dcache_enable();
}
/*
 * initialize higher level parts of CPU like timers
 */
int cpu_init_r(void)
{
	return (0);
}

void uart_port_conf(int port)
{
}
