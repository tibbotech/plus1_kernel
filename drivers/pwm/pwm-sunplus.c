// SPDX-License-Identifier: GPL-2.0
/*
 * PWM device driver for SUNPLUS SoCs
 *
 * Copyright (C) 2020 SUNPLUS Inc.
 *
 * Author:	Hammer Hsieh <hammer.hsieh@sunplus.com>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SUNPLUS_PWM_NUM		ePWM_MAX
#define SUNPLUS_PWM_FREQ	ePWM_FREQ_MAX
#define PWM_CNT_BIT_SHIFT	8
#define PWM_DD_SEL_BIT_MASK	0x3
#define ePWM_DD_MAX		4
#define ePWM_DD_UNKNOWN ePWM_DD_MAX

#if defined(CONFIG_SOC_SP7021)
#define DRV_NAME "sp7021-pwm"
#define DESC_NAME "Sunplus SP7021 PWM Driver"
#define PWM_CLK_MANAGEMENT
#ifdef PWM_CLK_MANAGEMENT
#define ePWM_MAX		8
#else
#define ePWM_MAX		4
#endif
#define PWM_FREQ_SCALER		256
#define ePWM_FREQ_MAX 0xffff
#define ePWM_DUTY_MAX 0x00ff
#define PWM_CONTROL0	0x000
#define PWM_CONTROL1	0x001
#define PWM_FREQ_BASE	0x002
#define PWM_DUTY_BASE	0x006
#define PWM_DD_SEL_BIT_SHIFT	8
#elif defined(CONFIG_SOC_Q645)
#define DRV_NAME "q645-pwm"
#define DESC_NAME "Sunplus Q645 PWM Driver"
//#define PWM_CLK_MANAGEMENT
#define ePWM_MAX		4
#define PWM_FREQ_SCALER		4096
#define ePWM_FREQ_MAX 0x3ffff
#define ePWM_DUTY_MAX 0x00fff
#define PWM_CONTROL0	0x000
#define PWM_CONTROL1	0x020
#define PWM_FREQ_BASE	0x011
#define PWM_DUTY_BASE	0x001
#define PWM_DD_SEL_BIT_SHIFT	16
#define PWM_INV_BIT_SHIFT	8
#elif defined(CONFIG_SOC_SP7350)
#define DRV_NAME "sp7350-pwm"
#define DESC_NAME "Sunplus SP7350 PWM Driver"
//#define PWM_CLK_MANAGEMENT
#define ePWM_MAX		4
#define PWM_FREQ_SCALER		4096
#define ePWM_FREQ_MAX 0x3ffff
#define ePWM_DUTY_MAX 0x00fff
#define PWM_CONTROL0	0x000
#define PWM_CONTROL1	0x020
#define PWM_FREQ_BASE	0x011
#define PWM_DUTY_BASE	0x001
#define PWM_DD_SEL_BIT_SHIFT	16
#define PWM_INV_BIT_SHIFT	8
#endif

struct sunplus_pwm {
	struct pwm_chip chip;
	void __iomem *regs;
	struct clk *clk;
};

static inline struct sunplus_pwm *to_sunplus_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct sunplus_pwm, chip);
}

static void sunplus_reg_init(void __iomem *p)
{
	u32 i;

#if defined(CONFIG_SOC_SP7021)
	writel(0x0000, p + PWM_CONTROL0 * 4);
	writel(0x0f0f, p + PWM_CONTROL1 * 4);
#elif defined(CONFIG_SOC_Q645)  || defined(CONFIG_SOC_SP7350)
	writel(0x00000000, p + PWM_CONTROL0 * 4);
	writel(0x00000000, p + PWM_CONTROL1 * 4);
#endif
	for (i = 0; i < ePWM_MAX; i++) {
		writel(0, p + (PWM_FREQ_BASE + i) * 4);
		writel(0, p + (PWM_DUTY_BASE + i) * 4);
	}
}

#ifdef PWM_CLK_MANAGEMENT
static void sunplus_savepwmclk(struct sunplus_pwm *chip,
		struct pwm_device *pwm)
{
	u32 cur_dd_sel, cur_dd_value, dd_sel_tmp;
	u32 pwm_du_en;
	int i;

	cur_dd_sel = readl(chip->regs + (PWM_DUTY_BASE + pwm->hwpwm) * 4);
	cur_dd_sel = (cur_dd_sel >> PWM_DD_SEL_BIT_SHIFT) & PWM_DD_SEL_BIT_MASK;
	cur_dd_value = readl(chip->regs + (PWM_FREQ_BASE + cur_dd_sel) * 4);
	pwm_du_en = readl(chip->regs + PWM_CONTROL0 * 4);
	pwm_du_en = pwm_du_en & 0xff;

	/* check pwm clk source */
	if (!(cur_dd_value))
		return;

	/* scan all enable pwm channel */
	for (i = 0; i < ePWM_MAX; ++i) {
		dd_sel_tmp = readl(chip->regs + (PWM_DUTY_BASE + i) * 4);
		dd_sel_tmp = (dd_sel_tmp >> PWM_DD_SEL_BIT_SHIFT) & PWM_DD_SEL_BIT_MASK;
		if ((pwm_du_en & (1 << i)) && (dd_sel_tmp == cur_dd_sel))
			break;
	}

	/* clean pwm channel value and clk source select */
	writel(0, chip->regs + (PWM_DUTY_BASE + pwm->hwpwm) * 4);

	/* turn off unused pwm clk source */
	if (i == ePWM_MAX)
		writel(0, chip->regs + (PWM_FREQ_BASE + cur_dd_sel) * 4);

}
#endif

