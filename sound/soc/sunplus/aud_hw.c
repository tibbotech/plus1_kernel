/***********************************************************************
 *  2022.12 / chingchou.huang
 *  This file is all about audio harware initialization.
 *  include ADC/DAC/SPDIF/PLL...etc
 ***********************************************************************/
#include <linux/device.h>
#include "aud_hw.h"

#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
#include "spsoc_pcm.h"
#include "spsoc_util.h"
#elif IS_ENABLED(CONFIG_SND_SOC_AUD645)
#include "spsoc_pcm-645.h"
#include "spsoc_util-645.h"
#endif

/******************************************************************************
	Local Defines
 *****************************************************************************/
#ifdef __LOG_NAME__
#undef __LOG_NAME__
#endif
#define	__LOG_NAME__	"audhw"

#define	ONEHOT_B10 0x00000400
#define	ONEHOT_B11 0x00000800
#define	ONEHOT_B12 0x00001000
#if IS_MODULE(CONFIG_SND_SOC_AUD645) || IS_MODULE(CONFIG_SND_SOC_AUD628)
auddrv_param aud_param;
#endif

void AUDHW_pin_mx(void)
{
#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
	//int i;
	volatile RegisterFile_G1 *regs0 = (volatile RegisterFile_G1 *) REG(1,0);
	//volatile RegisterFile_G2 *regs1 = (volatile RegisterFile_G2 *) REG(2,0);

	//i = regs0->rf_sft_cfg1;
	//regs0->rf_sft_cfg1 = (0xFFFF0000 | i | (0x1 << 15));
	////regs0->rf_sft_cfg2 = 0xffff0050;//TDMTX/RX, PDM
	//regs0->rf_sft_cfg2 = 0xffff004d;//I2STX, PDMRX,	SPDIFRX
	AUD_INFO("***rf_sft_cfg1 %08x\n", regs0->rf_sft_cfg1);
	AUD_INFO("***rf_sft_cfg2 %08x\n", regs0->rf_sft_cfg3);

	//regs1->G002_RESERVED[1]	= 0xffff0000; // Need to set when you want to use spdif
	//for(i=0; i<64; i++)
	//{
	//	regs1->G002_RESERVED[i]	= 0xffff0000;
	//}
#endif
}
EXPORT_SYMBOL_GPL(AUDHW_pin_mx);

void AUDHW_Mixer_Setting(void *auddrvdata)
{
	struct sunplus_audio_base *pauddrvdata = auddrvdata;
        UINT32 val;
        volatile RegisterFile_Audio	*regs0 = (volatile RegisterFile_Audio*) pauddrvdata->audio_base;
        //67. 0~4
        regs0->aud_grm_master_gain	= 0x80000000;	//aud_grm_master_gain
        regs0->aud_grm_gain_control_0 	= 0x80808080;	//aud_grm_gain_control_0
        regs0->aud_grm_gain_control_1 	= 0x80808080;	//aud_grm_gain_control_1
        regs0->aud_grm_gain_control_2 	= 0x808000;	//aud_grm_gain_control_2
        regs0->aud_grm_gain_control_3 	= 0x80808080;	//aud_grm_gain_control_3
        regs0->aud_grm_gain_control_4 	= 0x0000007f;	//aud_grm_gain_control_4
        //val = 0x204;				  	//1=pcm, mix75, mix73
        //val = val|0x08100000;			  	//1=pcm, mix79, mix77
        val = 0x20402040;
        regs0->aud_grm_mix_control_1 	= val;		//aud_grm_mix_control_1
        val = 0;
        regs0->aud_grm_mix_control_2 	= val;		//aud_grm_mix_control_2
        //EXT DAC I2S
        regs0->aud_grm_switch_0 	= 0x76543210;	//aud_grm_switch_0
#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
        regs0->aud_grm_switch_1 	= 0xBA98;	//aud_grm_switch_1
#else
	regs0->aud_grm_switch_1 	= 0x98;	//aud_grm_switch_1
#endif
        //INT DAC I2S
        regs0->aud_grm_switch_int	= 0x76543210;	//aud_grm_switch_int
        regs0->aud_grm_delta_volume	= 0x8000;	//aud_grm_delta_volume
        regs0->aud_grm_delta_ramp_pcm   = 0x8000;	//aud_grm_delta_ramp_pcm
        regs0->aud_grm_delta_ramp_risc  = 0x8000;	//aud_grm_delta_ramp_risc
        regs0->aud_grm_delta_ramp_linein= 0x8000;	//aud_grm_delta_ramp_linein
#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
        regs0->aud_grm_other	     	= 0x4;		//aud_grm_other for A20
#else
	regs0->aud_grm_other	     	= 0x0;		//aud_grm_other for A20
#endif
        regs0->aud_grm_switch_hdmi_tx   = 0x76543210; //aud_grm_switch_hdmi_tx
}
EXPORT_SYMBOL_GPL(AUDHW_Mixer_Setting);

