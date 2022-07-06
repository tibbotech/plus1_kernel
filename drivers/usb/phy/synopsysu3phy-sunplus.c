#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/usb/sp_usb.h>
#include <linux/nvmem-consumer.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>

static struct resource *u3phy_res_mem;
#if 0
static struct resource *moon_res_mem;
void __iomem *moon_base_addr;
struct moon0_regs {
	unsigned int stamp;            // 0.0
	unsigned int clken[5];         // 0.1 - 0.5
	unsigned int rsvd_1[5]; 	   	   // 0.6 - 0.10
	unsigned int gclken[5];        // 0.11
	unsigned int rsvd_2[5]; 	   	   // 0.16 - 0.20
	unsigned int reset[5];         // 0.21
	unsigned int rsvd_3[5];          // 0.26 - 030
	unsigned int hw_cfg;           // 0.31

	unsigned int cfg1[32];		       // 189.0
	unsigned int cfg2[32];		       // 189.0
	unsigned int cfg3[32];		       // 189.0
};
#endif
struct uphy_u3_regs {
	unsigned int cfg[32];		       // 189.0
};
//struct u3_portsc_regs {
//	unsigned int cfg[4];		       // 189.0
//};

//struct u3_test_regs {
//	unsigned int cfg[4];		       // 189.0
//};

struct usb3_phy {
	struct device		*dev;
	void __iomem 		*u3phy_base_addr;
	//void __iomem 		*u3_portsc_addr;
	//void __iomem 		*u3_test_addr;
	struct clk 		*u3_clk;
	struct clk 		*u3phy_clk;
	struct reset_control 	*u3phy_rst;
	int			irq;
	wait_queue_head_t 	wq;
	int			busy;
	//struct work_struct	typecdir;
	struct delayed_work	typecdir;
	int			dir;
};
#if 1
static irqreturn_t u3phy_int(int irq, void *dev)
{
	struct usb3_phy *u3phy = dev;
	struct uphy_u3_regs *dwc3phy_reg = u3phy->u3phy_base_addr;
	unsigned int result;

	result = readl(&dwc3phy_reg->cfg[2]);
	u3phy->busy = 0;
	//printk("cfg[2] 0x%x\n", result);
	writel(result | 0x1, &dwc3phy_reg->cfg[2]);
	wake_up(&u3phy->wq);
	return IRQ_HANDLED;
}
#endif

static void typec_gpio(struct work_struct *work)
{
	struct usb3_phy *u3phy = container_of(work, struct usb3_phy, typecdir.work);
	//struct usb3_phy *u3phy = container_of(work, struct usb3_phy, typecdir);
	volatile uint32_t *dwc3portsc_reg = ioremap(0xf80a1430, 32);
	volatile uint32_t *dwc3test_reg = ioremap(0xf80ad164, 32);
	struct uphy_u3_regs *dwc3phy_reg;
	unsigned int result;

	dwc3phy_reg = (struct uphy_u3_regs *) u3phy->u3phy_base_addr;

	if (u3phy->dir != gpio_get_value(98)) {
		result = readl(dwc3portsc_reg);
		writel(result | 0x2, dwc3portsc_reg);

		result = readl(&dwc3phy_reg->cfg[5]) & 0xffe0;
		if (gpio_get_value(98)) {
			writel(result | 0x15, &dwc3phy_reg->cfg[5]);
			u3phy->busy = 1;
			wait_event(u3phy->wq, !u3phy->busy);
			u3phy->dir = 1;
		} else {
			writel(result | 0x11, &dwc3phy_reg->cfg[5]);
			u3phy->busy = 1;
			wait_event(u3phy->wq, !u3phy->busy);
			u3phy->dir = 0;
		}
		result = readl(dwc3portsc_reg) & ~((0xf<<5) |(0x1<<16));
		writel(result | (0x1<<16) | (0x5<<5), dwc3portsc_reg);
		iounmap(dwc3portsc_reg);
		iounmap(dwc3test_reg);
	}
	schedule_delayed_work(&u3phy->typecdir, msecs_to_jiffies(1));
	//schedule_work(&u3phy->typecdir);
}

