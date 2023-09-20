/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015  Angelo Dureghello <angelo@sysam.it>
 */

#ifndef __serial_mc68681_h
#define __serial_mc68681_h

/*
 * struct coldfire_serial_plat - information about a coldfire port
 *
 * @base:               Uart port base register address
 * @port:               Uart port index, for cpu with pinmux for uart / gpio
 * baudrtatre:          Uart port baudrate
 */
struct mc68681_serial_plat {
	unsigned long base;
	int port;
	int baudrate;
};

#endif /* __serial_mc68681_h */
