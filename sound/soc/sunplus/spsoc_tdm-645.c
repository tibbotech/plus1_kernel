// SPDX-License-Identifier: GPL-2.0
// ALSA	SoC Q645 tdm driver
//
// Author:	 <@sunplus.com>
//
//

#include <sound/pcm_params.h>
#include "spsoc_pcm-645.h"
#include "spsoc_util-645.h"
#include "aud_hw.h"

void __iomem *tdmaudio_base;
// Audio Registers
#define	Main_PCM7			BIT(27)
#define	Main_PCM6			BIT(26)
#define	TDM_PDM_RX3			BIT(25)
#define	TDM_PDM_RX2			BIT(24)
#define	TDM_PDM_RX1			BIT(23)
#define	TDM_PDM_RX0			BIT(22)
#define	TDM_PDM_RX7			BIT(21)
#define	Main_PCM5			BIT(20)
#define	TDM_PDM_RX6			BIT(18)
#define	TDM_PDM_RX5			BIT(17)
#define	TDM_PDM_RX4			BIT(14)
#define	Main_PCM4			BIT(4)
#define	Main_PCM3			BIT(3)
#define	Main_PCM2			BIT(2)
#define	Main_PCM1			BIT(1)
#define	Main_PCM0			BIT(0)

#define	TDM_RX_SLAVE_ENABLE		BIT(8)
#define	RX_PATH0_1_SELECT		BIT(4)
#define	RX_PATH0_1_2_3_SELECT		(BIT(4) | BIT(5))
#define	TDM_RX_ENABLE			BIT(0)

#define	TDM_TX_ENABLE			BIT(0)

void aud_tdm_clk_cfg(int pll_id, int source, unsigned int SAMPLE_RATE)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	int capture = (source == SNDRV_PCM_STREAM_CAPTURE);

	pr_debug("%s rate %d capture %d fmt 0x%x\n", __func__, SAMPLE_RATE, capture, source);
	//147M settings
	if (SAMPLE_RATE == 48000) {
		if (source == SNDRV_PCM_FORMAT_S24_3LE)	{
			regs0->tdm_tx_xck_cfg		= 0x6880; //PLLA 147M, 4 chs 128 bits 48k = 147M/6(3/2 xck)/4(bck)/128
							  	  //	       8 chs 256 bits 48k = 147M/3(xck)/4(bck)/256
							  	  //	      16 chs 512 bits 48k = 147M/3(xck)/2(bck)/512
			regs0->tdm_tx_bck_cfg		= 0x6001; // 48k = 147M/xck/bck/256, (8	channels)
			regs0->aud_ext_dac_xck_cfg	= 0x6883; // The ext_dac_xck/bck need to set to	48k.
			regs0->aud_ext_dac_bck_cfg	= 0x6003;
		} else { //SNDRV_PCM_FORMAT_S16_LE
			regs0->tdm_tx_xck_cfg		= 0x6880; //PLLA 147M, 4 chs  64 bits 48k = 147M/12(3/4 xck)/4(bck)/64
							  	  //	       8 chs 128 bits 48k = 147M/6(3/2 xck)/4(bck)/128
							  	  //	      16 chs 256 bits 48k = 147M/3(xck)/4(bck)/256
			regs0->tdm_tx_bck_cfg		= 0x6003; // 48k = 147M/xck/bck/256, (8	channels)
			regs0->aud_ext_dac_xck_cfg	= 0x6883; // The ext_dac_xck/bck need to set to	48k.
			regs0->aud_ext_dac_bck_cfg	= 0x6007;
		}
	} else {
		regs0->tdm_tx_xck_cfg		= 0;
		regs0->tdm_tx_bck_cfg		= 0;
		regs0->tdm_rx_xck_cfg		= 0;
		regs0->tdm_rx_bck_cfg		= 0;
		regs0->aud_ext_dac_xck_cfg	= 0;
		regs0->aud_ext_dac_bck_cfg	= 0;
	}
}

