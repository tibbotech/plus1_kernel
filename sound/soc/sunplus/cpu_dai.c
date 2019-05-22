/*
 * ALSA SoC AUD3502 codec driver
 *
 * Author:	 <@sunplus.com>
 *
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "spsoc_util.h"
#include "aud_hw.h"
#include "spsoc_pcm.h"
#include "audclk.h"
//#include <mach/sp_clk.h>



#define AUD_FORMATS	(SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE|SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)|(SNDRV_PCM_FMTBIT_S24_3BE )

static struct platform_device *spsoc_pcm_device1;

static int aud_cpudai_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	AUD_INFO("%s IN\n", __func__);
	return 0;
}

static int aud_cpudai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	AUD_INFO("%s IN\n", __func__ );
	return 0;
}

static int spsoc_cpu_set_fmt(struct snd_soc_dai *codec_dai,unsigned int fmt)
{
	AUD_INFO("%s IN\n", __func__ );

	return 0;
}

static int spsoc_cpu_set_sysclk(struct snd_soc_dai *cpu_dai,int clk_id, unsigned int freq, int dir)
{
	AUD_INFO("%s IN\n", __func__ );
	return 0;
}

static int spsoc_cpu_set_pll(struct snd_soc_dai *dai, int pll_id, int source,unsigned int freq_in, unsigned int freq_out)
{
	AUD_INFO("%s IN\n", __func__ );
	AUD_Set_PLL(freq_out);
	aud_clk_cfg(freq_out);
	return 0;
}

static const struct snd_soc_dai_ops aud_dai_cpu_ops = {
	.startup = aud_cpudai_startup,
	.hw_params = aud_cpudai_hw_params,
	.set_fmt	= spsoc_cpu_set_fmt,
	.set_sysclk	= spsoc_cpu_set_sysclk,
	.set_pll	= spsoc_cpu_set_pll,
};


static struct snd_soc_dai_driver  aud_cpu_dai[]=
{
	{
		.name = "spsoc-i2s-dai",
		.playback = {
			.channels_min = 2,
			.channels_max = 8,
			.rates = AUD_RATES,
			.formats = AUD_FORMATS,
		},
		.capture = {
			.channels_min = 2,
			.channels_max = 2,
			.rates = AUD_RATES_C,
			.formats = AUD_FORMATS,
		},
		.ops=&aud_dai_cpu_ops
	},
};

static const struct snd_soc_component_driver sunplus_i2s_component = {
	.name		= "spsoc-i2s-dai",
};

static int __devinit aud_cpu_dai_probe(struct platform_device *pdev)
{
	  int ret=0;
	  AUD_INFO("%s\n",__func__);

	  AUDHW_Set_PLL();
	  AUDHW_pin_mx();
	  AUDHW_clk_cfg();
	  AUDHW_Mixer_Setting();
	  AUDHW_int_dac_adc_Setting();
	  AUDHW_SystemInit();
	  snd_aud_config();
	  AUD_INFO("Q628 aud set done\n");

	  ret = devm_snd_soc_register_component(&pdev->dev,
					                                &sunplus_i2s_component,
					                                &aud_cpu_dai[0],1);
	  if (ret)
		    pr_err("failed to register the dai\n");

	  return ret;
}

static int __devexit aud_cpudai_remove(struct platform_device *pdev)
{
	//snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static struct platform_driver aud_cpu_dai_driver = {
	.probe		= aud_cpu_dai_probe,
	.remove		= __devexit_p(aud_cpudai_remove),
	.driver	= {
		.name	= "spsoc-i2s-dai",
		.owner	= THIS_MODULE,
	},
};

static int __init aud_cpu_dai_init(void)
{

	int ret = platform_driver_register(&aud_cpu_dai_driver);

	spsoc_pcm_device1 = platform_device_alloc("spsoc-i2s-dai", -1);
	if (!spsoc_pcm_device1)
		return -ENOMEM;

	AUD_INFO("%s IN, create soc_card\n", __func__);


	ret = platform_device_add(spsoc_pcm_device1);
	if (ret)
		platform_device_put(spsoc_pcm_device1);


	return 0;
}
module_init(aud_cpu_dai_init);

static void __exit aud_unregister(void)
{
	platform_driver_unregister(&aud_cpu_dai_driver);
}
module_exit(aud_unregister);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC AUD cpu dai driver");
MODULE_AUTHOR("Sunplus DSP");
