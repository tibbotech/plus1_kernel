// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/kernel.h>

#define STC_PRESCALER	0x0C

/* 32-bit counter */
#define TIMER_CTRL	0x24
#define TIMER_CNT	0x28
#define TIMER_RELOAD	0x2C

#define REG_WIDTH	0x0C
/* 64-bit counter */
#define TIMER_CNT_L	0x4C
#define TIMER_CNT_H	0x50
#define TIMER_RELOAD_L	0x54
#define TIMER_RELOAD_H	0x58

#define NUM_REG		7
#define NUM_IRQ		28

#define STC_CLK		1000000

#define TIMER_TEST_FUNC_DEBUG
#ifdef TIMER_TEST_FUNC_DEBUG
#define DBG_INFO(fmt, args ...)	pr_info("K_TIMER_TEST: " fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif

struct test_timer_dev {
	void __iomem *base;
	int irqn;
};

struct test_timer_dev *timer_priv_data;
static void __iomem *base[NUM_REG];
static int irqn[NUM_IRQ];
static volatile int occur_irq = 0;

static irqreturn_t sp_timer_test_isr(int irq, void *dev_id)
{
	DBG_INFO("%d\n", irq);
	occur_irq++;
	return IRQ_HANDLED;
}

static void test_help(void)
{
	DBG_INFO(
		"sp_timer test:\n"
		"  echo [stc_idx] [timer_idx] > /sys/module/sp_timer_test/parameters/test\n"
		"  stc index:\n"
		"    0: STC TIMESTAMP\n"
		"    1: STC AV3\n"
		"    2: STC \n"
		"    3: STC AV0\n"
		"    4: STC AV1\n"
		"    5: STC AV2\n"
		"    6: STC AV4\n"
		"  timer index:\n"
		"    0:  timer0\n"
		"    1:  timer1\n"
		"    2:  timer2\n"
		"    3:  timer3\n"
	);
}

void timer_test(int stc_idx, int timer_idx)
{
	u32 val, sysclk;

	struct test_timer_dev *priv = timer_priv_data;

	DBG_INFO("stc %d timer %d test start!\n", stc_idx, timer_idx);

	/* priv data init */
	priv->base = base[stc_idx];
	priv->irqn = irqn[stc_idx * 4 + timer_idx];

	/* reset the timer*/
	val = 0;
	writel(val, priv->base + TIMER_CTRL + timer_idx * REG_WIDTH);

	/* STC clk */
	if(stc_idx < 2)
		sysclk = 500000000;//MAINDOMAIN BUS freq
	else
		sysclk = 25000000;//AODOMAIN BUS freq

	val = sysclk / STC_CLK - 1;
	writel(val, priv->base + STC_PRESCALER);

	/* timer cnt: set 1s */
	val = STC_CLK;
	if(unlikely(timer_idx == 3)) { /* 64-bit counter */
		writel(val, priv->base + TIMER_CNT_L);
		writel(0, priv->base + TIMER_CNT_H);
		writel(val, priv->base + TIMER_RELOAD_L);
		writel(0, priv->base + TIMER_RELOAD_H);
	} else {/* 32-bit counter */
		writel(val, priv->base + TIMER_CNT + timer_idx * REG_WIDTH);
		writel(val, priv->base + TIMER_RELOAD + timer_idx * REG_WIDTH);
	}

	/* timer ctrl: stc src, repeat, go */
	val = (1 << 14) | (1 << 13) | (1 << 11);
	writel(val, priv->base + TIMER_CTRL + timer_idx * REG_WIDTH);

	/* trigger twice and end test*/
	while(1) {
		if (occur_irq == 2) {
			occur_irq = 0;
			val = 0;
			writel(val, priv->base + TIMER_CTRL + timer_idx * REG_WIDTH);
			DBG_INFO("stc %d timer %d paused and test done!\n", stc_idx, timer_idx);
			break;
		}
	}
}

static int test_set(const char *val, const struct kernel_param *kp)
{
	int stc_idx, timer_idx;
	u32 cnt;

	cnt = sscanf(val, "%d %d", &stc_idx, &timer_idx);

	switch (cnt) {
	case 0:
	case 1:

		break;
	case 2:
		if (stc_idx > 6 || timer_idx > 3)
			test_help();
		else
			timer_test(stc_idx, timer_idx);
	break;
	}
	return 0;
}

static const struct kernel_param_ops test_ops = {
	.set = test_set,
};

module_param_cb(test, &test_ops, NULL, 0600);

static const struct of_device_id sp_timer_test_of_match[] = {
	#ifdef CONFIG_SOC_SP7350
	{ .compatible = "sunplus,sp7350-timer-test" },
	#endif
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_timer_test_of_match);

static int sp_timer_test_probe(struct platform_device *pdev)
{
	struct sp_timer_test_dev *dev;
	struct resource *res_mem, *res_irq;
	int i, ret = 0;

	//DBG_INFO(">>>>>>>>>>>>>>>>entry timer test probe\n");

	timer_priv_data = devm_kzalloc(&pdev->dev, sizeof(struct test_timer_dev), GFP_KERNEL);
	if (!timer_priv_data)
	{
		ret = -ENOMEM;
		goto fail_kmalloc;
	}
#if 0
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_timestamp");
	sp_timer_test->stc_timestamp = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_timestamp))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_timestamp);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_av3");
	sp_timer_test->stc_av3 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_av3))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_av3);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc");
	sp_timer_test->stc = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_av0");
	sp_timer_test->stc_av0 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_av0))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_av0);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_av1");
	sp_timer_test->stc_av1 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_av1))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_av1);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_av2");
	sp_timer_test->stc_av2 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_av2))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_av2);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stc_av4");
	sp_timer_test->stc_av4 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_timer_test->stc_av4))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_timer_test->stc_av4);
	}
#endif


	for (i = 0; i < NUM_REG; i++) {
		res_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		base[i] = devm_ioremap_resource(&pdev->dev, res_mem);
		if (IS_ERR(base[i]))
		{
			dev_err(&pdev->dev, "ioremap fail\n");
			return PTR_ERR(base[i]);
		}
	}

	for (i = 0; i < NUM_IRQ; i++) {


		res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res_irq) {
			DBG_INFO("%s:%d\n", __func__, __LINE__);
			return -ENODEV;
		}
		irqn[i] = platform_get_irq(pdev, i);
		//DBG_INFO("irq_number=%d\n", irq_number);
		ret = devm_request_irq(&pdev->dev, irqn[i], sp_timer_test_isr, 0, "sp_timer_test", dev);
		if (ret) {
			DBG_INFO("%s:%d\n", __func__, __LINE__);
			return -ENODEV;
		}
		//DBG_INFO("%s:%d %d\n", __func__, __LINE__, ret);
	}
	//DBG_INFO(">>>>>>>>>>>>>>>>exit timer test probe\n");
	return ret;

	fail_kmalloc:
	return ret;
}

static struct platform_driver sp_timer_test_driver = {
	.probe		= sp_timer_test_probe,
	.driver		= {
		#ifdef CONFIG_SOC_SP7350
		.name	= "sp7350_timer_test",
		#endif
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_timer_test_of_match),
	},
};

module_platform_driver(sp_timer_test_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus timer test driver");