static void sp_tdm_tx_en(bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	unsigned int val;

	val =  regs0->tdm_tx_cfg0;
	if (on)	{
		val |= TDM_TX_ENABLE;
		regs0->tdmpdm_tx_sel = 0x01;
	} else
		val &= ~(TDM_TX_ENABLE);

	regs0->tdm_tx_cfg0 = val;
	regs0->tdm_rx_cfg0 = val;

	pr_debug("tdm_tx_cfg0 0x%x\n", regs0->tdm_tx_cfg0);
}

static void sp_tdm_rx_en(bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	unsigned int val;

	val = regs0->tdm_rx_cfg0;
	if (on)	{
		val |= TDM_RX_ENABLE;
		regs0->tdmpdm_tx_sel = 0x01;
	} else
		val &= ~(TDM_RX_ENABLE);

	regs0->tdm_rx_cfg0 = val;
	regs0->tdm_tx_cfg0 = val;

	pr_debug("tdm_rx_cfg0 0x%x\n", regs0->tdm_rx_cfg0);
}

static void sp_tdm_tx_dma_en(bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile	RegisterFile_Audio *) tdmaudio_base;
	unsigned int val;

	val = regs0->aud_fifo_enable;
	if (on)	{
		if ((val & TDM_P_INC0) != 0)
			return;

		val |= TDM_P_INC0;
	} else
		val &= ~TDM_P_INC0;

	regs0->aud_fifo_enable = val;

	pr_debug("aud_fifo_enable 0x%x\n", regs0->aud_fifo_enable);

	if (on)	{
		regs0->aud_fifo_reset =	val;
		while ((regs0->aud_fifo_reset &	val))
			;
	}

	val = regs0->aud_enable;
	if (on)
		val |= aud_enable_i2stdm_p;
	else
		val &= (~aud_enable_i2stdm_p);

	regs0->aud_enable = val;

	pr_debug("aud_enable 0x%x\n", regs0->aud_enable);
}

static void sp_tdm_rx_dma_en(bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	unsigned int val;

	val = regs0->aud_fifo_enable;
	if (on)
		val |= TDMPDM_C_INC0;
	else
		val &= ~TDMPDM_C_INC0;

	regs0->aud_fifo_enable = val;

	pr_debug("aud_fifo_enable 0x%x\n", regs0->aud_fifo_enable);

	if (on)	{
		regs0->aud_fifo_reset =	val;
		while ((regs0->aud_fifo_reset & val))
			;
	}

	val = regs0->aud_enable;
	if (on)
		val |= aud_enable_tdmpdm_c;
	else
		val &= (~aud_enable_tdmpdm_c);

	regs0->aud_enable = val;

	pr_debug("aud_enable 0x%x\n", regs0->aud_enable);
}

//#if 0
//#define	SP_TDM_RATES	SNDRV_PCM_RATE_44100//(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)

//#define	SP_TDM_FMTBIT	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FORMAT_MU_LAW | SNDRV_PCM_FORMAT_A_LAW)
//#endif

static int sp_tdm_dai_probe(struct snd_soc_dai *dai)
{
	struct sunplus_audio_base *sp_tdm = dev_get_drvdata(dai->dev);

	dev_dbg(dai->dev, "%s \n", __func__);
	sp_tdm->dma_playback.maxburst =	16;
	sp_tdm->dma_capture.maxburst = 16;
	snd_soc_dai_init_dma_data(dai, &sp_tdm->dma_playback, &sp_tdm->dma_capture);
	snd_soc_dai_set_drvdata(dai, sp_tdm);

	return 0;
}

