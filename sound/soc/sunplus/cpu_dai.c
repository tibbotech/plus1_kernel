/*
 * ALSA	SoC AUD7021 codec driver
 *
 * Author:	 <@sunplus.com>
 *
*/

#include "aud_hw.h"
#include "spsoc_pcm.h"

void __iomem *i2saudio_base;
#define	AUD_FORMATS	(SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE|SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)|(SNDRV_PCM_FMTBIT_S24_3BE )

void aud_clk_cfg(int pll_id, int source, unsigned int SAMPLE_RATE)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio*) i2saudio_base;

	// 147M	Setting
	if((SAMPLE_RATE	== 44100) || (SAMPLE_RATE == 48000)) {
		if (source == SNDRV_PCM_STREAM_PLAYBACK) {
			regs0->aud_ext_dac_xck_cfg 			= 0x6883; //PLLA.
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6003; //64FS. 48kHz = 147Mhz/3/4/4/(64)
			} else if(pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6001; //128FS. 48kHz = 147MHz/3/4/2/(128)
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6883; //PLLA, 256FS
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg	= 0x6003; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6003; //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6001; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001;
				}
	  			//pr_info("**aud_hdmi_tx_mclk_cfg 0x%x\n",	regs0->aud_hdmi_tx_mclk_cfg);
			}
		#if 0
			regs0->aud_hdmi_tx_mclk_cfg 			= 0x6883; //PLLA, 256FS
			regs0->aud_hdmi_tx_bck_cfg 			= 0x6001; //64FS
		#endif
		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC883;
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6003; //64FS
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001; //64FS
			}
		}
	} else if((SAMPLE_RATE == 88200) || (SAMPLE_RATE == 96000)) {
		regs0->aud_ext_dac_xck_cfg 				= 0x6881; //PLLA, 256FS
		if (source == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6003; //64FS
			} else if(pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6001; //128FS
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6881; //PLLA, 256FS
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg 	= 0x6003; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6003; //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6001; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001;
				}
			}
		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC881; //256FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6003; //64FS
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001; //64FS
			}
		}
	} else if((SAMPLE_RATE == 176400) || (SAMPLE_RATE == 192000)) {
		if (source == SNDRV_PCM_STREAM_PLAYBACK){
			regs0->aud_ext_dac_xck_cfg 			= 0x6880; //256FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6003; //64FS
			} else if(pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6001; //128FS
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6880; //PLLA, 256FS
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg 	= 0x6003; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6003; //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6001; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001;
				}
			}
		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC880; //256FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6003; //64FS
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001;
			}
		}
	} else if(SAMPLE_RATE == 32000) {
		if (source == SNDRV_PCM_STREAM_PLAYBACK) {
			regs0->aud_ext_dac_xck_cfg 			= 0x6983; //128FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6001; //64FS
			} else if(pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6000; //128FS
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6983; //PLLA, 128FS
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg 	= 0x6001; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001; //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6000; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6000;
				}
			}
		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC983; //128FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001;
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6000; //64FS
			}
		}
	} else if(SAMPLE_RATE == 64000) {
		regs0->aud_ext_dac_xck_cfg 				= 0x6981;
		if (source == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6001; //64FS
			} else if (pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6000; //128FS
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6981;
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg 	= 0x6001; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001; //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6000; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6000;
				}
			}

		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC981; //128FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001; //64FS
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6000;
			}
		}
	} else if(SAMPLE_RATE == 128000) {
		regs0->aud_ext_dac_xck_cfg 				= 0x6980;
		if (source == SNDRV_PCM_STREAM_PLAYBACK) {
			if (pll_id == SP_I2S) {
				regs0->aud_ext_dac_bck_cfg 		= 0x6001; //64FS
			} else if(pll_id == SP_SPDIF) {
				regs0->aud_iec0_bclk_cfg 		= 0x6000; //128FS
			} else {
				regs0->aud_hdmi_tx_mclk_cfg 		= 0x6980;
				if (pll_id == SP_I2SHDMI) {
					regs0->aud_ext_dac_bck_cfg 	= 0x6001; //64FS. 48kHz = 147Mhz/3/4/4/(64)
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6001;   //64FS
				} else {
					regs0->aud_iec1_bclk_cfg 	= 0x6000; //128FS
					regs0->aud_hdmi_tx_bck_cfg 	= 0x6000;
				}
			}
		} else {
			regs0->aud_ext_adc_xck_cfg 			= 0xC980; //128FS
			if (pll_id == SP_I2S) {
				regs0->aud_ext_adc_bck_cfg 		= 0x6001; //64FS
			} else {
				regs0->aud_ext_adc_bck_cfg 		= 0x6000;
			}
		}
        } else {
		regs0->aud_hdmi_tx_mclk_cfg 				= 0;
		regs0->aud_ext_adc_xck_cfg  				= 0;
		regs0->aud_ext_dac_xck_cfg  				= 0;
		regs0->aud_int_dac_xck_cfg  				= 0;
		regs0->aud_int_adc_xck_cfg  				= 0;

		regs0->aud_hdmi_tx_bck_cfg  				= 0;
		regs0->aud_ext_dac_bck_cfg  				= 0;
		regs0->aud_int_dac_bck_cfg  				= 0;
		regs0->aud_ext_adc_bck_cfg  				= 0;
		regs0->aud_iec0_bclk_cfg    				= 0;
		regs0->aud_iec1_bclk_cfg    				= 0;
        }
}