static void synopsys_u3phy_init(struct platform_device *pdev)
{
	struct usb3_phy *u3phy = platform_get_drvdata(pdev);
	struct uphy_u3_regs *dwc3phy_reg;// = (struct uphy_u3_regs *) u3phy_base_addr;
	unsigned int result;

	clk_prepare_enable(u3phy->u3_clk);
	clk_prepare_enable(u3phy->u3phy_clk);

	reset_control_assert(u3phy->u3phy_rst);
	mdelay(1);
	reset_control_deassert(u3phy->u3phy_rst);

	dwc3phy_reg = (struct uphy_u3_regs *) u3phy->u3phy_base_addr;

	result = readl(&dwc3phy_reg->cfg[1]);
	writel(result | 0x3, &dwc3phy_reg->cfg[1]);
	u3phy->busy = 1;
	wait_event(u3phy->wq, !u3phy->busy);

	result = readl(&dwc3phy_reg->cfg[5]) & 0xFFE0;

	writel(result | 0x15, &dwc3phy_reg->cfg[5]);
	u3phy->busy = 1;
	wait_event(u3phy->wq, !u3phy->busy);
	u3phy->dir = 1;
}

static int sunplus_usb_synopsys_u3phy_probe(struct platform_device *pdev)
{
	u32 ret;
#if 0
	s32 ret;
	u32 port_id = 1;

	usb_vbus_gpio[port_id] = of_get_named_gpio(pdev->dev.of_node, "vbus-gpio", 0);
	if ( !gpio_is_valid( usb_vbus_gpio[port_id]))
		printk(KERN_NOTICE "Wrong pin %d configured for vbus", usb_vbus_gpio[port_id]);
	ret = gpio_request( usb_vbus_gpio[port_id], "usb1-vbus");
	if ( ret < 0)
		printk(KERN_NOTICE "Can't get vbus pin %d", usb_vbus_gpio[port_id]);
	printk(KERN_NOTICE "%s,usb1_vbus_gpio_pin:%d\n",__FUNCTION__,usb_vbus_gpio[port_id]);
#endif
	struct device *dev = &pdev->dev;
	struct usb3_phy *u3phy;
	int typecdir;

	dev_dbg(dev, "%s\n", __func__);
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
	//clk_prepare_enable(u3phy->u3phy_clk);

	u3phy->u3phy_rst = devm_reset_control_get(&pdev->dev, "rstc_u3phy");
	if (IS_ERR(u3phy->u3phy_rst)) {
		dev_err(dev, "not found u3phy reset source\n");
		return PTR_ERR(u3phy->u3phy_rst);
	}
#if 0
	u3_rst = devm_reset_control_get(&pdev->dev, "rstc_u3");
	if (IS_ERR(u3_rst)) {
		dev_err(dev, "not found u3phy reset source\n");
		return PTR_ERR(u3_rst);
	}
#endif
    	//phy3 settings
    	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	//u3phy_base_addr = devm_ioremap(&pdev->dev, u3phy_res_mem->start, resource_size(u3phy_res_mem));
	u3phy->u3phy_base_addr = devm_ioremap_resource(&pdev->dev, u3phy_res_mem);
	if (IS_ERR(u3phy->u3phy_base_addr)) {
		return PTR_ERR(u3phy->u3phy_base_addr);
	}
#if 0
	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	//u3phy_base_addr = devm_ioremap(&pdev->dev, u3phy_res_mem->start, resource_size(u3phy_res_mem));
	u3phy->u3_portsc_addr = devm_ioremap_resource(&pdev->dev, u3phy_res_mem);
	if (IS_ERR(u3phy->u3_portsc_addr)) {
		return PTR_ERR(u3phy->u3_portsc_addr);
	}
	printk("u3 portsc addr 0x%lx", u3phy_res_mem->start);
	printk("u3 portsc addr 0x%lx", u3phy->u3_portsc_addr);

	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	//u3phy_base_addr = devm_ioremap(&pdev->dev, u3phy_res_mem->start, resource_size(u3phy_res_mem));
	u3phy->u3_test_addr = devm_ioremap_resource(&pdev->dev, u3phy_res_mem);
	if (IS_ERR(u3phy->u3_test_addr)) {
		return PTR_ERR(u3phy->u3_test_addr);
	}
	printk("u3 test addr 0x%lx", u3phy_res_mem->start);
	printk("u3 test addr 0x%lx", u3phy->u3_test_addr);
#endif
#if 0
	moon_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	moon_base_addr = devm_ioremap(&pdev->dev, moon_res_mem->start, resource_size(moon_res_mem));
	if (IS_ERR(moon_base_addr)) {
		return PTR_ERR(moon_base_addr);
	}
	//clk_disable(u3phy->u3_clk);
	volatile struct moon0_regs *moon_reg = (volatile struct moon0_regs *) moon_base_addr;
	int val;

	val = readl(&moon_reg->clken[3]);
	printk("moon clk 0x%x", val);
	clk_disable_unprepare(u3phy->u3_clk);
	val = readl(&moon_reg->clken[3]);
	printk("af moon clk 0x%x", val);
#endif
	u3phy->irq = platform_get_irq(pdev, 0);
	init_waitqueue_head(&u3phy->wq);
	if (u3phy->irq <= 0) {
		printk("ERR\n");
		return -EINVAL;
	}
	ret = devm_request_irq(dev, u3phy->irq, u3phy_int,
			       IRQF_SHARED, pdev->name, u3phy);//request_irq(u3phy->irq, u3phy_int, IRQF_SHARED, dev->name, u3phy);
	if (ret) {
		dev_dbg(u3phy->dev, "usb3 phy: unable to register IRQ(%d)\n", u3phy->irq);
		free_irq(u3phy->irq, u3phy);
	}
	//disable_irq(u3phy->irq);

	typecdir = of_get_named_gpio(pdev->dev.of_node, "typec-gpios", 0);
	if (!gpio_is_valid(typecdir))
		dev_err(dev, "Wrong pin %d configured for type c", typecdir);

	//enable_irq(u3phy->irq);
	//INIT_WORK(&u3phy->typecdir, typec_gpio);
	INIT_DELAYED_WORK(&u3phy->typecdir, typec_gpio);
	synopsys_u3phy_init(pdev);
	//disable_irq(u3phy->irq);
	schedule_delayed_work(&u3phy->typecdir, msecs_to_jiffies(100));
	//schedule_work(&u3phy->typecdir);
	return 0;
}

