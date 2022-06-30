// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/usb/sp_usb.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "ohci-sunplus.h"

static int ohci1_sunplus_platform_probe(struct platform_device *dev)
{
	dev->id = 2;

	return ohci_sunplus_probe(dev);
}

static const struct of_device_id ohci1_sunplus_dt_ids[] = {
	{ .compatible = "sunplus,sp7021-usb-ohci1" },
	{ }
};
MODULE_DEVICE_TABLE(of, ohci1_sunplus_dt_ids);

static struct platform_driver ohci1_hcd_sunplus_driver = {
	.probe			= ohci1_sunplus_platform_probe,
	.remove			= ohci_sunplus_remove,
	.shutdown		= usb_hcd_platform_shutdown,
	.driver = {
		.name		= "ohci1-sunplus",
		.of_match_table = ohci1_sunplus_dt_ids,
#ifdef CONFIG_PM
		.pm = &ohci_sunplus_pm_ops,
#endif
	}
};

/*-------------------------------------------------------------------------*/

static int __init ohci1_sunplus_init(void)
{
#if defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	return -1;
#else
	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("register ohci1_hcd_sunplus_driver\n");
		return platform_driver_register(&ohci1_hcd_sunplus_driver);
	}

	pr_notice("warn,port1 not enable,not register ohci1 sunplus hcd driver\n");
	return -1;
#endif
}
module_init(ohci1_sunplus_init);

static void __exit ohci1_sunplus_cleanup(void)
{
	platform_driver_unregister(&ohci1_hcd_sunplus_driver);
}
module_exit(ohci1_sunplus_cleanup);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB OHCI (port 1) driver");
MODULE_LICENSE("GPL v2");

