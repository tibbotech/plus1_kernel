/*
 * ALSA SoC AUD7021 aud driver
 *
 * Author:	 <@sunplus.com>
 *
*/

#include <linux/module.h>
#include <sound/pcm_params.h>
#include "aud_hw.h"

static int spsoc_hw_params(struct snd_pcm_substream *substream,
	                   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	unsigned int pll_out;
	int ret=0;

	pr_info("%s IN\n",__func__);
	pll_out = params_rate(params);
	pr_info("%s, pull out %d channels %d\n",__func__, pll_out, params_channels(params));
	pr_info("%s, periods %d period_size %d\n",__func__, params_periods(params),params_period_size(params));
	pr_info("%s, periods_bytes 0x%x\n",__func__, params_period_bytes(params));
	pr_info("%s, buffer_size 0x%x buffer_bytes 0x%x\n",__func__, params_buffer_size(params),params_buffer_bytes(params));
	switch (pll_out) {
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
			pr_err("NO support the rate");
			break;
	}
	if( substream->stream == SNDRV_PCM_STREAM_CAPTURE ) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
	}

	pr_info("%s OUT\n",__func__);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

static struct snd_soc_ops spsoc_aud_ops = {
	.hw_params = spsoc_hw_params,
};

SND_SOC_DAILINK_DEFS(sp_i2s,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-i2s-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-codec-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

SND_SOC_DAILINK_DEFS(sp_tdm,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-tdm-driver-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-codec-tdm-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

SND_SOC_DAILINK_DEFS(sp_pdm,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-pdm-driver-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-codec-pdm-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

SND_SOC_DAILINK_DEFS(sp_spdif,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-spdif-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-spdif-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

SND_SOC_DAILINK_DEFS(sp_i2shdmi,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-i2s-hdmi-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-i2s-hdmi-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

SND_SOC_DAILINK_DEFS(sp_spdifhdmi,
	DAILINK_COMP_ARRAY(COMP_CPU("spsoc-spdif-hdmi-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("soc-B:aud-codec", "aud-spdif-hdmi-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc-B:spsoc-pcm-driver")));

static struct snd_soc_dai_link spsoc_aud_dai[] = {
	{
		.name		= "aud_i2s",
		.stream_name	= "aud_dac0",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_i2s),
	},
	{
		.name 		= "aud_tdm",
		.stream_name	= "aud_tdm0",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_tdm),
	},
	{
		.name 		= "aud_pdm",
		.stream_name	= "aud_pdm0",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_pdm),
	},
	{
		.name 		= "aud_spdif",
		.stream_name	= "aud_spdif0",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_spdif),
	},
	{
		.name 		= "aud_i2s_hdmi",
		.stream_name	= "aud_i2shdmi",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_i2shdmi),
	},
	{
		.name 		= "aud_spdif_hdmi",
		.stream_name	= "aud_spdifhdmi",
		.ops 		= &spsoc_aud_ops,
		SND_SOC_DAILINK_REG(sp_spdifhdmi),
	},
};

static struct snd_soc_card spsoc_smdk = {
	.name		= "sp-aud",		// card name
	.long_name 	= "Q628, Sunplus Technology Inc.",
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
	if (!spsoc_snd_device) {
		return -ENOMEM;
	}

	pr_info("%s IN, create soc_card\n", __func__);

	platform_set_drvdata(spsoc_snd_device, &spsoc_smdk);

	ret = platform_device_add(spsoc_snd_device);
	if (ret) {
		platform_device_put(spsoc_snd_device);
	}

	//pr_info("platform_device_add:: %d\n", ret );
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