#ifdef PWM_CLK_MANAGEMENT
static int sunplus_setpwm(struct sunplus_pwm *chip,
		struct pwm_device *pwm,
		int duty_ns,
		int period_ns)
{
	u32 dd_sel_old = ePWM_DD_MAX, dd_sel_new = ePWM_DD_MAX;
	u32 dd_sel_tmp, du_value_tmp, duty;
	u32 dd_value_tmp, dd_freq, value;
	u32 pwm_dd_en, pwm_du_en, dd_used_status;
	u64 tmp;
	int i;

#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	value = readl(chip->regs + PWM_CONTROL1 * 4);

	if (pwm->state.polarity == PWM_POLARITY_NORMAL)
		value &= ~(1 << (pwm->hwpwm + PWM_INV_BIT_SHIFT));
	else
		value |= 1 << (pwm->hwpwm + PWM_INV_BIT_SHIFT);

	writel(value, chip->regs + PWM_CONTROL1 * 4);
#endif

	/* cal pwm freq and check value under range */
	tmp = clk_get_rate(chip->clk) * (u64)period_ns;
	tmp = DIV_ROUND_CLOSEST_ULL(tmp, NSEC_PER_SEC);
	tmp = DIV_ROUND_CLOSEST_ULL(tmp, PWM_FREQ_SCALER);
	dd_freq = (u32)tmp;

	if (dd_freq == 0)
		return -EINVAL;

	if (dd_freq >= ePWM_FREQ_MAX)
		dd_freq = ePWM_FREQ_MAX;

	/* read all clk source and PWM channel status */
	pwm_du_en = readl(chip->regs + PWM_CONTROL0 * 4);
	pwm_du_en = pwm_du_en & 0xff;
	for (i = 0; i < ePWM_DD_MAX; ++i) {
		dd_value_tmp = readl(chip->regs + (PWM_FREQ_BASE + i) * 4);
		if (dd_value_tmp)
			pwm_dd_en |= (1 << i);
	}

	/* get current pwm channel dd_sel */
	dd_sel_old = readl(chip->regs + (PWM_DUTY_BASE + pwm->hwpwm) * 4);
	dd_sel_old = (dd_sel_old >> PWM_DD_SEL_BIT_SHIFT) & PWM_DD_SEL_BIT_MASK;
	if (!(pwm_dd_en & (1 << dd_sel_old)))
		dd_sel_old = ePWM_DD_UNKNOWN;

	/* find the same freq and turn on clk source */
	for (i = 0; i < ePWM_DD_MAX; ++i) {
		dd_value_tmp = readl(chip->regs + (PWM_FREQ_BASE + i) * 4);
		if ((pwm_dd_en & (1 << i))
			&& (dd_value_tmp == dd_freq))
			break;
	}
	if (i != ePWM_DD_UNKNOWN)
		dd_sel_new = i;

	/* in case of all clk source unused */
	if ((!pwm_dd_en) && (!pwm_du_en))
		dd_sel_new = 0;

	/* dd_sel only myself used */
	if (dd_sel_new == ePWM_DD_UNKNOWN) {
		for (i = 0; i < ePWM_MAX; ++i) {
			if (i == pwm->hwpwm)
				continue;

			dd_sel_tmp = readl(chip->regs + (PWM_DUTY_BASE + i) * 4);
			dd_sel_tmp = (dd_sel_tmp >> PWM_DD_SEL_BIT_SHIFT) & PWM_DD_SEL_BIT_MASK;

			if ((pwm_du_en & (1 << i))
					&& (dd_sel_tmp == dd_sel_old))
				break;
		}
		if (i == ePWM_MAX)
			dd_sel_new = dd_sel_old;
	}

	/* find unused clk source */
	if (dd_sel_new == ePWM_DD_UNKNOWN) {
		for (i = 0; i < ePWM_DD_MAX; ++i) {
			if (!(pwm_dd_en & (1 << i)))
				break;
		}
		dd_sel_new = i;
	}

	if (dd_sel_new == ePWM_DD_UNKNOWN) {
		pr_err("pwm%d Can't found clk source[0x%x(%d)].\n",
				pwm->hwpwm, dd_freq, dd_freq);
		return -EBUSY;
	}
	/* set clk source for pwm freq */
	writel(dd_freq, chip->regs + (PWM_FREQ_BASE + dd_sel_new) * 4);

	/* cal and set pwm duty */
	value = readl(chip->regs + PWM_CONTROL0 * 4);
	if (duty_ns == period_ns) {
		value |= (1 << (pwm->hwpwm + PWM_CNT_BIT_SHIFT));
		writel(value, chip->regs + PWM_CONTROL0 * 4);
		duty = (ePWM_DUTY_MAX + 1);
	} else {
		value &= ~(1 << (pwm->hwpwm + PWM_CNT_BIT_SHIFT));
		writel(value, chip->regs + PWM_CONTROL0 * 4);

		tmp = (u64)duty_ns * (ePWM_DUTY_MAX + 1) + (period_ns >> 1);
		tmp = DIV_ROUND_CLOSEST_ULL(tmp, (u64)period_ns);
		duty = (u32)tmp;
		value = duty;
		value |= (dd_sel_new << PWM_DD_SEL_BIT_SHIFT);
		writel(value, chip->regs + (PWM_DUTY_BASE + pwm->hwpwm) * 4);
	}

	/* chk unused pwm clk source and turn off it */
	if ((dd_sel_old != dd_sel_new) && (dd_sel_old != ePWM_DD_MAX)) {
		dd_used_status = 0;
		for (i = 0; i < ePWM_MAX; ++i) {
			value = readl(chip->regs + (PWM_DUTY_BASE + i) * 4);
			dd_sel_tmp = (value >> PWM_DD_SEL_BIT_SHIFT) & PWM_DD_SEL_BIT_MASK;
			du_value_tmp = value & ePWM_DUTY_MAX;
			if (du_value_tmp != 0)
				dd_used_status |= (1 << dd_sel_tmp);
		}

		if ((pwm_dd_en & (1 << dd_sel_old)) != (dd_used_status & (1 << dd_sel_old)))
			writel(0, chip->regs + (PWM_FREQ_BASE + dd_sel_old) * 4);
	}

	pr_debug("pwm:%d, output freq:%lu Hz, duty:%u%%\n",
			pwm->hwpwm, clk_get_rate(chip->clk) / (dd_freq * PWM_FREQ_SCALER),
			(duty * 100) / (ePWM_DUTY_MAX + 1));

	return 0;
}
#else
static int sunplus_setpwm(struct sunplus_pwm *chip,
		struct pwm_device *pwm,
		int duty_ns,
		int period_ns)
{
	u32 duty, dd_freq, value;
	u64 tmp;

#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
	value = readl(chip->regs + PWM_CONTROL1 * 4);

	if (pwm->state.polarity == PWM_POLARITY_NORMAL)
		value &= ~(1 << (pwm->hwpwm + PWM_INV_BIT_SHIFT));
	else
		value |= 1 << (pwm->hwpwm + PWM_INV_BIT_SHIFT);

	writel(value, chip->regs + PWM_CONTROL1 * 4);
#endif

	/* cal pwm freq and check value under range */
	tmp = clk_get_rate(chip->clk) * (u64)period_ns;
	tmp = DIV_ROUND_CLOSEST_ULL(tmp, NSEC_PER_SEC);
	tmp = DIV_ROUND_CLOSEST_ULL(tmp, PWM_FREQ_SCALER);
	dd_freq = (u32)tmp;

	if (dd_freq == 0)
		return -EINVAL;

	if (dd_freq >= ePWM_FREQ_MAX)
		dd_freq = ePWM_FREQ_MAX;

	/* set clk source for pwm freq */
	writel(dd_freq, chip->regs + (PWM_FREQ_BASE + pwm->hwpwm) * 4);

	/* cal and set pwm duty */
	value = readl(chip->regs + PWM_CONTROL0 * 4);
	if (duty_ns == period_ns) {
		value |= (1 << (pwm->hwpwm + PWM_CNT_BIT_SHIFT));
		writel(value, chip->regs + PWM_CONTROL0 * 4);
		duty = (ePWM_DUTY_MAX + 1);
	} else {
		value &= ~(1 << (pwm->hwpwm + PWM_CNT_BIT_SHIFT));
		writel(value, chip->regs + PWM_CONTROL0 * 4);

		tmp = (u64)duty_ns * (ePWM_DUTY_MAX + 1) + (period_ns >> 1);
		tmp = DIV_ROUND_CLOSEST_ULL(tmp, (u64)period_ns);
		duty = (u32)tmp;
		value = duty;
		value |= (pwm->hwpwm << PWM_DD_SEL_BIT_SHIFT);
		writel(value, chip->regs + (PWM_DUTY_BASE + pwm->hwpwm) * 4);
	}

	pr_debug("pwm:%d, output freq:%lu Hz, duty:%u%%\n",
			pwm->hwpwm, clk_get_rate(chip->clk) / (dd_freq * PWM_FREQ_SCALER),
			(duty * 100) / (ePWM_DUTY_MAX + 1));

	return 0;
}
#endif

