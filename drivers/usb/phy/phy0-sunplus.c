// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/usb/sp_usb.h>
#include <linux/nvmem-consumer.h>

static struct clk *uphy0_clk;
static struct resource *uphy0_res_mem;
void __iomem *uphy0_base_addr;
void __iomem *uphy0_res_moon0;
#if defined (CONFIG_SOC_SP7021)|| defined (CONFIG_SOC_SP7350)
void __iomem *uphy0_res_moon4;
#elif defined (CONFIG_SOC_Q645)
void __iomem *uphy0_res_moon3;
#endif
int uphy0_irq_num = -1;

u8 sp_port0_enabled;
EXPORT_SYMBOL_GPL(sp_port0_enabled);

static void sp_get_port0_state(void)
{
#ifdef CONFIG_USB_PORT0
	sp_port0_enabled |= PORT0_ENABLED;
#endif
}

char *otp_read_disc0(struct device *_d, ssize_t *_l, char *_name)
{
	char *ret = NULL;
	struct nvmem_cell *c = nvmem_cell_get(_d, _name);

	if (IS_ERR_OR_NULL(c)) {
		dev_err(_d, "OTP %s read failure: %ld", _name, PTR_ERR(c));
		return NULL;
	}

	ret = nvmem_cell_read(c, _l);
	nvmem_cell_put(c);
#if defined (CONFIG_SOC_SP7021)
	dev_dbg(_d, "%d bytes read from OTP %s", *_l, _name);
#else
	dev_dbg(_d, "%ld bytes read from OTP %s", *_l, _name);
#endif

	return ret;
}

static void uphy0_init(struct platform_device *pdev)
{
	u32 val;
	u32 set = 0;
	char *disc_name = "disc_vol";
	ssize_t otp_l = 0;
	char *otp_v;

	/* 1. reset UPHY0 */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 13), uphy0_res_moon0 + USB_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 13), uphy0_res_moon0 + USB_RESET_OFFSET);
#elif defined (CONFIG_SOC_Q645)
	writel(RF_MASK_V_SET(1 << 8), uphy0_res_moon0 + USBC0_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 8), uphy0_res_moon0 + USBC0_RESET_OFFSET);
#elif defined (CONFIG_SOC_SP7350)
	writel(RF_MASK_V_SET(1 << 12), uphy0_res_moon0 + USBC0_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 12), uphy0_res_moon0 + USBC0_RESET_OFFSET);
#endif

	mdelay(1);

	/* 2. Default value modification */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V(0xffff, 0x4002), uphy0_res_moon4 + UPHY0_CTL0_OFFSET);
	writel(RF_MASK_V(0xffff, 0x8747), uphy0_res_moon4 + UPHY0_CTL1_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	writel(0x28888102, uphy0_base_addr + GLO_CTRL0_OFFSET);
#endif

	/* 3. PLL power off/on twice */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V(0xffff, 0x88), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x80), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x88), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x80), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x00), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	writel(0x88, uphy0_base_addr + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(0x80, uphy0_base_addr + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(0x88, uphy0_base_addr + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(0x80, uphy0_base_addr + GLO_CTRL2_OFFSET);
	mdelay(1);
	writel(0x00, uphy0_base_addr + GLO_CTRL2_OFFSET);
#endif

	/* 4. board uphy 0 internal register modification for tid certification */
	otp_v = otp_read_disc0(&pdev->dev, &otp_l, disc_name);

	if (!IS_ERR_OR_NULL(otp_v)) {
		set = *otp_v & OTP_DISC_LEVEL_BIT;
		kfree(otp_v);
	}

	if (set == 0)
#if defined (CONFIG_SOC_SP7021)
		set = 0xD;
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
		set = 0x7;
#endif

	val = readl(uphy0_base_addr + DISC_LEVEL_OFFSET);
	val = (val & ~OTP_DISC_LEVEL_BIT) | set;
	writel(val, uphy0_base_addr + DISC_LEVEL_OFFSET);

#if defined (CONFIG_SOC_SP7021)
	val = readl(uphy0_base_addr + ECO_PATH_OFFSET);
	val &= ~(ECO_PATH_SET);
	writel(val, uphy0_base_addr + ECO_PATH_OFFSET);
#endif

#if defined (CONFIG_SOC_SP7021)
	val = readl(uphy0_base_addr + POWER_SAVING_OFFSET);
	val &= ~(POWER_SAVING_SET);
	writel(val, uphy0_base_addr + POWER_SAVING_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	val = readl(uphy0_base_addr + POWER_SAVING_OFFSET);
	val |= POWER_SAVING_SET;
	writel(val, uphy0_base_addr + POWER_SAVING_OFFSET);
#endif

#if defined (CONFIG_SOC_SP7021)
	val = readl(uphy0_base_addr + APHY_PROBE_OFFSET);
	val = (val & ~APHY_PROBE_CTRL_MASK) | APHY_PROBE_CTRL;
	writel(val, uphy0_base_addr + APHY_PROBE_OFFSET);
#endif

	/* 5. USBC 0 reset */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 10), uphy0_res_moon0 + USB_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 10), uphy0_res_moon0 + USB_RESET_OFFSET);
#elif defined (CONFIG_SOC_Q645)
	writel(RF_MASK_V_SET(1 << 13), uphy0_res_moon0 + USBC0_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 13), uphy0_res_moon0 + USBC0_RESET_OFFSET);
