/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * xr68c681.h -- Register definitions for the Exar XR68C681 / Philips SC28L681
 *               Dual Universal Asynchronous Receiver/Transmitter (DUART).
 *
 * This is a discrete parallel-bus chip; registers are contiguous bytes
 * with no padding (unlike the ColdFire internal UART which has 4-byte
 * spacing).  All three U-Boot drivers that use this chip (serial, timer,
 * GPIO) include this header.
 *
 * Copyright (C) 2023  Graeme Harker <graeme.harker@gmail.com>
 */

#ifndef xr68c681_h
#define xr68c681_h

/* Register map — byte-wide, contiguous */
typedef struct xr68c681 {
	u8 umr;			/* 0x0 Mode Register (MR1/MR2 via pointer) */
	union {
		u8 usr;		/* 0x1 Status Register */
		u8 ucsr;	/* 0x1 Clock Select Register */
	};
	u8 ucr;			/* 0x2 Command Register */
	union {
		u8 utb;		/* 0x3 Transmit Buffer */
		u8 urb;		/* 0x3 Receive Buffer */
	};
	union {
		u8 uipcr;	/* 0x4 Input Port Change Register (read) */
		u8 uacr;	/* 0x4 Auxiliary Control Register (write) */
	};
	union {
		u8 uimr;	/* 0x5 Interrupt Mask Register (write) */
		u8 uisr;	/* 0x5 Interrupt Status Register (read) */
	};
	u8 uctu;		/* 0x6 Counter/Timer Upper Register */
	u8 uctl;		/* 0x7 Counter/Timer Lower Register */
	u8 umrb;		/* 0x8 Mode Register B */
	union {
		u8 usrb;	/* 0x9 Status Register B */
		u8 ucsrb;	/* 0x9 Clock Select Register B */
	};
	u8 ucrb;		/* 0xa Command Register B */
	union {
		u8 utbb;	/* 0xb Transmit Buffer B */
		u8 urbb;	/* 0xb Receive Buffer B */
	};
	u8 ivr;			/* 0xc Interrupt Vector Register */
	union {
		u8 uip;		/* 0xd Input Port Register (read) */
		u8 opcr;	/* 0xd Output Port Configuration Register (write) */
	};
	u8 uops;		/* 0xe Output Port Set Register */
	u8 uopc;		/* 0xf Output Port Clear Register / stop-counter (read) */
} xr68c681_t;

/*********************************************************************
 * UMR — Mode Register (MR1 on first access, MR2 on second)
 *********************************************************************/
#define UART_UMR_BC(x)			(((x)&0x03))
#define UART_UMR_PT			(0x04)
#define UART_UMR_PM(x)			(((x)&0x03)<<3)
#define UART_UMR_ERR			(0x20)
#define UART_UMR_RXIRQ			(0x40)
#define UART_UMR_RXRTS			(0x80)
#define UART_UMR_SB(x)			(((x)&0x0F))
#define UART_UMR_TXCTS			(0x10)
#define UART_UMR_TXRTS			(0x20)
#define UART_UMR_CM(x)			(((x)&0x03)<<6)
#define UART_UMR_PM_MULTI_ADDR		(0x1C)
#define UART_UMR_PM_MULTI_DATA		(0x18)
#define UART_UMR_PM_NONE		(0x10)
#define UART_UMR_PM_FORCE_HI		(0x0C)
#define UART_UMR_PM_FORCE_LO		(0x08)
#define UART_UMR_PM_ODD			(0x04)
#define UART_UMR_PM_EVEN		(0x00)
#define UART_UMR_BC_5			(0x00)
#define UART_UMR_BC_6			(0x01)
#define UART_UMR_BC_7			(0x02)
#define UART_UMR_BC_8			(0x03)
#define UART_UMR_CM_NORMAL		(0x00)
#define UART_UMR_CM_ECH			(0x40)
#define UART_UMR_CM_LOCAL_LOOP		(0x80)
#define UART_UMR_CM_REMOTE_LOOP		(0xC0)
#define UART_UMR_SB_STOP_BITS_1		(0x07)
#define UART_UMR_SB_STOP_BITS_15	(0x08)
#define UART_UMR_SB_STOP_BITS_2		(0x0F)

/*********************************************************************
 * USR — Status Register
 *********************************************************************/
