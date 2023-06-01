// SPDX-License-Identifier: GPL-2.0-or-later

/**************************************************************************************************/
/* Generic platform ehci driver									  */
/*												  */
/* Copyright 2007 Steven Brown <sbrown@cortland.com>						  */
/* Copyright 2010-2012 Hauke Mehrtens <hauke@hauke-m.de>					  */
/*												  */
/* Derived from the ohci-ssb driver								  */
/* Copyright 2007 Michael Buesch <m@bues.ch>							  */
/*												  */
/* Derived from the EHCI-PCI driver								  */
/* Copyright (c) 2000-2004 by David Brownell							  */
/*												  */
/* Derived from the ohci-pci driver								  */
/* Copyright 1999 Roman Weissgaerber								  */
/* Copyright 2000-2002 David Brownell								  */
/* Copyright 1999 Linus Torvalds								  */
/* Copyright 1999 Gregory P. Smith								  */
/*												  */
/* Licensed under the GNU/GPL. See COPYING for details						  */
/**************************************************************************************************/
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/of.h>

#include "ehci.h"

#include <linux/usb/ehci_pdriver.h>
#include <linux/kthread.h>
#include <linux/usb/sp_usb.h>

#if defined (CONFIG_USB_SUNPLUS_OTG) || defined (CONFIG_USB_SUNPLUS_SP7350_OTG)
#include "../phy/otg-sunplus.h"
#endif

struct clk *ehci_clk[USB_PORT_NUM];

#if defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
static struct phy *uphy[USB_PORT_NUM];
#endif

static int ehci_platform_reset(struct usb_hcd *hcd)
{
	struct platform_device *pdev = to_platform_device(hcd->self.controller);
	struct usb_ehci_pdata *pdata = pdev->dev.platform_data;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int port_num = pdev->id - 1;
	int retval;

	hcd->has_tt = pdata->has_tt;
	ehci->has_synopsys_hc_bug = pdata->has_synopsys_hc_bug;
	ehci->big_endian_desc = pdata->big_endian_desc;
	ehci->big_endian_mmio = pdata->big_endian_mmio;

	ehci->caps = hcd->regs + pdata->caps_offset;
	retval = ehci_setup(hcd);
	if (retval)
		return retval;

	if (pdata->power_on)
		ehci_port_power(ehci, port_num, 1);
	if (pdata->power_off)
		ehci_port_power(ehci, port_num, 0);

	return 0;
}

static const struct hc_driver ehci_platform_hc_driver = {
	.description = hcd_name,
	.product_desc = "Generic Platform EHCI Controller",
	.hcd_priv_size = sizeof(struct ehci_hcd),
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_DMA | HCD_USB2 | HCD_BH,

	.reset = ehci_platform_reset,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset = ehci_endpoint_reset,

	.get_frame_number = ehci_get_frame,

	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
#if defined(CONFIG_PM)
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
#endif
	.relinquish_port = ehci_relinquish_port,
	.port_handed_over = ehci_port_handed_over,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

#ifdef CONFIG_SWITCH_USB_ROLE
#define USB_UPHY_REG				(3)

static ssize_t usb_role_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t usb_role_switch_store(struct device *dev, struct device_attribute *attr,
							const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 switch_usb_role;
	u32 *grop1 = (u32 *) VA_IOB_ADDR(2 * 32 * 4);
	u32 ret;

	/* 0:host , 1:device */
	if (kstrtouint(buf, 0, &switch_usb_role) == 0) {
		pr_debug("usb rold -> %x\n", switch_usb_role);
		ret = ioread32(grop1 + USB_UPHY_REG);

		ret |= (1 << ((pdev->id - 1) * 8 + 4));
		if (switch_usb_role & 0x1) {
			ret &= ~(1 << ((pdev->id - 1) * 8 + 5));
			ret |= (1 << ((pdev->id - 1) * 8 + 6));
		} else
			ret |= (3 << ((pdev->id - 1) * 8 + 5));

		pr_debug("usb ret -> %x\n", ret);
		iowrite32(ret, grop1 + USB_UPHY_REG);
	}

	return count;
}

static DEVICE_ATTR_RW(usb_role_switch);

/* switch usb speed ( fs/hs ) */
static ssize_t usb_speed_switch_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = (struct usb_hcd *)platform_get_drvdata(pdev);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 result;

	result = ehci_readl(ehci, &ehci->regs->configured_flag);

	return sprintf(buf, "%d\n", result);
}

