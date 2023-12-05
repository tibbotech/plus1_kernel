// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

/* mailbox regs offset */
#define MBOX_TRIGGER	0
#define MBOX_WRITELOCK	4
#define MBOX_OVERWRITE	8
#define MBOX_DATA	16

#define MBOX_DATA_SIZE	20
#define MBOX_DATA_MASK	0x00fffff0	// BIT[23:4]

struct sunplus_mbox {
	void __iomem *tx_regs;
	void __iomem *rx_regs;
	spinlock_t lock;
	struct mbox_controller controller;
};

static struct sunplus_mbox *sunplus_link_mbox(struct mbox_chan *link)
{
	return container_of(link->mbox, struct sunplus_mbox, controller);
}

static irqreturn_t sunplus_mbox_irq(int irq, void *dev_id)
{
	struct sunplus_mbox *mbox = dev_id;
	struct mbox_chan *link = &mbox->controller.chans[0];
	u32 msg[MBOX_DATA_SIZE];
	int i;

	writel(0, mbox->rx_regs + MBOX_TRIGGER); // clear intr
	for (i = 0; i < MBOX_DATA_SIZE; i++)
		msg[i] = readl(mbox->rx_regs + MBOX_DATA + i * 4);
	mbox_chan_received_data(link, msg);

	return IRQ_HANDLED;
}

static int sunplus_send_data(struct mbox_chan *link, void *data)
{
	struct sunplus_mbox *mbox = sunplus_link_mbox(link);
	u32 *msg = (u32 *)data;
	int i;

	spin_lock(&mbox->lock);
	for (i = 0; i < MBOX_DATA_SIZE; i++)
		writel(msg[i], mbox->tx_regs + MBOX_DATA + i * 4);
	writel(1, mbox->tx_regs + MBOX_TRIGGER);
	spin_unlock(&mbox->lock);
	return 0;
}

static bool sunplus_last_tx_done(struct mbox_chan *link)
{
	struct sunplus_mbox *mbox = sunplus_link_mbox(link);
	bool ret;

	spin_lock(&mbox->lock);
	ret = !(readl(mbox->tx_regs + MBOX_WRITELOCK) & MBOX_DATA_MASK);
	spin_unlock(&mbox->lock);
	return ret;
}

static const struct mbox_chan_ops sunplus_mbox_chan_ops = {
	.send_data	= sunplus_send_data,
	.last_tx_done	= sunplus_last_tx_done
};

static int sunplus_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct resource *iomem;
	struct sunplus_mbox *mbox;

	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (mbox == NULL)
		return -ENOMEM;
	spin_lock_init(&mbox->lock);

	ret = devm_request_irq(dev, irq_of_parse_and_map(dev->of_node, 0),
			       sunplus_mbox_irq, 0, dev_name(dev), mbox);
	if (ret) {
		dev_err(dev, "Failed to register a mailbox IRQ handler: %d\n",
			ret);
		return -ENODEV;
	}

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mbox->tx_regs = devm_ioremap_resource(&pdev->dev, iomem);
	if (IS_ERR(mbox->tx_regs)) {
		ret = PTR_ERR(mbox->tx_regs);
		dev_err(&pdev->dev, "Failed to remap mailbox tx_regs: %d\n", ret);
		return ret;
	}

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	mbox->rx_regs = devm_ioremap_resource(&pdev->dev, iomem);
	if (IS_ERR(mbox->rx_regs)) {
		ret = PTR_ERR(mbox->rx_regs);
		dev_err(&pdev->dev, "Failed to remap mailbox rx_regs: %d\n", ret);
		return ret;
	}

	mbox->controller.txdone_poll = true;
	mbox->controller.txpoll_period = 5;
	mbox->controller.ops = &sunplus_mbox_chan_ops;
	mbox->controller.dev = dev;
	mbox->controller.num_chans = 1;
	mbox->controller.chans = devm_kzalloc(dev,
		sizeof(*mbox->controller.chans), GFP_KERNEL);
	if (!mbox->controller.chans)
		return -ENOMEM;

	ret = devm_mbox_controller_register(dev, &mbox->controller);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, mbox);
	dev_info(dev, "mailbox enabled\n");

	return ret;
}

static const struct of_device_id sunplus_mbox_of_match[] = {
	{ .compatible = "sunplus,sunplus-mbox", },
	{},
};
MODULE_DEVICE_TABLE(of, sunplus_mbox_of_match);

static struct platform_driver sunplus_mbox_driver = {
	.driver = {
		.name = "sunplus-mbox",
		.of_match_table = sunplus_mbox_of_match,
	},
	.probe		= sunplus_mbox_probe,
};
module_platform_driver(sunplus_mbox_driver);

MODULE_DESCRIPTION("Sunplus mailbox IPC driver");
MODULE_LICENSE("GPL v2");
