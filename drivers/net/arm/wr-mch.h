/*
 * White Rabbit MCH NIC driver
 *
 *  Copyright (c) 2009 GSI
 *  Written by Mathias Kreider <m.kreider@gsi.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bitops.h>

/*
 * NIC Reset Register
 * writing to it resets the whole NIC
 * all the registers are set to '0' when the NIC is reset
 */
#define WR_NIC_RST		0x0000

/*
 * NIC Control Register
 */
#define WR_NIC_CTL		0x0001
#define WR_NIC_CTL_TX_EN	BIT(0)	/* enable TX if there's data */
#define WR_NIC_CTL_RX_EN	BIT(1)	/* enable RX if there's free space */
#define WR_NIC_CTL_TX_PAUSE	BIT(2)	/* send pause frame immediately */
#define WR_NIC_CTL_RX_BAD_CRC	BIT(3)	/* accept frames with invalid CRC */
#define WR_NIC_CTL_RX_RUNT	BIT(4)	/* enable reception of 'runt' frames */
#define WR_NIC_FLOW_CTL_ENABLE	BIT(5)	/* enable flow control */
#define WR_NIC_CTL_MASK		0x003F

/*
 * NIC Interrupt Enable Register
 */
#define WR_NIC_IER		0x0002
#define WR_NIC_IER_TXI		BIT(0)	/* TX done */
#define WR_NIC_IER_RXI		BIT(1)	/* RX done */
#define WR_NIC_IER_RXPAUSE	BIT(2)	/* pause frame received */
#define WR_NIC_IER_TXERR	BIT(3)	/* TX error (on fabric i/f) */
#define WR_NIC_IER_RXERR	BIT(4)	/* RX error (on fabric or overflow) */
#define WR_NIC_IER_MASK		0x001F

/*
 * NIC Interrupt Status Register
 * each bit is Clear-on-write(1)
 */
#define WR_NIC_ISR		0x0003
#define WR_NIC_ISR_TXI		BIT(0)
#define WR_NIC_ISR_RXI		BIT(1)
#define WR_NIC_ISR_RXPAUSE	BIT(2)
#define WR_NIC_ISR_TXERR	BIT(3)
#define WR_NIC_ISR_RXERR	BIT(4)
#define WR_NIC_ISR_MASK		0x001F

/*
 * NIC Status Register
 */
#define WR_NIC_STAT		0x0004
#define WR_NIC_STAT_RXOV	BIT(0)	 /* RX overflow. Clear-on-write(1) */
#define WR_NIC_STAT_RXER	BIT(1)	 /* RX error. Clear-on-write(1) */
#define WR_NIC_STAT_RXER_CODE	(0xF<<2) /* RX error code from i/f. R/O */
#define WR_NIC_STAT_TXER	BIT(6)	 /* TX error. Clear-on-write(1) */
#define WR_NIC_STAT_TXER_CODE	(0xF<<7) /* TX error code from i/f. R/O */
#define WR_NIC_STAT_MASK	0x07FF

/*
 * NIC TX Buffer Threshold
 * minimum amount of free space in TX buffer the NIC should keep
 */
#define WR_NIC_TX_BUF_THRES	0x0005

/*
 * NIC RX Buffer Threshold
 * minimum amount of free space in RX buffer the NIC should keep
 */
#define WR_NIC_RX_BUF_THRES	0x0006

/*
 * NIC TX Pause Counter
 * value for pause frame to be sent
 */
#define WR_NIC_TX_PAUSE_LEN	0x0007

/*
 * NIC RX Pause Counter
 * value for pause frame to be sent
 */
#define WR_NIC_RX_PAUSE_LEN	0x0008

/*
 * NIC TX Desc Start - End
 * width is 12 bit: address (8192B, 32b -> 11 bits) + 1 wraparound bit
 */
/* address of the first word of first (oldest) TX descriptor */
#define WR_NIC_TX_DESC_START	0x0009
/* address of the last word of the last (newest) TX descriptor */
#define WR_NIC_TX_DESC_END	0x000A

/*
 * NIC RX Desc Start - End
 * width is 13 bit: address (16384B, 32b -> 12 bits) + 1 wraparound bit
 */
/* address of the first word of first (oldest) RX descriptor */
#define WR_NIC_RX_DESC_START	0x000B
/* address of the last word of the last(newest) RX descriptor */
#define WR_NIC_RX_DESC_END	0x000C
#define WR_NIC_DESC_MASK	0x0FFF

/*
 * NIC Memory Offsets
 * mapped TX memory. x4000 - x5FFF real memory, x6000 - x7FFF is wraparound area
 * mapped RX memory. x8000 - xBFFF real memory, xC000 - xFFFF is wraparound area
 */
#define WR_NIC_TX_OFFSET	0x4000
#define WR_NIC_RX_OFFSET	0x8000