#define MAX_PORT_NUM				2
#define POWER_RESET_TIME			500
#define ENABLE_FORCE_HOST_DISCONNECT		1
#define DISABLE_FORCE_HOST_DISCONNECT		0
static ssize_t usb_speed_switch_store(struct device *dev, struct device_attribute *attr,
							const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = (struct usb_hcd *)platform_get_drvdata(pdev);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int port_num = pdev->id - 1;

	u32 usb_host_owner;

	/* 0:full speed 1:high speed */
	if (kstrtouint(buf, 0, &usb_host_owner) == 0) {
		if (port_num < MAX_PORT_NUM) {
			DISABLE_VBUS_POWER(port_num);
			uphy_force_disc(ENABLE_FORCE_HOST_DISCONNECT, port_num);
			msleep(POWER_RESET_TIME);
			ehci_writel(ehci, FLAG_CF & usb_host_owner,
						&ehci->regs->configured_flag);
			uphy_force_disc(DISABLE_FORCE_HOST_DISCONNECT,
					port_num);
			ENABLE_VBUS_POWER(port_num);
		} else if (port_num == MAX_PORT_NUM) {
			pr_notice("warning,port 2 is not supported to power reset\n");
			ehci_writel(ehci, FLAG_CF & usb_host_owner,
						&ehci->regs->configured_flag);
		} else {
			pr_notice("error port num:%d\n", port_num);
		}
	}

	return count;
}

static DEVICE_ATTR_RW(usb_speed_switch);

static ssize_t usb_uphy_swing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);

	return sprintf(buf, "%d\n", get_uphy_swing(pdev->id - 1));
}

static ssize_t usb_uphy_swing_store(struct device *dev, struct device_attribute *attr,
							const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 swing_val;

	if (kstrtouint(buf, 0, &swing_val) == 0)
		set_uphy_swing(swing_val, pdev->id - 1);

	return count;
}

static DEVICE_ATTR_RW(usb_uphy_swing);

static ssize_t usb_disconnect_level_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 disc_level;

	disc_level = get_disconnect_level(pdev->id - 1);
	pr_notice("port:%d,get disconnect level:0x%x\n", pdev->id - 1, disc_level);

	return sprintf(buf, "0x%x\n", disc_level);
}

static ssize_t usb_disconnect_level_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 disc_level;

	if (kstrtouint(buf, 0, &disc_level) == 0) {
		pr_notice("port:%d,set disconnect level:0x%x\n", pdev->id - 1, disc_level);
		set_disconnect_level(disc_level, pdev->id - 1);
	}

	return count;
}

static DEVICE_ATTR_RW(usb_disconnect_level);

static ssize_t usb_ctrl_reset_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	return 0;
}

static ssize_t usb_ctrl_reset_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = (struct usb_hcd *)platform_get_drvdata(pdev);
	u32 val = 1;

	if (kstrtouint(buf, 0, &val))
		return 1;

	if (hcd == NULL) {
		pr_notice("store_usb_ctrl_reset: usb controller invalid\n");
		return count;
	}
	pr_notice("Will reset usb controller val=%d\n", val);

	return count;
}

static DEVICE_ATTR_RW(usb_ctrl_reset);
#endif

