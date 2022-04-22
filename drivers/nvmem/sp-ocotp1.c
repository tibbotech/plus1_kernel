// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include <linux/firmware/sp-ocotp.h>

static int efuse1_sunplus_platform_probe(struct platform_device *dev)
{
#if defined(CONFIG_SOC_Q645)
	dev->id = 2;
#endif

	return sp_ocotp_probe(dev);
}

const struct sp_otp_vX_t  sp_otp1_v0 = {
	.size = QAK645_EFUSE1_SIZE,
};

static const struct of_device_id sp_ocotp1_dt_ids[] = {
#if defined(CONFIG_SOC_Q645)
	{ .compatible = "sunplus,q645-ocotp1", .data = &sp_otp1_v0  },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, sp_ocotp1_dt_ids);

static struct platform_driver sp_otp1_driver = {
	.probe     = efuse1_sunplus_platform_probe,
	.remove    = sp_ocotp_remove,
	.driver    = {
		.name           = "sunplus,ocotp1",
		.of_match_table = sp_ocotp1_dt_ids,
	}
};

static int __init sp_otp1_drv_new(void)
{
#if defined(CONFIG_SOC_Q645)
	return platform_driver_register(&sp_otp1_driver);
#else
	return -1;
#endif
}
subsys_initcall(sp_otp1_drv_new);

static void __exit sp_otp1_drv_del(void)
{
	platform_driver_unregister(&sp_otp1_driver);
}
module_exit(sp_otp1_drv_del);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus On-Chip OTP (eFuse 1) driver");
MODULE_LICENSE("GPL v2");

