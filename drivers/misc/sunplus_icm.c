// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Sunplus SP7021 SoC ICM(Input Capture Module) driver
 *
 * Author:	Hammer Hsieh <hammer.hsieh@sunplus.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <misc/sunplus_icm.h>

#define NUM_ICM 4

//#define TRACE(s) printk("### %s:%d %s\n", __FUNCTION__, __LINE__, s)
#define TRACE(s)

#define RPM_GET(dev) \
do { \
	TRACE("RPM_GET"); \
	pm_runtime_get_sync(dev); \
} while (0)

#define RPM_PUT(dev) \
do { \
	TRACE("RPM_PUT"); \
	pm_runtime_put(dev); \
} while (0)

/*
 * cfg0
 */
#define ICM_ENABLE		0x00010001	/* enable icm */
#define ICM_DISABLE		0x00010000	/* disable icm */
#define ICM_RELOAD		0x00020002	/* reload icm settings */
#define ICM_INTCLR		0x00040004	/* clear icm interrupt */

#define MUXSEL_OFS	3
#define MUXSEL_BITS	3
#define CLKSEL_OFS	6
#define CLKSEL_BITS	3

/*
 * cfg1
 */
#define EEMODE_OFS	0
#define EEMODE_BITS	2
#define ETIMES_OFS	2
#define ETIMES_BITS	4
#define DTIMES_OFS	6
#define DTIMES_BITS	3

#define ICM_FCLEAR	0x20002000
#define ICM_FMASK	(ICM_FDDROP | ICM_FEMPTY | ICM_FFULL)

#define ICM_MSK(field)  (((1 << field##_BITS) - 1) << field##_OFS)

