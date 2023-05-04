/*
 * ALSA SoC AUD7021 pdm driver
 *
 * Author:	 <@sunplus.com>
 *
*/

#include <sound/pcm_params.h>
#include "spsoc_pcm.h"
#include "aud_hw.h"

void __iomem *pdmaudio_base;
// Audio Registers
//#define Audio_Interface_Control           0x0000
//#define FIFO_RST_B                        BIT(0)

//#define Audio_FIFO_Request_Enable         0x0004
//#define TDM_PDM_IN_ENABLE                 BIT(12)
//#define PCM_ENABLE                        BIT(0)
//#define PCM_ENABLE                        BIT(0)

//#define Audio_DRAM_Base_Address_Offset    0x00D0
//#define Audio_Inc_0                       0x00D4
//#define Audio_Delta_0                     0x00D8

//#define Audio_FIFO_Enable                 0x00DC
#define TDM_PDM_RX3                       BIT(25)
#define TDM_PDM_RX2                       BIT(24)
#define TDM_PDM_RX1                       BIT(23)
#define TDM_PDM_RX0                       BIT(22)

//#define Host_FIFO_Reset                   0x00E8
//#define HOST_FIFO_RST                     GENMASK(29, 0)
//#define HOST_FIFO_RST_SHIFT               0

//#define PDM_RXCFG0                        0x0638
#define CFG_PDM_RGST_SET                  BIT(20)
//#define CFG_PDM_BIST_EN                   BIT(16)
//#define CFG_DALIGN_LSB                    BIT(12)
//#define PDM_RXCFG1                        0x063C
//#define PDM_RXCFG2                        0x0640

//#define PDM_RX_XCK_CFG                    0x0668    //G72.26
//#define PDM_RX_BCK_CFG                    0x066C    //G72.27
//#define TDM_PDM_TX_SEL                    0x0678

void aud_pdm_clk_cfg(unsigned int SAMPLE_RATE)
{
    	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;

   	 pr_info("%s %d\n", __func__, SAMPLE_RATE);
        //147M settings
        if(SAMPLE_RATE > 0) {
        	regs0->pdm_rx_xck_cfg = 0x6883;
    	  	regs0->pdm_rx_bck_cfg = 0x6003;
    	} else {
    	  	regs0->pdm_rx_xck_cfg = 0;
    	 	regs0->pdm_rx_bck_cfg = 0;
    	}
}

static void sp_pdm_rx_en(bool on)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;
    	unsigned long val;

    	val = regs0->pdm_rx_cfg0;
    	if (on) {
        	val |= CFG_PDM_RGST_SET;
        	regs0->tdmpdm_tx_sel = 0;
    	} else
        	val = CFG_PDM_RGST_SET;
    	regs0->pdm_rx_cfg0 = val;
    	val = regs0->pdm_rx_cfg0;
    	pr_info("pdm_rx_cfg0 0x%lx\n", val);
}

static void sp_pdm_rx_dma_en(bool on)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;
    	unsigned long val;

    	val = regs0->aud_fifo_enable;
    	if (on)
        	val |= (TDM_PDM_RX3 | TDM_PDM_RX2 | TDM_PDM_RX1 | TDM_PDM_RX0);
    	else
        	val &= ~(TDM_PDM_RX3 | TDM_PDM_RX2 | TDM_PDM_RX1 | TDM_PDM_RX0);
    	regs0->aud_fifo_enable = val;
    	val = regs0->aud_fifo_enable;
    	pr_info("aud_fifo_enable 0x%lx\n", val);

    	if (on) {
        	regs0->aud_fifo_reset = val;
        	while ((regs0->aud_fifo_reset & val));

        	//pr_info("aud_audhwya 0x%lx\n", regs0->aud_audhwya);
	     	//pr_info("aud_a23_base 0x%lx\n", regs0->aud_a23_base);
	      	//pr_info("aud_a23_length 0x%lx\n", regs0->aud_a23_length);
	      	//pr_info("aud_a23_ptr 0x%lx\n", regs0->aud_a23_ptr);
        	//pr_info("aud_a23_cnt 0x%lx\n", regs0->aud_a23_cnt);
    	}

    	val = regs0->aud_enable;
    	if (on)
        	val |= aud_enable_tdmpdm_c;
    	else
        	val &= (~aud_enable_tdmpdm_c);
    	regs0->aud_enable = val;
    	val = regs0->aud_enable;
    	pr_info("aud_enable 0x%lx\n", val);
}

#define SP_pdm_RATES    SNDRV_PCM_RATE_44100//(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)
#define SP_pdm_FMTBIT \
        (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FORMAT_MU_LAW | \
        SNDRV_PCM_FORMAT_A_LAW)

static int sp_pdm_dai_probe(struct snd_soc_dai *dai)
{
    	struct sunplus_audio_base *sp_pdm = dev_get_drvdata(dai->dev);

    	dev_info(dai->dev, "%s IN\n", __func__);

    	sp_pdm->pdm_dma_playback.maxburst = 16;
    	sp_pdm->pdm_dma_capture.maxburst = 16;
    	snd_soc_dai_init_dma_data(dai, &sp_pdm->pdm_dma_playback, &sp_pdm->pdm_dma_capture);
    	snd_soc_dai_set_drvdata(dai, sp_pdm);

    	return 0;
}

static int sp_pdm_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;
    	struct sunplus_audio_base *pdm = snd_soc_dai_get_drvdata(cpu_dai);
    	unsigned long val;

    	pr_info("%s IN, fmt=0x%x\n", __func__, fmt);
    	pdm->pdm_master = 1;

    	val = 0x04;
    	regs0->pdm_rx_cfg0 = val;

    	return 0;
}

