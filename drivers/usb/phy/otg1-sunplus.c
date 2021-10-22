// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/sp_usb.h>
#include "otg-sunplus.h"

struct sp_otg *sp_otg1_host;
EXPORT_SYMBOL(sp_otg1_host);

static const struct of_device_id otg1_sunplus_dt_ids[] = {
#ifdef CONFIG_SOC_SP7021
	{ .compatible = "sunplus,sp7021-usb-otg1" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, otg1_sunplus_dt_ids);

static struct platform_driver sunplus_usb_otg1_driver = {
	.probe		= sp_otg_probe,
	.remove		= sp_otg_remove,
	.suspend	= sp_otg_suspend,
	.resume		= sp_otg_resume,
	.driver		= {
		.name	= "sunplus-usb-otg1",
		.of_match_table = otg1_sunplus_dt_ids,
	},
};

static int __init usb_otg1_sunplus_init(void)
{
	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("register sunplus_usb_otg1_driver\n");
		return platform_driver_register(&sunplus_usb_otg1_driver);
	}

	pr_notice("otg1 not enabled\n");

	return 0;
}
fs_initcall(usb_otg1_sunplus_init);

static void __exit usb_otg1_sunplus_exit(void)
{
	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("unregister sunplus_usb_otg1_driver\n");
		platform_driver_unregister(&sunplus_usb_otg1_driver);
	} else {
		pr_notice("otg1 not enabled\n");
		return;
	}
}
module_exit(usb_otg1_sunplus_exit);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB OTG (port 1) driver");
MODULE_LICENSE("GPL v2");

