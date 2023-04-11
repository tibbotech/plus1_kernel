// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/hw_random.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define SP7350_RNG_DATA		0x00	/* Data register */
#define SP7350_RNG_CTRL		0x04	/* Control register */

#define SP7350_RNG_DIV		0	/* 25M / (DIV + 1) */
#define SP7350_RNG_EN		BIT(10)

#define SP7350_RNG_HWM		0x7ff0000	/* HIWORD_MASK */

#define to_sp7350_rng(p)	container_of(p, struct sp7350_rng, rng)

struct sp7350_rng {
	struct hwrng rng;
	void __iomem *base;
	struct clk *clk;
	struct reset_control *rstc;
};

static int sp7350_rng_init(struct hwrng *rng)
{
	struct sp7350_rng *priv = to_sp7350_rng(rng);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	writel(SP7350_RNG_DIV | SP7350_RNG_EN | SP7350_RNG_HWM,
	       priv->base + SP7350_RNG_CTRL);

	return 0;
}

static void sp7350_rng_cleanup(struct hwrng *rng)
{
	struct sp7350_rng *priv = to_sp7350_rng(rng);

	writel(SP7350_RNG_DIV | SP7350_RNG_HWM,
	       priv->base + SP7350_RNG_CTRL);

	clk_disable_unprepare(priv->clk);
}

static int sp7350_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct sp7350_rng *priv = to_sp7350_rng(rng);

	pm_runtime_get_sync((struct device *)priv->rng.priv);

	*(u32 *)buf = readl(priv->base + SP7350_RNG_DATA);

	pm_runtime_mark_last_busy((struct device *)priv->rng.priv);
	pm_runtime_put_sync_autosuspend((struct device *)priv->rng.priv);

	return 4;
}

static int sp7350_rng_probe(struct platform_device *pdev)
{
	struct sp7350_rng *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	priv->rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(priv->rstc))
		return PTR_ERR(priv->rstc);
	reset_control_deassert(priv->rstc);

	dev_set_drvdata(&pdev->dev, priv);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 100);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

#ifndef CONFIG_PM
	priv->rng.init = sp7350_rng_init;
	priv->rng.cleanup = sp7350_rng_cleanup;
#endif
	priv->rng.name = pdev->name;
	priv->rng.read = sp7350_rng_read;
	priv->rng.priv = (unsigned long)&pdev->dev;

	ret = devm_hwrng_register(&pdev->dev, &priv->rng);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register rng device: %d\n",
			ret);
		pm_runtime_disable(&pdev->dev);
		pm_runtime_set_suspended(&pdev->dev);
		return ret;
	}

	return 0;
}

static int sp7350_rng_remove(struct platform_device *pdev)
{
	struct sp7350_rng *priv = platform_get_drvdata(pdev);

	devm_hwrng_unregister(&pdev->dev, &priv->rng);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM
static int sp7350_rng_runtime_suspend(struct device *dev)
{
	struct sp7350_rng *priv = dev_get_drvdata(dev);

	sp7350_rng_cleanup(&priv->rng);

	return 0;
}

static int sp7350_rng_runtime_resume(struct device *dev)
{
	struct sp7350_rng *priv = dev_get_drvdata(dev);

	return sp7350_rng_init(&priv->rng);
}
#endif

static const struct dev_pm_ops sp7350_rng_pm_ops = {
	SET_RUNTIME_PM_OPS(sp7350_rng_runtime_suspend,
			   sp7350_rng_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static const struct of_device_id sp7350_rng_dt_id[] __maybe_unused = {
	{ .compatible = "sunplus,sp7350-rng",  },
	{},
};
MODULE_DEVICE_TABLE(of, sp7350_rng_dt_id);

static struct platform_driver sp7350_rng_driver = {
	.driver = {
		.name		= "sp7350-rng",
		.pm		= &sp7350_rng_pm_ops,
		.of_match_table = of_match_ptr(sp7350_rng_dt_id),
	},
	.probe		= sp7350_rng_probe,
	.remove		= sp7350_rng_remove,
};

module_platform_driver(sp7350_rng_driver);

MODULE_DESCRIPTION("Sunplus SP7350 Random Number Generator Driver");
MODULE_AUTHOR("qinjian <qinjian@sunmedia.com.cn>");
MODULE_LICENSE("GPL v2");
