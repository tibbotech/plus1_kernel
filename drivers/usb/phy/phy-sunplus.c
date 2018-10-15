#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>

#include <linux/usb/sp_usb.h>


void sp_get_port_state(void)
{
#ifdef CONFIG_USB_PORT0
	sp_port_enabled |= 0x01;
#endif

#ifdef CONFIG_USB_PORT1
	sp_port_enabled |= 0x02;
#endif
}

void uphy_init(void)
{
	u32 val, set;
	volatile u32 *group_base;
	bool sp_b_version = false;

	group_base = (u32 *)VA_IOB_ADDR(MONG0_REG_BASE * REG_BASE_PARAM);
	val = ioread32(group_base + STAMP_ID_OFFSET);
	if (SP_IC_A_VERSION == val) {
		printk(KERN_NOTICE "IC version is v1.0\n");
	} else if (SP_IC_B_VERSION == val) {
		sp_b_version = true;
		printk(KERN_NOTICE "IC version is v1.1\n");
	}

	if (sp_port_enabled & PORT0_ENABLED) {
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		/* 1. Default value modification */
		group_base[UPHY0_CTRL_OFFSET] = UPHY0_CTRL_DEFAULT_VALUE;

		/* 2. PLL power off/on twice */
		group_base[PLL_CTRL_OFFSET] = PORT0_PLL_POWER_OFF;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT0_PLL_POWER_ON;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT0_PLL_POWER_OFF;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT0_PLL_POWER_ON;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PLL_CTRL_DEFAULT_VALUE;

		/* 3. reset UPHY0 */
		group_base = (u32 *)VA_IOB_ADDR(MONG0_REG_BASE * REG_BASE_PARAM);
		group_base[HW_RESET_CTRL_OFFSET] |= ENABLE_UPHY0_RESET;
		group_base[HW_RESET_CTRL_OFFSET] &= ~(ENABLE_UPHY0_RESET);
		mdelay(UPHY_HANDLE_DELAY_TIME);

		/* 4.b board uphy 0 internal register modification for tid certification */
		group_base = (u32 *)VA_IOB_ADDR(OTP_REG_BASE * REG_BASE_PARAM);
		val = group_base[OTP_DISC_LEVEL_OFFSET];
		set = val & OTP_DISC_LEVEL_BIT;
		if (!set || set >= OTP_DISC_LEVEL_BIT) {
			set = DISC_LEVEL_DEFAULT;
		}
		group_base = (u32 *)VA_IOB_ADDR(UPHY0_REG_BASE * REG_BASE_PARAM);
		group_base[DISC_LEVEL_OFFSET] = (group_base[DISC_LEVEL_OFFSET] & ~OTP_DISC_LEVEL_BIT) | (OTP_DISC_LEVEL_TEMP - set);
		if (sp_b_version) {
			val = ioread32(group_base + ECO_PATH_OFFSET);
			val &= ~(ECO_PATH_SET);
			iowrite32(val, group_base + ECO_PATH_OFFSET);

			val = ioread32(group_base + POWER_SAVING_OFFSET);
			val &= ~(POWER_SAVING_SET);
			iowrite32(val, group_base + POWER_SAVING_OFFSET);
		}
		/* 5. USBC 0 reset */
		group_base = (u32 *)VA_IOB_ADDR(MONG0_REG_BASE * REG_BASE_PARAM);
		group_base[HW_RESET_CTRL_OFFSET] |= ENABLE_USBC0_RESET;
		group_base[HW_RESET_CTRL_OFFSET] &= ~(ENABLE_USBC0_RESET);
		/* port 0 uphy clk fix */
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		group_base[UPHY_CLOCK_OFFSET] |= UPHY0_NEGEDGE_CLK;

		/* 6. switch to host */
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		group_base[PINMUX_CTRL7_OFFSET] |= PORT0_OTG_EN_PINMUX;
		group_base = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * REG_BASE_PARAM);
		group_base[USB_UPHY_OTG_REG] |= PORT0_SWITCH_HOST;

#ifdef CONFIG_USB_SUNPLUS_OTG
		group_base = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * REG_BASE_PARAM);
		group_base[USB_UPHY_OTG_REG] &= ~(OTG0_SELECTED_BY_HW);
		mdelay(UPHY_HANDLE_DELAY_TIME);
