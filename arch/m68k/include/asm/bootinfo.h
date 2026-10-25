/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025, Kuan-Wei Chiu <visitorckw@gmail.com>
 *
 * Definitions for the m68k bootinfo interface.
 */

#ifndef _ASM_M68K_BOOTINFO_H
#define _ASM_M68K_BOOTINFO_H

#ifndef __ASSEMBLY__

struct bi_record {
	unsigned short tag;        /* tag ID */
	unsigned short size;       /* size of record (in bytes) */
	unsigned long data[0];     /* data */
};

struct mem_info {
	unsigned long addr;        /* physical address of memory chunk */
	unsigned long size;        /* length of memory chunk (in bytes) */
};

/* Machine, CPU, FPU and MMU types for the bootinfo a kernel is given */
struct m68k_bootinfo_types {
	unsigned long machtype;
	unsigned long cputype;
	unsigned long fputype;
	unsigned long mmutype;
};

void board_bootinfo_types(struct m68k_bootinfo_types *t);

#endif /* __ASSEMBLY__ */

/* Bootinfo Tag IDs */
#define BI_LAST            0x0000
#define BI_MACHTYPE        0x0001
#define BI_CPUTYPE         0x0002
#define BI_FPUTYPE         0x0003
#define BI_MMUTYPE         0x0004
#define BI_MEMCHUNK        0x0005
#define BI_RAMDISK         0x0006
#define BI_COMMAND_LINE    0x0007

#define BI_RNG_SEED        0x0008

/* Machine types (BI_MACHTYPE) */
#define MACH_AMIGA         1
#define MACH_ATARI         2
#define MACH_MAC           3
#define MACH_APOLLO        4
#define MACH_SUN3          5
#define MACH_MVME147       6
#define MACH_MVME16x       7
#define MACH_BVME6000      8
#define MACH_HP300         9
#define MACH_Q40           10
#define MACH_SUN3X         11
#define MACH_M54XX         12
#define MACH_M5441X        13
#define MACH_VIRT          14

/* CPU, FPU and MMU types (BI_CPUTYPE, BI_FPUTYPE, BI_MMUTYPE) */
#define CPU_68020          (1 << 0)
#define CPU_68030          (1 << 1)
#define CPU_68040          (1 << 2)
#define CPU_68060          (1 << 3)

#define FPU_68881          (1 << 0)
#define FPU_68882          (1 << 1)
#define FPU_68040          (1 << 2)	/* internal FPU */
#define FPU_68060          (1 << 3)	/* internal FPU */

#define MMU_68851          (1 << 0)
#define MMU_68030          (1 << 1)	/* internal MMU */
#define MMU_68040          (1 << 2)	/* internal MMU */
#define MMU_68060          (1 << 3)	/* internal MMU */

/*
 * A kernel image starts with a bra.s over this magic and a table of
 * (machine type, bootinfo version) pairs ending in 0.
 */
#define BOOTINFOV_MAGIC    0x4249561A	/* 'BIV^Z' */

/* QEMU virt specific tags */
#define BI_VIRT_QEMU_VERSION    0x8000
#define BI_VIRT_GF_PIC_BASE     0x8001
#define BI_VIRT_GF_RTC_BASE     0x8002
#define BI_VIRT_GF_TTY_BASE     0x8003
#define BI_VIRT_VIRTIO_BASE     0x8004
#define BI_VIRT_CTRL_BASE       0x8005

#endif /* _ASM_M68K_BOOTINFO_H */