static void sunplus_pwm_unexport(struct pwm_chip *chip,
		struct pwm_device *pwm)
{
	if (pwm_is_enabled(pwm)) {
		struct pwm_state state;

		pwm_get_state(pwm, &state);
		state.enabled = 0;
		pwm_apply_state(pwm, &state);
	}

}

static int sunplus_pwm_enable(struct pwm_chip *chip,
		struct pwm_device *pwm)
{
	struct sunplus_pwm *pdata = to_sunplus_pwm(chip);
	u32 period = pwm_get_period(pwm);
	u32 duty_cycle = pwm_get_duty_cycle(pwm);
	u32 value;
	int ret;

	pm_runtime_get_sync(chip->dev);

	ret = sunplus_setpwm(pdata, pwm, duty_cycle, period);
	if (!ret) {
		value = readl(pdata->regs + PWM_CONTROL0 * 4);
		value |= (1 << pwm->hwpwm);
		writel(value, pdata->regs + PWM_CONTROL0 * 4);
	} else {
		pm_runtime_put(chip->dev);
	}

	return ret;
}

static void sunplus_pwm_disable(struct pwm_chip *chip,
		struct pwm_device *pwm)
{
	struct sunplus_pwm *pdata = to_sunplus_pwm(chip);
	u32 value;