static int sp_pdm_hw_params(struct snd_pcm_substream *substream,
                            struct snd_pcm_hw_params *params,
                            struct snd_soc_dai *socdai)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;
	struct snd_pcm_runtime *runtime = substream->runtime;
    	struct snd_dmaengine_dai_dma_data *dma_data;
    	unsigned int ch_num = 32;
    	unsigned int ret = 0;

    	pr_info("%s IN\n", __func__);
    	//pr_info("%s, stream=%d, pdm->dma_capture=0x%px\n", __func__, substream->stream, pdm->dma_capture);
    	dma_data = snd_soc_dai_get_dma_data(socdai, substream);
    	dma_data->addr_width = ch_num >> 3;
    	ch_num = params_channels(params);
    	pr_info("%s, params=%x\n", __func__, params_format(params));
    	pr_info("ts_width=%d\n", runtime->frame_bits);

    	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
    	  	regs0->pdm_rx_cfg1 = 0x76543210;
    	  	regs0->pdm_rx_cfg2 = 0;
        	pr_info("pdm_rx_cfg1 0x%x\n", regs0->pdm_rx_cfg1);
        	pr_info("pdm_rx_cfg2 0x%x\n", regs0->pdm_rx_cfg2);
    	}

    	return ret;
}

static int sp_pdm_trigger(struct snd_pcm_substream *substream, int cmd,
                          struct snd_soc_dai *dai)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;
    	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);
    	int ret = 0;

    	pr_info("%s IN, cmd=%d, capture=%d\n", __func__, cmd, capture);

    	switch (cmd) {
    		case SNDRV_PCM_TRIGGER_START:
        		if (capture) {
            			sp_pdm_rx_dma_en(true);
            			sp_pdm_rx_en(true);//Need to add this.
        		}
        		break;
    		case SNDRV_PCM_TRIGGER_RESUME:
    		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
        		if (capture)
            			sp_pdm_rx_dma_en(true);

       			 break;
    		case SNDRV_PCM_TRIGGER_STOP:
        		if (capture) {
            			sp_pdm_rx_dma_en(false);
            			//sp_pdm_rx_en(false);
        		}
        		break;
    		case SNDRV_PCM_TRIGGER_SUSPEND:
    		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
        		if (capture)
            			sp_pdm_rx_dma_en(false);

        		break;
    		default:
        		ret = -EINVAL;
        		break;
    	}
    	pr_info("TDM_PDM_TX_SEL 0x%x\n", regs0->tdmpdm_tx_sel);
    	return ret;
}

static int sp_pdm_startup(struct snd_pcm_substream *substream,
                          struct snd_soc_dai *dai)
{
    	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

    	pr_info("%s IN, operation c or p %d\n", __func__, capture);
    	if (capture)
        	sp_pdm_rx_en(true);
    	return 0;
}

static void sp_pdm_shutdown(struct snd_pcm_substream *substream,
                            struct snd_soc_dai *dai)
{
    	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

    	pr_info("%s IN\n", __func__);
    	if (capture)
        	sp_pdm_rx_en(false);
    	aud_pdm_clk_cfg(0);
}

static int sp_pdm_set_pll(struct snd_soc_dai *dai, int pll_id, int source, unsigned int freq_in, unsigned int freq_out)
{
    	pr_info("%s IN, freq_out=%d\n", __func__, freq_out);

    	aud_pdm_clk_cfg(freq_out);
    	return 0;
}

static struct snd_soc_dai_ops sp_pdm_dai_ops = {
    	.trigger	= sp_pdm_trigger,
    	.hw_params  	= sp_pdm_hw_params,
    	.set_fmt    	= sp_pdm_set_fmt,
    	.set_pll    	= sp_pdm_set_pll,
    	.startup    	= sp_pdm_startup,
    	.shutdown   	= sp_pdm_shutdown,
};

static const struct snd_soc_component_driver sp_pdm_component = {
    	.name       	= "spsoc-pdm-driver-dai",
};

static void sp_pdm_init_state(void)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*) pdmaudio_base;

	regs0->pdm_rx_cfg0 = 0;
	regs0->pdm_rx_cfg1 = 0x76543210;
	regs0->pdm_rx_cfg2 = 0;

    	pr_info("%s, TDM_RXCFG1 0x%x\n",__func__, regs0->pdm_rx_cfg1);
}

#define AUD_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE|SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_3BE )
static struct snd_soc_dai_driver sp_pdm_dai = {
    	.name   = "spsoc-pdm-driver-dai",
    	.probe	= sp_pdm_dai_probe,
    	.capture= {
        	.channels_min 	= 2,
        	.channels_max 	= 8,
        	.rates		= AUD_RATES_C,
        	.formats      	= AUD_FORMATS,
    	},
    	.ops	= &sp_pdm_dai_ops,
};

int sunplus_pdm_register(struct device *dev)
{
    	struct sunplus_audio_base *spauddata = dev_get_drvdata(dev);
    	int ret;

	dev_info(dev, "%s IN\n", __func__);
        if (!of_device_is_available(dev->of_node)) {
		dev_err(dev, "devicetree status is not available\n");
		return -ENODEV;
	}
	pdmaudio_base = spauddata->audio_base;

    	sp_pdm_init_state();

    	ret = devm_snd_soc_register_component(dev, &sp_pdm_component, &sp_pdm_dai, 1);
    	if (ret) {
        	dev_err(dev, "Register DAI failed: %d\n", ret);
        	return ret;
    	}

    	return ret;
}
EXPORT_SYMBOL_GPL(sunplus_pdm_register);