/************sub api**************/
///// utilities	of test	program	/////
void AUDHW_Cfg_AdcIn(void *auddrvdata)
{
	struct sunplus_audio_base *pauddrvdata = auddrvdata;
	int val;
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio*) pauddrvdata->audio_base;//(volatile RegisterFile_Audio *)REG(60,0);

	regs0->adcp_ch_enable   = 0x0;		//adcp_ch_enable
	regs0->adcp_fubypass	= 0x7777;	//adcp_fubypass
#if IS_ENABLED(CONFIG_SND_SOC_AUD645)
	regs0->adcp_mode_ctrl	|= 0x300;       //enable ch2/3
#endif
	regs0->adcp_risc_gain   = 0x1111;	//adcp_risc_gain, all gains are 1x
	regs0->G069_reserved_00 = 0x3;		//adcprc A16~18
	val			= 0x650100;	//steplen0=0, Eth_off=0x65, Eth_on=0x100, steplen0=0
	regs0->adcp_agc_cfg	= val;		//adcp_agc_cfg0
   //ch0
	val = (1<<6)|ONEHOT_B11;
	regs0->adcp_init_ctrl = val;
	do {
		val = regs0->adcp_init_ctrl;
	} while ((val&ONEHOT_B12) !=	0);

	val = (1<<6)|2|ONEHOT_B10;
	regs0->adcp_init_ctrl = val;

	val = 0x800000;
	regs0->adcp_gain_0 =	val;

	val = regs0->adcp_risc_gain;
	val = val&0xfff0;
	val = val | 1;
	regs0->adcp_risc_gain = val;
   //ch1
	val = (1<<6)|(1<<4)|ONEHOT_B11;
	regs0->adcp_init_ctrl = val;
	do {
		val = regs0->adcp_init_ctrl;
	} while ((val&ONEHOT_B12) !=	0);

	val = (1<<6)|(1<<4)|2|ONEHOT_B10;
	regs0->adcp_init_ctrl = val;

	val = 0x800000;
	regs0->adcp_gain_1 =	val;

	val = regs0->adcp_risc_gain;
	val = val&0xff0f;
	val = val|0x10;
	regs0->adcp_risc_gain = val;
}

void AUDHW_SystemInit(void *auddrvdata)
{
	struct sunplus_audio_base *pauddrvdata = auddrvdata;
        volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio *) pauddrvdata->audio_base;

        AUD_INFO("!!!audio_base 0x%px\n", regs0);
        AUD_INFO("!!!aud_fifo_reset	0x%px\n", &(regs0->aud_fifo_reset));
        //reset aud fifo
        regs0->audif_ctrl	= 0x1;	   //aud_ctrl=1
        AUD_INFO("aud_fifo_reset 0x%x\n", regs0->aud_fifo_reset);
        regs0->audif_ctrl	= 0x0;	   //aud_ctrl=0
        while(regs0->aud_fifo_reset);
#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
        regs0->pdm_rx_cfg2 	= 0;
        regs0->pdm_rx_cfg1 	= 0x76543210;
        regs0->pdm_rx_cfg0 	= 0x110004;
        regs0->pdm_rx_cfg0 	= 0x10004;
#endif
        
#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
	regs0->pcm_cfg	   	= 0x4d; //q645 tx0
        regs0->hdmi_tx_i2s_cfg 	= 0x4d; //q645 tx2 if tx2(slave) -> rx0 -> tx1/tx0  0x24d
        regs0->hdmi_rx_i2s_cfg 	= 0x24d; // 0x14d for extenal i2s-in and	CLKGENA	to be master mode, 0x1c	for int-adc
        regs0->int_adc_dac_cfg	= 0x001c004d;	//0x001c004d
        regs0->ext_adc_cfg	= 0x24d; //rx0
#else
	regs0->pcm_cfg	   	= 0x41; //q645 tx0
	regs0->ext_adc_cfg	= 0x41; //rx0
        regs0->hdmi_tx_i2s_cfg 	= 0x41; //q645 tx2 if tx2(slave) -> rx0 -> tx1/tx0  0x24d
	regs0->hdmi_rx_i2s_cfg 	= 0x41; //rx2
	regs0->int_adc_dac_cfg	= 0x00410041;	//0x001c004d // rx1 tx1	
