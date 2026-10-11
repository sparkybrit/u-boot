/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *  cpu.h
 *
 *  Copyright (c) 2009 Freescale Semiconductor, Inc.
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <command.h>

/* Use this to create board specific reset functions */
void board_reset(void) __attribute__((__weak__));

/* start.S: pulse /RESET and restart; call at its link-time address */
void reset_from_rom(void);

#endif /* _CPU_H_ */
