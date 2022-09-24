// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Sunplus Inc.
 * Author: Li-hao Kuo <lhjeff911@gmail.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/thermal.h>
#include <linux/slab.h>

#define DISABLE_THERMAL		(BIT(31) | BIT(15))
#define ENABLE_THERMAL		BIT(31)
#define SP_THERMAL_MASK		GENMASK(10, 0)
#define SP_TCODE_HIGH_MASK	GENMASK(10, 8)
#define SP_TCODE_LOW_MASK	GENMASK(7, 0)

#define TEMP_RATE		608
#define TEMP_BASE		3500
#define TEMP_OTP_BASE		1518

#define SP_THERMAL_CTL0_REG	0x0000
#define SP_THERMAL_STS0_REG	0x0030

/* common data structures */
struct sp_thermal_data {
	struct thermal_zone_device *pcb_tz;
	struct platform_device *pdev;
	enum thermal_device_mode mode;
	void __iomem *regs;
	int otp_temp0;
	u32 id;
};

static int sp7021_get_otp_temp_coef(struct sp_thermal_data *sp_data, struct device *dev)
{
	struct nvmem_cell *cell;
	ssize_t otp_l;
	char *otp_v;

	cell = nvmem_cell_get(dev, "calib");
	if (IS_ERR(cell))
		return (int) cell;

	otp_v = nvmem_cell_read(cell, &otp_l);
	nvmem_cell_put(cell);

	if (otp_l < 3)
		return -EINVAL;
	sp_data->otp_temp0 = FIELD_PREP(SP_TCODE_LOW_MASK, otp_v[0]) |
			     FIELD_PREP(SP_TCODE_HIGH_MASK, otp_v[1]);

	if (!IS_ERR(otp_v))
		kfree(otp_v);

	if (sp_data->otp_temp0 == 0)
		sp_data->otp_temp0 = TEMP_OTP_BASE;
	return 0;
}

static int sp_thermal_get_sensor_temp(void *data, int *temp)
{
	struct sp_thermal_data *sp_data = data;
	int t_code;

	t_code = readl(sp_data->regs + SP_THERMAL_STS0_REG);
	t_code = FIELD_GET(SP_THERMAL_MASK, t_code);
	*temp = ((sp_data->otp_temp0 - t_code) * 10000 / TEMP_RATE) + TEMP_BASE;
	*temp *= 10;
	return 0;
}

static struct thermal_zone_of_device_ops sp_of_thermal_ops = {
	.get_temp = sp_thermal_get_sensor_temp,
};

static int sp_thermal_register_sensor(struct platform_device *pdev,
				      struct sp_thermal_data *data, int index)
{
	data->id = index;
	data->pcb_tz = devm_thermal_zone_of_sensor_register(&pdev->dev,
							    data->id,
							    data, &sp_of_thermal_ops);
	if (IS_ERR_OR_NULL(data->pcb_tz))
		return PTR_ERR(data->pcb_tz);
	return 0;
}

static int sp7021_thermal_probe(struct platform_device *pdev)
{
	struct sp_thermal_data *sp_data;
	struct resource *res;
	int ret;

	sp_data = devm_kzalloc(&pdev->dev, sizeof(*sp_data), GFP_KERNEL);
	if (!sp_data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res))
		return dev_err_probe(&pdev->dev, PTR_ERR(res), "resource get fail\n");

	sp_data->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(sp_data->regs))
		return dev_err_probe(&pdev->dev, PTR_ERR(sp_data->regs), "mas_base get fail\n");

	writel(ENABLE_THERMAL, sp_data->regs + SP_THERMAL_CTL0_REG);

	platform_set_drvdata(pdev, sp_data);
	ret = sp7021_get_otp_temp_coef(sp_data, &pdev->dev);
	ret = sp_thermal_register_sensor(pdev, sp_data, 0);

	return ret;
}

static int sp7021_thermal_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_sp7021_thermal_ids[] = {
	{ .compatible = "sunplus,sp7021-thermal" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_sp7021_thermal_ids);

static struct platform_driver sp7021_thermal_driver = {
	.probe	= sp7021_thermal_probe,
	.remove	= sp7021_thermal_remove,
	.driver	= {
		.name	= "sp7021-thermal",
		.of_match_table = of_match_ptr(of_sp7021_thermal_ids),
		},
};
module_platform_driver(sp7021_thermal_driver);

MODULE_AUTHOR("Li-hao Kuo <lhjeff911@gmail.com>");
MODULE_DESCRIPTION("Thermal driver for SP7021 SoC");
MODULE_LICENSE("GPL");