#elif defined (CONFIG_SOC_SP7350)
	writel(RF_MASK_V_SET(1 << 15), uphy0_res_moon0 + USBC0_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 15), uphy0_res_moon0 + USBC0_RESET_OFFSET);
#endif

	/* 6. port 0 uphy clk fix */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 6), uphy0_res_moon4 + UPHY0_CTL2_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	val = readl(uphy0_base_addr + GLO_CTRL1_OFFSET);
	val &= ~0x60;
	writel(val, uphy0_base_addr + GLO_CTRL1_OFFSET);
#endif

	/* 7. switch to host */
#if defined(CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(3 << 4), uphy0_res_moon4 + USBC_CTL_OFFSET);
#elif defined (CONFIG_SOC_Q645)
	writel(RF_MASK_V_SET(3 << 0), uphy0_res_moon3 + USBC_CTL_OFFSET);
#elif defined (CONFIG_SOC_SP7350)
	writel(RF_MASK_V_SET(3 << 0), uphy0_res_moon4 + USBC_CTL_OFFSET);
#endif

#if defined (CONFIG_USB_SUNPLUS_OTG) || defined (CONFIG_USB_SUNPLUS_SP7350_OTG)
	#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 2), uphy0_res_moon0 + PIN_MUX_CTRL);
	writel(RF_MASK_V_CLR(1 << 4), uphy0_res_moon4 + USBC_CTL_OFFSET);
	#elif defined (CONFIG_SOC_SP7350)
	writel(RF_MASK_V_CLR(1 << 0), uphy0_res_moon4 + USBC_CTL_OFFSET);
	#endif

	mdelay(1);
#endif

	/* 8. AC & ACB */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 11), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
	writel(RF_MASK_V_SET(1 << 14), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
#endif
}

static int sunplus_usb_phy0_probe(struct platform_device *pdev)
{
	struct resource *res_mem;

	/* enable uphy0 system clock */
	uphy0_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(uphy0_clk)) {
		pr_err("not found clk source\n");
		return PTR_ERR(uphy0_clk);
	}
	clk_prepare(uphy0_clk);
	clk_enable(uphy0_clk);

	uphy0_irq_num = platform_get_irq(pdev, 0);
	if (uphy0_irq_num < 0) {
		pr_notice("no irq provieded,ret:%d\n", uphy0_irq_num);
		return uphy0_irq_num;
	}
	pr_notice("uphy0_irq:%d\n", uphy0_irq_num);

	uphy0_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!uphy0_res_mem) {
		pr_notice("no memory recourse provieded");
		return -ENXIO;
	}

	if (!request_mem_region(uphy0_res_mem->start, resource_size(uphy0_res_mem), "uphy0")) {
		pr_notice("hw already in use");
		return -EBUSY;
	}

	uphy0_base_addr = ioremap(uphy0_res_mem->start, resource_size(uphy0_res_mem));
	if (!uphy0_base_addr) {
		release_mem_region(uphy0_res_mem->start, resource_size(uphy0_res_mem));
		return -EFAULT;
	}

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	uphy0_res_moon0 = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(uphy0_res_moon0))
		return PTR_ERR(uphy0_res_moon0);

