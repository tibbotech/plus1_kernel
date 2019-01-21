/**************************************************************************
 *                                                                        *
 *         Copyright (c) 2018 by Sunplus Inc.                             *
 *                                                                        *
 *  This software is copyrighted by and is the property of Sunplus        *
 *  Inc. All rights are reserved by Sunplus Inc.                          *
 *  This software may only be used in accordance with the                 *
 *  corresponding license agreement. Any unauthorized use, duplication,   *
 *  distribution, or disclosure of this software is expressly forbidden.  *
 *                                                                        *
 *  This Copyright notice MUST not be removed or modified without prior   *
 *  written consent of Sunplus Technology Co., Ltd.                       *
 *                                                                        *
 *  Sunplus Inc. reserves the right to modify this software               *
 *  without notice.                                                       *
 *                                                                        *
 *  Sunplus Inc.                                                          *
 *  19, Innovation First Road, Hsinchu Science Park                       *
 *  Hsinchu City 30078, Taiwan, R.O.C.                                    *
 *                                                                        *
 **************************************************************************/
/**
 * @file sc7021-audio.c
 */
/**************************************************************************
 *                         H E A D E R   F I L E S
 **************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include "spsoc_util.h"


/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int _sc7021_audio_probe(struct platform_device *pdev);
static int _sc7021_audio_remove(struct platform_device *pdev);

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
void __iomem *audio_base;
void __iomem *audio_plla_base;
void __iomem *moon0_base;


static const struct of_device_id _sc7021_audio_dt_ids[] = {
	{ .compatible = "sunplus,sc7021-audio", },
};
MODULE_DEVICE_TABLE(of, _sc7021_audio_dt_ids);


static struct platform_driver _sc7021_audio_driver = {
	.probe		= _sc7021_audio_probe,
	.remove		= _sc7021_audio_remove,
	.driver		= {
		.name	= "sc7021-audio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(_sc7021_audio_dt_ids),
	},
};
module_platform_driver(_sc7021_audio_driver);

static int _sc7021_audio_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	volatile RegisterFile_G0 *regs0;

	AUD_INFO("%s IN\n", __func__);

	if (!np) {
		dev_err(&pdev->dev, "invalid devicetree node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_err(&pdev->dev, "devicetree status is not available\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "get resource memory from devicetree node 0.\n");
		return PTR_ERR(res);
	}
	audio_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(audio_base)) {
		dev_err(&pdev->dev, "mapping resource memory 0.\n");
		return PTR_ERR(audio_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "get resource memory from devicetree node 1.\n");
		return PTR_ERR(res);
	}
	audio_plla_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(audio_plla_base)) {
		dev_err(&pdev->dev, "mapping resource memory 1.\n");
		return PTR_ERR(audio_plla_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "get resource memory from devicetree node 1.\n");
		return PTR_ERR(res);
	}
	moon0_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(moon0_base)) {
		dev_err(&pdev->dev, "mapping resource memory 2.\n");
		return PTR_ERR(moon0_base);
	}

	// Enable AUD hardware clock.
	regs0 = (volatile RegisterFile_G0 *)moon0_base;
	regs0->clken0 = 0x08000800;
	regs0->clken2 = 0x00400040;

	AUD_INFO("audio_base = %08x, audio_plla_base = %08x, moon0_base = %08x\n", audio_base, audio_plla_base, moon0_base);
	return 0;
}

static int _sc7021_audio_remove(struct platform_device *pdev)
{
	AUD_INFO("%s IN\n", __func__);
	audio_base = NULL;
	return 0;
}


MODULE_DESCRIPTION("SC7021 audio Driver");
MODULE_DESCRIPTION("S+ SoC ALSA PCM module");
MODULE_LICENSE("GPL");