#define ICM_SETCFG(_cfg, field, val) \
	(icm->cfg##_cfg = (((val) << field##_OFS) | (ICM_MSK(field) << 16)))

#define ICM_GETCFG(_cfg, field) \
	(((icm->cfg##_cfg) & ICM_MSK(field)) >> field##_OFS)

/*
 * cfg0
 * u32 enable:1;
 * u32 reload:1; reload setting
 * u32 intclr:1; write 1 clear interrupt
 * u32 muxsel:3; select input signal source
 * u32 clksel:3; select clock source for counter
 * u32 rsv0:7;
 * u32 msk0:16;
 *
 * cfg1
 * u32 eemode:2; edge mode: 0 rising / 1 falling / 2 both
 * u32 etimes:4; event times (0~15)
 * u32 dtimes:3; debounce times (0~7)
 * u32 rsv1:3;
 * u32 fddrop:1; fifo data drop
 * u32 fclear:1; fifo clear
 * u32 fempty:1; fifo empty
 * u32 ffull :1; fifo full
 * u32 msk1:16;
 *
 * u32 cntscl; counter clock prescaler:
 *     cnt_clk = ext_clk / (cnt_scl + 1)
 * u32 tstscl; test signal clock prescaler:
 *     tst_clk = sysclk / (tst_scl + 1)
 * u32 cnt; counter, read from fifo
 *
 * u32 pwh; pulse width high
 * u32 pwl; pulse width low
 */
struct sunplus_icm_reg {
	u32 cfg0;
	u32 cfg1;

	u32 cntscl;
	u32 tstscl;
	u32 cnt;

	u32 pwh;
	u32 pwl;
};

struct sunplus_icm_dev {
	struct sunplus_icm_reg *reg;
	int irq;
	struct clk *clk;
	struct device *dev;
};

static struct sunplus_icm_dev sunplus_icm;
static sunplus_icm_cbf cbfs[NUM_ICM];

static irqreturn_t sunplus_icm_isr(int irq, void *dev_id)
{
	int i = irq - sunplus_icm.irq;
	struct sunplus_icm_reg *icm = &sunplus_icm.reg[i];
	u32 cnt, fstate;
	int clk_bk;
	//TRACE("");
	clk_bk = icm->cfg0 & 0x000001C0;
	icm->cfg0 = 0x01C00100;
	icm->cfg0 = ICM_INTCLR; /* clear interrupt */
	icm->cfg0 = 0x01C00000 | clk_bk;

	while (!((fstate = icm->cfg1) & ICM_FEMPTY)) { /* fifo not empty */
		cnt = icm->cnt; /* read counter from fifo */
		if (cbfs[i])
			(*cbfs[i])(i, cnt, fstate & ICM_FMASK); /* callback */
	}

	return IRQ_HANDLED;
}

int sunplus_icm_setcfg(int i, struct sunplus_icm_cfg *cfg)
{
	struct sunplus_icm_reg *icm;

	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	RPM_GET(sunplus_icm.dev);
	icm = &sunplus_icm.reg[i];
	ICM_SETCFG(0, MUXSEL, cfg->muxsel);
	ICM_SETCFG(0, CLKSEL, cfg->clksel);
	ICM_SETCFG(1, EEMODE, cfg->eemode);
	ICM_SETCFG(1, ETIMES, cfg->etimes);
	ICM_SETCFG(1, DTIMES, cfg->dtimes);
	icm->cntscl = cfg->cntscl;
	icm->tstscl = cfg->tstscl;
	RPM_PUT(sunplus_icm.dev);

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_setcfg);

int sunplus_icm_getcfg(int i, struct sunplus_icm_cfg *cfg)
{
	struct sunplus_icm_reg *icm;

	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	RPM_GET(sunplus_icm.dev);
	icm = &sunplus_icm.reg[i];
	cfg->muxsel = ICM_GETCFG(0, MUXSEL);
	cfg->clksel = ICM_GETCFG(0, CLKSEL);
	cfg->eemode = ICM_GETCFG(1, EEMODE);
	cfg->etimes = ICM_GETCFG(1, ETIMES);
	cfg->dtimes = ICM_GETCFG(1, DTIMES);
	cfg->cntscl = icm->cntscl;
	cfg->tstscl = icm->tstscl;
	RPM_PUT(sunplus_icm.dev);

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_getcfg);

int sunplus_icm_reload(int i)
{
	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	RPM_GET(sunplus_icm.dev);
	sunplus_icm.reg[i].cfg0 = ICM_RELOAD;
	RPM_PUT(sunplus_icm.dev);

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_reload);

int sunplus_icm_enable(int i, sunplus_icm_cbf cbf)
{
	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	cbfs[i] = cbf;
	RPM_GET(sunplus_icm.dev);
	sunplus_icm.reg[i].cfg0 = ICM_ENABLE;

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_enable);

int sunplus_icm_disable(int i)
{
	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	sunplus_icm.reg[i].cfg0 = ICM_DISABLE;
	sunplus_icm.reg[i].cfg1 = ICM_FCLEAR; /* clear fifo */
	RPM_PUT(sunplus_icm.dev);
	cbfs[i] = NULL;

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_disable);

int sunplus_icm_fstate(int i, u32 *fstate)
{
	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	RPM_GET(sunplus_icm.dev);
	*fstate = sunplus_icm.reg[i].cfg1 & ICM_FMASK;
	RPM_PUT(sunplus_icm.dev);

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_fstate);

int sunplus_icm_pwidth(int i, u32 *pwh, u32 *pwl)
{
	if (i < 0 || i >= NUM_ICM)
		return -EINVAL;

	RPM_GET(sunplus_icm.dev);
	*pwh = sunplus_icm.reg[i].pwh;
	*pwl = sunplus_icm.reg[i].pwl;
	RPM_PUT(sunplus_icm.dev);

	return 0;
}
EXPORT_SYMBOL(sunplus_icm_pwidth);

#ifdef CONFIG_SUNPLUS_ICM_TEST /* test & example */
static u32 tscnt; /* test signal counter */

static void test_cbf(int i, u32 cnt, u32 fstate)
{
	u32 pwh, pwl;

	sunplus_icm_pwidth(i, &pwh, &pwl);
	pr_info("icm%d_%05u: %10u %04x %u %u\n", i, ++tscnt, cnt, fstate,
			pwh, pwl);
}

static void test_help(void)
{
	pr_info(
		"sunplus_icm test:\n"
		"  echo <icm:0~3> [function] [params...]\n"
		" > /sys/module/sunplus_icm/parameters/test\n"
		"  * if no function & params, dump icm cfg & state\n"
		"  function:\n"
		"    0: disable icm, no params\n"
		"    1: enable icm\n"
		"    2: reload icm\n"
		"  params in order:\n"
		"    muxsel, clksel, eemode, etimes, dtimes,\n"
		"    cntscl, tstscl, tstime(ms)\n"
		"    * -1 means no change\n"
	);
}

static int test_set(const char *val, const struct kernel_param *kp)
{
	int i, f, muxsel, clksel, eemode, etimes, dtimes;
	int cntscl, tstscl, tstime;
	u32 fstate, pwh, pwl;
	struct sunplus_icm_cfg cfg;
	int ret;

	i = f = muxsel = clksel = eemode = etimes = dtimes
		= cntscl = tstscl = tstime = -1;
	ret = sscanf(val, "%d %d %d %d %d %d %d %d %d %d",
		&i, &f, &muxsel, &clksel, &eemode, &etimes,
		&dtimes, &cntscl, &tstscl, &tstime);

	if (i >= 0 && i < NUM_ICM) {
		switch (f) {
		case 0: /* disable */
disable:
			sunplus_icm_disable(i);
			pr_info("icm%d: tscnt = %u\n", i, tscnt);
			tscnt = 0;
			return 0;

		case 1: /* enable */
		case 2: /* reload */
			sunplus_icm_getcfg(i, &cfg);
			if (muxsel != -1)
				cfg.muxsel = (u32)muxsel;
			if (clksel != -1)
				cfg.clksel = (u32)clksel;
			if (eemode != -1)
				cfg.eemode = (u32)eemode;
			if (etimes != -1)
				cfg.etimes = (u32)etimes;
			if (dtimes != -1)
				cfg.dtimes = (u32)dtimes;
			if (cntscl != -1)
				cfg.cntscl = (u32)cntscl;
			if (tstscl != -1)
				cfg.tstscl = (u32)tstscl;
			sunplus_icm_setcfg(i, &cfg);
			if (f == 1) {
				sunplus_icm_enable(i, test_cbf);
				if (tstime > 0) {
					mdelay(tstime);
					goto disable;
				}
			} else
				sunplus_icm_reload(i);
			return 0;

		case -1:
			sunplus_icm_getcfg(i, &cfg);
			sunplus_icm_fstate(i, &fstate);
			sunplus_icm_pwidth(i, &pwh, &pwl);
			pr_info("sunplus_icm%d cfg & state:", i);
			pr_info("muxsel: %u", cfg.muxsel);
			pr_info("clksel: %u", cfg.clksel);
			pr_info("eemode: %u", cfg.eemode);
			pr_info("etimes: %u", cfg.etimes);
			pr_info("dtimes: %u", cfg.dtimes);
			pr_info("cntscl: %u", cfg.cntscl);
			pr_info("tstscl: %u", cfg.tstscl);
			pr_info("fstate: %04x", fstate);
			pr_info("pwidth: %u %u\n", pwh, pwl);
			return 0;
		}
	}

	test_help();
	return 0;
}

static const struct kernel_param_ops test_ops = {
	.set = test_set,
};
module_param_cb(test, &test_ops, NULL, 0600);
#endif


static const struct of_device_id sunplus_icm_of_match[] = {
	{ .compatible = "sunplus,sp7021-icm" },
	{},
};
MODULE_DEVICE_TABLE(of, sunplus_icm_of_match);

static int sunplus_icm_probe(struct platform_device *pdev)
{
	struct sunplus_icm_dev *dev = &sunplus_icm;
	struct resource *res_mem, *res_irq;
	int i = 0;
	int ret = 0;

	TRACE("");
	dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dev->clk)) {
		dev_err(&pdev->dev, "not found clk source\n");
		return PTR_ERR(dev->clk);
	}

	ret = clk_prepare_enable(dev->clk);
	if (ret)
		return ret;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	dev->reg = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR((void *)dev->reg))
		return PTR_ERR((void *)dev->reg);

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq)
		return -ENODEV;

	dev->irq = res_irq->start;
	platform_set_drvdata(pdev, dev);

	dev->dev = &pdev->dev;
	pm_runtime_set_active(dev->dev);
	pm_runtime_enable(dev->dev);
	RPM_GET(dev->dev);
	RPM_PUT(dev->dev);

	while (i < NUM_ICM) {
		ret = devm_request_irq(&pdev->dev, dev->irq + i, sunplus_icm_isr,
			IRQF_TRIGGER_RISING, "sunplus_icm", dev);
		if (ret)
			return -ENODEV;
		i++;
	}

	return ret;
}

