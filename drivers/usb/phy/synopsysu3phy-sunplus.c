// SPDX-License-Identifier: GPL-2.0

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/usb/sp_usb.h>
#include <linux/reset.h>

struct u3phy_regs {
	unsigned int cfg[32];		       // 189.0
};

struct u3c_regs {
	unsigned int cfg[32];		       // 189.0
};

struct usb3_phy {
	struct device		*dev;
	void __iomem		*u3phy_base_addr;
	void __iomem		*u3_portsc_addr;
	struct clk		*u3_clk;
	struct clk		*u3phy_clk;
	struct reset_control	*u3phy_rst;
	int			irq;
	wait_queue_head_t	wq;
	int			busy;
	struct delayed_work	typecdir;
	int			dir;
	struct gpio_desc	*gpiodir;
};

static irqreturn_t u3phy_int(int irq, void *dev)
{
	struct usb3_phy *u3phy = dev;
	struct u3phy_regs *dwc3phy_reg = u3phy->u3phy_base_addr;
	unsigned int result;

	result = readl(&dwc3phy_reg->cfg[2]);
	u3phy->busy = 0;
	//printk("cfg[2] 0x%x\n", result);
	writel(result | 0x1, &dwc3phy_reg->cfg[2]);
	wake_up(&u3phy->wq);
	return IRQ_HANDLED;
}

static void typec_gpio(struct work_struct *work)
{
	struct usb3_phy *u3phy = container_of(work, struct usb3_phy, typecdir.work);

	if (u3phy->dir != gpiod_get_value(u3phy->gpiodir)) {
		struct u3phy_regs *dwc3phy_reg;
		struct u3c_regs *dwc3portsc_reg;
		unsigned int result;

		dwc3phy_reg = (struct u3phy_regs *) u3phy->u3phy_base_addr;
		dwc3portsc_reg = (struct u3c_regs *) u3phy->u3_portsc_addr;

		result = readl(&dwc3portsc_reg->cfg[0]);
		writel(result | 0x2, &dwc3portsc_reg->cfg[0]);

		result = readl(&dwc3phy_reg->cfg[5]) & 0xffe0;
		if (gpiod_get_value(u3phy->gpiodir)) {
			writel(result | 0x15, &dwc3phy_reg->cfg[5]);
			u3phy->busy = 1;
			result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
			//if (!result)
			//	dev_dbg(u3phy->dev, "reset failed 3\n");
				//return -ETIME;
			u3phy->dir = 1;
		} else {
			writel(result | 0x11, &dwc3phy_reg->cfg[5]);
			u3phy->busy = 1;
			result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
			//if (!result)
			//	dev_dbg(u3phy->dev, "reset failed 4\n");
				//return -ETIME;
			u3phy->dir = 0;
		}
		result = readl(&dwc3portsc_reg->cfg[0]) & ~((0xf<<5) | (0x1<<16));
		writel(result | (0x1<<16) | (0x5<<5), &dwc3portsc_reg->cfg[0]);
	}
	schedule_delayed_work(&u3phy->typecdir, msecs_to_jiffies(10));
}
#if 0
static void synopsys_u3phy_init(struct platform_device *pdev)
{
	struct usb3_phy *u3phy = platform_get_drvdata(pdev);
	struct u3phy_regs *dwc3phy_reg;
	unsigned int result;

	clk_prepare_enable(u3phy->u3_clk);
	clk_prepare_enable(u3phy->u3phy_clk);

	reset_control_assert(u3phy->u3phy_rst);
	mdelay(1);
	reset_control_deassert(u3phy->u3phy_rst);

	dwc3phy_reg = (struct u3phy_regs *) u3phy->u3phy_base_addr;

	result = readl(&dwc3phy_reg->cfg[1]);
	writel(result | 0x3, &dwc3phy_reg->cfg[1]);
	u3phy->busy = 1;
	result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
	//if (!result)
	//	dev_dbg(dev, "reset failed 1\n");
		//return -ETIME;

	result = readl(&dwc3phy_reg->cfg[5]) & 0xFFE0;
	writel(result | 0x15, &dwc3phy_reg->cfg[5]);
	u3phy->busy = 1;
	result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
	//if (!result)
	//	dev_dbg(dev, "reset failed 2\n");
		//return -ETIME;
	u3phy->dir = 1;
}
#endif
static int sp_u3phy_init(struct phy *phy)
{
	struct usb3_phy *u3phy = phy_get_drvdata(phy);

	INIT_DELAYED_WORK(&u3phy->typecdir, typec_gpio);
	schedule_delayed_work(&u3phy->typecdir, msecs_to_jiffies(100));
	return 0;
}

