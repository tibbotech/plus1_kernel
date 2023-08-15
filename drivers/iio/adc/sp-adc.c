// SPDX-License-Identifier: GPL-2.0
/* sp-adc.c - Analog Devices ADC driver 12 bits 4 channels
 */

#include <linux/hwspinlock.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define SP_ADC_CFG02			0x0008
#define SP_ADC_CFG0B			0x002c
#define SP_ADC_CFG0D			0x0034
#define SP_ADC_CFG0E			0x0038
#define SP_ADC_CFG11			0x0044
#define SP_ADC_CFG12			0x0048
#define SP_ADC_CFG13			0x004c
#define SP_ADC_CFG14			0x0050

#define SP_ADC_CLK_DIV_MASK		GENMASK(10, 8)

#define SP_ADC_EN			BIT(1)
#define SP_ADC_SRFS			BIT(2)
#define SP_ADC_BYPASS			BIT(5)
#define SP_ADC_DATA_READY		BIT(15)  
#define SP_ADC_CHAN_SEL			GENMASK(5, 3)
#define SP_ADC_CHAN0_SEL		GENMASK(2, 0)
#define SP_ADC_CHAN1_SEL		GENMASK(5, 3)
#define SP_ADC_CHAN2_SEL		GENMASK(8, 6)
#define SP_ADC_CHAN3_SEL		GENMASK(11, 9)

/* Timeout (ms) for the trylock of hardware spinlocks */
#define SP_ADC_HWLOCK_TIMEOUT	5000

enum {
	SP_ADC_CHAN0 = 0,
	SP_ADC_CHAN1 = 1,
	SP_ADC_CHAN2 = 2,
	SP_ADC_CHAN3 = 3,
};

struct sp_adc_spec {
	u8 num_channels;
	u8 resolution;
};

static const struct sp_adc_spec sp_adc_spec = {
	.num_channels = 4, 
	.resolution = 12, 
};

/**
 * struct sp_adc_chip
 * @lock: protects write sequences
 * @indio_dev: reference to iio structure
 * @resolution: resolution of the chip
 * @buffer: buffer to send / receive data to / from device
 */
struct sp_adc_chip {
	struct device *dev;
	struct hwspinlock *hwlock;
	struct mutex lock;
	struct iio_dev	*indio_dev;
	void __iomem	*regs;
	u8 resolution;
};

static int sp_adc_ini(struct sp_adc_chip *sp_adc)
{
	u32 reg_temp;

	reg_temp = readl(sp_adc->regs + SP_ADC_CFG0B);
	reg_temp |= SP_ADC_EN;
	writel(reg_temp, sp_adc->regs + SP_ADC_CFG0B);	// adc enable

	reg_temp = readl(sp_adc->regs + SP_ADC_CFG02);
	reg_temp |= FIELD_PREP(SP_ADC_CLK_DIV_MASK, 0);
	writel(reg_temp, sp_adc->regs + SP_ADC_CFG02);	// set adc clk 

	return 0;
}

static int sp_adc_read_channel(struct sp_adc_chip *sp_adc, int *val,
				   unsigned int channel)
{
	int ret;
	int mask = GENMASK(sp_adc->resolution - 1, 0);
	u32 data,reg_temp;

	if (sp_adc->hwlock) {
		ret = hwspin_lock_timeout_raw(sp_adc->hwlock, SP_ADC_HWLOCK_TIMEOUT);
		if (ret) {
			dev_err(sp_adc->dev, "timeout to get the hwspinlock\n");
			return ret;
		}
	}

	reg_temp = readl(sp_adc->regs + SP_ADC_CFG0B);
	reg_temp &= ~SP_ADC_CHAN_SEL;

	switch (channel) {
		case SP_ADC_CHAN0:
			reg_temp |= FIELD_PREP(SP_ADC_CHAN_SEL, SP_ADC_CHAN0);
		break;
		case SP_ADC_CHAN1:
			reg_temp |= FIELD_PREP(SP_ADC_CHAN_SEL, SP_ADC_CHAN1);
		break;
		case SP_ADC_CHAN2:
			reg_temp |= FIELD_PREP(SP_ADC_CHAN_SEL, SP_ADC_CHAN2);
		break;
		case SP_ADC_CHAN3:
			reg_temp |= FIELD_PREP(SP_ADC_CHAN_SEL, SP_ADC_CHAN3);
		break;
	}

	reg_temp &= ~SP_ADC_SRFS;
	writel(reg_temp, sp_adc->regs + SP_ADC_CFG0B);	// adc reset
	reg_temp |= SP_ADC_SRFS;
	writel(reg_temp, sp_adc->regs + SP_ADC_CFG0B);	// adc reset SRFS low-> high
	msleep(1);

	reg_temp = readl(sp_adc->regs + SP_ADC_CFG02);
	reg_temp |=  SP_ADC_BYPASS;
	writel(reg_temp, sp_adc->regs + SP_ADC_CFG02);

	reg_temp = readl(sp_adc->regs + SP_ADC_CFG0D);
	while(!(reg_temp & SP_ADC_DATA_READY))
	{
		reg_temp = readl(sp_adc->regs + SP_ADC_CFG0D);
	}

	data = readl(sp_adc->regs + SP_ADC_CFG0D);

	*val = data & mask;

	if(sp_adc->hwlock)
		hwspin_unlock_raw(sp_adc->hwlock);

	return 0;
}