void sp_i2s_spdif_tx_dma_en(int	dev_no,	bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio*) i2saudio_base;
	volatile RegisterFile_G382 *regs1 = (volatile RegisterFile_G382*) REG(382,0);

	if ((dev_no == SP_I2S) || (dev_no == SP_I2SHDMI)) {
		if (on) {
			if (dev_no == SP_I2SHDMI) {
				regs1->G382_RESERVED[18] &= 0xfffffffe;
			}
			if ((regs0->aud_fifo_enable & I2S_P_INC0)) {
				return;
			}
		        regs0->aud_fifo_enable |= I2S_P_INC0;
		        regs0->aud_fifo_reset	= I2S_P_INC0;
	                while ((regs0->aud_fifo_reset & I2S_P_INC0)) {};
		        regs0->aud_enable |= aud_enable_i2stdm_p;
	        } else {
		        regs0->aud_fifo_enable &= (~I2S_P_INC0);
		        regs0->aud_enable      &= (~aud_enable_i2stdm_p);
	        }
	} else if(dev_no == SP_SPDIF) {
		if (on) {
		        if ((regs0->aud_fifo_enable & SPDIF_P_INC0)) {
				return;
			}
		        regs0->aud_fifo_enable |= SPDIF_P_INC0;
		        regs0->aud_fifo_reset	= SPDIF_P_INC0;
	                while ((regs0->aud_fifo_reset & SPDIF_P_INC0)) {};
		        regs0->aud_enable |= aud_enable_spdiftx0_p;
	        } else {
		        regs0->aud_fifo_enable &= (~SPDIF_P_INC0);
		        regs0->aud_enable      &= (~aud_enable_spdiftx0_p);
	        }
	} else {
		if (on) {
			regs1->G382_RESERVED[18] |= 0x1;
		        if ((regs0->aud_fifo_enable & SPDIF_HDMI_P_INC0)) {
				return;
			}
		        regs0->aud_fifo_enable |= SPDIF_HDMI_P_INC0;
		        regs0->aud_fifo_reset	= SPDIF_HDMI_P_INC0;
	                while ((regs0->aud_fifo_reset & SPDIF_HDMI_P_INC0)) {};
		        regs0->aud_enable |= aud_enable_spdiftx1_p;
	        } else {
		        regs0->aud_fifo_enable &= (~SPDIF_HDMI_P_INC0);
		        regs0->aud_enable      &= (~aud_enable_spdiftx1_p);
	        }
	}

	pr_info("aud_fifo_enable 0x%x\n", regs0->aud_fifo_enable);
	pr_info("aud_enable 0x%x\n",	regs0->aud_enable);
}

void sp_i2s_spdif_rx_dma_en(int	dev_no,	bool on)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio*) i2saudio_base;

	if ((dev_no == SP_I2S) || (dev_no == SP_I2SHDMI)) {
		if (on) {
		        if ((regs0->aud_fifo_enable & I2S_C_INC0)) {
				return;
			}
		      	regs0->aud_fifo_enable |= I2S_C_INC0;
		        regs0->aud_fifo_reset	= I2S_C_INC0;
	    		while ((regs0->aud_fifo_reset & I2S_C_INC0)) {};
			regs0->aud_enable |= aud_enable_i2s_c;
	        } else {
		      regs0->aud_fifo_enable &=	(~I2S_C_INC0);
		      regs0->aud_enable	&= (~aud_enable_i2s_c);
	      	}
	} else {
	      	if (on) {
		      	if ((regs0->aud_fifo_enable & SPDIF_C_INC0)) {
			      	return;
			}
		      	regs0->aud_fifo_enable |= SPDIF_C_INC0;
		  	regs0->aud_fifo_reset	= SPDIF_C_INC0;
	    		while ((regs0->aud_fifo_reset & SPDIF_C_INC0)) {};
		  	regs0->aud_enable |= aud_enable_spdif_c;
	      	} else {
		      	regs0->aud_fifo_enable &= (~SPDIF_C_INC0);
		      	regs0->aud_enable      &= (~aud_enable_spdif_c);
	      	}
	}
	pr_info("aud_fifo_enable 0x%x\n", regs0->aud_fifo_enable);
	pr_info("aud_enable 0x%x\n",	regs0->aud_enable);
}

static int aud_cpudai_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	pr_info("%s IN\n", __func__);
	return 0;
}

