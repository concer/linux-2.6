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
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/dmapool.h>
#include <linux/device.h>
#include <linux/module.h>

#define DRV_NAME		"wr-mch"
#define WR_NIC_WATCHDOG_PERIOD	(2 * HZ) /* @todo arbitrary right now */
#define WR_NIC_NAPI_WEIGHT	16

#define wr_readl(wrnic,reg)				\
	__raw_readl((wrnic)->regs + WR_NIC_##reg)

#define wr_writel(wrnic,reg,value)			\
	__raw_writel((value), (wrnic)->regs + WR_NIC_##reg)

struct rx_desc {
	u32	start;
	u32	end;
};

struct tx_desc {
	u32	start;
	u32	end;
};

struct wrnic {
	u32		msg_enable;
	void __iomem	*regs;
	spinlock_t		lock;
	struct platform_device	*pdev;
	struct net_device	*netdev;
	struct net_device_stats	stats;
	struct napi_struct	napi;
	/* @todo fill this in */
	struct dma_pool		*rx_pool;
	struct rx_desc		*rx;
	struct dma_pool		*tx_pool;
	struct tx_desc		*tx;
};

struct wr_buff {
	struct sk_buff	*skb;
	dma_addr_t	dma;
	unsigned long	timestamp;
	u16		length;
	u16		next_to_watch;
};

struct wr_tx_ring {
	void		*desc;
	dma_addr_t	dma;
	unsigned int	size;
	unsigned int	count;
	unsigned int	next_to_use;
	unsigned int	next_to_clean;
	struct wr_buff	*buffer_info;
	u16		tdh;
	u16		tdt;
	bool last_tx_tso;
};


MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DESCRIPTION("White Rabbit MCH NIC driver");
MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");

static int wr_debug __initdata = NETIF_MSG_DRV | NETIF_MSG_PROBE;
module_param(wr_debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static u32 wr_mac __initdata;
module_param(wr_mac, int, 0);
MODULE_PARM_DESC(mac, "Mac Address (u32)");


static irqreturn_t wr_intr(int irq, void *dev_id)
{
	return IRQ_NONE;
}

static int wr_get_settings(struct net_device *netdev, struct ethtool_cmd *cmd)
{
	struct wrnic *nic = netdev_priv(dev);
}

static void __devinit wr_get_mac_addr(struct wrnic *nic)
{
	u8	addr[6];

	if (wr_mac) {
		addr[0] = 0xff;
		addr[1] = 0x00;
		for (i = 0; i < 4; i ++)
			addr[i + 2] = (wr_mac >> i) & 0xff;
	}

	if (!wr_mac || !is_valid_ether_addr(addr))
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

static void wr_hw_enable_intr(struct wrnic *nic)
{
	wr_writel(nic, IER, WR_NIC_IER_TXI | WR_NIC_IER_RXI);
}

static void wr_hw_enable_rxtx(struct wrnic *nic)
{
	wr_writel(nic, CTL, WR_NIC_CTL_TX_EN | WR_NIC_CTL_RX_EN);
}

static void wr_reset_hw(struct wrnic *nic)
{
	wr_writel(nic, RST, 1);
}

static void wr_hw_quiesce(struct wrnic *nic);
{
	/* disable TX, RX */
	wr_writel(nic, CTL, 0);

	/* clear status register */
	wr_writel(nic, STAT, 0);

	/* disable interrupts */
	wr_writel(nic, IER, 0);
}

static void wr_hw_init(struct wrnic *nic)
{
	wr_hw_reset(nic);
	wr_hw_quiesce(nic);
}

static int wr_open(struct net_device *netdev)
{
	struct wrnic *nic = netdev_priv(netdev);

	if (!is_valid_ether_addr(netdev->dev_addr))
		return -EADDRNOTAVAIL;

	wr_hw_init(nic);
	wr_hw_enable_rxtx(nic);

	netif_wake_queue(nic->netdev);
	napi_enable(&nic->napi);

	wr_hw_enable_intr(nic);
	return 0;
}

static const struct ethtool_ops wr_ethtool_ops = {
	.get_settings		= wr_get_settings,
	.set_settings		= wr_set_settings,
	.get_drvinfo		= wr_get_drvinfo,
	.get_link		= wr_get_link,
};

static int __devinit wr_probe(struct platform_device *pdev)
{
	struct net_device	*netdev;
	struct resource		*regs;
	int			err = -ENXIO;
	struct wrnic		*nic;
	DECLARE_MAC_BUF(mac);

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "no mmio resource defined\n");
		goto err_out;
	}

	err = -ENOMEM;
	netdev = alloc_etherdev(sizeof(struct wrnic));
	if (!netdev) {
		dev_err(&pdev->dev, "etherdev alloc failed, aborting.\n");
		goto err_out;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	/* @todo: shall I fill this in? */
	netdev->features |= 0;

	/* initialise the nic structure */
	nic = netdev_priv(dev);
	nic->pdev = pdev;
	nic->netdev = netdev;
	nic->msg_enable = (1 << wr_debug) - 1;
	spin_lock_init(&nic->lock);
	nic->regs = ioremap(regs->start, regs->end - regs->start + 1);
	if (!nic->regs) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "failed to map registers\n");
		err = -ENOMEM;
		goto err_out_free_netdev;
	}

	/* register the isr */
	netdev->irq = platform_get_irq(pdev, 0);
	err = request_irq(netdev->irq, wr_intr,
			IRQF_SHARED | IRQF_SAMPLE_RANDOM, netdev->name, netdev);
	if (err) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "unable to request IRQ %d, err%d\n",
			netdev->irq, err);
		goto err_out_iounmap;
	}

	/* fill in the net_device methods */
	netdev->open			= wr_open;
	netdev->stop			= wr_close;
	netdev->hard_start_xmit		= wr_start_xmit;
	netdev->get_stats		= wr_get_stats;
	netdev->set_multicast_list	= wr_set_multicast_list;
	netdev->do_ioctl		= wr_do_ioctl;
	/* i may not need the following for the demo */
	netdev->tx_timeout		= wr_tx_timeout;
	netdev->watchdog_timeo		= WR_NIC_WATCHDOG_PERIOD;
	netdev->set_mac_address		= wr_set_mac_address;
