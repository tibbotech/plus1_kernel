// SPDX-License-Identifier: GPL-2.0-only
/*
 * sunplus Watchdog Driver
 *
 * Copyright (C) 2021 Sunplus Technology Co., Ltd.
 *
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>

#define WDT_CTRL		0x00
#define WDT_CNT			0x04

#ifdef CONFIG_SOC_SP7350
#define RBUS_WDT_RST		BIT(0)
#define STC_WDT_RST		BIT(4)
#define MASK_SET(mask)		((mask) | (mask << 16))
/* Mode selection */
#define WDT_MODE_INTR		0x0
#define WDT_MODE_RST		0x2
#define WDT_MODE_INTR_RST	0x3
#endif

#define WDT_STOP		0x3877
#define WDT_RESUME		0x4A4B
#define WDT_CLRIRQ		0x7482
#define WDT_UNLOCK		0xAB00
#define WDT_LOCK		0xAB01
#define WDT_CONMAX		0xDEAF

#ifdef CONFIG_SOC_SP7350
//#define STC_CLK		1000000
#define ZEBU_TEMP		500//zebu 3200MHz ---> 1.5MHz
#define STC_CLK			ZEBU_TEMP
/* HW_TIMEOUT_MAX = 0xffffffff/1MHz = 4294 */
#define SP_WDT_MAX_TIMEOUT	4294U
#define SP_WDT_DEFAULT_TIMEOUT	10
#else
#define STC_CLK			90000
/* HW_TIMEOUT_MAX = 0xffff0/90kHz =11.65 */
#define SP_WDT_MAX_TIMEOUT	11U
#define SP_WDT_DEFAULT_TIMEOUT	10
#endif

#define DEVICE_NAME		"sunplus-wdt"

static unsigned int timeout;
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct sp_wdt_priv {
	struct watchdog_device wdev;
	void __iomem *base;
#ifdef CONFIG_SOC_SP7350
	void __iomem *mode_sel;
	void __iomem *base_rst;
	int irqn;
#endif
	struct clk *clk;
	struct reset_control *rstc;
	u32 mode;
};

static int sp_wdt_restart(struct watchdog_device *wdev,
			  unsigned long action, void *data)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;
#ifdef CONFIG_SOC_SP7350
	void __iomem *base_rst = priv->base_rst;
#endif
	writel(WDT_STOP, base + WDT_CTRL);
	writel(WDT_UNLOCK, base + WDT_CTRL);
	writel(0x1, base + WDT_CNT);
#ifdef CONFIG_SOC_SP7350
	writel(0x1, base_rst + WDT_CNT);
#endif
	writel(WDT_LOCK, base + WDT_CTRL);
	writel(WDT_RESUME, base + WDT_CTRL);

	return 0;
}

static int sp_wdt_ping(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;
#ifdef CONFIG_SOC_SP7350
	void __iomem *base_rst = priv->base_rst;
	u32 count_f;
	u32 count_b;
	u32 time_f;
	u32 time_b;
	int irqn = priv->irqn;

	time_f = wdev->timeout - wdev->pretimeout;
	time_b = wdev->pretimeout;

	count_f = time_f * STC_CLK;
	count_b = time_b * STC_CLK;
#else
	u32 count;
#endif
	if (wdev->timeout > SP_WDT_MAX_TIMEOUT) {
		/* WDT_CONMAX sets the count to the maximum (down-counting). */
		writel(WDT_CONMAX, base + WDT_CTRL);
	} else {
		writel(WDT_UNLOCK, base + WDT_CTRL);
#ifdef CONFIG_SOC_SP7350
		writel(count_f, base + WDT_CNT);
		writel(count_b, base_rst + WDT_CNT);
#else

		/*
		 * Watchdog timer is a 20-bit down-counting based on STC_CLK.
		 * This register bits[16:0] is from bit[19:4] of the watchdog
		 * timer counter.
		 */
		count = (wdev->timeout * STC_CLK) >> 4;
		writel(count, base + WDT_CNT);
#endif
		writel(WDT_LOCK, base + WDT_CTRL);
	}
#ifdef CONFIG_SOC_SP7350
	/*
	 * Workaround for pretimeout counter. See the function sp_wdt_isr()
	 * for details .
	 */
	writel(WDT_CLRIRQ, base + WDT_CTRL);
	enable_irq(irqn);
#endif
	return 0;
}

static int sp_wdt_stop(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;

	writel(WDT_STOP, base + WDT_CTRL);

	return 0;
}

static int sp_wdt_start(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;

	writel(WDT_RESUME, base + WDT_CTRL);

	return 0;
}

static unsigned int sp_wdt_get_timeleft(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;
	u32 val;

	val = readl(base + WDT_CNT);
#ifndef CONFIG_SOC_SP7350
	val &= 0xffff;
	val = val << 4;
#endif
	return val;
}

#ifdef CONFIG_SOC_SP7350
static int sp_wdt_set_mode(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *mode_sel = priv->mode_sel;
	u32 val = priv->mode;

	writel(val, mode_sel);

	return 0;
}

static int sp_wdt_hw_init(struct watchdog_device *wdev)
{
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;
	void __iomem *base_rst = priv->base_rst;
	u32 val;

	writel(WDT_CLRIRQ, base + WDT_CTRL);
	val = readl(base_rst);
	val |= MASK_SET(STC_WDT_RST);
	writel(val, base_rst);

	sp_wdt_stop(wdev);

	return 0;
}

