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

static struct clk *u3_clk;
static struct reset_control *u3_rst;
static struct clk *u3phy_clk;
static struct reset_control *u3phy_rst;

static struct resource *u3phy_res_mem;
void __iomem *u3phy_base_addr = NULL;

struct uphy_u3_regs {
	unsigned int cfg[32];		       // 189.0
};

struct usb3_phy {
	struct device		*dev;
	int			irq;
};

static irqreturn_t u3phy_int(int irq, void *dev)
{
	printk("int\n ");
	return IRQ_HANDLED;
}
u8 sp_port2_enabled = 0;
EXPORT_SYMBOL_GPL(sp_port2_enabled);

static void sp_get_port2_state(void)
{
//#ifdef CONFIG_USB_PORT2
	sp_port2_enabled |= PORT2_ENABLED;
//#endif
}

static void synopsys_u3phy_init(struct platform_device *pdev)
{
	volatile struct uphy_u3_regs *dwc3phy_reg = (volatile struct uphy_u3_regs *) u3phy_base_addr;
	int result, i = 0;
	
	//reset_control_assert(u3_rst);			
	reset_control_assert(u3phy_rst);
	mdelay(1);
	//reset_control_deassert(u3_rst);
	reset_control_deassert(u3phy_rst);
	dwc3phy_reg->cfg[1] |= 0x03;
	for (;;)
	{
		result = dwc3phy_reg->cfg[2] & 0x3;
		if (result == 0x01)
			break;
		
		if (i++ > 10) {
			printk("PHY0_TIMEOUT_ERR0 ");
			i = 0;
			break;
		}
		mdelay(1);		
	}

	i = 0;
	dwc3phy_reg->cfg[2] |= 0x01;
	if (gpio_get_value(98))
		dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x15;
	else
		dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x11;
	for (;;)
	{
		result = dwc3phy_reg->cfg[2] & 0x3;
		if (result == 0x01)
			break;

		if (i++ > 10) {
			printk("PHY0_TIMEOUT_ERR1 ");
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
	int ret, typecdir;
	
	u3phy = devm_kzalloc(dev, sizeof(*u3phy), GFP_KERNEL);
	if (!u3phy)
		return -ENOMEM;
	u3phy->dev = dev;
	/*enable u3 system clock*/
        u3_clk = devm_clk_get(&pdev->dev, "clkc_u3");
	if (IS_ERR(u3_clk)) {
		pr_err("not found clk source\n");
		printk("not found clk source\n");
		return PTR_ERR(u3_clk);
	}
	clk_prepare(u3_clk);
	clk_enable(u3_clk);
        /*enable uphy3 system clock*/
        
        u3phy_clk = devm_clk_get(&pdev->dev, "clkc_u3phy");
	if (IS_ERR(u3phy_clk)) {
		pr_err("not found clk source\n");
		printk("not found clk1 source\n");
		return PTR_ERR(u3phy_clk);
	}
	//clk_prepare(u3phy_clk);
	//clk_enable(u3phy_clk);
	
	u3phy_rst = devm_reset_control_get(&pdev->dev, "rstc_u3phy");
	if (IS_ERR(u3phy_rst)) {
		pr_err("not found u3phy reset source\n");
		printk("not found u3phy reset source\n");
		return PTR_ERR(u3phy_rst);
	}
	
	//u3_rst = devm_reset_control_get(&pdev->dev, "rstc_u3");
	//if (IS_ERR(u3_rst)) {
	//	pr_err("not found u3phy reset source\n");
	//	printk("not found u3 reset source\n");
	//	return PTR_ERR(u3_rst);
	//}

    	//phy3 settings
    	u3phy_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	u3phy_base_addr = devm_ioremap(&pdev->dev, u3phy_res_mem->start, resource_size(u3phy_res_mem));
	if (IS_ERR(u3phy_base_addr)) {
		return PTR_ERR(u3phy_base_addr);
	}
	printk("phy3 base 0x%x, remap 0x%x\n", u3phy_res_mem->start, u3phy_base_addr);
	
	u3phy->irq = platform_get_irq(pdev, 0);
	if (u3phy->irq <= 0) {
		printk("ERR\n");
		return -EINVAL;
	}
	ret = devm_request_irq(dev, u3phy->irq, u3phy_int,
			       IRQF_SHARED, pdev->name, u3phy);//request_irq(u3phy->irq, u3phy_int, IRQF_SHARED, dev->name, u3phy);
	if (ret) {
		dev_dbg(u3phy->dev, "usb3 phy: unable to register IRQ(%d)\n", u3phy->irq);
		printk("usb3 phy: unable to register IRQ(%d)\n", u3phy->irq);
		free_irq(u3phy->irq, u3phy);
	}
	
	typecdir = of_get_named_gpio(pdev->dev.of_node, "typec-gpios", 0);
	if (!gpio_is_valid(typecdir))
		printk("Wrong pin %d configured for type c", typecdir);
	
	clk_prepare(u3phy_clk);
	clk_enable(u3phy_clk);		
	synopsys_u3phy_init(pdev);
	printk("USB3 phy end");	
	return 0;
}

static int sunplus_usb_synopsys_u3phy_remove(struct platform_device *pdev)
{
#if 0
	u32 port_id = 1;
#endif
	iounmap(u3phy_base_addr);
	release_mem_region(u3phy_res_mem->start, resource_size(u3phy_res_mem));
	/*disable uphy2/3 system clock*/
	clk_disable(u3phy_clk);
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

static int __init usb_synopsys_u3phy_sunplus_init(void)
{
	sp_get_port2_state();

	if (sp_port2_enabled & PORT2_ENABLED) {
		//printk(KERN_NOTICE "register sunplus_synopsys_u3phy_driver\n");
		printk("register sunplus_synopsys_u3phy_driver\n");
		return platform_driver_register(&sunplus_synopsys_u3phy_driver);	
	} else {
		//printk(KERN_NOTICE "synopsys_u3phy not enabled\n");
		printk("synopsys_u3phy not enabled\n");
		return 0;
	}
}
fs_initcall(usb_synopsys_u3phy_sunplus_init);

static void __exit usb_synopsys_u3phy_sunplus_exit(void)
{
	sp_get_port2_state();

	if (sp_port2_enabled & PORT2_ENABLED) {
		printk(KERN_NOTICE "unregister sunplus_synopsys_u3phy_driver\n");
		platform_driver_unregister(&sunplus_synopsys_u3phy_driver);
	} else {
		printk(KERN_NOTICE "synopsys u3phy not enabled\n");
		return;
	}
}
module_exit(usb_synopsys_u3phy_sunplus_exit);

MODULE_ALIAS("sunplus_usb_synpsys_u3phy");
MODULE_LICENSE("GPL");
