/*
 * File:         sound/soc/sunplus/spsoc-aud3502.c
 * Author:       <@sunplus.com>
 *
 * Created:      Mar 8 2013
 * Description:  Board driver for aud3502 sound chip
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "spsoc_util.h"
#include "spsoc_pcm.h"
#include "aud_hw.h"



/***********************************************************
*		Feature Definition
************************************************************/
#define En_SPDIFIN0
#define En_SPDIFIN1
#define En_SPDIFIN2

#ifdef En_AUD_BT
	#undef En_SPDIFIN1
#endif

#ifdef En_SPDIFIN1
	#undef En_AUD_BT
#endif

static int spsoc_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out;
	int ret=0;
	AUD_INFO("%s\n",__func__);
	pll_out = params_rate(params);
	switch(pll_out)
	{
		case 8000:
		case 16000:
		case 32000:
		case 48000:
		case 64000:
		case 96000:
		case 128000:
		case 192000:
			ret=snd_soc_dai_set_pll(cpu_dai, substream->pcm->device, PLLA,PLLA_FRE, pll_out);
			break;
		case 11025:
		case 22050:
		case 44100:
		case 88200:
		case 176400:
			ret=snd_soc_dai_set_pll(cpu_dai, substream->pcm->device, DPLL,DPLL_FRE, pll_out);
			break;
		default:
			AUD_INFO("NO support the rate");
			break;

	}
	if (ret < 0)
		return ret;
	return 0;
}

static struct snd_soc_ops spsoc_aud_ops = {
	.hw_params	= spsoc_hw_params,
};

static struct snd_soc_dai_link spsoc_aud_dai[] = {
	{
		.name = "aud_playback",
		.stream_name	= "aud_dac0",
		.codec_name = "aud-codec",
		.codec_dai_name = "aud-codec-dai",
		.cpu_dai_name= "spsoc-i2s-dai",
		.platform_name = "spsoc-pcm-driver",
		.ops = &spsoc_aud_ops,
	},
};
static struct snd_soc_card spsoc_smdk = {
	.name = "sp-aud",		// card name
	.owner = THIS_MODULE,
	.dai_link = spsoc_aud_dai,
	.num_links = ARRAY_SIZE(spsoc_aud_dai),
};

static struct platform_device *spsoc_snd_device;


static int __init snd_spsoc_audio_init(void)
{
	int ret;

//********************************************************************
	spsoc_snd_device = platform_device_alloc("soc-audio", -1);	// soc-audio   aud3502-codec"
	if (!spsoc_snd_device)
		return -ENOMEM;

	AUD_INFO("%s IN, create soc_card\n", __func__);

	platform_set_drvdata(spsoc_snd_device, &spsoc_smdk);

	ret = platform_device_add(spsoc_snd_device);
	if (ret)
		platform_device_put(spsoc_snd_device);

	//AUD_INFO("platform_device_add:: %d\n", ret );

//********************************************************************
	return ret;

}
module_init(snd_spsoc_audio_init);

static void __exit snd_spsoc_audio_exit(void)
{
	platform_device_unregister(spsoc_snd_device);
}
module_exit(snd_spsoc_audio_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ALSA SoC Sunplus AUD");
MODULE_AUTHOR("Sunplus DSP");
MODULE_ALIAS("platform:spsoc-audio");