#define UART_USR_RXRDY			(0x01)
#define UART_USR_FFULL			(0x02)
#define UART_USR_TXRDY			(0x04)
#define UART_USR_TXEMP			(0x08)
#define UART_USR_OE			(0x10)
#define UART_USR_PE			(0x20)
#define UART_USR_FE			(0x40)
#define UART_USR_RB			(0x80)

/*********************************************************************
 * UCSR — Clock Select Register
 *********************************************************************/
#define UART_UCSR_TCS(x)		(((x)&0x0F))
#define UART_UCSR_RCS(x)		(((x)&0x0F)<<4)
#define UART_UCSR_RCS_SYS_CLK		(0xD0)
#define UART_UCSR_RCS_CTM16		(0xE0)
#define UART_UCSR_RCS_CTM		(0xF0)
#define UART_UCSR_TCS_SYS_CLK		(0x0D)
#define UART_UCSR_TCS_CTM16		(0x0E)
#define UART_UCSR_TCS_CTM		(0x0F)

/*********************************************************************
 * UCR — Command Register
 *********************************************************************/
#define UART_UCR_RXC(x)			(((x)&0x03))
#define UART_UCR_TXC(x)			(((x)&0x03)<<2)
#define UART_UCR_MISC(x)		(((x)&0x07)<<4)
#define UART_UCR_NONE			(0x00)
#define UART_UCR_STOP_BREAK		(0x70)
#define UART_UCR_START_BREAK		(0x60)
#define UART_UCR_BKCHGINT		(0x50)
#define UART_UCR_RESET_ERROR		(0x40)
#define UART_UCR_RESET_TX		(0x30)
#define UART_UCR_RESET_RX		(0x20)
#define UART_UCR_RESET_MR		(0x10)
#define UART_UCR_TX_DISABLED		(0x08)
#define UART_UCR_TX_ENABLED		(0x04)
#define UART_UCR_RX_DISABLED		(0x02)
#define UART_UCR_RX_ENABLED		(0x01)

/*
 * XR68C681 BRG Select Extend Bit commands (Table 10).
 * Written to CR[3:0]; CR[7:4] must be zero.
 * X=1 selects the extended baud-rate column (Table 8).
 */
#define UART_UCR_SET_RX_EXTEND		(0x08)	/* Set   Rx BRG extend bit (X=1) */
#define UART_UCR_CLR_RX_EXTEND		(0x09)	/* Clear Rx BRG extend bit (X=0) */
#define UART_UCR_SET_TX_EXTEND		(0x0A)	/* Set   Tx BRG extend bit (X=1) */
#define UART_UCR_CLR_TX_EXTEND		(0x0B)	/* Clear Tx BRG extend bit (X=0) */

/*********************************************************************
 * UIPCR — Input Port Change Register / UACR — Auxiliary Control Register
 * Note: address 0x4 reads UIPCR, writes UACR (write-only).
 *********************************************************************/
#define UART_UIPCR_CTS			(0x01)
#define UART_UIPCR_COS			(0x10)
#define UART_UACR_IEC			(0x01)
/* ACR=0xF0: BRG Set 2 (bit 7) + Timer mode X1/CLK÷16 (bits 6:4=111) */
#define UART_UACR_CLK			(0xF0)

/*********************************************************************
 * UIMR — Interrupt Mask Register / UISR — Interrupt Status Register
 *********************************************************************/
#define UART_UIMR_TXRDY			(0x01)
#define UART_UIMR_RXRDY_FU		(0x02)
#define UART_UIMR_DB			(0x04)
#define UART_UIMR_COS			(0x80)

#define UART_UISR_TXRDY			(0x01)
#define UART_UISR_RXRDY_FU		(0x02)
#define UART_UISR_DB			(0x04)
#define UART_UISR_RXFTO			(0x08)
#define UART_UISR_TXFIFO		(0x10)
#define UART_UISR_RXFIFO		(0x20)
#define UART_UISR_COS			(0x80)

/*********************************************************************
 * UIP — Input Port / Output Port registers
 *********************************************************************/
#define UART_UIP_CTS			(0x01)
#define UART_UOP1_RTS			(0x01)
#define UART_UOP0_RTS			(0x01)

#endif	/* xr68c681_h */
