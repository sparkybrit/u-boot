// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker <graeme.harker@gmail.com>
 */

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
