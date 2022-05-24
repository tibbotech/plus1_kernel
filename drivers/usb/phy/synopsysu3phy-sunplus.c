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

struct usb3_phy {
	struct device		*dev;
	void __iomem 		*u3phy_base_addr;
	struct clk 		*u3_clk;
	struct clk 		*u3phy_clk;
	struct reset_control 	*u3phy_rst;
	int			irq;
};
#if 0
static irqreturn_t u3phy_int(int irq, void *dev)
{
	printk("int\n ");
	return IRQ_HANDLED;
}
#endif
static void synopsys_u3phy_init(struct platform_device *pdev)
{
	struct usb3_phy *u3phy = platform_get_drvdata(pdev);
	struct uphy_u3_regs *dwc3phy_reg;// = (struct uphy_u3_regs *) u3phy_base_addr;
	unsigned int result, i = 0;
	
	clk_prepare_enable(u3phy->u3_clk);
	clk_prepare_enable(u3phy->u3phy_clk);
	
	reset_control_assert(u3phy->u3phy_rst);
	mdelay(1);
	reset_control_deassert(u3phy->u3phy_rst);

	dwc3phy_reg = (struct uphy_u3_regs *) u3phy->u3phy_base_addr;
	
	result = readl(&dwc3phy_reg->cfg[1]) | 0x03;
	writel(result, &dwc3phy_reg->cfg[1]);
	for (;;)
	{
		result = readl(&dwc3phy_reg->cfg[2]) & 0x3;
		if (result == 0x01)
			break;
		
		if (i++ > 10) {
			dev_err(&pdev->dev, "PHY0_TIMEOUT_ERR0 0x%x", result);
			i = 0;
			break;
		}
		mdelay(1);		
	}

	i = 0;
	result = readl(&dwc3phy_reg->cfg[2]) | 0x01;
	writel(result, &dwc3phy_reg->cfg[2]);
	//if (gpio_get_value(98)) {
	//	result = (readl(&dwc3phy_reg->cfg[5]) & 0xFFE0) | 0x15;
	//} else {
	result = (readl(&dwc3phy_reg->cfg[5]) & 0xFFE0) | 0x11;		
	//}
	writel(result, &dwc3phy_reg->cfg[5]);	
	for (;;)
	{
		result = readl(&dwc3phy_reg->cfg[2]) & 0x3;
		if (result == 0x01)
			break;

		if (i++ > 10) {
			dev_err(&pdev->dev, "PHY0_TIMEOUT_ERR1 0x%x", result);
			i = 0;
			break;
		}		
		mdelay(1);
	}	
}

static int sunplus_usb_synopsys_u3phy_probe(struct platform_device *pdev)
{
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
	//int ret, typecdir;
	
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
	
	u3phy->irq = platform_get_irq(pdev, 0);
	if (u3phy->irq <= 0) {
		printk("ERR\n");
		return -EINVAL;
	}
	//ret = devm_request_irq(dev, u3phy->irq, u3phy_int,
	//		       IRQF_SHARED, pdev->name, u3phy);//request_irq(u3phy->irq, u3phy_int, IRQF_SHARED, dev->name, u3phy);
	//if (ret) {
	//	dev_dbg(u3phy->dev, "usb3 phy: unable to register IRQ(%d)\n", u3phy->irq);
	//	free_irq(u3phy->irq, u3phy);
	//}
	
	typecdir = of_get_named_gpio(pdev->dev.of_node, "typec-gpios", 0);
	if (!gpio_is_valid(typecdir))
		dev_err(dev, "Wrong pin %d configured for type c", typecdir);
#endif
	synopsys_u3phy_init(pdev);	
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
#if 0
	gpio_free(usb_vbus_gpio[port_id]);
#endif
	return 0;
}

static const struct of_device_id synopsys_u3phy_sunplus_dt_ids[] = {
	{ .compatible = "sunplus,q645-usb3-phy" },
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