#if defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
static int sp_ehci_platform_power_on(struct platform_device *pdev)
{
	int ret;

	ret = phy_power_on(uphy[pdev->id - 1]);
	if (ret)
		return ret;

	ret = phy_init(uphy[pdev->id - 1]);
	if (ret)
		return ret;

	ret = clk_prepare_enable(ehci_clk[pdev->id - 1]);
	if (ret)
		clk_disable_unprepare(ehci_clk[pdev->id - 1]);

	return ret;
}

static void sp_ehci_platform_power_off(struct platform_device *pdev)
{
	clk_disable_unprepare(ehci_clk[pdev->id - 1]);

	phy_power_off(uphy[pdev->id - 1]);
	phy_exit(uphy[pdev->id - 1]);
}
#endif

static struct usb_ehci_pdata usb_ehci_pdata = {
#if defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	.power_on = sp_ehci_platform_power_on,
	.power_suspend = sp_ehci_platform_power_off,
	.power_off = sp_ehci_platform_power_off,
#endif
};

int ehci_sunplus_probe(struct platform_device *dev)
{
#if defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	struct usb_ehci_pdata *pdata = &usb_ehci_pdata;
#endif
	struct usb_hcd *hcd;
	struct resource *res_mem;
	int irq;
	int err = -ENOMEM;
#if defined (CONFIG_USB_SUNPLUS_OTG) || defined (CONFIG_USB_SUNPLUS_SP7350_OTG)
	struct usb_phy *otg_phy;
#endif

	if (usb_disabled())
		return -ENODEV;

	dev->dev.platform_data = &usb_ehci_pdata;

#ifdef CONFIG_SOC_SP7021
	/* enable usb controller clock */
	ehci_clk[dev->id - 1] = devm_clk_get(&dev->dev, NULL);
	if (IS_ERR(ehci_clk[dev->id - 1])) {
		pr_err("not found clk source\n");
		return PTR_ERR(ehci_clk[dev->id - 1]);
	}
	clk_prepare(ehci_clk[dev->id - 1]);
	clk_enable(ehci_clk[dev->id - 1]);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	/* phy */
	if ((dev->id - 1) == USB_PORT0_ID) {
		uphy[dev->id - 1] = devm_phy_get(&dev->dev, "uphy");
		if (IS_ERR(uphy[dev->id - 1])) {
			dev_err(&dev->dev, "no USB phy0 configured\n");
			return PTR_ERR(uphy[dev->id - 1]);
		}
	} else if ((dev->id - 1) == USB_PORT1_ID) {
		uphy[dev->id - 1] = devm_phy_get(&dev->dev, "uphy");
		if (IS_ERR(uphy[dev->id - 1])) {
			dev_err(&dev->dev, "no USB phy1 configured\n");
			return PTR_ERR(uphy[dev->id - 1]);
		}
	}

	/* enable usb controller clock */
	ehci_clk[dev->id - 1] = devm_clk_get(&dev->dev, NULL);
	if (IS_ERR(ehci_clk[dev->id - 1])) {
		pr_err("not found clk source\n");
		return PTR_ERR(ehci_clk[dev->id - 1]);
	}

	if (pdata->power_on) {
		err = pdata->power_on(dev);
		if (err)
			return err;
	}
#endif

	irq = platform_get_irq(dev, 0);
	if (irq < 0) {
		pr_err("no irq provieded,ret:%d\n", irq);
		return irq;
	}
	pr_debug("ehci_id:%d,irq:%d\n", dev->id, irq);

	res_mem = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		pr_err("no memory recourse provieded");
		return -ENXIO;
	}

	hcd = usb_create_hcd(&ehci_platform_hc_driver, &dev->dev, dev_name(&dev->dev));
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = res_mem->start;
	hcd->rsrc_len = resource_size(res_mem);

	hcd->tpl_support = of_usb_host_tpl_support(dev->dev.of_node);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_err("controller already in use");
		err = -EBUSY;
		goto err_put_hcd;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs)
		goto err_release_region;

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	pr_debug("hcd_irq:%d,%d\n", hcd->irq, irq);
	if (err)
		goto err_iounmap;

	platform_set_drvdata(dev, hcd);

