/*
 * White Rabbit MCH MAC Ethernet driver
 *
 *  Copyright (c) 2009 Emilio G. Cota <cota@braap.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/etherdevice.h>
#include <linux/net_tstamp.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/swab.h>
#include <linux/irq.h>
#include <linux/if.h>

#include "wr-mch-fpga.h"
#include "wr-mch.h"

#define DRV_NAME		"wr-mch"
#define WR_NIC_TIMEOUT		HZ	/* 1 second */
#define WR_NIC_NAPI_WEIGHT	10
#define ETHERMTU		1500
#define ETHERCRC		4
#define WR_NIC_BUFSIZE		((ETHERMTU) + 14 + (ETHERCRC) + 2)
#define WR_NIC_ZLEN		60
/* note: these should be calculated from the offsets in the header file */
#define WR_NIC_TX_MAX		5
#define WR_NIC_RX_MAX		10
#define WR_NIC_RX_DESC_SIZE	128
/*
 * These masks count in the 1-bit wrap-around area
 */
#define WR_NIC_TX_MASK		0xfff
#define WR_NIC_RX_MASK		0x1fff

#define TX_BUFFS_AVAIL(nic)	((nic)->tx_count)

struct wrnic {
	void __iomem		*regs;
	void __iomem		*cs0; /* FPGA Chip Select 0 */
	spinlock_t		lock;
	unsigned int		tx_head;
	unsigned int		tx_count;
	struct platform_device	*pdev;
	struct device		*dev;
	struct net_device	*netdev;
	struct net_device_stats	stats;
	struct napi_struct	napi;
	u32			msg_enable;
};

MODULE_DESCRIPTION("White Rabbit MCH NIC driver");
MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wr-mch");

static int debug = NETIF_MSG_DRV | NETIF_MSG_PROBE;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static u32 mac;
module_param(mac, int, 0);
MODULE_PARM_DESC(mac, "Mac Address (u32)");

#define wr_readl(wrnic,offs)			\
	__raw_readl((wrnic)->regs + (offs))

#define wr_writel(wrnic,offs,value)			\
	__raw_writel((value), (wrnic)->regs + (offs))

#define wr_cs0_readl(wrnic,offs)		\
	__raw_readl((wrnic)->cs0 + (offs))

#define wr_cs0_writel(wrnic,offs,value)			\
	__raw_writel((value), (wrnic)->cs0 + (offs))

/*
 * NUXI swapping functions: UNIX -> NUXI
 */
static void
__wr_readsl(struct wrnic *nic, unsigned offset, void *dst, unsigned count)
{
	if (unlikely((unsigned long)dst & 0x3)) {
		while (count--) {
			struct S { int x __packed; };

			((struct S *)dst)->x = swahb32(wr_readl(nic, offset));
                        dst += 4;
			offset += 4;
                }
	} else {
		while (count--) {
			*(u32 *)dst = swahb32(wr_readl(nic, offset));
			dst += 4;
			offset += 4;
		}
	}
}

static void
__wr_writesl(struct wrnic *nic, unsigned offset, void *src, unsigned count)
{
	if (unlikely((unsigned long)src & 0x3)) {
		while (count--) {
			struct S { int x __packed; };

			wr_writel(nic, offset, swahb32(((struct S *)src)->x));
                        src += 4;
			offset += 4;
                }
	} else {
		while (count--) {
			wr_writel(nic, offset, swahb32(*(u32 *)src));
			src += 4;
			offset += 4;
		}
	}
}

static void dump_packet(char *data, unsigned len)
{
	int i = 0;

	while (len--) {
		printk("%02x ", (u8)data[i]);
		if (((++i) % 16) == 0)
			printk("\n");
	}
	if (i % 16)
		printk("\n");
}