#define SP_ADC_CHANNEL(chan) {				\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = (chan),					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
}

static const struct iio_chan_spec sp_adc_channels[] = {
	SP_ADC_CHANNEL(SP_ADC_CHAN0),
	SP_ADC_CHANNEL(SP_ADC_CHAN1),
	SP_ADC_CHANNEL(SP_ADC_CHAN2),
	SP_ADC_CHANNEL(SP_ADC_CHAN3),
};

static int sp_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct sp_adc_chip *sp_adc = iio_priv(indio_dev);
	int ret;

	if (!val)
		return -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&sp_adc->lock);
		ret = sp_adc_read_channel(sp_adc, val, chan->channel);
		mutex_unlock(&sp_adc->lock);

		if (ret < 0)
			return ret;

		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static const struct iio_info sp_info = {
	.read_raw = sp_read_raw,
};

static int sp_adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct sp_adc_spec *spec;
	struct sp_adc_chip *sp_adc;
	struct iio_dev *indio_dev;
	struct resource *res;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*sp_adc));
	if (!indio_dev) {
		dev_err(dev, "can not allocate iio device\n");
		return -ENOMEM;
	}

	sp_adc = iio_priv(indio_dev);

	indio_dev->info = &sp_info;
	indio_dev->name = "sunplus-adc";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sp_adc_channels;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res))
		return dev_err_probe(&pdev->dev, PTR_ERR(res), "resource get fail\n");

	sp_adc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sp_adc->regs))
		return dev_err_probe(&pdev->dev, PTR_ERR(sp_adc->regs), "mas_base get fail\n");

	ret = of_hwspin_lock_get_id(np, 0);
	if (ret < 0) {
		dev_err(dev, "failed to get hwspinlock id\n");
		return ret;
	}

	sp_adc->hwlock = devm_hwspin_lock_request_specific(dev, ret);
	if (!sp_adc->hwlock) {
		dev_err(dev, "failed to request hwspinlock\n");
		return -ENXIO;
	}

	sp_adc->dev = dev;
	sp_adc->indio_dev = indio_dev;
	spec = &sp_adc_spec;
	indio_dev->num_channels = spec->num_channels;
	sp_adc->resolution = spec->resolution;
	mutex_init(&sp_adc->lock);

	ret = sp_adc_ini(sp_adc);

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret) 
		dev_err_probe(&pdev->dev, PTR_ERR(&ret), "fail to register iio device\n");

	return ret;
}

static const struct of_device_id sp_adc_of_ids[] = {
	{ .compatible = "sunplus,sp7350-adc" },
	{ }
};
MODULE_DEVICE_TABLE(of, ad7949_spi_of_id);

static struct platform_driver sp_adc_driver = {
	.probe = sp_adc_probe,
	.driver = {
		.name = "sunplus,sp7350-adc",
		.of_match_table = sp_adc_of_ids,
	},
};
module_platform_driver(sp_adc_driver);

MODULE_AUTHOR("Li-hao Kuo <lhjeff911@gmail.com>");
MODULE_DESCRIPTION("Sunplus ADC driver");
MODULE_LICENSE("GPL");