static int sunplus_usb_synopsys_u3phy_remove(struct platform_device *pdev)
{
#if 0
	u32 port_id = 1;
#endif
	struct usb3_phy *u3phy = platform_get_drvdata(pdev);

	iounmap(u3phy->u3phy_base_addr);
	release_mem_region(u3phy_res_mem->start, resource_size(u3phy_res_mem));
	/*disable uphy2/3 system clock*/
	clk_disable_unprepare(u3phy->u3_clk);
	clk_disable_unprepare(u3phy->u3phy_clk);
	//free_irq(u3phy->irq, u3phy_int);
	cancel_delayed_work_sync(&u3phy->typecdir);
#if 0
	gpio_free(usb_vbus_gpio[port_id]);
#endif
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
#if 0
static int __init usb_synopsys_u3phy_sunplus_init(void)
{
	printk(KERN_NOTICE "register sunplus_synopsys_u3phy_driver\n");
	return platform_driver_register(&sunplus_synopsys_u3phy_driver);
}
fs_initcall(usb_synopsys_u3phy_sunplus_init);

static void __exit usb_synopsys_u3phy_sunplus_exit(void)
{
	printk(KERN_NOTICE "unregister sunplus_synopsys_u3phy_driver\n");
	platform_driver_unregister(&sunplus_synopsys_u3phy_driver);
}
module_exit(usb_synopsys_u3phy_sunplus_exit);
#endif
MODULE_ALIAS("sunplus_usb_synpsys_u3phy");
MODULE_LICENSE("GPL");
