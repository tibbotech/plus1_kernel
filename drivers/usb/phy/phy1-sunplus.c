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

static struct clk *uphy1_clk;
static struct resource *uphy1_res_mem;
void __iomem *uphy1_base_addr;
void __iomem *uphy1_res_moon0;
void __iomem *uphy1_res_moon4;
int uphy1_irq_num = -1;

u8 sp_port1_enabled;
EXPORT_SYMBOL_GPL(sp_port1_enabled);

static void sp_get_port1_state(void)
{
#ifdef CONFIG_USB_PORT1
	sp_port1_enabled |= PORT1_ENABLED;
#endif
}

char *otp_read_disc1(struct device *_d, ssize_t *_l, char *_name)
{
	char *ret = NULL;
	struct nvmem_cell *c = nvmem_cell_get(_d, _name);

	if (IS_ERR_OR_NULL(c)) {
		dev_err(_d, "OTP %s read failure: %ld", _name, PTR_ERR(c));
		return NULL;
	}

	ret = nvmem_cell_read(c, _l);
	nvmem_cell_put(c);
#if defined(CONFIG_SOC_SP7021)
	dev_dbg(_d, "%d bytes read from OTP %s", *_l, _name);
#else
	dev_dbg(_d, "%ld bytes read from OTP %s", *_l, _name);
#endif

	return ret;
}

static void uphy1_init(struct platform_device *pdev)
{
	u32 val;
	u32 set = 0;
	char *disc_name = "disc_vol";
	ssize_t otp_l = 0;
	char *otp_v;

	/* 1. reset UPHY 1 */
	writel(RF_MASK_V_SET(1 << 14), uphy1_res_moon0 + USB_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 14), uphy1_res_moon0 + USB_RESET_OFFSET);
	mdelay(1);

	/* 2. Default value modification */
	writel(RF_MASK_V(0xffff, 0x4002), uphy1_res_moon4 + UPHY1_CTL0_OFFSET);
	writel(RF_MASK_V(0xffff, 0x8747), uphy1_res_moon4 + UPHY1_CTL1_OFFSET);

	/* 3. PLL power off/on twice */
	writel(RF_MASK_V(0xffff, 0x88), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x80), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x88), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x80), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
	mdelay(1);
	writel(RF_MASK_V(0xffff, 0x00), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);

	/* 4. board uphy 1 internal register modification for tid certification */
	otp_v = otp_read_disc1(&pdev->dev, &otp_l, disc_name);

	if (!IS_ERR_OR_NULL(otp_v)) {
		set = ((*otp_v >> 5) | (*(otp_v + 1) << 3)) & OTP_DISC_LEVEL_BIT;
		kfree(otp_v);
	}

	if (set == 0)
		set = 0xD;

	val = readl(uphy1_base_addr + DISC_LEVEL_OFFSET);
	val = (val & ~OTP_DISC_LEVEL_BIT) | set;
	writel(val, uphy1_base_addr + DISC_LEVEL_OFFSET);

	val = readl(uphy1_base_addr + ECO_PATH_OFFSET);
	val &= ~(ECO_PATH_SET);
	writel(val, uphy1_base_addr + ECO_PATH_OFFSET);

	val = readl(uphy1_base_addr + POWER_SAVING_OFFSET);
	val &= ~(POWER_SAVING_SET);
	writel(val, uphy1_base_addr + POWER_SAVING_OFFSET);

	val = readl(uphy1_base_addr + APHY_PROBE_OFFSET);
	val = (val & ~APHY_PROBE_CTRL_MASK) | APHY_PROBE_CTRL;
	writel(val, uphy1_base_addr + APHY_PROBE_OFFSET);

	/* 5. USBC 1 reset */
	writel(RF_MASK_V_SET(1 << 11), uphy1_res_moon0 + USB_RESET_OFFSET);
	writel(RF_MASK_V_CLR(1 << 11), uphy1_res_moon0 + USB_RESET_OFFSET);

	/* 6. port 1 uphy clk fix */
	writel(RF_MASK_V_SET(1 << 6), uphy1_res_moon4 + UPHY1_CTL2_OFFSET);

	/* 7. switch to host */
	writel(RF_MASK_V_SET(3 << 12), uphy1_res_moon4 + USBC_CTL_OFFSET);

	#ifdef CONFIG_USB_SUNPLUS_OTG
	writel(RF_MASK_V_SET(1 << 3), uphy1_res_moon0 + PIN_MUX_CTRL);
	writel(RF_MASK_V_CLR(1 << 12), uphy1_res_moon4 + USBC_CTL_OFFSET);
	mdelay(1);
	#endif

	/* 8. AC & ACB */
	writel(RF_MASK_V_SET(1 << 11), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
	writel(RF_MASK_V_SET(1 << 14), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);
}

