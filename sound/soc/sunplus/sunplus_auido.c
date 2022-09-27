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
 * @file sp7021-audio.c
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
#include <linux/reset.h>
#if defined(CONFIG_SND_SOC_AUD628)
#include "spsoc_util.h"
#elif defined(CONFIG_SND_SOC_AUD645)
#include "spsoc_util-645.h"
#endif

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int sunplus_audio_probe(struct platform_device *pdev);
static int sunplus_audio_remove(struct platform_device *pdev);

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
void __iomem *audio_base;
#if defined(CONFIG_SND_SOC_AUD628)
struct clk *peri0_clocken;
#elif defined(CONFIG_SND_SOC_AUD645)
struct reset_control *clk_rst;
#endif
struct clk *aud_clocken;
struct clk *plla_clocken;


static const struct of_device_id sunplus_audio_dt_ids[] = {
	{ .compatible = "sunplus,sp7021-audio", },
	{ .compatible = "sunplus,Q645-audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, sunplus_audio_dt_ids);

static struct platform_driver sunplus_audio_driver = {
	.probe	= sunplus_audio_probe,
	.remove	= sunplus_audio_remove,
	.driver	= {
		.name		= "sunplus-audio",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(sunplus_audio_dt_ids),
	},
};
module_platform_driver(sunplus_audio_driver);

static int sunplus_audio_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int err;

	AUD_INFO("%s IN\n", __func__);

	if (!np) {
		dev_err(&pdev->dev, "invalid devicetree node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_err(&pdev->dev, "devicetree status is not available\n");
		return -ENODEV;
	}
        //audio register base
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
	//AUD_INFO("start=%zx end=%zx\n", res->start, res->end);
	AUD_INFO("audio_base=%px\n", audio_base);
	//clock enable
#if defined(CONFIG_SND_SOC_AUD628)
    	peri0_clocken = devm_clk_get(&pdev->dev, "peri0");
    	if (IS_ERR(peri0_clocken)) {
		dev_err(&pdev->dev, "get clock from devicetree node 1.\n");
		return PTR_ERR(peri0_clocken);
	}
	err = clk_prepare_enable(peri0_clocken);
	if (err) {
	  	dev_err(&pdev->dev, "enable clock 1 false.\n");
		return err;
	}
#endif
	//clock enable
    	aud_clocken = devm_clk_get(&pdev->dev, "aud");
    	if (IS_ERR(aud_clocken)) {
		dev_err(&pdev->dev, "get clock from devicetree node 0.\n");
		return PTR_ERR(aud_clocken);
	}

	err = clk_prepare_enable(aud_clocken);
	if (err) {
	  	dev_err(&pdev->dev, "enable clock 0 false.\n");
		return err;
	}
#if defined(CONFIG_SND_SOC_AUD645)
	//reset
	clk_rst = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(clk_rst)) {
		dev_err(&pdev->dev, "aud failed to retrieve reset controlle\n");
		return PTR_ERR(clk_rst);
	}

	err = reset_control_assert(clk_rst);
	if (err)
		dev_err(&pdev->dev, "reset assert fail\n");
	err = reset_control_deassert(clk_rst);
	if (err)
		dev_err(&pdev->dev, "reset deassert fail\n");
#endif
	//plla setting
	plla_clocken = devm_clk_get(&pdev->dev, "pll_a");
    	if (IS_ERR(plla_clocken)) {
		dev_err(&pdev->dev, "get clock from devicetree node 2.\n");
		return PTR_ERR(plla_clocken);
	}
	err = clk_set_rate(plla_clocken, 147456000);//135475200, 147456000, 196608000 Hz, //driver/clk-sp-q628.c

	if (err) {
	  	dev_err(&pdev->dev, "enable clock 2 set rate false.\n");
		return err;
	}
	err = clk_prepare_enable(plla_clocken);
	if (err) {
	  	dev_err(&pdev->dev, "enable clock 2 false.\n");
		return err;
	}

	return 0;
}

static int sunplus_audio_remove(struct platform_device *pdev)
{
	AUD_INFO("%s IN\n", __func__);
	audio_base = NULL;
#if defined(CONFIG_SND_SOC_AUD628)
	clk_disable(peri0_clocken);
#endif
	clk_disable(aud_clocken);
	clk_disable(plla_clocken);
	return 0;
}


MODULE_DESCRIPTION("sunplus audio Driver");
MODULE_DESCRIPTION("S+ SoC ALSA PCM module");
MODULE_LICENSE("GPL");
