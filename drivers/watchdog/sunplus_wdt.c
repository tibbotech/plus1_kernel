// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      sunplus Watchdog Driver
 *
 *      Copyright (c) 2019 Sunplus Technology Co., Ltd.
 *
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Based on xen_wdt.c
 *      (c) Copyright 2010 Novell, Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/watchdog.h>
/* ---------------------------------------------------------------------------------------------- */

//#define WDT_FUNC_DEBUG
//#define WDT_DBG_INFO
#define WDT_DBG_ERR

#ifdef WDT_FUNC_DEBUG
#define FUNC_DBG(fmt, args ...) printk(KERN_INFO "[WDT] dbg %s() (%d) " fmt "\n", __FUNCTION__, __LINE__, ## args)
#else
#define FUNC_DBG(fmt, args ...)
#endif

#ifdef WDT_DBG_INFO
#define DBG_INF(fmt, args ...) printk(KERN_INFO "[WDT] inf (%d): "  fmt "\n", __LINE__ , ## args)
#else
#define DBG_INF(fmt, args ...)
#endif

#ifdef WDT_DBG_ERR
#define DBG_ERR(fmt, args ...) printk(KERN_ERR "[WDT] err (%d): "  fmt "\n", __LINE__ , ## args)
#else
#define DBG_ERR(fmt, args ...)
#endif
/* ---------------------------------------------------------------------------------------------- */

#define WDT_CTRL                0x00
#define WDT_CNT                 0x04
//#define MISCELLANEOUS_CTRL		0x00

#define WDT_STOP				0x3877
#define WDT_RESUME				0x4A4B
#define WDT_CLRIRQ				0x7482
#define WDT_UNLOCK				0xAB00
#define WDT_LOCK				0xAB01
#define WDT_CONMAX				0xDEAF

#ifdef CONFIG_SOC_SP7021
#define RBUS_WDT_RST        (1 << 1)
#define STC_WDT_RST         (1 << 4)
#endif

#ifdef CONFIG_SOC_Q645
#define RBUS_WDT_RST        (1 << 9)
#define STC_WDT_RST         (1 << 10)
#endif

#define MASK_SET(mask)		((mask) | (mask << 16))

#define WDT_MAX_TIMEOUT		11
#define WDT_MIN_TIMEOUT		1

#define STC_CLK				90000

#define DRV_NAME		"sunplus-wdt"
#define DRV_VERSION		"1.0"

static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;

struct sunplus_wdt_dev {
	struct watchdog_device wdt_dev;
	void __iomem *wdt_base;
	void __iomem *miscellaneous;
};

/**
 *	1.We need to reset watchdog flag(clear watchdog interrupt) here
 *	because watchdog timer driver does not have an interrupt handler,
 *	and before enalbe STC and RBUS watchdog timeout. Otherwise,
 *	the intr is always in the triggered state.
 *	2.enable STC and RBUS watchdog timeout trigger.
 *	provied by xt.hu
 */
static int sunplus_wdt_hw_init(struct watchdog_device *wdt_dev)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;
	void __iomem *miscellaneous;
	u32 val;

	wdt_base = sunplus_wdt->wdt_base;
	miscellaneous = sunplus_wdt->miscellaneous;
	writel(WDT_CLRIRQ, wdt_base + WDT_CTRL);
	val = readl(miscellaneous);
	val |= MASK_SET(STC_WDT_RST);
	val |= MASK_SET(RBUS_WDT_RST);
	writel(val, miscellaneous);

	return 0;
}

static int sunplus_wdt_restart(struct watchdog_device *wdt_dev,
			       unsigned long action, void *data)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;

	wdt_base = sunplus_wdt->wdt_base;
	writel(WDT_STOP, wdt_base + WDT_CTRL);
	writel(WDT_UNLOCK, wdt_base + WDT_CTRL);
	writel(0x0001, wdt_base + WDT_CNT);
	writel(WDT_RESUME, wdt_base + WDT_CTRL);

	return 0;
}

/* TIMEOUT_MAX = ffff0/90kHz =11.65,so longer than 11 seconds will time out */
static int sunplus_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = sunplus_wdt->wdt_base;
	int cnt_val;

	writel(WDT_STOP, wdt_base + WDT_CTRL);
	writel(WDT_UNLOCK, wdt_base + WDT_CTRL);
	/*tiemrw_cnt[3:0]cant be write,only [19:4] can be write.*/
	cnt_val = (wdt_dev->timeout * STC_CLK) >> 4;
	//cnt_val = (wdt_dev->timeout * 90) >> 4; //test_for_q645,time dilation > 6000,so cnt div 1000
	writel(cnt_val, wdt_base + WDT_CNT);
	writel(WDT_RESUME, wdt_base + WDT_CTRL);

	return 0;
}

static int sunplus_wdt_set_timeout(struct watchdog_device *wdt_dev,
				   unsigned int timeout)
{
	wdt_dev->timeout = timeout;

	return 0;
}