static int aud_cpudai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *socdai)
{
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) i2saudio_base;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
	      	if (substream->pcm->device == SP_I2S) {
		  	regs0->G063_reserved_7 = 0x4B0; //[7:4] if0  [11:8] if1
		  	regs0->G063_reserved_7 = regs0->G063_reserved_7|0x1; // enable
	      	}
	} else {
		sp_i2s_spdif_tx_dma_en(substream->pcm->device, true);
	}

	pr_info("%s IN! G063_reserved_7 0x%x\n", __func__, regs0->G063_reserved_7);
	return 0;
}

static int aud_cpudai_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	//volatile RegisterFile_Audio	* regs0	= (volatile RegisterFile_Audio *) i2saudio_base;//(volatile	RegisterFile_Audio *)REG(60,0);
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);
    	int ret = 0;

    	pr_info("%s IN, cmd=%d, capture=%d\n", __func__, cmd, capture);

    	switch (cmd) {
    		case SNDRV_PCM_TRIGGER_START:
			if (capture) {
	    			sp_i2s_spdif_rx_dma_en(substream->pcm->device, true);
			} //else
	    		//	sp_i2s_spdif_tx_dma_en(substream->pcm->device, true);
			break;

    		case SNDRV_PCM_TRIGGER_RESUME:
    		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (capture) {
	    			sp_i2s_spdif_rx_dma_en(substream->pcm->device, true);
			} //else
	    		//	sp_i2s_spdif_tx_dma_en(substream->pcm->device, true);
			break;

    		case SNDRV_PCM_TRIGGER_STOP:
			if (capture) {
	    			sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
			} //else
	    		//	sp_i2s_spdif_tx_dma_en(substream->pcm->device, false);

			break;

    		case SNDRV_PCM_TRIGGER_SUSPEND:
    		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (capture) {
	    			sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
			} //else
	    		//	sp_i2s_spdif_tx_dma_en(substream->pcm->device, false);
			break;

    		default:
			ret = -EINVAL;
			break;
    	}
    	return ret;
}

static int spsoc_cpu_set_fmt(struct snd_soc_dai	*codec_dai,unsigned int	fmt)
{
	pr_info("%s IN\n", __func__ );

	return 0;
}

static void aud_cpudai_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai	*dai)
{
    	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

    	pr_info("%s IN\n", __func__ );
    	if (capture) {
		sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
    	} else {
		sp_i2s_spdif_tx_dma_en(substream->pcm->device, false);
	}
    	//if (substream->pcm->device == 0)
	aud_clk_cfg(0, 0, 0);
}

static int spsoc_cpu_set_pll(struct snd_soc_dai	*dai, int pll_id, int source, unsigned int freq_in, unsigned int freq_out)
{
	pr_info("%s IN %d\n", __func__, freq_out);

	aud_clk_cfg(pll_id, source, freq_out);
	return 0;
}

static const struct snd_soc_dai_ops aud_dai_cpu_ops = {
	.trigger    	= aud_cpudai_trigger,
	.hw_params  	= aud_cpudai_hw_params,
	.set_fmt	= spsoc_cpu_set_fmt,
	.set_pll	= spsoc_cpu_set_pll,
	.startup    	= aud_cpudai_startup,
	.shutdown   	= aud_cpudai_shutdown,
};

static struct snd_soc_dai_driver  aud_cpu_dai[] =
{
	{
		.name = "spsoc-i2s-dai",
		.playback = {
			.channels_min 	= 2,
			.channels_max 	= 10,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.capture = {
			.channels_min 	= 2,
			.channels_max 	= 8,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.ops=&aud_dai_cpu_ops
	},
	{
		.name = "spsoc-spdif-dai",
		.playback = {
			.channels_min 	= 1,
			.channels_max 	= 2,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.capture = {
			.channels_min 	= 1,
			.channels_max 	= 2,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.ops=&aud_dai_cpu_ops
	},
	{
		.name = "spsoc-i2s-hdmi-dai",
		.playback = {
			.channels_min 	= 1,
			.channels_max 	= 2,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.ops=&aud_dai_cpu_ops
	},
	{
		.name = "spsoc-spdif-hdmi-dai",
		.playback = {
			.channels_min 	= 1,
			.channels_max 	= 2,
			.rates 		= AUD_RATES,
			.formats 	= AUD_FORMATS,
		},
		.ops=&aud_dai_cpu_ops
	},
};

static const struct snd_soc_component_driver sunplus_cpu_component = {
	.name = "spsoc-cpu-dai",
};

int sunplus_i2s_register(struct device *dev)
{
	int ret = 0;
	struct sunplus_audio_base *spauddata = dev_get_drvdata(dev);

	dev_info(dev, "%s IN\n", __func__);
	i2saudio_base = spauddata->audio_base;
	AUDHW_pin_mx();
	AUDHW_Mixer_Setting(spauddata);
	AUDHW_SystemInit(spauddata);
	snd_aud_config(spauddata);

	pr_info("Q628 aud set done\n");

    	ret = devm_snd_soc_register_component(dev, &sunplus_cpu_component, aud_cpu_dai, ARRAY_SIZE(aud_cpu_dai));
    	return ret;
}
EXPORT_SYMBOL_GPL(sunplus_i2s_register);