static irqreturn_t sp_wdt_isr(int irq, void *arg)
{
	struct watchdog_device *wdev = arg;
	struct sp_wdt_priv *priv = watchdog_get_drvdata(wdev);
	void __iomem *base = priv->base;
	int irqn = priv->irqn;

	//writel(WDT_CLRIRQ, base + WDT_CTRL);

	watchdog_notify_pretimeout(wdev);

	/*
	 * There are two counter in WDG, wdg_cnt and wdg_intrst_cnt.
	 * When the wdg_cnt down to 0, interrupt flag = 1. For wdg_intrst_cnt
	 * to keep counting, we need to keep the flag = 1. But if flag = 1,
	 * the interrupt handle always acknowledge. So mask the irq, clear
	 * the flag and enable the irq before reloading the counters.
	 */
	disable_irq_nosync(irqn);

	return IRQ_HANDLED;
}
#endif
static const struct watchdog_info sp_wdt_info = {
	.identity	= DEVICE_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_MAGICCLOSE |
			  WDIOF_KEEPALIVEPING,
};
#ifdef CONFIG_SOC_SP7350
static const struct watchdog_info sp_wdt_pt_info = {
	.identity	= DEVICE_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_PRETIMEOUT |
			  WDIOF_MAGICCLOSE |
			  WDIOF_KEEPALIVEPING,
};
#endif
static const struct watchdog_ops sp_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= sp_wdt_start,
	.stop		= sp_wdt_stop,
	.ping		= sp_wdt_ping,
	.get_timeleft	= sp_wdt_get_timeleft,
	.restart	= sp_wdt_restart,
};

static void sp_clk_disable_unprepare(void *data)
{
	clk_disable_unprepare(data);
}

static void sp_reset_control_assert(void *data)
{
	reset_control_assert(data);
}

static int sp_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sp_wdt_priv *priv;
	int ret;
#ifdef CONFIG_SOC_SP7350
	int irq;
#endif
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk))
		return dev_err_probe(dev, PTR_ERR(priv->clk), "Failed to get clock\n");

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to enable clock\n");

	ret = devm_add_action_or_reset(dev, sp_clk_disable_unprepare, priv->clk);
	if (ret)
		return ret;

	/* The timer and watchdog shared the STC reset */
	priv->rstc = devm_reset_control_get_shared(dev, NULL);
	if (IS_ERR(priv->rstc))
		return dev_err_probe(dev, PTR_ERR(priv->rstc), "Failed to get reset\n");

	reset_control_deassert(priv->rstc);

	ret = devm_add_action_or_reset(dev, sp_reset_control_assert, priv->rstc);
	if (ret)
		return ret;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

#ifdef CONFIG_SOC_SP7350
		priv->mode_sel = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(priv->mode_sel))
		return PTR_ERR(priv->mode_sel);

	priv->base_rst = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(priv->base_rst))
		return PTR_ERR(priv->base_rst);

	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, sp_wdt_isr, 0, "wdt_bark",
				       &priv->wdev);
		if (ret)
			return ret;

		priv->wdev.info = &sp_wdt_pt_info;
		priv->wdev.pretimeout = SP_WDT_DEFAULT_TIMEOUT / 2;
		priv->mode = WDT_MODE_INTR_RST;
		priv->irqn = irq;
	} else {
		if (irq == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		priv->wdev.info = &sp_wdt_info;
		priv->mode = WDT_MODE_RST;
	}
#else
	priv->wdev.info = &sp_wdt_info;
#endif
	priv->wdev.ops = &sp_wdt_ops;
	priv->wdev.timeout = SP_WDT_DEFAULT_TIMEOUT;
	priv->wdev.max_hw_heartbeat_ms = SP_WDT_MAX_TIMEOUT * 1000;
	priv->wdev.min_timeout = 1;
	priv->wdev.parent = dev;

	watchdog_set_drvdata(&priv->wdev, priv);
#ifdef CONFIG_SOC_SP7350
	sp_wdt_set_mode(&priv->wdev);
	sp_wdt_hw_init(&priv->wdev);
#endif
	watchdog_init_timeout(&priv->wdev, timeout, dev);
	watchdog_set_nowayout(&priv->wdev, nowayout);
	watchdog_stop_on_reboot(&priv->wdev);
	watchdog_set_restart_priority(&priv->wdev, 128);

	return devm_watchdog_register_device(dev, &priv->wdev);
}

static const struct of_device_id sp_wdt_of_match[] = {
	{.compatible = "sunplus,sp7021-wdt", },
	{.compatible = "sunplus,q645-wdt", },
	{.compatible = "sunplus,sp7350-wdt", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_wdt_of_match);

static struct platform_driver sp_wdt_driver = {
	.probe = sp_wdt_probe,
	.driver = {
		   .name = DEVICE_NAME,
		   .of_match_table = sp_wdt_of_match,
	},
};

module_platform_driver(sp_wdt_driver);

MODULE_AUTHOR("Xiantao Hu <xt.hu@sunmedia.com.cn>");
MODULE_DESCRIPTION("Sunplus Watchdog Timer Driver");
MODULE_LICENSE("GPL v2");