static int sunplus_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = sunplus_wdt->wdt_base;

	writel(WDT_STOP, wdt_base + WDT_CTRL);

	return 0;
}

static int sunplus_wdt_start(struct watchdog_device *wdt_dev)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;
	int ret;

	wdt_base = sunplus_wdt->wdt_base;

	ret = sunplus_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
	if (ret < 0)
		return ret;

	sunplus_wdt_ping(wdt_dev);

	return 0;
}

static unsigned int sunplus_wdt_get_timeleft(struct watchdog_device *wdt_dev)
{
	struct sunplus_wdt_dev *sunplus_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = sunplus_wdt->wdt_base;
	unsigned int ret;

	ret = readl(wdt_base + WDT_CNT);
	ret &= 0xffff;
	ret = ret << 4;

	return ret;
}

static const struct watchdog_info sunplus_wdt_info = {
	.identity = DRV_NAME,
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops sunplus_wdt_ops = {
	.owner = THIS_MODULE,
	.start = sunplus_wdt_start,
	.stop = sunplus_wdt_stop,
	.ping = sunplus_wdt_ping,
	.set_timeout = sunplus_wdt_set_timeout,
	.get_timeleft = sunplus_wdt_get_timeleft,
	.restart = sunplus_wdt_restart,
};

static int sunplus_wdt_probe(struct platform_device *pdev)
{
	struct sunplus_wdt_dev *sunplus_wdt;
	struct resource *wdt_res;
	struct clk *clk;
	int err;
	int ret;
	int val;
	sunplus_wdt =
	    devm_kzalloc(&pdev->dev, sizeof(*sunplus_wdt), GFP_KERNEL);
	if (!sunplus_wdt)
		return -EINVAL;

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		DBG_ERR("Can't find clock source\n");
		return PTR_ERR(clk);
	} else {
		ret = clk_prepare_enable(clk);
		if (ret) {
			DBG_ERR("Clock can't be enabled correctly\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, sunplus_wdt);

	wdt_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sunplus_wdt->wdt_base = devm_ioremap_resource(&pdev->dev, wdt_res);
	if (IS_ERR(sunplus_wdt->wdt_base))
		return PTR_ERR(sunplus_wdt->wdt_base);

	wdt_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	sunplus_wdt->miscellaneous =
	    devm_ioremap(&pdev->dev, wdt_res->start, resource_size(wdt_res));
	if (IS_ERR(sunplus_wdt->miscellaneous))
		return PTR_ERR(sunplus_wdt->miscellaneous);

	sunplus_wdt->wdt_dev.info = &sunplus_wdt_info;
	sunplus_wdt->wdt_dev.ops = &sunplus_wdt_ops;
	sunplus_wdt->wdt_dev.timeout = WDT_MAX_TIMEOUT;
	sunplus_wdt->wdt_dev.max_timeout = WDT_MAX_TIMEOUT;
	sunplus_wdt->wdt_dev.min_timeout = WDT_MIN_TIMEOUT;
	sunplus_wdt->wdt_dev.parent = &pdev->dev;

	watchdog_init_timeout(&sunplus_wdt->wdt_dev, timeout, &pdev->dev);
	watchdog_set_nowayout(&sunplus_wdt->wdt_dev, nowayout);
	watchdog_set_restart_priority(&sunplus_wdt->wdt_dev, 128);

	watchdog_set_drvdata(&sunplus_wdt->wdt_dev, sunplus_wdt);

	sunplus_wdt_hw_init(&sunplus_wdt->wdt_dev);
	sunplus_wdt_stop(&sunplus_wdt->wdt_dev);
	err = watchdog_register_device(&sunplus_wdt->wdt_dev);
	if (unlikely(err))
		return err;

	dev_info(&pdev->dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
		 sunplus_wdt->wdt_dev.timeout, nowayout);

	return 0;
}

static void sunplus_wdt_shutdown(struct platform_device *pdev)
{
	struct sunplus_wdt_dev *sunplus_wdt = platform_get_drvdata(pdev);

	if (watchdog_active(&sunplus_wdt->wdt_dev))
		sunplus_wdt_stop(&sunplus_wdt->wdt_dev);
}

static int sunplus_wdt_remove(struct platform_device *pdev)
{
	struct sunplus_wdt_dev *sunplus_wdt = platform_get_drvdata(pdev);

	watchdog_unregister_device(&sunplus_wdt->wdt_dev);

	return 0;
}

static const struct of_device_id sunplus_wdt_dt_ids[] = {
	{.compatible = "sunplus,sp7021-wdt"},
	{.compatible = "sunplus,q645-wdt"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, sunplus_wdt_dt_ids);

static struct platform_driver sunplus_wdt_driver = {
	.probe = sunplus_wdt_probe,
	.remove = sunplus_wdt_remove,
	.shutdown = sunplus_wdt_shutdown,
	.driver = {
		   .name = DRV_NAME,
		   .of_match_table = sunplus_wdt_dt_ids,
	},
};

module_platform_driver(sunplus_wdt_driver);

module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus WatchDog Timer Driver");