static int sp_tdm_set_fmt(struct snd_soc_dai *cpu_dai, unsigned	int fmt)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	//struct sunplus_audio_base *tdm = snd_soc_dai_get_drvdata(cpu_dai);
	//unsigned int val;

	pr_debug("%s IN, fmt = 0x%x\n", __func__, fmt);
	switch (fmt) {
	case SNDRV_PCM_FORMAT_S24_3LE: //0x20
		regs0->aud_fifo_mode	= 0x20000;
		break;
	case SNDRV_PCM_FORMAT_S16_LE: //2
		regs0->aud_fifo_mode	= 0x20001;
		break;
	//case SND_SOC_DAIFMT_CBM_CFM: //	TX/RX master
	//	tdm->master = 1;
	//	val = regs0->tdm_rx_cfg0;
	//	val = regs0->tdm_tx_cfg0;
	//	val &= ~TDM_RX_SLAVE_ENABLE;
	//	regs0->tdm_rx_cfg0 = val;
	//	regs0->tdm_tx_cfg0 = val;
	//	break;
	//case SND_SOC_DAIFMT_CBS_CFS: //	TX/RX slave
	//	tdm->master = 0;
	//	val = regs0->tdm_rx_cfg0;
	//	val = regs0->tdm_tx_cfg0;
	//	val |= TDM_RX_SLAVE_ENABLE;
	//	regs0->tdm_rx_cfg0 = val;
	//	regs0->tdm_tx_cfg0 = val;
	//	break;
	default:
		pr_err("Unknown	data format\n");
		return -EINVAL;
	}
	return 0;
}

static int sp_tdm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params, struct snd_soc_dai *socdai)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;
	//struct snd_pcm_runtime *runtime	= substream->runtime;
	//struct sp_tdm_info *tdm = snd_soc_dai_get_drvdata(socdai);
	struct snd_dmaengine_dai_dma_data *dma_data;
	unsigned int ts_width;
	unsigned int wd_width;
	unsigned int ch_num = 32;
	unsigned int ret = 0;
	unsigned int val;

	pr_debug("%s IN\n", __func__);
	dma_data = snd_soc_dai_get_dma_data(socdai, substream);
	dma_data->addr_width = ch_num >> 3;
	ch_num = params_channels(params);
	pr_debug("%s, params = %x\n", __func__, params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_3LE:
		wd_width = 24;
		ts_width = 0x00;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		wd_width = 16;
		ts_width = 0x02;
		break;
	default:
		wd_width = 0;
		ts_width = 0;
		pr_err("Unknown	data format\n");
		return -EINVAL;
	}

	//pr_debug("ts_width = %d\n", runtime->frame_bits);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		regs0->tdm_rx_cfg1 = 0x00100000;// slot	delay =	1T
		regs0->tdm_tx_cfg1 = 0x00100000;
		regs0->tdm_rx_cfg2 = 0x00010000;// FSYNC_HI_WIDTH = 1
		regs0->tdm_tx_cfg2 = 0x00010000;// FSYNC_HI_WIDTH = 1
		val = (wd_width	<< 12) | (ts_width << 8) | 0x10;//ch_num;// bit# per word = 20,	bit# per slot =	24, slot# per frame = 8
		regs0->tdm_rx_cfg3 = val;
		regs0->tdm_tx_cfg3 = val;

		pr_debug("tdm_rx_cfg1 0x%x\n", regs0->tdm_rx_cfg1);
		pr_debug("tdm_rx_cfg2 0x%x\n", regs0->tdm_rx_cfg2);
		pr_debug("tdm_rx_cfg3 0x%x\n", regs0->tdm_rx_cfg3);
	} else {
		regs0->tdm_tx_cfg1 = 0x00100000;// slot	delay =	1T, //0x00100110 word and slot right justify
		regs0->tdm_tx_cfg2 = 0x00010000;// FSYNC_HI_WIDTH = 1
		val = (wd_width	<< 12) | (ts_width << 8) | 0x10;//ch_num; hard code set	16 chs// bit# per word = 20, bit# per slot = 24, slot# per frame = 8
		regs0->tdm_tx_cfg3 = val;

		pr_debug("tdm_tx_cfg1 0x%x\n", regs0->tdm_tx_cfg1);
		pr_debug("tdm_tx_cfg2 0x%x\n", regs0->tdm_tx_cfg2);
		pr_debug("tdm_tx_cfg3 0x%x\n", regs0->tdm_tx_cfg3);
		sp_tdm_tx_dma_en(true);//Need to add here
	}
	return ret;
}

