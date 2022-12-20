/*
 * ALSA SoC AUD7021 aud driver
 *
 * Author:	 <@sunplus.com>
 *
*/

#include <linux/module.h>
#include <sound/pcm_params.h>
#include "aud_hw.h"

/***********************************************************
*		Feature Definition
************************************************************/
static int spsoc_hw_params(struct snd_pcm_substream *substream,
	                   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out;
	int ret=0;

	AUD_INFO("%s IN\n",__func__);
	pll_out = params_rate(params);
	AUD_INFO("%s, pull out %d channels %d\n",__func__, pll_out, params_channels(params));
	AUD_INFO("%s, periods %d period_size %d\n",__func__, params_periods(params),params_period_size(params));
	AUD_INFO("%s, periods_bytes 0x%x\n",__func__, params_period_bytes(params));
	AUD_INFO("%s, buffer_size 0x%x buffer_bytes 0x%x\n",__func__, params_buffer_size(params),params_buffer_bytes(params));
	switch(pll_out) {
		case 8000:
		case 16000:
		case 32000:
		case 48000:
		case 64000:
		case 96000:
		case 128000:
		case 192000:
			ret = snd_soc_dai_set_pll(cpu_dai, substream->pcm->device, substream->stream, PLLA_FRE, pll_out);
			break;
		case 11025:
		case 22050:
		case 44100:
		case 88200:
		case 176400:
			ret = snd_soc_dai_set_pll(cpu_dai, substream->pcm->device, substream->stream, DPLL_FRE, pll_out);
			break;
		default:
			AUD_INFO("NO support the rate");
			break;
	}
	if( substream->stream == SNDRV_PCM_STREAM_CAPTURE )
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);

	AUD_INFO("%s OUT\n",__func__);
	if (ret < 0)
		return ret;
	return 0;
}

static struct snd_soc_ops spsoc_aud_ops = {
	.hw_params = spsoc_hw_params,
};

static struct snd_soc_dai_link spsoc_aud_dai[] = {
	{
		.name		= "aud_i2s",
		.stream_name	= "aud_dac0",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-codec-dai",
		.cpu_dai_name	= "spsoc-i2s-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
	{
		.name 		= "aud_tdm",
		.stream_name	= "aud_tdm0",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-codec-tdm-dai",
		.cpu_dai_name	= "spsoc-tdm-driver-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
	{
		.name 		= "aud_pdm",
		.stream_name	= "aud_pdm0",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-codec-pdm-dai",
		.cpu_dai_name	= "spsoc-pdm-driver-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
	{
		.name 		= "aud_spdif",
		.stream_name	= "aud_spdif0",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-spdif-dai",
		.cpu_dai_name	= "spsoc-spdif-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
	{
		.name 		= "aud_i2s_hdmi",
		.stream_name	= "aud_i2shdmi",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-i2s-hdmi-dai",
		.cpu_dai_name	= "spsoc-i2s-hdmi-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
	{
		.name 		= "aud_spdif_hdmi",
		.stream_name	= "aud_spdifhdmi",
		.codec_name 	= "soc@B:aud-codec",
		.codec_dai_name = "aud-spdif-hdmi-dai",
		.cpu_dai_name	= "spsoc-spdif-hdmi-dai",
		.platform_name 	= "soc@B:spsoc-pcm-driver",
		.ops 		= &spsoc_aud_ops,
	},
};
static struct snd_soc_card spsoc_smdk = {
	.name		= "sp-aud",		// card name
	.long_name 	= "AUD628, Sunplus Technology Inc.",
	.owner 		= THIS_MODULE,
	.dai_link 	= spsoc_aud_dai,
	.num_links 	= ARRAY_SIZE(spsoc_aud_dai),
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

MODULE_AUTHOR("Sunplus Technology Inc.");
MODULE_DESCRIPTION("Sunplus sp7021 audio card driver");
MODULE_LICENSE("GPL");

