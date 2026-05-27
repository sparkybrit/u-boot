/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2003	Josef Baumgartner <josef.baumgartner@telex.de>
 * Based on code from Bernhard Kuhn <bkuhn@metrowerks.com>
 */

#include <asm-offsets.h>
#include <config.h>
#include <asm/cache.h>

#define _START	_start
#define _FAULT	_fault


#define SAVE_ALL						\
	move.w	#0x2700,%sr;		/* disable intrs */	\
	subl	#60,%sp;		/* space for 15 regs */ \
	moveml	%d0-%d7/%a0-%a6,%sp@;				\

#define RESTORE_ALL						\
	moveml	%sp@,%d0-%d7/%a0-%a6;				\
	addl	#60,%sp;		/* space for 15 regs */ \
	rte

/* If we come from a pre-loader we don't need an initial exception
 * table.
 */
#if !defined(CONFIG_MONITOR_IS_IN_RAM)

.text

/*
 * Vector table. This is used for initial platform startup.
 * These vectors are to catch any un-intended traps.
 */
_vectors:
/* 0x000 */
.long	0x00000000,	  _START,       _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x020 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x040 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x060 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x080 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x0a0 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x0c0 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x0e0 */
.long	_exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler, _exc_handler
/* 0x100 */
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler

.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler

.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler
.long	_int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler, _int_handler,_int_handler

#endif

.text

.globl _start
_start:
	nop
	nop
	move.w	#0x2700,%sr

	/* put relocation table address to a5 */
	move.l	#__got_start, %a5

	/* setup stack initially on top of internal static ram  */
	move.l	#(CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_RAM_SIZE), %sp

	/*
	 * if configured, malloc_f arena will be reserved first,
	 * then (and always) gd struct space will be reserved
	 */
	move.l	%sp, -(%sp)
	move.l	#board_init_f_alloc_reserve, %a1
	jsr	(%a1)

	/* update stack and frame-pointers */
	move.l	%d0, %sp
	move.l	%sp, %fp

	/* initialize reserved area */
	move.l	%d0, -(%sp)
	move.l	#board_init_f_init_reserve, %a1
	jsr	(%a1)

	/* run low-level CPU init code (from flash) */
	move.l	#cpu_init_f, %a1
	jsr	(%a1)

#ifdef CONFIG_DEBUG_UART
	move.l	#debug_uart_init, %a1
	jsr	(%a1)
#endif

	/* run low-level board init code (from flash) */
	clr.l	%sp@-
	move.l	#board_init_f, %a1
	jsr	(%a1)

	/* board_init_f() does not return */

/******************************************************************************/

/*
 * void relocate_code(addr_sp, gd, addr_moni)
 *
 * This "function" does not return, instead it continues in RAM
 * after relocating the monitor code.
 *
 * r3 = dest
 * r4 = src
 * r5 = length in bytes
 * r6 = cachelinesize
 */
.globl relocate_code
relocate_code:
	link.w	%a6,#0
	move.l	8(%a6), %sp		/* set new stack pointer */

	move.l	12(%a6), %d0		/* Save copy of Global Data pointer */
	move.l	16(%a6), %a0		/* Save copy of Destination Address */

	move.l	#CONFIG_SYS_MONITOR_BASE, %a1
	move.l	#__init_end, %a2
	move.l	%a0, %a3
	/* copy the code to RAM */
1:
	move.l	(%a1)+, (%a3)+
	cmp.l	%a1,%a2
	bgt.s	1b

/*
 * Process .rela.dyn to relocate the RAM copy.
 *
 * Without -fPIC/-msep-data the linker emits only R_68K_RELATIVE (22)
 * relocations for absolute data pointers.  For each entry the addend
 * (stored in the slot by relocate-rela at build time) equals the
 * link-time VMA; add the relocation delta (dest_addr - MONITOR_BASE)
 * to produce the final RAM address.
 *
 * Vector-table entries (r_offset < MONITOR_BASE) are not copied to RAM
 * and are skipped.  R_68K_32 and R_68K_JMP_SLOT paths are retained for
 * completeness but are currently dead code.
 */
#define R_68K_32        1
#define R_68K_RELATIVE  22
#define R_68K_JMP_SLOT  21

	move.l	#__rel_dyn_start, %a1
	move.l	#__rel_dyn_end, %a2

