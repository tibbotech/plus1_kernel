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
void aud_i2s_clk_cfg(unsigned int SAMPLE_RATE)
{
	  volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
    if (SAMPLE_RATE != 0){
        // 147M Setting
        regs0->aud_hdmi_tx_mclk_cfg = 0x6883;  //PLLA, 256FS
        regs0->aud_ext_adc_xck_cfg  = 0x6883;   //PLLA, 256FS
        regs0->aud_ext_dac_xck_cfg  = 0x6883;   //PLLA, 256FS
        regs0->aud_int_dac_xck_cfg  = 0x6887;   //PLLA, 128FS
        regs0->aud_int_adc_xck_cfg  = 0x6883;   //PLLA, 256FS

        regs0->aud_hdmi_tx_bck_cfg  = 0x6003;   //64FS
        regs0->aud_ext_dac_bck_cfg  = 0x6003;   //64FS
        regs0->aud_int_dac_bck_cfg  = 0x6001;   //64FS
        regs0->aud_ext_adc_bck_cfg  = 0x6003;   //64FS
        regs0->aud_bt_bck_cfg       = 0x6007;   //32FS, 16/16, 2 slot
        regs0->aud_iec0_bclk_cfg    = 0x6001;   //XCK from EXT_DAC_XCK, 128FS
        regs0->aud_iec1_bclk_cfg    = 0x6001;   //XCK from EXT_DAC_XCK, 128FS (HDMI SPDIF)
        regs0->aud_pcm_iec_bclk_cfg = 0x6001;   //XCK from EXT_DAC_XCK, 128FS
    }else{
    	  regs0->aud_hdmi_tx_mclk_cfg = 0;  //PLLA, 256FS
        regs0->aud_ext_adc_xck_cfg  = 0;   //PLLA, 256FS
        regs0->aud_ext_dac_xck_cfg  = 0;   //PLLA, 256FS
        regs0->aud_int_dac_xck_cfg  = 0;   //PLLA, 128FS
        regs0->aud_int_adc_xck_cfg  = 0;   //PLLA, 256FS

        regs0->aud_hdmi_tx_bck_cfg  = 0;   //64FS
        regs0->aud_ext_dac_bck_cfg  = 0;   //64FS
        regs0->aud_int_dac_bck_cfg  = 0;   //64FS
        regs0->aud_ext_adc_bck_cfg  = 0;   //64FS
        regs0->aud_bt_bck_cfg       = 0;   //32FS, 16/16, 2 slot
        regs0->aud_iec0_bclk_cfg    = 0;   //XCK from EXT_DAC_XCK, 128FS
        regs0->aud_iec1_bclk_cfg    = 0;   //XCK from EXT_DAC_XCK, 128FS (HDMI SPDIF)
        regs0->aud_pcm_iec_bclk_cfg = 0;   //XCK from EXT_DAC_XCK, 128FS
    }
}

void sp_i2s_spdif_tx_dma_en(int dev_no, bool on)
{
	  volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;
	  
	  if (dev_no == 0){
	      if (on){
	  	      if ((regs0->aud_fifo_enable & I2S_P_INC0))
	  	  	      return;
	  	      regs0->aud_fifo_enable = I2S_P_INC0;
	          regs0->aud_fifo_reset = I2S_P_INC0;
            while ((regs0->aud_fifo_reset & I2S_P_INC0)){};
	          regs0->aud_enable = (0x01 | (0x5f<<16));
	      }else{
	  	      regs0->aud_fifo_enable &= (~I2S_P_INC0);
	  	      regs0->aud_enable &= (~(0x01 | (0x5f<<16)));
	      }
	  }else{
	  	  if (on){
	  	      if ((regs0->aud_fifo_enable & SPDIF_P_INC0))
	  	  	      return;
	  	      regs0->aud_fifo_enable = SPDIF_P_INC0;
	          regs0->aud_fifo_reset = SPDIF_P_INC0;
            while ((regs0->aud_fifo_reset & SPDIF_P_INC0)){};
	          regs0->aud_enable = 0x2;
	      }else{
	  	      regs0->aud_fifo_enable &= (~SPDIF_P_INC0);
	  	      regs0->aud_enable &= (~(0x2));
	      }
	  }
}

void sp_i2s_spdif_rx_dma_en(int dev_no, bool on)
{
	  volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;
	  
	  if (dev_no == 0){
	      if (on){
	  	      if ((regs0->aud_fifo_enable & I2S_C_INC0))
	  	  	      return;
	  	      regs0->aud_fifo_enable = I2S_C_INC0;
	          regs0->aud_fifo_reset = I2S_C_INC0;
            while ((regs0->aud_fifo_reset & I2S_C_INC0)){};
	          //regs0->aud_enable = (0x01 | (0x5f<<16));//need to check
	      }else{
	  	      regs0->aud_fifo_enable &= (~I2S_C_INC0);
	  	      //regs0->aud_enable &= (~(0x01 | (0x5f<<16)));//need to check
	      }
	  }else{
	      if (on){
	  	      if ((regs0->aud_fifo_enable & SPDIF_C_INC0))
	  	  	      return;
	  	      regs0->aud_fifo_enable = SPDIF_C_INC0;
	          regs0->aud_fifo_reset = SPDIF_C_INC0;
            while ((regs0->aud_fifo_reset & SPDIF_C_INC0)){};
	          regs0->aud_enable = 0x200;//need to check
	      }else{
	  	      regs0->aud_fifo_enable &= (~SPDIF_C_INC0);
	  	      regs0->aud_enable &= (~(0x200));//need to check
	      }
	  }
}