static void wr_disable_irq(struct wrnic *nic, u32 mask)
{
	u32 imask = wr_readl(nic, WR_NIC_IER);
	printk(KERN_ERR "wr_disable_irq() - mask %x IER %x\n", mask, imask);

	imask &= ~mask;
	wr_writel(nic, WR_NIC_IER, imask);
	dev_info(nic->dev, "%s: IER=%08x\n", __func__, wr_readl(nic, WR_NIC_IER));
}

static void wr_enable_irq(struct wrnic *nic, u32 mask)
{
	u32 imask = wr_readl(nic, WR_NIC_IER);

	imask |= mask;
	printk(KERN_ERR "wr_enable_irq() - mask %x IER %x\n", mask, imask);

	wr_writel(nic, WR_NIC_IER, imask);
	dev_info(nic->dev, "%s: IER=%08x\n", __func__, wr_readl(nic, WR_NIC_IER));
}

static void wr_clear_irq(struct wrnic *nic, u32 mask)
{
	wr_writel(nic, WR_NIC_ISR, mask);
}

static int wr_update_tx_stats(struct wrnic *nic)
{
	u32 statsreg = wr_readl(nic, WR_NIC_STAT);
	int ret = 0;

	/* @todo: interpret i/f error codes */
	if (statsreg & WR_NIC_STAT_TXER) {
		dev_err(nic->dev, "TX: error detected\n");
		ret++;
	}
	/* clear the errors */
	wr_writel(nic, WR_NIC_STAT, WR_NIC_STAT_TXER);
	nic->stats.tx_errors += ret;
	return ret;
}

static int wr_update_rx_stats(struct wrnic *nic)
{
	u32 statsreg = wr_readl(nic, WR_NIC_STAT);
	int ret = 0;

	/* @todo: interpret i/f error codes */
	if (statsreg & WR_NIC_STAT_RXER) {
		dev_err(nic->dev, "RX: error detected\n");
		ret++;
	}
	if (statsreg & WR_NIC_STAT_RXOV) {
		dev_err(nic->dev, "RX: overflow detected\n");
		nic->stats.rx_over_errors++;
		ret++;
	}
	/* clear them all */
	wr_writel(nic, WR_NIC_STAT, WR_NIC_STAT_RXOV | WR_NIC_STAT_RXER);
	nic->stats.rx_errors += ret;
	return ret;
}

static struct net_device_stats *wr_get_stats(struct net_device *netdev)
{
	struct wrnic *nic = netdev_priv(netdev);

	wr_update_tx_stats(nic);
	wr_update_rx_stats(nic);
	return &nic->stats;
}

/*
 * There are three fields in an RX WR header:
 * ||	size	|	flags	|	OOB	|	tstamp	||
 * ||	32b	|	32b	|	32b	|	32b	||
 */
static int wr_rx_frame(struct wrnic *nic)
{
	struct net_device	*netdev = nic->netdev;
	struct sk_buff		*skb;
	unsigned int	start32	= wr_readl(nic, WR_NIC_RX_DESC_START);
	unsigned int	start8	= start32 << 2;
	/* the NIC returns the size with the MSB first */
	ssize_t		size8	= wr_readl(nic, WR_NIC_RX_OFFSET + start8) >> 16;
	unsigned int	size32	= (size8 + 3) >> 2;
	unsigned int	newhead;
	char tempbuf[WR_NIC_BUFSIZE];

	dev_info(nic->dev, "%s: frame size: %d bytes\n", __func__, size8);
	skb = netdev_alloc_skb(netdev, (size32 << 2) + NET_IP_ALIGN);
	if (unlikely(skb == NULL)) {
		if (net_ratelimit())
			dev_warn(nic->dev, "-ENOMEM - packet dropped\n");
		goto drop;
	}
	/* Make the IP header word-aligned (the ethernet header is 14 bytes) */
	skb_reserve(skb, NET_IP_ALIGN);

	/* read size32 double words starting just after the WR header */
	__wr_readsl(nic, WR_NIC_RX_OFFSET + start8 + 0x10,
		skb_put(skb, size32 << 2), size32);
	/* fixme: grab the data from the skb and avoid reading it again */
	__wr_readsl(nic, WR_NIC_RX_OFFSET + start8 + 0x10, tempbuf, size32);
	dump_packet(tempbuf, size8);

	/* determine protocol id */
	skb->protocol = eth_type_trans(skb, netdev);

	/* @fixme ignore the checksum for the time being */
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	netdev->last_rx = jiffies;
	nic->stats.rx_packets++;
	nic->stats.rx_bytes += size32 << 2;
	netif_receive_skb(skb);

	/* tell the hardware we've processed the buffer */
	wmb();
	newhead = (start32 + 4 + size32) & WR_NIC_RX_MASK;
	wr_writel(nic, WR_NIC_RX_DESC_START, newhead);
	return 0;
drop:
	nic->stats.rx_dropped++;
	return 1;
}

