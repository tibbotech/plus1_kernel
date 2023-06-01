// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Sunplus SP7021 USB 2.0 phy driver
 *
 * Copyright (C) 2022 Sunplus Technology Inc., All rights reserved.
 *
 * Note 1 : non-posted write command for the registers accesses of
 * Sunplus SP7021.
 *
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#define HIGH_MASK_BITS				GENMASK(31, 16)
#define LOW_MASK_BITS				GENMASK(15, 0)

#define PORT0_ENABLED				(1 << 0)
#define PORT1_ENABLED				(1 << 1)
#define PORT2_ENABLED				(1 << 3)
#define USB_PORT0_ID				0
#define USB_PORT1_ID				1
#define USB_PORT_NUM				3

#define OTP_DISC_LEVEL_DEFAULT			0x7

/* GROUP UPHY */
#define CONFIG1					0x4
#define J_HS_TX_PWRSAV				BIT(5)
#define CONFIG3					0xc
#define J_FORCE_DISC_ON				BIT(5)
#define J_DEBUG_CTRL_ADDR_MACRO			BIT(0)
#define CONFIG7					0x1c
#define J_DISC					0X1f
#define CONFIG9					0x24
#define J_ECO_PATH				BIT(6)
#define CONFIG16				0x40
#define J_TBCWAIT_MASK				GENMASK(6, 5)
#define J_TBCWAIT_1P1_MS			FIELD_PREP(J_TBCWAIT_MASK, 0)
#define J_TVDM_SRC_DIS_MASK			GENMASK(4, 3)
#define J_TVDM_SRC_DIS_8P2_MS			FIELD_PREP(J_TVDM_SRC_DIS_MASK, 3)
#define J_TVDM_SRC_EN_MASK			GENMASK(2, 1)
#define J_TVDM_SRC_EN_1P6_MS			FIELD_PREP(J_TVDM_SRC_EN_MASK, 0)
#define J_BC_EN					BIT(0)
#define CONFIG17				0x44
#define IBG_TRIM0_MASK				GENMASK(7, 5)
#define IBG_TRIM0_SSLVHT			FIELD_PREP(IBG_TRIM0_MASK, 4)
#define J_VDATREE_TRIM_MASK			GENMASK(4, 1)
#define J_VDATREE_TRIM_DEFAULT			FIELD_PREP(J_VDATREE_TRIM_MASK, 9)
#define CONFIG23				0x5c
#define PROB_MASK				GENMASK(5, 3)
#define PROB					FIELD_PREP(PROB_MASK, 7)
#define GLO_CTRL0_OFFSET			0x70
#define GLO_CTRL1_OFFSET			0x74
#define GLO_CTRL2_OFFSET			0x78
#define PLL_PD_SEL				BIT(7)
#define PLL_PD					BIT(3)

/* GROUP MOON3 */
#ifdef CONFIG_SOC_Q645
#define USBC_CTL_OFFSET				0x58
#elif defined(CONFIG_SOC_SP7350)
#define USBC_CTL_OFFSET				0x28
#endif
#define MO1_USBC0_USB0_TYPE			BIT(2)
#define MASK_MO1_USBC0_USB0_TYPE		BIT(2 + 16)
#define MO1_USBC0_USB0_SEL			BIT(1)
#define MASK_MO1_USBC0_USB0_SEL			BIT(1 + 16)
#define MO1_USBC0_USB0_CTRL			BIT(0)
#define MASK_MO1_USBC0_USB0_CTRL		BIT(0 + 16)

/* GROUP MOON4 */
#define UPHY_CONTROL0				0x0
#define UPHY_CONTROL1				0x4
#define UPHY_CONTROL2				0x8
#define MO1_UPHY_RX_CLK_SEL			BIT(6)
#define MASK_MO1_UPHY_RX_CLK_SEL		BIT(6 + 16)
#define MO1_UPHY_TX_CLK_SEL			BIT(5)
#define MASK_MO1_UPHY_TX_CLK_SEL		BIT(5 + 16)
#define UPHY_CONTROL3				0xc
#define MO1_UPHY_PLL_POWER_OFF_SEL		BIT(7)
#define MASK_MO1_UPHY_PLL_POWER_OFF_SEL		BIT(7 + 16)
#define MO1_UPHY_PLL_POWER_OFF			BIT(3)
#define MASK_UPHY_PLL_POWER_OFF			BIT(3 + 16)

#ifdef CONFIG_USB_PORT0
u8 sp_port0_enabled;
EXPORT_SYMBOL_GPL(sp_port0_enabled);

static void sp_get_port0_state(void)
{
	sp_port0_enabled |= PORT0_ENABLED;
}
#endif

#ifdef CONFIG_USB_PORT1
u8 sp_port1_enabled;
EXPORT_SYMBOL_GPL(sp_port1_enabled);

static void sp_get_port1_state(void)
{
	sp_port1_enabled |= PORT1_ENABLED;
}
#endif