static int sunplus_usb_phy1_probe(struct platform_device *pdev)
{
	struct resource *res_mem;

	/* enable uphy1 system clock */
	uphy1_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(uphy1_clk)) {
		pr_err("not found clk source\n");
		return PTR_ERR(uphy1_clk);
	}
	clk_prepare(uphy1_clk);
	clk_enable(uphy1_clk);

	uphy1_irq_num = platform_get_irq(pdev, 0);
	if (uphy1_irq_num < 0) {
		pr_notice("no irq provieded,ret:%d\n", uphy1_irq_num);
		return uphy1_irq_num;
	}
	pr_notice("uphy1_irq:%d\n", uphy1_irq_num);

	uphy1_res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!uphy1_res_mem) {
		pr_notice("no memory recourse provieded");
		return -ENXIO;
	}

	if (!request_mem_region(uphy1_res_mem->start, resource_size(uphy1_res_mem), "uphy0")) {
		pr_notice("hw already in use");
		return -EBUSY;
	}

	uphy1_base_addr = ioremap(uphy1_res_mem->start, resource_size(uphy1_res_mem));
	if (!uphy1_base_addr) {
		release_mem_region(uphy1_res_mem->start, resource_size(uphy1_res_mem));
		return -EFAULT;
	}

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	uphy1_res_moon0 = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(uphy1_res_moon0))
		return PTR_ERR(uphy1_res_moon0);

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	uphy1_res_moon4 = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
	if (IS_ERR(uphy1_res_moon4))
		return PTR_ERR(uphy1_res_moon4);

	uphy1_init(pdev);

	writel(0x19, uphy1_base_addr + CDP_REG_OFFSET);
	writel(0x92, uphy1_base_addr + DCP_REG_OFFSET);
	writel(0x17, uphy1_base_addr + UPHY_INTER_SIGNAL_REG_OFFSET);

	return 0;
}

static int sunplus_usb_phy1_remove(struct platform_device *pdev)
{
	u32 val;

	val = readl(uphy1_base_addr + CDP_REG_OFFSET);
	val &= ~(1u << CDP_OFFSET);
	writel(val, uphy1_base_addr + CDP_REG_OFFSET);

	iounmap(uphy1_base_addr);
	release_mem_region(uphy1_res_mem->start, resource_size(uphy1_res_mem));

	/* pll power off */
	writel(RF_MASK_V(0xffff, 0x88), uphy1_res_moon4 + UPHY1_CTL3_OFFSET);

	/* disable uphy1 system clock */
	clk_disable(uphy1_clk);

	return 0;
}

static const struct of_device_id phy1_sunplus_dt_ids[] = {
	{ .compatible = "sunplus,sp7021-usb-phy1" },
	{ }
};

MODULE_DEVICE_TABLE(of, phy1_sunplus_dt_ids);

void phy1_otg_ctrl(void)
{
	writel(RF_MASK_V_SET(1 << 8), uphy1_res_moon4 + UPHY1_CTL0_OFFSET);
}
EXPORT_SYMBOL(phy1_otg_ctrl);

static struct platform_driver sunplus_usb_phy1_driver = {
	.probe		= sunplus_usb_phy1_probe,
	.remove		= sunplus_usb_phy1_remove,
	.driver		= {
		.name	= "sunplus-usb-phy1",
		.of_match_table = phy1_sunplus_dt_ids,
	},
};


static int __init usb_phy1_sunplus_init(void)
{
	sp_get_port1_state();

	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("register sunplus_usb_phy1_driver\n");
		return platform_driver_register(&sunplus_usb_phy1_driver);
	}

	pr_notice("uphy1 not enabled\n");
	return 0;
}
fs_initcall(usb_phy1_sunplus_init);

static void __exit usb_phy1_sunplus_exit(void)
{
	sp_get_port1_state();

	if (sp_port1_enabled & PORT1_ENABLED) {
		pr_notice("unregister sunplus_usb_phy1_driver\n");
		platform_driver_unregister(&sunplus_usb_phy1_driver);
	} else {
		pr_notice("uphy1 not enabled\n");
		return;
	}
}
module_exit(usb_phy1_sunplus_exit);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB PHY (port 1) driver");
MODULE_LICENSE("GPL v2");