	value = readl(pdata->regs + PWM_CONTROL0 * 4);
	value &= ~(1 << pwm->hwpwm);
	writel(value, pdata->regs + PWM_CONTROL0 * 4);

#ifdef PWM_CLK_MANAGEMENT
	sunplus_savepwmclk(pdata, pwm);
#endif

	pm_runtime_put(chip->dev);
}

static int sunplus_pwm_config(struct pwm_chip *chip,
		struct pwm_device *pwm,
		int duty_ns,
		int period_ns)
{
	struct sunplus_pwm *pdata = to_sunplus_pwm(chip);

	if (pwm_is_enabled(pwm))
		return( sunplus_setpwm(pdata, pwm, duty_ns, period_ns));

	return 0;
}

#if defined(CONFIG_SOC_Q645) || defined(CONFIG_SOC_SP7350)
static int sunplus_pwm_polarity(struct pwm_chip *chip,
		struct pwm_device *pwm,
		enum pwm_polarity polarity)
{
	return 0;
}
#endif

static const struct pwm_ops _sunplus_pwm_ops = {
	.free = sunplus_pwm_unexport,
	.enable = sunplus_pwm_enable,
	.disable = sunplus_pwm_disable,
	.config = sunplus_pwm_config,
#if defined(CONFIG_SOC_Q645)  || defined(CONFIG_SOC_SP7350)
	.set_polarity = sunplus_pwm_polarity,
#endif
	.owner = THIS_MODULE,
};