#endif
	}

	if (sp_port_enabled & PORT1_ENABLED) {
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		/* 1. Default value modification */
		group_base[UPHY1_CTRL_OFFSET] = UPHY1_CTRL_DEFAULT_VALUE;

		/* 2. PLL power off/on twice */
		group_base[PLL_CTRL_OFFSET] = PORT1_PLL_POWER_OFF;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT1_PLL_POWER_ON;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT1_PLL_POWER_OFF;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PORT1_PLL_POWER_ON;
		mdelay(UPHY_HANDLE_DELAY_TIME);
		group_base[PLL_CTRL_OFFSET] = PLL_CTRL_DEFAULT_VALUE;

		/* 3. reset UPHY 1 */
		group_base = (u32 *)VA_IOB_ADDR(MONG0_REG_BASE * REG_BASE_PARAM);
		group_base[HW_RESET_CTRL_OFFSET] |= ENABLE_UPHY1_RESET;
		group_base[HW_RESET_CTRL_OFFSET] &= ~(ENABLE_UPHY1_RESET);
		mdelay(UPHY_HANDLE_DELAY_TIME);

		/* 4.b board uphy 1 internal register modification for tid certification */
		group_base = (u32 *)VA_IOB_ADDR(OTP_REG_BASE * REG_BASE_PARAM);
		val = group_base[OTP_DISC_LEVEL_OFFSET];
		set = (val >> UPHY1_OTP_DISC_LEVEL_OFFSET) & OTP_DISC_LEVEL_BIT;
		if (!set || set >= OTP_DISC_LEVEL_BIT) {
			set = DISC_LEVEL_DEFAULT;
		}
		group_base = (u32 *)VA_IOB_ADDR(UPHY1_REG_BASE * REG_BASE_PARAM);
		group_base[DISC_LEVEL_OFFSET] = (group_base[DISC_LEVEL_OFFSET] & ~OTP_DISC_LEVEL_BIT) | (OTP_DISC_LEVEL_TEMP - set);
		if (sp_b_version) {
			val = ioread32(group_base + ECO_PATH_OFFSET);
			val &= ~(ECO_PATH_SET);
			iowrite32(val, group_base + ECO_PATH_OFFSET);

			val = ioread32(group_base + POWER_SAVING_OFFSET);
			val &= ~(POWER_SAVING_SET);
			iowrite32(val, group_base + POWER_SAVING_OFFSET);
		}
		/* 5. USBC 1 reset */
		group_base = (u32 *)VA_IOB_ADDR(MONG0_REG_BASE * REG_BASE_PARAM);
		group_base[HW_RESET_CTRL_OFFSET] |= (ENABLE_USBC1_RESET);
		group_base[HW_RESET_CTRL_OFFSET] &= ~(ENABLE_USBC1_RESET);
		/* port 1 uphy clk fix */
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		group_base[UPHY_CLOCK_OFFSET] |= UPHY1_NEGEDGE_CLK;

		/* 6. switch to host */
		group_base = (u32 *)VA_IOB_ADDR(MONG1_REG_BASE * REG_BASE_PARAM);
		group_base[PINMUX_CTRL7_OFFSET] |= PORT1_OTG_EN_PINMUX;
		group_base = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * REG_BASE_PARAM);
		group_base[USB_UPHY_OTG_REG] |= PORT1_SWITCH_HOST;

#ifdef CONFIG_USB_SUNPLUS_OTG
		group_base = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * REG_BASE_PARAM);
		group_base[USB_UPHY_OTG_REG] &= ~(OTG1_SELECTED_BY_HW);
		mdelay(UPHY_HANDLE_DELAY_TIME);
#endif
	}
}

static int sunplus_usb_phy_probe(struct platform_device *pdev)
{
	uphy_init();
	up(&uphy_init_sem);

	return 0;
}

static int sunplus_usb_phy_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id phy_sunplus_dt_ids[] = {
	{ .compatible = "sunplus,sunplus-usb-phy" },
	{ }
};

MODULE_DEVICE_TABLE(of, phy_sunplus_dt_ids);

static struct platform_driver sunplus_usb_phy_driver = {
	.probe		= sunplus_usb_phy_probe,
	.remove		= sunplus_usb_phy_remove,
	.driver		= {
		.name	= "sunplus-usb-phy",
		.of_match_table = phy_sunplus_dt_ids,
	},
};


static int __init usb_phy_sunplus_init(void)
{
	int i;

	for (i = 0; i < USB_HOST_NUM; i++) {
		sema_init(&enum_rx_active_reset_sem[i], 0);
	}
#ifdef CONFIG_GADGET_USB0
	accessory_port_id = ACC_PORT0;
#else
	accessory_port_id = ACC_PORT1;
#endif
	printk(KERN_NOTICE "usb acc config port= %d\n",accessory_port_id);

	sp_get_port_state();
	printk(KERN_NOTICE "register sunplus_usb_phy_driver\n");
	return platform_driver_register(&sunplus_usb_phy_driver);
}
subsys_initcall(usb_phy_sunplus_init);

static void __exit usb_phy_sunplus_exit(void)
{
	platform_driver_unregister(&sunplus_usb_phy_driver);
	return;
}
module_exit(usb_phy_sunplus_exit);


MODULE_ALIAS("sunplus_usb_phy");
MODULE_AUTHOR("qiang.deng");
MODULE_LICENSE("GPL");