struct sp_usbphy {
	struct device *dev;
	struct resource *phy_res_mem;
#ifdef CONFIG_SOC_Q645
	struct resource *moon3_res_mem;
#elif defined(CONFIG_SOC_SP7350)
	struct resource *moon4_res_mem;
#endif
	struct reset_control *rstc;
	struct clk *phy_clk;
	void __iomem *phy_regs;
#ifdef CONFIG_SOC_Q645
	void __iomem *moon3_regs;
#elif defined(CONFIG_SOC_SP7350)
	void __iomem *moon4_regs;
#endif
	u32 port_num;
	u32 disc_vol_addr_off;
};

static int update_disc_vol(struct sp_usbphy *usbphy)
{
	struct nvmem_cell *cell;
	char *disc_name = "disc_vol";
	ssize_t otp_l = 0;
	char *otp_v;
	u32 val, set;

	cell = nvmem_cell_get(usbphy->dev, disc_name);
	if (IS_ERR_OR_NULL(cell)) {
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
	}

	otp_v = nvmem_cell_read(cell, &otp_l);
	nvmem_cell_put(cell);

	if (!IS_ERR(otp_v)) {
		set = *(otp_v + 1);
		set = (set << (sizeof(char) * 8)) | *otp_v;
		set = (set >> usbphy->disc_vol_addr_off) & J_DISC;
	}

	if (IS_ERR(otp_v) || set == 0)
		set = OTP_DISC_LEVEL_DEFAULT;

	val = readl(usbphy->phy_regs + CONFIG7);
	val = (val & ~J_DISC) | set;
	writel(val, usbphy->phy_regs + CONFIG7);

	return 0;
}

static int sp_uphy_init(struct phy *phy)
{
	struct sp_usbphy *usbphy = phy_get_drvdata(phy);
	u32 val;
	int ret;

	/* default value modification */
	writel(0x28888102, usbphy->phy_regs + GLO_CTRL0_OFFSET);

	/* disconnect voltage */
	ret = update_disc_vol(usbphy);
	if (ret < 0)
		return ret;

	/* board uphy 0 internal register modification for tid certification */
	val = readl(usbphy->phy_regs + CONFIG1);
	val |= J_HS_TX_PWRSAV;
	writel(val, usbphy->phy_regs + CONFIG1);

	val = readl(usbphy->phy_regs + CONFIG23);
	val = (val & ~PROB) | PROB;
	writel(val, usbphy->phy_regs + CONFIG23);

	/* port 0 uphy clk fix */
	val = readl(usbphy->phy_regs + GLO_CTRL1_OFFSET);
	val &= ~(MO1_UPHY_RX_CLK_SEL | MO1_UPHY_TX_CLK_SEL);
	writel(val, usbphy->phy_regs + GLO_CTRL1_OFFSET);

	/* battery charger */
	writel(J_TBCWAIT_1P1_MS | J_TVDM_SRC_DIS_8P2_MS | J_TVDM_SRC_EN_1P6_MS | J_BC_EN,
	       usbphy->phy_regs + CONFIG16);
	writel(IBG_TRIM0_SSLVHT | J_VDATREE_TRIM_DEFAULT, usbphy->phy_regs + CONFIG17);

	/* chirp mode */
	writel(J_FORCE_DISC_ON | J_DEBUG_CTRL_ADDR_MACRO, usbphy->phy_regs + CONFIG3);

	/* switch to host (sw control) */
	val = MO1_USBC0_USB0_TYPE | MO1_USBC0_USB0_SEL | MO1_USBC0_USB0_CTRL
		| MO1_USBC0_USB0_TYPE | MASK_MO1_USBC0_USB0_SEL | MASK_MO1_USBC0_USB0_CTRL;
#ifdef CONFIG_SOC_Q645
	writel(val, usbphy->moon3_regs + USBC_CTL_OFFSET);
#elif defined(CONFIG_SOC_SP7350)
	writel(val, usbphy->moon4_regs + USBC_CTL_OFFSET);
#endif

	/* OTG control host and device */
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	val = MASK_MO1_USBC0_USB0_CTRL;
	writel(val, usbphy->moon4_regs + USBC_CTL_OFFSET);
#endif

	mdelay(1);

	return 0;
}