static int sunplus_pwm_probe(struct platform_device *pdev)
{
	struct sunplus_pwm *pdata;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "invalid devicetree node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_err(&pdev->dev, "devicetree status is not available\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "get resource memory from devicetree node.\n");
		return PTR_ERR(res);
	}
	pdata->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pdata->regs)) {
		dev_err(&pdev->dev, "mapping resource memory.\n");
		return PTR_ERR(pdata->regs);
	}

	pdata->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pdata->clk)) {
		dev_err(&pdev->dev, "not found clk source.\n");
		return PTR_ERR(pdata->clk);
	}
	ret = clk_prepare_enable(pdata->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clk source.\n");
		return ret;
	}

	pdata->chip.dev = &pdev->dev;
	pdata->chip.ops = &_sunplus_pwm_ops;
	pdata->chip.base = -1;
	pdata->chip.npwm = SUNPLUS_PWM_NUM;
	/* pwm cell = 2 (of_pwm_simple_xlate) */
	pdata->chip.of_xlate = NULL;

	sunplus_reg_init(pdata->regs);

	platform_set_drvdata(pdev, pdata);

	ret = pwmchip_add(&pdata->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip\n");
		clk_disable_unprepare(pdata->clk);
		return ret;
	}
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_put(&pdev->dev);

	return 0;
}

static int sunplus_pwm_remove(struct platform_device *pdev)
{
	struct sunplus_pwm *pdata;
	int ret;

	pm_runtime_disable(&pdev->dev);

	pdata = platform_get_drvdata(pdev);
	if (pdata == NULL)
		return -ENODEV;

	ret = pwmchip_remove(&pdata->chip);

#ifndef CONFIG_PM
	clk_disable_unprepare(pdata->clk);
#endif
	return ret;
}

static const struct of_device_id sunplus_pwm_dt_ids[] = {
	{ .compatible = "sunplus,sp7021-pwm", },
	{ .compatible = "sunplus,q645-pwm", },
	{ .compatible = "sunplus,sp7350-pwm", },
	{}
};
MODULE_DEVICE_TABLE(of, sunplus_pwm_dt_ids);

#ifdef CONFIG_PM
static int __maybe_unused sunplus_pwm_suspend(struct device *dev)
{
	return pm_runtime_force_suspend(dev);
}

static int __maybe_unused sunplus_pwm_resume(struct device *dev)
{
	return pm_runtime_force_resume(dev);
}

static int __maybe_unused sunplus_pwm_runtime_suspend(struct device *dev)
{
	clk_disable(devm_clk_get(dev, NULL));
	return 0;
}

static int __maybe_unused sunplus_pwm_runtime_resume(struct device *dev)
{
	return clk_enable(devm_clk_get(dev, NULL));
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops sunplus_pwm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sunplus_pwm_suspend, sunplus_pwm_resume)
	SET_RUNTIME_PM_OPS(sunplus_pwm_runtime_suspend,
				sunplus_pwm_runtime_resume, NULL)
};
#endif

static struct platform_driver sunplus_pwm_driver = {
	.probe		= sunplus_pwm_probe,
	.remove		= sunplus_pwm_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sunplus_pwm_dt_ids,
#ifdef CONFIG_PM
		.pm		= &sunplus_pwm_pm_ops,
#endif
	},
};
module_platform_driver(sunplus_pwm_driver);

MODULE_DESCRIPTION(DESC_NAME);
MODULE_AUTHOR("Hammer Hsieh <hammer.hsieh@sunplus.com>");
MODULE_LICENSE("GPL v2");