static int sp_u3phy_power_on(struct phy *phy)
{
	struct usb3_phy *u3phy = phy_get_drvdata(phy);
	struct u3phy_regs *dwc3phy_reg;
	unsigned int result;

	clk_prepare_enable(u3phy->u3_clk);
	clk_prepare_enable(u3phy->u3phy_clk);

	reset_control_assert(u3phy->u3phy_rst);
	mdelay(1);
	reset_control_deassert(u3phy->u3phy_rst);

	dwc3phy_reg = (struct u3phy_regs *) u3phy->u3phy_base_addr;

	result = readl(&dwc3phy_reg->cfg[1]);
	writel(result | 0x3, &dwc3phy_reg->cfg[1]);
	u3phy->busy = 1;
	result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
	//if (!result)
	//	dev_dbg(dev, "reset failed 1\n");
		//return -ETIME;

	result = readl(&dwc3phy_reg->cfg[5]) & 0xFFE0;
	writel(result | 0x15, &dwc3phy_reg->cfg[5]);
	u3phy->busy = 1;
	result = wait_event_timeout(u3phy->wq, !u3phy->busy, msecs_to_jiffies(100));
	//if (!result)
	//	dev_dbg(dev, "reset failed 2\n");
		//return -ETIME;
	u3phy->dir = 1;

	return 0;
}

static int sp_u3phy_power_off(struct phy *phy)
{
	struct usb3_phy *u3phy = phy_get_drvdata(phy);

	clk_disable_unprepare(u3phy->u3phy_clk);
	return 0;
}

static int sp_u3phy_exit(struct phy *phy)
{
	struct usb3_phy *u3phy = phy_get_drvdata(phy);

	cancel_delayed_work_sync(&u3phy->typecdir);
	return 0;
}

static const struct phy_ops sp_u3phy_ops = {
	.init		= sp_u3phy_init,
	.power_on	= sp_u3phy_power_on,
	.power_off	= sp_u3phy_power_off,
	.exit		= sp_u3phy_exit,
};