static int sp_uphy_power_on(struct phy *phy)
{
	struct sp_usbphy *usbphy = phy_get_drvdata(phy);
	u32 pll_pwr_on, pll_pwr_off;
	int ret;

	/* enable clock */
	ret = clk_prepare_enable(usbphy->phy_clk);
	if (ret) {
		goto err_clk;
	}

	/* reset UPHY0 */
	ret = reset_control_deassert(usbphy->rstc);
	if (ret) {
		goto err_reset;
	}

	/* PLL power off/on twice */
	pll_pwr_on = readl(usbphy->phy_regs + GLO_CTRL2_OFFSET);
	pll_pwr_on = (pll_pwr_on | PLL_PD_SEL) & ~PLL_PD;

	pll_pwr_off = readl(usbphy->phy_regs + GLO_CTRL2_OFFSET);
	pll_pwr_off |= PLL_PD_SEL | PLL_PD;

	writel(pll_pwr_off, usbphy->phy_regs + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(pll_pwr_on, usbphy->phy_regs + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(pll_pwr_off, usbphy->phy_regs + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(pll_pwr_on, usbphy->phy_regs + GLO_CTRL2_OFFSET);
	mdelay(1);

	pll_pwr_on = readl(usbphy->phy_regs + GLO_CTRL2_OFFSET);
	pll_pwr_on &= (~PLL_PD_SEL & ~PLL_PD);
	writel(pll_pwr_on, usbphy->phy_regs + GLO_CTRL2_OFFSET);

	return 0;

err_reset:
	reset_control_assert(usbphy->rstc);
err_clk:
	clk_disable_unprepare(usbphy->phy_clk);

	return ret;
}

static int sp_uphy_power_off(struct phy *phy)
{
	struct sp_usbphy *usbphy = phy_get_drvdata(phy);
	u32 temp;

	/* PLL power down */
	temp = readl(usbphy->phy_regs + GLO_CTRL2_OFFSET);
	temp |= PLL_PD_SEL | PLL_PD;
	writel(temp, usbphy->phy_regs + GLO_CTRL2_OFFSET);

	return 0;
}

static int sp_uphy_exit(struct phy *phy)
{
	struct sp_usbphy *usbphy = phy_get_drvdata(phy);

	clk_disable_unprepare(usbphy->phy_clk);

	return 0;
}

static const struct phy_ops sp_uphy_ops = {
	.init		= sp_uphy_init,
	.power_on	= sp_uphy_power_on,
	.power_off	= sp_uphy_power_off,
	.exit		= sp_uphy_exit,
};

static const struct of_device_id sp_uphy_dt_ids[] = {
	{.compatible = "sunplus,q645-usb2-phy", },
	{.compatible = "sunplus,sp7350-usb2-phy", },
	{ }
};
MODULE_DEVICE_TABLE(of, sp_uphy_dt_ids);

static int sp_usb_phy_probe(struct platform_device *pdev)
{
	struct sp_usbphy *usbphy;
	struct phy_provider *phy_provider;
	struct phy *phy;
	int ret;

	usbphy = devm_kzalloc(&pdev->dev, sizeof(*usbphy), GFP_KERNEL);
	if (!usbphy)
		return -ENOMEM;

	usbphy->dev = &pdev->dev;

	usbphy->phy_res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	usbphy->phy_regs = devm_ioremap_resource(&pdev->dev, usbphy->phy_res_mem);
	if (IS_ERR(usbphy->phy_regs))
		return PTR_ERR(usbphy->phy_regs);

#ifdef CONFIG_SOC_Q645
	usbphy->moon3_res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "moon3");
	if (!usbphy->moon3_res_mem)
		return -EINVAL;

	usbphy->moon3_regs = devm_ioremap(&pdev->dev, usbphy->moon3_res_mem->start,
						resource_size(usbphy->moon3_res_mem));
	if (!usbphy->moon3_regs)
		return -ENOMEM;
#elif defined(CONFIG_SOC_SP7350)
	usbphy->moon4_res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "moon4");
	if (!usbphy->moon4_res_mem)
		return -EINVAL;

	usbphy->moon4_regs = devm_ioremap(&pdev->dev, usbphy->moon4_res_mem->start,
						resource_size(usbphy->moon4_res_mem));
	if (!usbphy->moon4_regs)
		return -ENOMEM;
#endif

	usbphy->phy_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(usbphy->phy_clk))
		return PTR_ERR(usbphy->phy_clk);

	usbphy->rstc = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(usbphy->rstc))
		return PTR_ERR(usbphy->rstc);

#ifdef CONFIG_USB_PORT0
	if (usbphy->port_num == USB_PORT0_ID) {
		sp_get_port0_state();
	}
#endif

#ifdef CONFIG_USB_PORT1
	if (usbphy->port_num == USB_PORT1_ID)
		sp_get_port1_state();
#endif

	of_property_read_u32(pdev->dev.of_node, "sunplus,disc-vol-addr-off",
			     &usbphy->disc_vol_addr_off);

	phy = devm_phy_create(&pdev->dev, NULL, &sp_uphy_ops);
	if (IS_ERR(phy)) {
		ret = -PTR_ERR(phy);
		return ret;
	}

	phy_set_drvdata(phy, usbphy);
	phy_provider = devm_of_phy_provider_register(&pdev->dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static struct platform_driver sunplus_usb_phy_driver = {
	.probe		= sp_usb_phy_probe,
	.driver		= {
		.name	= "sunplus-usb2-phy",
		.of_match_table = sp_uphy_dt_ids,
	},
};
module_platform_driver(sunplus_usb_phy_driver);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB 2.0 phy driver");
MODULE_LICENSE("GPL");

