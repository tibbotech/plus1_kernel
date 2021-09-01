// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#ifdef CONFIG_SUNPLUS_IOP
#include <../drivers/misc/iop/sp_iop.h>
#endif
#include <linux/reboot.h>
#include <linux/io.h>
#include <asm/system_misc.h>


static void sp_power_off(void)
{
	early_printk("%s\n", __func__);
	#ifdef CONFIG_SUNPLUS_IOP	//for iop power off
	sp_iop_platform_driver_poweroff();
	#endif
	while (1);
}

void sp_restart(enum reboot_mode mode, const char *cmd)
{
	void __iomem *regs = NULL;

	#ifdef CONFIG_SUNPLUS_IOP
	unsigned int reg_value;
	#endif
	early_printk("%s\n", __func__);
	regs = ioremap(0xF8000000, 0x32);
	/* MOON : enable watchdog reset */
	writel(0x06000600, (regs + 0x0248));

	/* STC: watchdog control */
	writel(0x3877, (regs + 0x0628)); /* stop */
	writel(0xAB00, (regs + 0x0628)); /* unlock */
	writel(0x0001, (regs + 0x062C)); /* counter */
	writel(0x4A4B, (regs + 0x0628)); /* resume */

	#ifdef CONFIG_SUNPLUS_IOP
	reg_value = readl((regs + 0x400));
	writel((reg_value|0x0001), (regs + 0x400));
	#endif
}



static int sp_poweroff_probe(struct platform_device *pdev)
{
	//pr_err("sp_poweroff_probe\n");
	pm_power_off = sp_power_off;
	arm_pm_restart = sp_restart;
	return 0;
}

static int sp_poweroff_remove(struct platform_device *pdev)
{
	pm_power_off = NULL;
	return 0;
}

static const struct of_device_id sp_poweroff_match[] = {
	{ .compatible = "sunplus,sp-poweroff", },
	{},
};

static struct platform_driver sp_poweroff_driver = {
	.probe = sp_poweroff_probe,
	.remove = sp_poweroff_remove,
	.driver = {
		.name = "sunplus,sp-poweroff",
		.of_match_table = sp_poweroff_match,
	},
};

module_platform_driver(sp_poweroff_driver);


MODULE_DESCRIPTION("SUNPLUS poweroff driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:poweroff-sp");