fixloop:
	move.l	(%a1)+, %d1		/* r_offset */
	move.l	(%a1)+, %d2		/* r_info  */
	addq.l	#4, %a1			/* skip r_addend (not needed) */

	move.l	%d2, %d3
	andi.l	#0xff, %d3		/* d3 = reloc type */

	cmp.l	#R_68K_RELATIVE, %d3
	beq.s	fixup
	cmp.l	#R_68K_32, %d3
	beq.s	fixup
	cmp.l	#R_68K_JMP_SLOT, %d3
	beq.s	fixup_jmp_slot
	bra	fixnext

fixup:
	/* skip vector table (VMA < MONITOR_BASE — not in RAM copy) */
	cmp.l	#CONFIG_SYS_MONITOR_BASE, %d1
	bcs.s	fixnext
	/* RAM address of the slot to update */
	move.l	%a0, %a3
	add.l	%d1, %a3
	sub.l	#CONFIG_SYS_MONITOR_BASE, %a3
	/* read link-time value, apply relocation delta */
	move.l	(%a3), %d4
	add.l	%a0, %d4
	sub.l	#CONFIG_SYS_MONITOR_BASE, %d4
	move.l	%d4, (%a3)
	bra	fixnext

fixup_jmp_slot:
	/* skip vector table (VMA < MONITOR_BASE — not in RAM copy) */
	cmp.l	#CONFIG_SYS_MONITOR_BASE, %d1
	bcs.s	fixnext
	/* symbol index = r_info >> 8 */
	move.l	%d2, %d4
	lsr.l	#8, %d4
	/* offset into .dynsym (Elf32_Sym = 16 bytes) */
	lsl.l	#4, %d4
	move.l	#__dyn_sym_start, %a3
	add.l	%d4, %a3
	/* read st_value (offset 4 in Elf32_Sym) from flash */
	move.l	4(%a3), %d4
	/* apply relocation delta */
	add.l	%a0, %d4
	sub.l	#CONFIG_SYS_MONITOR_BASE, %d4
	/* write resolved address to GOT.PLT slot in RAM */
	move.l	%a0, %a3
	add.l	%d1, %a3
	sub.l	#CONFIG_SYS_MONITOR_BASE, %a3
	move.l	%d4, (%a3)

fixnext:
	cmp.l	%a1, %a2
	bgt.w	fixloop			/* word form: loop body too large for short branch */

	/* point %a5 at the RAM copy of the GOT */
	move.l	%a0, %a5
	add.l	#(__got_start - CONFIG_SYS_MONITOR_BASE), %a5

	move.l	%a0, %a1
	add.l	#(in_ram - CONFIG_SYS_MONITOR_BASE), %a1
	jmp	(%a1)

in_ram:

clear_bss:
	/*
	 * Clear BSS in the RAM copy.
	 *
	 * PC-relative lea avoids R_68K_RELATIVE relocations in this code.
	 * Running in RAM at dest_addr + offset, lea sym(%pc) resolves to
	 * dest_addr + (sym_vma - MONITOR_BASE) — the correct RAM address.
	 */
	lea	_sbss(%pc), %a1
	lea	_ebss(%pc), %a2
	cmpl	%a1, %a2
	ble.s	7f
6:
	clr.l	(%a1)+
	cmpl	%a1, %a2
	bgt.s	6b
7:

	/* jump to board_init_r in RAM via PC-relative addressing */
	lea	board_init_r(%pc), %a1

	/* set parameters for board_init_r */
	move.l	%a0,-(%sp)		/* dest_addr */
	move.l	%d0,-(%sp)		/* gd */
	jsr	(%a1)

/******************************************************************************/

/* exception code */
.globl _fault
_fault:
	bra	_fault

.globl _exc_handler
_exc_handler:
	SAVE_ALL
	movel	%sp,%sp@-
	bsr	exc_handler
	addql	#4,%sp
	RESTORE_ALL

.globl _int_handler
_int_handler:
	SAVE_ALL
	movel	%sp,%sp@-
	bsr	int_handler
	addql	#4,%sp
	RESTORE_ALL

/******************************************************************************/

.align 4