#ifdef CONFIG_NET_POLL_CONTROLLER
	netdev->poll_controller		= wr_netpoll;
#endif
	SET_ETHTOOL_OPS(netdev, &wr_ethtool_ops);

	netdev->base_addr = regs->start;
	wr_get_mac_addr(nic);

	err = register_netdev(netdev);
	if (err) {
		if (netif_msg_probe(nic))
			dev_err(&pdev->dev, "unable to register net device\n");
		goto err_out_freeirq;
	}

	/* make ethtool happy */
	netif_carrier_off(netdev);

	platform_set_drvdata(pdev, netdev);

	if (netif_msg_probe(nic)) {
		dev_info(&pdev->dev, "White Rabbit MCH NIC at 0x%08lx irq %d\n",
			netdev->base_addr, netdev->irq);
	}

	return 0;

err_out_freeirq:
	free_irq(dev->irq, dev);
err_out_iounmap:
	iounmap(nic->regs);
err_out_free_netdev:
	free_netdev(dev);
err_out:
	platform_set_drvdata(pdev, NULL);
	return err;
}

static int __devexit wr_remove(struct platform device *pdev)
{
	struct net_device *netdev;
	struct wrnic *nic;

	netdev = platform_get_drvdata(pdev);
	if (!dev)
		return 0;

	nic = netdev_priv(netdev);
	unregister_netdev(netdev);
	free_irq(netdev->irq, netdev);
	iounmap(nic->regs);
	free_netdev(netdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

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
