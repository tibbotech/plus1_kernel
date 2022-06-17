// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/usb/sp_usb.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "ehci-sunplus.h"

static int ehci0_sunplus_platform_probe(struct platform_device *dev)
{
	dev->id = 1;

	return ehci_sunplus_probe(dev);
}

static const struct of_device_id ehci0_sunplus_dt_ids[] = {
#if defined (CONFIG_SOC_SP7021)
	{ .compatible = "sunplus,sp7021-usb-ehci0" },
#elif defined (CONFIG_SOC_Q645)
	{ .compatible = "sunplus,q645-usb-ehci0" },
#elif defined (CONFIG_SOC_SP7350)
	{ .compatible = "sunplus,sp7350-usb-ehci0" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, ehci0_sunplus_dt_ids);

static struct platform_driver ehci0_hcd_sunplus_driver = {
	.probe			= ehci0_sunplus_platform_probe,
	.remove			= ehci_sunplus_remove,
	.shutdown		= usb_hcd_platform_shutdown,
	.driver = {
		.name		= "ehci0-sunplus",
		.of_match_table = ehci0_sunplus_dt_ids,
#ifdef CONFIG_PM
		.pm = &ehci_sunplus_pm_ops,
#endif
	}
};

/* ---------------------------------------------------------------------------------------------- */
static int __init ehci0_sunplus_init(void)
{
	if (sp_port0_enabled & PORT0_ENABLED) {
		pr_notice("register ehci0_hcd_sunplus_driver\n");
		return platform_driver_register(&ehci0_hcd_sunplus_driver);
	}

	pr_notice("warn,port0 not enable,not register ehci0 sunplus hcd driver\n");
	return -1;
}
module_init(ehci0_sunplus_init);

static void __exit ehci0_sunplus_cleanup(void)
{
	platform_driver_unregister(&ehci0_hcd_sunplus_driver);
}
module_exit(ehci0_sunplus_cleanup);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB EHCI (port 0) driver");
MODULE_LICENSE("GPL v2");