#if defined (CONFIG_SOC_SP7021) || defined (CONFIG_SOC_SP7350)
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	uphy0_res_moon4 = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(uphy0_res_moon4))
		return PTR_ERR(uphy0_res_moon4);
#elif defined (CONFIG_SOC_Q645)
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	uphy0_res_moon3 = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(uphy0_res_moon3))
		return PTR_ERR(uphy0_res_moon3);
#endif

	uphy0_init(pdev);

#if defined(CONFIG_SOC_SP7021)
	writel(0x19, uphy0_base_addr + CDP_REG_OFFSET);
	writel(0x92, uphy0_base_addr + DCP_REG_OFFSET);
	writel(0x21, uphy0_base_addr + UPHY_INTER_SIGNAL_REG_OFFSET);
#endif

	return 0;
}

static int sunplus_usb_phy0_remove(struct platform_device *pdev)
{
#if defined (CONFIG_SOC_SP7021)
	u32 val;

	val = readl(uphy0_base_addr + CDP_REG_OFFSET);
	val &= ~(1u << CDP_OFFSET);
	writel(val, uphy0_base_addr + CDP_REG_OFFSET);
#endif

	iounmap(uphy0_base_addr);
	release_mem_region(uphy0_res_mem->start, resource_size(uphy0_res_mem));

	/* pll power off */
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V(0xffff, 0x88), uphy0_res_moon4 + UPHY0_CTL3_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	writel(0x88, uphy0_base_addr + GLO_CTRL2_OFFSET);
#endif

	/* disable uphy0 system clock */
	clk_disable(uphy0_clk);

	return 0;
}

static const struct of_device_id phy0_sunplus_dt_ids[] = {
#if defined (CONFIG_SOC_SP7021)
	{ .compatible = "sunplus,sp7021-usb-phy0" },
#elif defined (CONFIG_SOC_Q645)
	{ .compatible = "sunplus,q645-usb-phy0" },
#elif defined (CONFIG_SOC_SP7350)
	{ .compatible = "sunplus,sp7350-usb-phy0" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, phy0_sunplus_dt_ids);

void phy0_otg_ctrl(void)
{
#if defined (CONFIG_SOC_SP7021)
	writel(RF_MASK_V_SET(1 << 8), uphy0_res_moon4 + UPHY0_CTL0_OFFSET);
#elif defined (CONFIG_SOC_Q645) || defined (CONFIG_SOC_SP7350)
	u32 val;

	val = readl(uphy0_base_addr + GLO_CTRL0_OFFSET);
	val |= 0x100;
	writel(val, uphy0_base_addr + GLO_CTRL0_OFFSET);
#endif
}
EXPORT_SYMBOL(phy0_otg_ctrl);


static struct platform_driver sunplus_usb_phy0_driver = {
	.probe		= sunplus_usb_phy0_probe,
	.remove		= sunplus_usb_phy0_remove,
	.driver		= {
		.name	= "sunplus-usb-phy0",
		.of_match_table = phy0_sunplus_dt_ids,
	},
};


static int __init usb_phy0_sunplus_init(void)
{
	sp_get_port0_state();

	if (sp_port0_enabled & PORT0_ENABLED) {
		pr_notice("register sunplus_usb_phy0_driver\n");
		return platform_driver_register(&sunplus_usb_phy0_driver);
	}

	pr_notice("uphy0 not enabled\n");

	return 0;
}
fs_initcall(usb_phy0_sunplus_init);

static void __exit usb_phy0_sunplus_exit(void)
{
	if (sp_port0_enabled & PORT0_ENABLED) {
		pr_notice("unregister sunplus_usb_phy0_driver\n");
		platform_driver_unregister(&sunplus_usb_phy0_driver);
	} else {
		pr_notice("uphy0 not enabled\n");

		return;
	}
}
module_exit(usb_phy0_sunplus_exit);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB PHY (port 0) driver");
MODULE_LICENSE("GPL v2");