static int sunplus_usb_synopsys_u3phy_probe(struct platform_device *pdev)
{
	u32 ret;
	struct resource *u3phy_res_mem;
	struct device *dev = &pdev->dev;
	struct usb3_phy *u3phy;
	struct phy_provider *phy_provider;
	struct phy *phy;

	dev_info(dev, "%s\n", __func__);
	u3phy = devm_kzalloc(dev, sizeof(*u3phy), GFP_KERNEL);
	if (!u3phy)
		return -ENOMEM;
	u3phy->dev = dev;
	platform_set_drvdata(pdev, u3phy);

	/*enable u3 system clock*/
	u3phy->u3_clk = devm_clk_get(&pdev->dev, "clkc_u3");
	if (IS_ERR(u3phy->u3_clk)) {
		dev_err(dev, "not found clk source\n");
		return PTR_ERR(u3phy->u3_clk);
	}
	clk_prepare_enable(u3phy->u3_clk);
	/*enable uphy3 system clock*/

	u3phy->u3phy_clk = devm_clk_get(&pdev->dev, "clkc_u3phy");
	if (IS_ERR(u3phy->u3phy_clk)) {
		dev_err(dev, "not found clk source\n");
		return PTR_ERR(u3phy->u3phy_clk);
	}

	u3phy->u3phy_rst = devm_reset_control_get(&pdev->dev, "rstc_u3phy");
	if (IS_ERR(u3phy->u3phy_rst)) {
		dev_err(dev, "not found u3phy reset source\n");
		return PTR_ERR(u3phy->u3phy_rst);
	}

	//phy3 settings
	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	u3phy->u3phy_base_addr = devm_ioremap_resource(&pdev->dev, u3phy_res_mem);
	if (IS_ERR(u3phy->u3phy_base_addr))
		return PTR_ERR(u3phy->u3phy_base_addr);

	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	u3phy->u3_portsc_addr = devm_ioremap(&pdev->dev, u3phy_res_mem->start, resource_size(u3phy_res_mem));
	if (IS_ERR(u3phy->u3_portsc_addr))
		return PTR_ERR(u3phy->u3_portsc_addr);

	u3phy->irq = platform_get_irq(pdev, 0);
	init_waitqueue_head(&u3phy->wq);
	if (u3phy->irq <= 0) {
		pr_info("ERR\n");
		return -EINVAL;
	}
	ret = devm_request_irq(dev, u3phy->irq, u3phy_int, IRQF_SHARED, pdev->name, u3phy);
	if (ret) {
		dev_dbg(u3phy->dev, "usb3 phy: unable to register IRQ(%d)\n", u3phy->irq);
		free_irq(u3phy->irq, u3phy);
	}

	u3phy->gpiodir = devm_gpiod_get(&pdev->dev, "typec", GPIOD_IN);
	if (IS_ERR(u3phy->gpiodir)) {
		dev_err(dev, "could not get type C gpio: %ld", PTR_ERR(u3phy->gpiodir));
		return PTR_ERR(u3phy->gpiodir);
	}

	//INIT_DELAYED_WORK(&u3phy->typecdir, typec_gpio);
	//synopsys_u3phy_init(pdev);
	//schedule_delayed_work(&u3phy->typecdir, msecs_to_jiffies(100));
	phy = devm_phy_create(&pdev->dev, NULL, &sp_u3phy_ops);
	if (IS_ERR(phy)) {
		ret = -PTR_ERR(phy);
		return ret;
	}

	phy_set_drvdata(phy, u3phy);
	phy_provider = devm_of_phy_provider_register(&pdev->dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int sunplus_usb_synopsys_u3phy_remove(struct platform_device *pdev)
{
	struct usb3_phy *u3phy = platform_get_drvdata(pdev);

	devm_iounmap(&pdev->dev, u3phy->u3phy_base_addr);
	devm_iounmap(&pdev->dev, u3phy->u3_portsc_addr);
	/*disable uphy2/3 system clock*/
	clk_disable_unprepare(u3phy->u3_clk);
	clk_disable_unprepare(u3phy->u3phy_clk);
	free_irq(u3phy->irq, u3phy_int);
	cancel_delayed_work_sync(&u3phy->typecdir);

	return 0;
}
static const struct of_device_id synopsys_u3phy_sunplus_dt_ids[] = {
	{ .compatible = "sunplus,usb3-phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, synopsys_u3phy_sunplus_dt_ids);

static struct platform_driver sunplus_synopsys_u3phy_driver = {
	.probe		= sunplus_usb_synopsys_u3phy_probe,
	.remove		= sunplus_usb_synopsys_u3phy_remove,
	.driver		= {
		.name	= "sunplus-usb-synopsys-u3phy",
		.of_match_table = synopsys_u3phy_sunplus_dt_ids,
	},
};
module_platform_driver(sunplus_synopsys_u3phy_driver);

MODULE_ALIAS("sunplus_usb_synpsys_u3phy");
MODULE_AUTHOR("Sunplus Technology Inc.");
MODULE_LICENSE("GPL");
