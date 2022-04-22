// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include <linux/firmware/sp-ocotp.h>

static int efuse2_sunplus_platform_probe(struct platform_device *dev)
{
#if defined(CONFIG_SOC_Q645)
	dev->id = 3;
#endif

	return sp_ocotp_probe(dev);
}

const struct sp_otp_vX_t  sp_otp2_v0 = {
	.size = QAK645_EFUSE2_SIZE,
};

static const struct of_device_id sp_ocotp2_dt_ids[] = {
#if defined(CONFIG_SOC_Q645)
	{ .compatible = "sunplus,q645-ocotp2", .data = &sp_otp2_v0  },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, sp_ocotp2_dt_ids);

static struct platform_driver sp_otp2_driver = {
	.probe     = efuse2_sunplus_platform_probe,
	.remove    = sp_ocotp_remove,
	.driver    = {
		.name           = "sunplus,ocotp2",
		.of_match_table = sp_ocotp2_dt_ids,
	}
};

static int __init sp_otp2_drv_new(void)
{
#if defined(CONFIG_SOC_Q645)
	return platform_driver_register(&sp_otp2_driver);
#else
	return -1;
#endif
}
subsys_initcall(sp_otp2_drv_new);

static void __exit sp_otp2_drv_del(void)
{
	platform_driver_unregister(&sp_otp2_driver);
}
module_exit(sp_otp2_drv_del);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus On-Chip OTP (eFuse 2) driver");
MODULE_LICENSE("GPL v2");

