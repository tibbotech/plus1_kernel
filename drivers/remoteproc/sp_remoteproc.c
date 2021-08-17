// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus Remote Processor driver
 *
 * Copyright (C) 2021 Sunplus
 *
 * Based on origin Zynq Remote Processor driver
 *
 * Copyright (C) 2012 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2012 PetaLogix
 *
 * Based on origin OMAP Remote Processor driver
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/delay.h>

#include "remoteproc_internal.h"

#define MAX_NUM_VRINGS 2
#define NOTIFYID_ANY (-1)
/* Maximum on chip memories used by the driver*/
#define MAX_ON_CHIP_MEMS        32

/* Structure for IPIs */
struct ipi_info {
	u32 notifyid;
	bool pending;
};

/**
 * struct sp_rproc_data - sunplus rproc private data
 * @rproc: pointer to remoteproc instance
 * @ipis: interrupt processor interrupts statistics
 * @fw_mems: list of firmware memories
 * @mbox: mbox register to trigger remote
 * @boot: boot register to boot remote
 */
struct sp_rproc_pdata {
	struct rproc *rproc;
	struct ipi_info ipis[MAX_NUM_VRINGS];
	struct list_head fw_mems;
	volatile u32 __iomem *mbox;
#ifdef CONFIG_ARCH_PENTAGRAM
	volatile u32 __iomem *boot;
#endif
	struct reset_control *rstc;
};

static bool autoboot __read_mostly;

/* Store rproc for IPI handler */
static struct rproc *rproc;
static struct work_struct workqueue;

static void handle_event(struct work_struct *work)
{
	struct sp_rproc_pdata *local = rproc->priv;

	if (rproc_vq_interrupt(local->rproc, local->ipis[0].notifyid) ==
				IRQ_NONE)
		dev_dbg(rproc->dev.parent, "no message found in vqid 0\n");
}

static void ipi_kick(void)
{
	dev_dbg(rproc->dev.parent, "KICK Linux because of pending message\n");
	schedule_work(&workqueue);
}

#define RPROC_TRIGGER \
do { \
	writel(local->ipis[i].notifyid, local->mbox); \
} while (0)

static void kick_pending_ipi(struct rproc *rproc)
{
	struct sp_rproc_pdata *local = rproc->priv;
	int i;

	for (i = 0; i < MAX_NUM_VRINGS; i++) {
		/* Send swirq to firmware */
		if (local->ipis[i].pending) {
			RPROC_TRIGGER;
			local->ipis[i].pending = false;
		}
	}
}

static int sp_rproc_start(struct rproc *rproc)
{
	struct device *dev = rproc->dev.parent;
	struct sp_rproc_pdata *local = rproc->priv;

	dev_dbg(dev, "%s\n", __func__);
	INIT_WORK(&workqueue, handle_event);

	/* Trigger pending kicks */
	kick_pending_ipi(rproc);

	reset_control_assert(local->rstc);
	reset_control_deassert(local->rstc);
#ifdef CONFIG_ARCH_PENTAGRAM
	writel(0xffffffff, local->boot); // CPU_WAIT_INIT_VAL
	msleep(1000); // wait B_cpu_wait
	// set remote start addr to boot register,
	writel(rproc->bootaddr, local->boot);
#endif

	return 0;
}

/* kick a firmware */
static void sp_rproc_kick(struct rproc *rproc, int vqid)
{
	struct device *dev = rproc->dev.parent;
	struct sp_rproc_pdata *local = rproc->priv;
	struct rproc_vdev *rvdev, *rvtmp;
	int i;

	dev_dbg(dev, "KICK Firmware to start send messages vqid %d\n", vqid);

	list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs, node) {
		for (i = 0; i < MAX_NUM_VRINGS; i++) {
			struct rproc_vring *rvring = &rvdev->vring[i];

			/* Send swirq to firmware */
			if (rvring->notifyid == vqid) {
				local->ipis[i].notifyid = vqid;
				/* As we do not turn off CPU1 until start,
				 * we delay firmware kick
				 */
				if (rproc->state == RPROC_RUNNING)
					RPROC_TRIGGER;
				else
					local->ipis[i].pending = true;
			}
		}
	}
}

/* power off the remote processor */
static int sp_rproc_stop(struct rproc *rproc)
{
	struct sp_rproc_pdata *local = rproc->priv;
	struct device *dev = rproc->dev.parent;

	dev_dbg(dev, "%s\n", __func__);
	reset_control_assert(local->rstc);

	return 0;
}

