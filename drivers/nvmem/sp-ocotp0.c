// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include <linux/firmware/sp-ocotp.h>

static int efuse0_sunplus_platform_probe(struct platform_device *dev)
{
#ifdef CONFIG_SOC_Q645
	void n78_clk_register(void);
	int ret;

	dev->id = 1;
	ret = sp_ocotp_probe(dev);

	/* n78_clk_register need read settings from OTP0,
	 * calling it after OTP0 ready.
	 */
	n78_clk_register();

	return ret;
#else
	return sp_ocotp_probe(dev);
#endif
}

#ifdef CONFIG_SOC_SP7021
const struct sp_otp_vX_t  sp_otp_v0 = {
	.size = QAC628_OTP_SIZE,
};
#elif defined CONFIG_SOC_Q645
const struct sp_otp_vX_t  sp_otp_v0 = {
	.size = QAK645_EFUSE0_SIZE,
};
#endif

static const struct of_device_id sp_ocotp0_dt_ids[] = {
#ifdef CONFIG_SOC_SP7021
	{ .compatible = "sunplus,sp7021-ocotp", .data = &sp_otp_v0  },
#elif defined CONFIG_SOC_Q645
	{ .compatible = "sunplus,q645-ocotp0", .data = &sp_otp_v0  },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, sp_ocotp0_dt_ids);

static struct platform_driver sp_otp0_driver = {
	.probe     = efuse0_sunplus_platform_probe,
	.remove    = sp_ocotp_remove,
	.driver    = {
#ifdef CONFIG_SOC_SP7021
		.name           = "sunplus,sp7021-ocotp",
#elif defined CONFIG_SOC_Q645
		.name           = "sunplus,q645-ocotp0",
#endif
		.of_match_table = sp_ocotp0_dt_ids,
	}
};

static int __init sp_otp0_drv_new(void)
{
	return platform_driver_register(&sp_otp0_driver);
}
subsys_initcall(sp_otp0_drv_new);

static void __exit sp_otp0_drv_del(void)
{
	platform_driver_unregister(&sp_otp0_driver);
}
module_exit(sp_otp0_drv_del);

MODULE_DESCRIPTION("Sunplus OCOTP0 driver");
MODULE_LICENSE("GPL v2");

