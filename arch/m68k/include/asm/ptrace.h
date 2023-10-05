/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _M68K_PTRACE_H
#define _M68K_PTRACE_H

/*
 * This struct defines the way the registers are stored on the
 * kernel stack during an exception.
 */
#ifndef __ASSEMBLY__

struct pt_regs {
	ulong d0;
	ulong d1;
	ulong d2;
	ulong d3;
	ulong d4;
	ulong d5;
	ulong d6;
	ulong d7;
	ulong a0;
	ulong a1;
	ulong a2;
	ulong a3;
	ulong a4;
	ulong a5;
	ulong a6;
#if defined(CONFIG_MCF520x)|| defined(CONFIG_MCF523x)|| defined(CONFIG_MCF52x2)||defined(CONFIG_MCF530x)||defined(CONFIG_MCF5301x)||defined(CONFIG_MCF532x)||defined(CONFIG_MCF537x)||defined(CONFIG_MCF5441x)
	unsigned format:4;	/* frame format specifier */
	unsigned vector:12;	/* vector offset */
	unsigned short sr;
	unsigned long pc;
#endif
#if defined(CONFIG_MC68030)
	unsigned short sr;
	unsigned long pc;
	unsigned format:4;	/* frame format specifier */
	unsigned vector:12;	/* vector offset */
#endif
};

#endif				/* #ifndef __ASSEMBLY__ */

#endif				/* #ifndef _M68K_PTRACE_H */