static bool wr_rx_pending(struct wrnic *nic)
{
	unsigned rxhead, rxtail;

	/*
	 * NOTE: this is not atomic -> anyway if we miss a packet in between,
	 * afterwards we'll poll again and fetch it.
	 */
	rxhead = wr_readl(nic, WR_NIC_RX_DESC_START) & WR_NIC_RX_MASK;
	rxtail = wr_readl(nic, WR_NIC_RX_DESC_END) & WR_NIC_RX_MASK;
	dev_info(nic->dev, "%s: head: 0x%x, tail: 0x%x\n", __func__,
		rxhead, rxtail);
	if (rxhead == rxtail ||	rxhead == ((rxtail + 1) & WR_NIC_RX_MASK))
		return false;
	return true;
}

static int wr_rx(struct wrnic *nic, int budget)
{
	int work_done = 0;

	dev_info(nic->dev, "%s\n", __func__);
	dev_info(nic->dev, "budget: %d, rx_pending: %d\n", budget,
		wr_rx_pending(nic));
	while (budget > 0 && wr_rx_pending(nic)) {
		if (!wr_rx_frame(nic))
			budget--;
		work_done++;
	}
	return work_done;
}

static int wr_poll(struct napi_struct *napi, int budget)
{
	struct wrnic *nic = container_of(napi, struct wrnic, napi);
	unsigned int work_done = 0;

	dev_info(nic->dev, "%s\n", __func__);
	work_done = wr_rx(nic, budget);

	/* if budget not fully consumed, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		wr_enable_irq(nic, WR_NIC_IER_RXI);
	}

	return work_done;
}

static inline void wr_tx_ack(struct wrnic *nic)
{
	struct net_device *netdev = nic->netdev;
	unsigned long flags;

	spin_lock_irqsave(&nic->lock, flags);

	nic->tx_count++;
	mb();
	dev_info(nic->dev, "TX: ack interrupt received from the NIC\n");
	if (netif_queue_stopped(nic->netdev) && TX_BUFFS_AVAIL(nic) > 0)
		netif_wake_queue(netdev);

	spin_unlock_irqrestore(&nic->lock, flags);
}

static unsigned int isr_counter;
static irqreturn_t wr_interrupt(int irq, void *dev_id)
{
	struct net_device *netdev = dev_id;
	struct wrnic *nic = netdev_priv(netdev);
	u32 isr;

	isr = wr_readl(nic, WR_NIC_ISR);
	if (unlikely(!isr)) {
		++isr_counter;
//		if (net_ratelimit())
//			dev_warn(nic->dev, "spurious ISR -- count=%08d", isr_counter);
		if (isr_counter % 100 == 0)
			printk(KERN_ERR "spurious ISR -- count=%d\n", isr_counter);
		return IRQ_NONE;
	}

	if (net_ratelimit())
		dev_info(nic->dev, "Interrupt %08x received\n", isr);
	if (isr & WR_NIC_ISR_TXI)
		wr_tx_ack(nic);

	if (isr & WR_NIC_ISR_RXI) {
		if (napi_schedule_prep(&nic->napi)) {
			/* disable the RXI and start polling */
			wr_disable_irq(nic, WR_NIC_IER_RXI);
			__napi_schedule(&nic->napi);
		}
	}

	if (isr & WR_NIC_ISR_TXERR)
		netdev->stats.tx_errors++;

	if (isr & WR_NIC_ISR_RXERR)
		netdev->stats.rx_errors++;

	/* the interrupt status register is clear-on-write for each bit */
	wr_clear_irq(nic, WR_NIC_ISR_MASK);
	dev_info(nic->dev, "%s: IER=%08x\n", __func__, wr_readl(nic, WR_NIC_IER));

	return IRQ_HANDLED;
}