static int sp_parse_fw(struct rproc *rproc, const struct firmware *fw)
{
	int num_mems, i, ret;
	struct device *dev = rproc->dev.parent;
	struct device_node *np = dev->of_node;
	struct rproc_mem_entry *mem;

	num_mems = of_count_phandle_with_args(np, "memory-region", NULL);
	if (num_mems <= 0)
		return 0;
	for (i = 0; i < num_mems; i++) {
		struct device_node *node;
		struct reserved_mem *rmem;

		node = of_parse_phandle(np, "memory-region", i);
		rmem = of_reserved_mem_lookup(node);
		if (!rmem) {
			dev_err(dev, "unable to acquire memory-region\n");
			return -EINVAL;
		}
		if (strstr(node->name, "vdev") &&
			strstr(node->name, "buffer")) {
			/* Register DMA region */
			mem = rproc_mem_entry_init(dev, NULL,
						   (dma_addr_t)rmem->base,
						   rmem->size, rmem->base,
						   NULL, NULL,
						   node->name);
			if (!mem) {
				dev_err(dev,
					"unable to initialize memory-region %s \n",
					node->name);
				return -ENOMEM;
			}
			rproc_add_carveout(rproc, mem);
		} else if (strstr(node->name, "vdev") &&
			   strstr(node->name, "vring")) {
			/* Register vring */
			mem = rproc_mem_entry_init(dev, NULL,
						   (dma_addr_t)rmem->base,
						   rmem->size, rmem->base,
						   NULL, NULL,
						   node->name);
			mem->va = devm_ioremap_wc(dev, rmem->base, rmem->size);
			if (!mem->va)
				return -ENOMEM;
			if (!mem) {
				dev_err(dev,
					"unable to initialize memory-region %s\n",
					node->name);
				return -ENOMEM;
			}
			rproc_add_carveout(rproc, mem);
		} else {
			mem = rproc_of_resm_mem_entry_init(dev, i,
							rmem->size,
							rmem->base,
							node->name);
			if (!mem) {
				dev_err(dev,
					"unable to initialize memory-region %s \n",
					node->name);
				return -ENOMEM;
			}
			mem->va = devm_ioremap_wc(dev, rmem->base, rmem->size);
			if (!mem->va)
				return -ENOMEM;

			rproc_add_carveout(rproc, mem);
		}
	}

	ret = rproc_elf_load_rsc_table(rproc, fw);
	if (ret == -EINVAL)
		ret = 0;
	return ret;
}

static struct rproc_ops sp_rproc_ops = {
	.start		= sp_rproc_start,
	.stop		= sp_rproc_stop,
	.load		= rproc_elf_load_segments,
	.parse_fw	= sp_parse_fw,
	.find_loaded_rsc_table = rproc_elf_find_loaded_rsc_table,
	.get_boot_addr	= rproc_elf_get_boot_addr,
	.kick		= sp_rproc_kick,
};

static irqreturn_t sp_remoteproc_interrupt(int irq, void *dev_id)
{
	ipi_kick();

	return IRQ_HANDLED;
}

static int sp_remoteproc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct sp_rproc_pdata *local;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "missing mbox irq\n");
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, ret, sp_remoteproc_interrupt,
							IRQF_TRIGGER_NONE, dev_name(&pdev->dev), NULL);
	if (ret) {
		dev_err(&pdev->dev, "request rproc irq failed: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(&pdev->dev, dev_name(&pdev->dev),
			    &sp_rproc_ops, NULL,
		sizeof(struct sp_rproc_pdata));
	if (!rproc) {
		dev_err(&pdev->dev, "rproc allocation failed\n");
		return -ENOMEM;
	}
	local = rproc->priv;
	local->rproc = rproc;

	platform_set_drvdata(pdev, rproc);

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_coherent_mask: %d\n", ret);
		goto probe_failed;
	}

	local->mbox = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR((void *)local->mbox)) {
		ret = PTR_ERR((void *)local->mbox);
		dev_err(&pdev->dev, "ioremap mbox reg failed: %d\n", ret);
		goto probe_failed;
	}

#ifdef CONFIG_ARCH_PENTAGRAM
	local->boot = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR((void *)local->boot)) {
		ret = PTR_ERR((void *)local->boot);
		dev_err(&pdev->dev, "ioremap boot reg failed: %d\n", ret);
		goto probe_failed;
	}
#endif
	local->rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(local->rstc)) {
		ret = PTR_ERR(local->rstc);
		dev_err(&pdev->dev, "missing reset controller\n");
		goto probe_failed;
	}

	rproc->auto_boot = autoboot;

	ret = rproc_add(local->rproc);
	if (ret) {
		dev_err(&pdev->dev, "rproc registration failed: %d\n", ret);
		goto probe_failed;
	}

	return 0;

probe_failed:
	rproc_free(rproc);

	return ret;
}

static int sp_remoteproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);

	rproc_del(rproc);
	of_reserved_mem_device_release(&pdev->dev);
	rproc_free(rproc);

	return 0;
}

/* Match table for OF platform binding */
static const struct of_device_id sp_remoteproc_match[] = {
	{ .compatible = "sunplus,sp_remoteproc", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, sp_remoteproc_match);

static struct platform_driver sp_remoteproc_driver = {
	.probe = sp_remoteproc_probe,
	.remove = sp_remoteproc_remove,
	.driver = {
		.name = "sp_remoteproc",
		.of_match_table = sp_remoteproc_match,
	},
};
module_platform_driver(sp_remoteproc_driver);

module_param_named(autoboot, autoboot, bool, 0444);
MODULE_PARM_DESC(autoboot,
		 "enable | disable autoboot. (default: false)");

MODULE_AUTHOR("qinjian@cqplus1.com");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sunplus remote processor control driver");