static int sp_tdm_trigger(struct snd_pcm_substream *substream, int cmd,
			  struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);
	int ret	= 0;

	pr_debug("%s IN, cmd = %d, capture = %d\n", __func__, cmd, capture);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (capture)
			sp_tdm_rx_dma_en(true);

		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (capture)
			sp_tdm_rx_dma_en(true);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (capture)
			sp_tdm_rx_dma_en(false);

		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (capture)
			sp_tdm_rx_dma_en(false);

		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sp_tdm_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	pr_debug("%s IN, operation c or p %d\n",	__func__, capture);
	//aud_tdm_clk_cfg(41100);
	if (capture)
		sp_tdm_rx_en(true);
	else
		sp_tdm_tx_en(true);

	return 0;
}

static void sp_tdm_shutdown(struct snd_pcm_substream *substream, struct	snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	pr_debug("%s \n", __func__);
	if (capture)
		sp_tdm_rx_en(false);
	else
		sp_tdm_tx_en(false);

	aud_tdm_clk_cfg(0, 0, 0);
}

static int sp_tdm_set_pll(struct snd_soc_dai *dai, int pll_id, int source, unsigned int	freq_in, unsigned int freq_out)
{
	pr_debug("%s IN, freq_out = %d\n", __func__, freq_out);
	aud_tdm_clk_cfg(pll_id, freq_in, freq_out);
	return 0;
}

static struct snd_soc_dai_ops sp_tdm_dai_ops = {
	.trigger	= sp_tdm_trigger,
	.hw_params	= sp_tdm_hw_params,
	.set_fmt	= sp_tdm_set_fmt,
	.set_pll	= sp_tdm_set_pll,
	.startup	= sp_tdm_startup,
	.shutdown	= sp_tdm_shutdown,
};

static const struct snd_soc_component_driver sp_tdm_component =	{
	.name	    = "spsoc-tdm-driver-dai",
};

static void sp_tdm_init_state(void)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) tdmaudio_base;

	regs0->tdm_rx_cfg0	= (0x0 << 8);
	regs0->tdm_rx_cfg1	= 0x00100000;
	regs0->tdm_rx_cfg2	= 0x00010000;
	regs0->tdm_rx_cfg3	= 0x00018010;

	regs0->tdm_tx_cfg0	= (0x0 << 8);
	regs0->tdm_tx_cfg1	= 0x00100000;
	regs0->tdm_tx_cfg2	= 0x00010000;
	regs0->tdm_tx_cfg3	= 0x00018010;
	regs0->tdmpdm_tx_sel	= 0x01;
}

#define	AUD_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE)
static struct snd_soc_dai_driver sp_tdm_dai = {
	.name		= "spsoc-tdm-driver-dai",
	//.id		= 0,
	.probe		= sp_tdm_dai_probe,
	.capture	= {
		.channels_min	= 2,
		.channels_max	= 16,
		.rates		= AUD_RATES_C,//SP_TDM_RATES,  //AUD_RATES_C
		.formats	= AUD_FORMATS,//SP_TDM_FMTBIT, //AUD_FORMATS
	},
	.playback	= {
		.channels_min	= 2,
		.channels_max	= 16,
		.rates		= AUD_RATES_C,
		.formats	= AUD_FORMATS,
	},
	.ops		= &sp_tdm_dai_ops
};

int sunplus_tdm_register(struct	device *dev)
{
	struct sunplus_audio_base *spauddata = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "%s \n", __func__);
	if (!of_device_is_available(dev->of_node)) {
		dev_err(dev, "devicetree status	is not available\n");
		return -ENODEV;
	}
	tdmaudio_base =	spauddata->audio_base;

	sp_tdm_init_state();

	ret = devm_snd_soc_register_component(dev, &sp_tdm_component, &sp_tdm_dai, 1);
	if (ret) {
		dev_err(dev, "Register DAI failed: %d\n", ret);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sunplus_tdm_register);

