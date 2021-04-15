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

static struct clk *u3phy_clk;
static struct reset_control *u3phy_rst;

void __iomem *u3phy_base_addr = NULL;

u8 sp_port2_enabled = 0;
EXPORT_SYMBOL_GPL(sp_port2_enabled);

static void sp_get_port2_state(void)
{
#ifdef CONFIG_USB_PORT2
	sp_port2_enabled |= PORT2_ENABLED;
#endif
}

static void synopsys_u3phy_init(struct platform_device *pdev)
{			
	reset_control_assert(u3phy_rst);
	reset_control_deassert(u3phy_rst);
	mdelay(1);	
}

static int sunplus_usb_synopsys_u3phy_probe(struct platform_device *pdev)
{
	struct resource *res_mem;

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

        /*enable uphy3 system clock*/
        u3phy_clk = devm_clk_get(&pdev->dev, "clkc_u3phy");
	if (IS_ERR(u3phy_clk)) {
		pr_err("not found clk source\n");
		return PTR_ERR(u3phy_clk);
	}
	clk_prepare(u3phy_clk);
	clk_enable(u3phy_clk);
	
	u3phy_rst = devm_reset_control_get(&pdev->dev, "rstc_u3phy");
	if (IS_ERR(u3phy_rst)) {
		pr_err("not found u3phy reset source\n");
		return PTR_ERR(u3phy_rst);
	}

    	//phy3 settings
    	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	u3phy_base_addr = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(u3phy_base_addr)) {
		return PTR_ERR(u3phy_base_addr);
	}
	//printk("phy3 base 0x%x, remap 0x%x\n", res_mem->start, u3phy_base_addr);
			
	synopsys_u3phy_init(pdev);

	return 0;
}

static int sunplus_usb_synopsys_u3phy_remove(struct platform_device *pdev)
{
#if 0
	u32 port_id = 1;
#endif
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
		printk(KERN_NOTICE "register sunplus_synopsys_u3phy_driver\n");
		return platform_driver_register(&sunplus_synopsys_u3phy_driver);	
	} else {
		printk(KERN_NOTICE "synopsys_u3phy not enabled\n");
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
