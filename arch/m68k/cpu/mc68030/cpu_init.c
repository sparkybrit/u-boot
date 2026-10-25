// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * Graeme Harker <graeme.harker@gmail.com>
 */

#include <bootm.h>
#include <cpu_func.h>
#include <init.h>
#include <watchdog.h>
#include <asm/cache.h>
#include <asm/immap.h>
#include <asm/io.h>

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

/*
 * Leave the CPU the way a Linux kernel expects to find it.  The data cache
 * is write-through on the 68030, so memory already holds everything that
 * was written and flushing it is the same as invalidating it.  Writing
 * CI and CD alone disables both caches and invalidates them in one go,
 * and clears the freeze, burst and write-allocate bits as well.
 *
 * Interrupts are masked to level 7, which on sparky1 covers the DUART at
 * level 5; the kernel lowers the mask once it has its own vectors in place.
 */
void arch_preboot_os(void)
{
	__asm__ __volatile__(
		"move.w	#0x2700,%%sr\n\t"
		"movec	%0,%%cacr\n"
		: : "d" (M68K_CACR_CI | M68K_CACR_CD) : "memory");
}