static int aud_cpudai_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	  AUD_INFO("%s IN\n", __func__);
	  return 0;
}

static int aud_cpudai_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *params,
                                struct snd_soc_dai *socdai)
{
	  volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio *)audio_base;
	  
	  sp_i2s_spdif_tx_dma_en(substream->pcm->device, true);
	  regs0->G063_reserved_7 = 0x4B0; //[7:4] if0  [11:8] if1
	  regs0->G063_reserved_7 = regs0->G063_reserved_7|0x1; // enable 
	  AUD_INFO("%s IN\n", __func__);
	  return 0;
}
static int aud_cpudai_trigger(struct snd_pcm_substream *substream, int cmd,
                              struct snd_soc_dai *dai)
{
	  //volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio *)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
	  int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);
    //unsigned int val;
    int ret = 0;

    AUD_INFO("%s IN, cmd=%d, capture=%d\n", __func__, cmd, capture);

    switch (cmd){
    case SNDRV_PCM_TRIGGER_START:
        if (capture){
                sp_i2s_spdif_rx_dma_en(substream->pcm->device, true);
        }else{
                sp_i2s_spdif_tx_dma_en(substream->pcm->device, true); 
        }
        break;

    case SNDRV_PCM_TRIGGER_RESUME:
    case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
        if (capture){
            sp_i2s_spdif_rx_dma_en(substream->pcm->device, true);
        }else{
            sp_i2s_spdif_tx_dma_en(substream->pcm->device, true);  
        }
        break;

    case SNDRV_PCM_TRIGGER_STOP:
        if (capture){
            sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
        }else{
            sp_i2s_spdif_tx_dma_en(substream->pcm->device, false);
        }
        break;

    case SNDRV_PCM_TRIGGER_SUSPEND:
    case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
        if (capture){
            sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
        }else{
            sp_i2s_spdif_tx_dma_en(substream->pcm->device, false); 
        }
        break;

    default:
        ret = -EINVAL;
        break;
    }
    
    return ret; 
}

static int spsoc_cpu_set_fmt(struct snd_soc_dai *codec_dai,unsigned int fmt)
{
	  AUD_INFO("%s IN\n", __func__ );

	  return 0;
}

static void aud_cpudai_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
    //int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

    AUD_INFO("%s IN\n", __func__ );
    //if (capture)
    //    sp_i2s_spdif_rx_dma_en(substream->pcm->device, false);
    //else
    //    sp_i2s_spdif_tx_dma_en(substream->pcm->device, false);
    //aud_i2s_clk_cfg(0);//Need to check

}

static int spsoc_cpu_set_pll(struct snd_soc_dai *dai, int pll_id, int source,unsigned int freq_in, unsigned int freq_out)
{
	  AUD_INFO("%s IN\n", __func__ );
	  //AUD_Set_PLL(freq_out);
	  aud_i2s_clk_cfg(freq_out);
	  return 0;
}

static const struct snd_soc_dai_ops aud_dai_cpu_ops = {
	.trigger    = aud_cpudai_trigger,
	.hw_params  = aud_cpudai_hw_params,
	.set_fmt  	= spsoc_cpu_set_fmt,
	.set_pll	  = spsoc_cpu_set_pll,
	.startup    = aud_cpudai_startup,
	.shutdown   = aud_cpudai_shutdown,
};


static struct snd_soc_dai_driver  aud_cpu_dai[]=
{
	  {
		    .name = "spsoc-i2s-dai",
		    .playback = {
			      .channels_min = 2,
			      .channels_max = 10,
			      .rates = AUD_RATES,
			      .formats = AUD_FORMATS,
		    },
		    .capture = {
			      .channels_min = 2,
			      .channels_max = 8,
			      .rates = AUD_RATES_C,
			      .formats = AUD_FORMATS,
		    },
		    .ops=&aud_dai_cpu_ops
	  },
	  {
		    .name = "spsoc-spdif-dai",
		    .playback = {
			      .channels_min = 2,
			      .channels_max = 2,
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

static const struct snd_soc_component_driver sunplus_cpu_component = {
	  .name		= "spsoc-cpu-dai",
};

static int __devinit aud_cpu_dai_probe(struct platform_device *pdev)
{
	  int ret=0;
	  AUD_INFO("%s\n",__func__);

	  //AUDHW_Set_PLL();
	  AUDHW_pin_mx();
	  AUDHW_clk_cfg();
	  AUDHW_Mixer_Setting();
	  AUDHW_int_dac_adc_Setting();
	  AUDHW_SystemInit();
	  snd_aud_config();
	  AUD_INFO("Q628 aud set done\n");

	  //ret = devm_snd_soc_register_component(&pdev->dev,
		//			                                &sunplus_i2s_component,
		//			                                &aud_cpu_dai[0], 1);
    ret = devm_snd_soc_register_component(&pdev->dev,
					                                &sunplus_cpu_component,
					                                aud_cpu_dai,ARRAY_SIZE(aud_cpu_dai));
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
		    .name	= "spsoc-cpu-dai",
		    .owner	= THIS_MODULE,
	  },
};

static int __init aud_cpu_dai_init(void)
{
	  int ret = platform_driver_register(&aud_cpu_dai_driver);

	  spsoc_pcm_device1 = platform_device_alloc("spsoc-cpu-dai", -1);
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