static void __devinit wr_get_mac_addr(struct wrnic *nic)
{
	u8 addr[6];
	int i;

	if (mac) {
		addr[0] = 0xff;
		addr[1] = 0x00;
		for (i = 0; i < 4; i ++)
			addr[i + 2] = (mac >> i) & 0xff;
	}

	if (!mac || !is_valid_ether_addr(addr))
		random_ether_addr(addr);
	memcpy(nic->netdev->dev_addr, addr, sizeof(addr));
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void wr_netpoll(struct net_device *netdev)
{
	unsigned long flags;

	/* @todo: revise locking here */
	local_irq_save(flags);
	wr_interrupt(netdev->irq, netdev);
	local_irq_restore(flags);
}
#endif

static void wr_hw_enable_interrupts(struct wrnic *nic)
{
	printk(KERN_INFO "%s\n", __func__);
	wr_writel(nic, WR_NIC_IER, WR_NIC_IER_TXI | WR_NIC_IER_RXI);
}

static void wr_hw_disable_interrupts(struct wrnic *nic)
{
	wr_writel(nic, WR_NIC_IER, 0);

}

static void wr_enable_rxtx(struct wrnic *nic)
{
	wr_writel(nic, WR_NIC_CTL, WR_NIC_CTL_TX_EN | WR_NIC_CTL_RX_EN);
}

static void wr_disable_rxtx(struct wrnic *nic)
{
	wr_writel(nic, WR_NIC_CTL, 0);
}

static void wr_hw_reset(struct wrnic *nic)
{
	wr_writel(nic, WR_NIC_RST, 1);
	udelay(5);
}

static void wr_hw_quiesce(struct wrnic *nic)
{
	/* disable TX, RX */
	wr_writel(nic, WR_NIC_CTL, 0);

	/* clear status register */
	wr_writel(nic, WR_NIC_STAT, 0);

	/* disable interrupts */
	wr_writel(nic, WR_NIC_IER, 0);
}

static void wr_hw_init(struct wrnic *nic)
{
	wr_hw_reset(nic);
	wr_hw_quiesce(nic);
}

/*
 * There are three fields in a TX WR header:
 * ||	size	|	flags	|	OOB	||
 * ||	32b	|	32b	|	32b	||
 *
 * NOTE: the CRC for the packet is appended in software--this will
 * be done in hardware soon.
 */
static void __wr_hw_tx(struct wrnic *nic, char *data, unsigned size)
{
	/* len is in double words (32bits) and doesn't include the CRC */
	unsigned int len = (size >> 2) + !!(size & 0x3);
	unsigned int txstart, txend;
	unsigned int crc;
	unsigned int h8; /* head in bytes */
	unsigned char my_txbuf[WR_NIC_BUFSIZE];

	/*
	 * @fixme: this copy is superfluous, but this way it's really easy
	 * to append the CRC
	 */
	memcpy(my_txbuf, data, size);
	crc = ~ether_crc_le(size, my_txbuf);

	my_txbuf[size] = crc & 0xff;
	my_txbuf[size+1] = (crc >> 8) & 0xff;
	my_txbuf[size+2] = (crc >> 16) & 0xff;
	my_txbuf[size+3] = (crc >> 24) & 0xff;

	txstart = wr_readl(nic, WR_NIC_TX_DESC_START) & WR_NIC_TX_MASK;
	txend = wr_readl(nic, WR_NIC_TX_DESC_END) & WR_NIC_TX_MASK;
	if (txend != txstart)
		txend = (txend + 1) & WR_NIC_TX_MASK;
	nic->tx_head = txend;
	h8 = nic->tx_head << 2;

	dev_info(nic->dev, "TX: txstart 0x%x, txend 0x%x len=0x%x, size=0x%x\n",
		txstart, txend,	len, size);
	dump_packet(data, size);
	dev_info(nic->dev, "CRC: %08x\n", crc);

	/* fill in WR headers: flags and OOB are set to 0 for the time being */
	wr_writel(nic, WR_NIC_TX_OFFSET + h8, size + 4);
	wr_writel(nic, WR_NIC_TX_OFFSET + h8 + 4, 0);
	wr_writel(nic, WR_NIC_TX_OFFSET + h8 + 8, 0);

	/* transfer the payload */
	__wr_writesl(nic, WR_NIC_TX_OFFSET + h8 + 0xc, my_txbuf, len + 1);

	/* update TX_END: the NIC will start the transfer straightaway */
	wr_writel(nic, WR_NIC_TX_DESC_END,
		(nic->tx_head + len + 4) & WR_NIC_TX_MASK);
}

static int wr_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct wrnic *nic = netdev_priv(netdev);
	char shortpkt[WR_NIC_ZLEN];
	char *data;
	unsigned len;

	printk(KERN_INFO "wr_start_xmit: len %d\n", skb->len);

	if (unlikely(skb->len > WR_NIC_BUFSIZE)) {
		nic->stats.tx_errors++;
		return -EMSGSIZE;
	}

	data = skb->data;
	len = skb->len;
	if (skb->len < WR_NIC_ZLEN) {
		memset(shortpkt, 0, WR_NIC_ZLEN);
		memcpy(shortpkt, skb->data, skb->len);
		data = shortpkt;
		len = WR_NIC_ZLEN;
	}

	spin_lock_irq(&nic->lock);
	if (TX_BUFFS_AVAIL(nic) <= 0) {
		netif_stop_queue(netdev);
		spin_unlock_irq(&nic->lock);
		dev_err(&netdev->dev, "BUG: Tx Ring full when queue awake\n");
		return 1;
	}
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		if (skb_checksum_help(skb)) {
			dev_warn(nic->dev, "packet checksum failed\n");
			goto out;
		}
	}

	dev_info(nic->dev, "%s: len %d\n", __func__, len);

	__wr_hw_tx(nic, data, len);
	nic->tx_count--;
	nic->stats.tx_packets++;
	nic->stats.tx_bytes += len;

	if (!TX_BUFFS_AVAIL(nic))
		netif_stop_queue(netdev);

	netdev->trans_start = jiffies;
