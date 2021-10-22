// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/usb/sp_usb.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "ehci-sunplus.h"

static int ehci1_sunplus_platform_probe(struct platform_device *dev)
{
	dev->id = 2;

	return ehci_sunplus_probe(dev);
}

static const struct of_device_id ehci1_sunplus_dt_ids[] = {
#ifdef CONFIG_SOC_SP7021
	{ .compatible = "sunplus,sp7021-usb-ehci1" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, ehci1_sunplus_dt_ids);

static struct platform_driver ehci1_hcd_sunplus_driver = {
	.probe			= ehci1_sunplus_platform_probe,
	.remove			= ehci_sunplus_remove,
	.shutdown		= usb_hcd_platform_shutdown,
	.driver = {
		.name		= "ehci1-sunplus",
		.of_match_table = ehci1_sunplus_dt_ids,
#ifdef CONFIG_PM
		.pm = &ehci_sunplus_pm_ops,
#endif
	}
};

/* ---------------------------------------------------------------------------------------------- */
static int __init ehci1_sunplus_init(void)
{
#ifdef CONFIG_SOC_Q645
	return -1;
#else
	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("register ehci1_hcd_sunplus_driver\n");
		return platform_driver_register(&ehci1_hcd_sunplus_driver);
	}

	pr_notice("warn,port1 not enable,not register ehci1 sunplus hcd driver\n");
	return -1;

#endif
}
module_init(ehci1_sunplus_init);

static void __exit ehci1_sunplus_cleanup(void)
{
	platform_driver_unregister(&ehci1_hcd_sunplus_driver);
}
module_exit(ehci1_sunplus_cleanup);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB EHCI (port 1) driver");
MODULE_LICENSE("GPL v2");

