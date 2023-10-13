/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2023  Graeme harker <graeme.harker@gmail.com>
 */

#ifndef __mc68681_h
#define __mc68681_h

/*
 * information about a uart port
 *
 * @base:               Uart port base register address
 * @port:               Uart port index, for cpu with pinmux for uart / gpio
 * baudrate: 	        Uart port baudrate
 */
struct mc68681_plat {
	unsigned long base;
	int port;
	int baudrate;
	u8  outreg;
};

#endif /* __mc68681_h */