#endif

        regs0->iec0_par0_out 	= 0x40009800;	//config PCM_IEC_TX, pcm_iec_par0_out
        regs0->iec0_par1_out 	= 0x00000000;	//pcm_iec_par1_out

        regs0->iec1_par0_out 	= 0x40009800;	//config PCM_IEC_TX, pcm_iec_par0_out
        regs0->iec1_par1_out 	= 0x00000000;	//pcm_iec_par1_out

        AUDHW_Cfg_AdcIn(auddrvdata);

        regs0->iec_cfg 		= 0x4066;	//iec_cfg
        // config playback timer //
        regs0->aud_apt_mode	= 1;		// aud_apt_mode, reset mode
        regs0->aud_apt_data	= 0x00f0001e;	// aud_apt_parameter, parameter for 48khz
#if IS_ENABLED(CONFIG_SND_SOC_AUD645)
        regs0->adcp_ch_enable  	= 0xf;		//adcp_ch_enable, Only enable ADCP ch2&3
        regs0->G067_reserved_30 |= 0x03;
#else
	regs0->adcp_ch_enable  	= 0x3;		//adcp_ch_enable, Only enable ADCP ch0&1
#endif
        regs0->aud_apt_mode	= 0;		//clear reset of PTimer before enable FIFO

        regs0->aud_fifo_enable 	= 0x0;		//aud_fifo_enable
        regs0->aud_enable	= 0x0;		//aud_enable  [21]PWM 5f

        regs0->int_dac_ctrl1 	&= 0x7fffffff;
        regs0->int_dac_ctrl1 	|= (0x1<<31);

        regs0->int_adc_ctrl	= 0x80000726;
        regs0->int_adc_ctrl2 	= 0x26;
        regs0->int_adc_ctrl1 	= 0x20;
        regs0->int_adc_ctrl	&= 0x7fffffff;
        regs0->int_adc_ctrl	|= (1<<31);

	regs0->aud_fifo_mode  	= 0x20001;
	//regs0->G063_reserved_7 = 0x4B0; //[7:4] if0  [11:8] if1
	//regs0->G063_reserved_7 = regs0->G063_reserved_7|0x1; // enable
	AUD_INFO("!!!aud_misc_ctrl 0x%x\n", regs0->aud_misc_ctrl);
	//regs0->aud_misc_ctrl |= 0x2;
}
EXPORT_SYMBOL_GPL(AUDHW_SystemInit);

void snd_aud_config(void *auddrvdata)
{
	struct sunplus_audio_base *pauddrvdata = auddrvdata;
	volatile RegisterFile_Audio *regs0 = (volatile RegisterFile_Audio*) pauddrvdata->audio_base;
	int dma_initial;

	/****************************************
	*			FIFO Allocation
	****************************************/
        dma_initial = DRAM_PCM_BUF_LENGTH * (NUM_FIFO_TX - 1);
	regs0->aud_audhwya 	= aud_param.fifoInfo.pcmtx_physAddrBase;
        regs0->aud_a0_base 	= dma_initial;
        regs0->aud_a1_base 	= dma_initial;
        regs0->aud_a2_base 	= dma_initial;
        regs0->aud_a3_base 	= dma_initial;
        regs0->aud_a4_base 	= dma_initial;
        regs0->aud_a5_base 	= dma_initial;
        regs0->aud_a6_base	= dma_initial;
        regs0->aud_a20_base	= dma_initial;
#if IS_ENABLED(CONFIG_SND_SOC_AUD645)
	regs0->aud_a19_base	= dma_initial;
	regs0->aud_a26_base	= dma_initial;
	regs0->aud_a27_base	= dma_initial;
#endif
        dma_initial = DRAM_PCM_BUF_LENGTH * (NUM_FIFO - 1);
        regs0->aud_a13_base	= dma_initial;
        regs0->aud_a16_base	= dma_initial;
        regs0->aud_a17_base	= dma_initial;
        regs0->aud_a18_base	= dma_initial;
        regs0->aud_a21_base	= dma_initial;
	regs0->aud_a22_base	= dma_initial;
	regs0->aud_a23_base 	= dma_initial;
	regs0->aud_a24_base 	= dma_initial;
	regs0->aud_a25_base 	= dma_initial;
#if IS_ENABLED(CONFIG_SND_SOC_AUD645)
	regs0->aud_a10_base	= dma_initial;
	regs0->aud_a11_base	= dma_initial;
	regs0->aud_a14_base	= dma_initial;
#endif
	return;
}
EXPORT_SYMBOL_GPL(snd_aud_config);