/**************************************************************************************************/
#if defined (CONFIG_USB_SUNPLUS_OTG) || defined (CONFIG_USB_SUNPLUS_SP7350_OTG)
	if (dev->id < 3) {
		otg_phy = usb_get_transceiver_sp(dev->id - 1);
		if (otg_phy) {
			err = otg_set_host(otg_phy->otg, &hcd->self);
			if (err < 0) {
				dev_err(&dev->dev, "unable to register with transceiver\n");
				goto err_iounmap;
			}
		}

		hcd->self.otg_port = 1;
		hcd->usb_phy = otg_phy;
	}
#endif

#ifdef CONFIG_SWITCH_USB_ROLE
	if (dev->id < 3) {
		device_create_file(&dev->dev, &dev_attr_usb_role_switch);
		device_create_file(&dev->dev, &dev_attr_usb_ctrl_reset);
	}

	device_create_file(&dev->dev, &dev_attr_usb_speed_switch);
	device_create_file(&dev->dev, &dev_attr_usb_uphy_swing);
	device_create_file(&dev->dev, &dev_attr_usb_disconnect_level);
#endif

	return err;

err_iounmap:
	iounmap(hcd->regs);
err_release_region:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err_put_hcd:
	usb_put_hcd(hcd);

	return err;
}
EXPORT_SYMBOL_GPL(ehci_sunplus_probe);

int ehci_sunplus_remove(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	struct usb_ehci_pdata *pdata = dev->dev.platform_data;
#endif

#ifdef CONFIG_SWITCH_USB_ROLE
	if (dev->id < 3) {
		device_remove_file(&dev->dev, &dev_attr_usb_role_switch);
		device_remove_file(&dev->dev, &dev_attr_usb_ctrl_reset);
	}

	device_remove_file(&dev->dev, &dev_attr_usb_speed_switch);
	device_remove_file(&dev->dev, &dev_attr_usb_uphy_swing);
	device_remove_file(&dev->dev, &dev_attr_usb_disconnect_level);
#endif

	usb_remove_hcd(hcd);

	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

	usb_put_hcd(hcd);
	platform_set_drvdata(dev, NULL);

#ifdef CONFIG_SOC_SP7021
	/* disable usb controller clock */
	clk_disable(ehci_clk[dev->id - 1]);
#elif defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	if (pdata->power_off)
		pdata->power_off(dev);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(ehci_sunplus_remove);

#ifdef CONFIG_PM
static int ehci_sunplus_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	struct usb_ehci_pdata *pdata = pdev->dev.platform_data;
#endif
	bool do_wakeup = device_may_wakeup(dev);
	int rc;

	pr_debug("%s.%d\n", __func__, __LINE__);

	rc = ehci_suspend(hcd, do_wakeup);
	if (rc)
		return rc;

	#ifdef CONFIG_SOC_SP7021
	/* disable usb controller clock */
	clk_disable(ehci_clk[pdev->id - 1]);
	#elif defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	if (pdata->power_suspend)
		pdata->power_suspend(pdev);
	#endif

	return 0;
}

static int ehci_sunplus_drv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = dev_get_drvdata(dev);
#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	struct usb_ehci_pdata *pdata = pdev->dev.platform_data;
	int err;
#endif

	pr_debug("%s.%d\n", __func__, __LINE__);

	#ifdef CONFIG_SOC_SP7021
	/* enable usb controller clock */
	clk_prepare(ehci_clk[pdev->id - 1]);
	clk_enable(ehci_clk[pdev->id - 1]);
	#elif defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	if (pdata->power_on) {
		err = pdata->power_on(pdev);
		if (err)
			return err;
	}
	#endif

	ehci_resume(hcd, false);

	return 0;
}

struct dev_pm_ops const ehci_sunplus_pm_ops = {
	.suspend = ehci_sunplus_drv_suspend,
	.resume = ehci_sunplus_drv_resume,
};
#endif