static int __maybe_unused sunplus_icm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq = platform_get_irq(pdev, 0);
	struct irq_data *data = irq_get_irq_data(irq);

	if (!irqd_is_wakeup_set(data))
		return pm_runtime_force_suspend(dev);

	return 0;
}

static int __maybe_unused sunplus_icm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq = platform_get_irq(pdev, 0);
	struct irq_data *data = irq_get_irq_data(irq);

	if (!irqd_is_wakeup_set(data))
		return pm_runtime_force_resume(dev);

	return 0;
}

static int __maybe_unused sunplus_icm_runtime_suspend(struct device *dev)
{
	TRACE("");
	clk_disable(sunplus_icm.clk);
	return 0;
}

static int __maybe_unused sunplus_icm_runtime_resume(struct device *dev)
{
	TRACE("");
	clk_enable(sunplus_icm.clk);
	return 0;
}

static const struct dev_pm_ops sunplus_icm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sunplus_icm_suspend, sunplus_icm_resume)
	SET_RUNTIME_PM_OPS(sunplus_icm_runtime_suspend, sunplus_icm_runtime_resume, NULL)
};

static struct platform_driver sunplus_icm_driver = {
	.probe		= sunplus_icm_probe,
	.driver		= {
		.name	= "sunplus_icm",
		.owner	= THIS_MODULE,
		.pm     = &sunplus_icm_pm_ops,
		.of_match_table = of_match_ptr(sunplus_icm_of_match),
	},
};

module_platform_driver(sunplus_icm_driver);

MODULE_DESCRIPTION("Sunplus ICM(Input Capture) driver");
MODULE_AUTHOR("Hammer Hsieh <hammer.hsieh@sunplus.com>");
MODULE_LICENSE("GPL v2");