out:
	dev_kfree_skb(skb);
	spin_unlock_irq(&nic->lock);
	return NETDEV_TX_OK;
}

static int wr_tstamp_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	struct hwtstamp_config	config;

	if (copy_from_user(&config, rq->ifr_data, sizeof(config)))
		return -EFAULT;

	/* reserver for future extensions of the interface */
	if (config.flags)
		return -EFAULT;

	if (config.tx_type == HWTSTAMP_TX_OFF &&
		config.rx_filter == HWTSTAMP_FILTER_NONE)
		return 0;

	return -ERANGE;
}

static int wr_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	switch (cmd) {
	case SIOCSHWTSTAMP:
		return wr_tstamp_ioctl(netdev, rq, cmd);
	default:
		return -EOPNOTSUPP;
	}
}

static void wr_init_tx_ring(struct wrnic *nic)
{
	nic->tx_count = 1;
}

static void wr_init_rx_ring(struct wrnic *nic)
{ }

static void print_silly_test(struct wrnic *nic)
{
	u32 reg;

	reg = wr_readl(nic, WR_NIC_CTL);
	dev_info(nic->dev, "Control register: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_IER);
	dev_info(nic->dev, "Interrupt Enable register: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_ISR);
	dev_info(nic->dev, "Interrupt Status register: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_STAT);
	dev_info(nic->dev, "Status register: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_TX_DESC_START);
	dev_info(nic->dev, "TX desc start: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_TX_DESC_END);
	dev_info(nic->dev, "TX desc end: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_RX_DESC_END);
	dev_info(nic->dev, "RX desc end: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_RX_DESC_END);
	dev_info(nic->dev, "RX desc end: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_TX_DESC_END);
	dev_info(nic->dev, "TX desc end: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_TX_OFFSET);
	dev_info(nic->dev, "TX offset: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_RX_OFFSET);
	dev_info(nic->dev, "RX offset: 0x%08x\n", reg);
	reg = wr_readl(nic, WR_NIC_RX_PENDING);
	dev_info(nic->dev, "RX pending: 0x%08x\n", reg);
}

static int wr_open(struct net_device *netdev)
{
	struct wrnic *nic = netdev_priv(netdev);

	printk(KERN_INFO "wr_open\n");

	if (!is_valid_ether_addr(netdev->dev_addr))
		return -EADDRNOTAVAIL;

	wr_hw_init(nic);
	wr_enable_rxtx(nic);
	wr_hw_enable_interrupts(nic);

	wr_init_tx_ring(nic);
	wr_init_rx_ring(nic);

	if (netif_queue_stopped(nic->netdev)) {
		dev_dbg(nic->dev, " resuming queue\n");
		netif_wake_queue(netdev);
	} else {
		dev_dbg(nic->dev, " starting queue\n");
		netif_start_queue(netdev);
	}
	napi_enable(&nic->napi);

	print_silly_test(nic);
	dev_info(nic->dev, "wr_open done\n");

	return 0;
}

static int wr_close(struct net_device *netdev)
{
	struct wrnic *nic = netdev_priv(netdev);

	/* disable device, etc */
	/* beep beep beep */

	dev_info(nic->dev, "%s\n", __func__);

	napi_disable(&nic->napi);
	if (!netif_queue_stopped(netdev))
		netif_stop_queue(netdev);

	wr_hw_disable_interrupts(nic);
	wr_disable_rxtx(nic);

	dev_info(nic->dev, "wr_close() done\n");

	return 0;
}

static const struct ethtool_ops wr_ethtool_ops = {
//	.get_settings		= wr_get_settings,
//	.set_settings		= wr_set_settings,
//	.get_drvinfo		= wr_get_drvinfo,
//	.get_link		= wr_get_link,
};

static const struct net_device_ops wr_netdev_ops = {
	.ndo_open		= wr_open,
	.ndo_stop		= wr_close,
	.ndo_start_xmit		= wr_start_xmit,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_get_stats		= wr_get_stats,
//	.ndo_set_multicast_list	= wr_set_multicast_list,
//	.ndo_set_mac_address	= wr_set_mac_address,
//	.ndo_change_mtu		= wr_change_mtu,
	.ndo_do_ioctl		= wr_ioctl,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= wr_netpoll;
#endif
};

static int __devinit wr_probe(struct platform_device *pdev)
{
	struct net_device	*netdev;
	struct resource		*regs, *cs0;
	struct wrnic		*nic;
	int			err;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "no mmio resource defined\n");
		err = -ENXIO;
		goto err_out;
	}

	cs0 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!cs0) {
		dev_err(&pdev->dev, "no fpga-cs0 resource defined\n");
		err = -ENXIO;
		goto err_out;
	}

	netdev = alloc_etherdev(sizeof(struct wrnic));
	if (!netdev) {
		dev_err(&pdev->dev, "etherdev alloc failed, aborting.\n");
		err = -ENOMEM;
		goto err_out;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	/* initialise the nic structure */
	nic = netdev_priv(netdev);
	nic->pdev = pdev;
	nic->netdev = netdev;
	nic->dev = &pdev->dev;
	nic->msg_enable = (1 << debug) - 1;
	spin_lock_init(&nic->lock);
	nic->regs = ioremap(regs->start, regs->end - regs->start + 1);
	if (!nic->regs) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "failed to map registers\n");
		err = -ENOMEM;
		goto err_out_free_netdev;
	}

	nic->cs0 = ioremap(cs0->start, cs0->end - cs0->start + 1);
	if (!nic->cs0) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "failed to map fpga-cs0\n");
		err = -ENOMEM;
		goto err_out_iounmap_regs;
	}

	netdev->base_addr = regs->start;
	wr_get_mac_addr(nic);

	netdev->irq = platform_get_irq(pdev, 0);
	if (netdev->irq < 0) {
		err = -ENXIO;
		goto err_out_iounmap;
	}
	err = request_irq(netdev->irq, wr_interrupt,
			IRQF_TRIGGER_HIGH | IRQF_SHARED, netdev->name, netdev);
	if (err) {
		if (netif_msg_probe(nic)) {
			dev_err(&netdev->dev, "request IRQ %d failed, err=%d\n",
				netdev->irq, err);
		}
		goto err_out_iounmap;
	}

	netdev->netdev_ops = &wr_netdev_ops;
	SET_ETHTOOL_OPS(netdev, &wr_ethtool_ops);
	netdev->features |= 0;

	/* setup NAPI */
	memset(&nic->napi, 0, sizeof(nic->napi));
	netif_napi_add(netdev, &nic->napi, wr_poll, WR_NIC_NAPI_WEIGHT);

	err = register_netdev(netdev);
	if (err) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "unable to register net device\n");
		goto err_out_freeirq;
	}

	platform_set_drvdata(pdev, netdev);

	/* WR NIC banner */
	if (netif_msg_probe(nic)) {
		dev_info(&pdev->dev, "White Rabbit MCH NIC at 0x%08lx irq %d\n",
			netdev->base_addr, netdev->irq);
	}

	return 0;

err_out_freeirq:
	free_irq(netdev->irq, &pdev->dev);
err_out_iounmap:
	iounmap(nic->cs0);
err_out_iounmap_regs:
	iounmap(nic->regs);
err_out_free_netdev:
	free_netdev(netdev);
err_out:
	platform_set_drvdata(pdev, NULL);
	return err;
}

static int __devexit wr_remove(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct wrnic *nic;

	netdev = platform_get_drvdata(pdev);
	if (!netdev)
		return 0;

	free_irq(netdev->irq, netdev);


	nic = netdev_priv(netdev);
	unregister_netdev(netdev);
	iounmap(nic->regs);
	iounmap(nic->cs0);
	free_netdev(netdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int wr_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *netdev = platform_get_drvdata(pdev);

	netif_device_detach(netdev);
	return 0;
}

static int wr_resume(struct platform_device *pdev)
{
	struct net_device *netdev = platform_get_drvdata(pdev);

	netif_device_attach(netdev);
	return 0;
}
#else
#define wr_suspend	NULL
#define wr_resume	NULL
#endif

static struct platform_driver wr_driver = {
	.remove		= __exit_p(wr_remove),
	.suspend	= wr_suspend,
	.resume		= wr_resume,
	.driver		= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
	},
};

static int __devinit wr_init_module(void)
{
	return platform_driver_probe(&wr_driver, wr_probe);
}

static void __devexit wr_cleanup_module(void)
{
	platform_driver_unregister(&wr_driver);
}

module_init(wr_init_module);
module_exit(wr_cleanup_module);
