/***********************************************************************
 *  2009.07 / chuancheng.yang
 *  This file is all about audio harware initialization.
 *  include ADC/DAC/SPDIF/PLL...etc
 ***********************************************************************/
#include "audclk.h"
#include "aud_hw.h"
#include "spsoc_pcm.h"
//#include <mach/gpio_drv.h>
//#include <mach/sp_config.h>
#include "spsoc_util.h"





/******************************************************************************
	Local Defines
 *****************************************************************************/
#ifdef __LOG_NAME__
	#undef __LOG_NAME__
#endif
#define __LOG_NAME__	"audhw"

void  AUD_Set_PLL(unsigned int SAMPLE_RATE)
{
	//	 //Set PLLA
	volatile RegisterFile_G4 * regs0 = (volatile RegisterFile_G4 *)audio_plla_base;//(volatile RegisterFile_G4 *)REG(4,0);

#if 1
	if( (SAMPLE_RATE==48000) || (SAMPLE_RATE==96000) || (SAMPLE_RATE==192000))  //147.456M
	{
		regs0->rf_pad_ctl7 = ((regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFF7FF); // PLLA Disable 4.7 [11]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) &0xFFFFEFFF;	// Disable Bypass PLLA 4.7[12]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFBFFF; // 4.7 [14]
		//regs0->rf_pad_ctl7 |= (0x1 << 14);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFF3FF;	// DIVM 4.9 [10:9]

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFFC0;	// DIVN 4.9 [5:0]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | 0x5;

		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) & 0xFFFF87FF;	//PH_SEL 4.8[14:11]
		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) | (0x4<<11);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFF9FFF;	// PH_STEP_SEL 4.9[14:13]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | (0x1<<13);

		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) & 0xFFFFF800;	// K_SDM 4.10[10:0]
		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) | 0x273;

		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) & 0xFFFFF800;	// M_SDM 4.11 [10:0]
		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) | 0x3FF;

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFF3F; // DIVR 4.9 [12:11]

		regs0->rf_pad_ctl7 |= (regs0->rf_pad_ctl7 | 0xffff0000) | (1<<11); // PLLA Enable 4.7[11]
	}
	else if( (SAMPLE_RATE==44100) || (SAMPLE_RATE==88200) || (SAMPLE_RATE==176400))  // 135.4752M
	{
		regs0->rf_pad_ctl7 = ((regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFF7FF); // PLLA Disable 4.7 [11]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) &0xFFFFEFFF;	// Disable Bypass PLLA 4.7[12]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFBFFF; // 4.7 [14]
		//regs0->rf_pad_ctl7 |= (0x1 << 14);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFF3FF;	// DIVM 4.9 [10:9]

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFFC0;	// DIVN 4.9 [5:0]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | 0x5;

		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) & 0xFFFF87FF;	//PH_SEL 4.8[14:11]
		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) | (0x0<<11);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFF9FFF;	// PH_STEP_SEL 4.9[14:13]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | (0x1<<13);

		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) & 0xFFFFF800;	// K_SDM 4.10[10:0]
		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) | 0xB4;

		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) & 0xFFFFF800;	// M_SDM 4.11 [10:0]
		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) | 0x3FF;

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFF3F; // DIVR 4.9 [12:11]

		regs0->rf_pad_ctl7 |= (regs0->rf_pad_ctl7 | 0xffff0000) | (1<<11); // PLLA Enable 4.7[11]
	}
	else if( (SAMPLE_RATE==32000) || (SAMPLE_RATE==64000) || (SAMPLE_RATE==128000)) // 196.608M
	{
		regs0->rf_pad_ctl7 = ((regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFF7FF); // PLLA Disable 4.7 [11]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) &0xFFFFEFFF;	// Disable Bypass PLLA 4.7[12]

		regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFBFFF; // 4.7 [14]
		//regs0->rf_pad_ctl7 |= (0x1 << 14);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFF3FF;	// DIVM 4.9 [10:9]

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFFC0;	// DIVN 4.9 [5:0]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | 0x7;

		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) & 0xFFFF87FF;	//PH_SEL 4.8[14:11]
		regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) | (0x2<<11);

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFF9FFF;	// PH_STEP_SEL 4.9[14:13]
		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | (0x1<<13);

		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) & 0xFFFFF800;	// K_SDM 4.10[10:0]
		regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) | 0x345;

		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) & 0xFFFFF800;	// M_SDM 4.11 [10:0]
		regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) | 0x3FF;

		regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFF3F; // DIVR 4.9 [12:11]

		regs0->rf_pad_ctl7 |= (regs0->rf_pad_ctl7 | 0xffff0000) | (1<<11); // PLLA Enable 4.7[11]
	}

#endif
	return;
}




void aud_clk_cfg(unsigned int SAMPLE_RATE)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio*)REG(60,0);

   // 147M Setting
	if((SAMPLE_RATE == 44100) || (SAMPLE_RATE == 48000))
	{
	   regs0->aud_hdmi_tx_mclk_cfg = 0x6883;  //PLLA, 256FS
	   regs0->aud_ext_adc_xck_cfg = 0x6883;   //PLLA, 256FS
	   regs0->aud_ext_dac_xck_cfg = 0x6883;   //PLLA, 256FS
	   regs0->aud_int_dac_xck_cfg = 0x6887;   //PLLA, 128FS
	   regs0->aud_int_adc_xck_cfg = 0x6883;   //PLLA, 256FS
	   //regs0->aud_bt_xck_cfg      = 0x7080;   //DPLL, 256FS
	}
	else if((SAMPLE_RATE == 88200) || (SAMPLE_RATE == 96000))
	{
		regs0->aud_hdmi_tx_mclk_cfg = 0x6881;  //PLLA, 256FS
		regs0->aud_ext_adc_xck_cfg = 0x6881;   //PLLA, 256FS
		regs0->aud_ext_dac_xck_cfg = 0x6881;   //PLLA, 256FS
		regs0->aud_int_dac_xck_cfg = 0x6883;   //PLLA, 128FS
		regs0->aud_int_adc_xck_cfg = 0x6881;   //PLLA, 256FS
		//regs0->aud_bt_xck_cfg 	 = 0x7080;	 //DPLL, 256FS
	}
	else if((SAMPLE_RATE == 176400) || (SAMPLE_RATE == 192000))
	{
		regs0->aud_hdmi_tx_mclk_cfg = 0x6880;  //PLLA, 256FS
		regs0->aud_ext_adc_xck_cfg = 0x6880;   //PLLA, 256FS
		regs0->aud_ext_dac_xck_cfg = 0x6880;   //PLLA, 256FS
		regs0->aud_int_dac_xck_cfg = 0x6881;   //PLLA, 128FS
		regs0->aud_int_adc_xck_cfg = 0x6880;   //PLLA, 256FS
		//regs0->aud_bt_xck_cfg 	 = 0x7080;	 //DPLL, 256FS
	}
	else if(SAMPLE_RATE == 32000)
	{
		regs0->aud_hdmi_tx_mclk_cfg = 0x6887;  //PLLA, 256FS
		regs0->aud_ext_adc_xck_cfg = 0x6887;   //PLLA, 256FS
		regs0->aud_ext_dac_xck_cfg = 0x6887;   //PLLA, 256FS
		regs0->aud_int_dac_xck_cfg = 0x688F;   //PLLA, 128FS
		regs0->aud_int_adc_xck_cfg = 0x6887;   //PLLA, 256FS
		//regs0->aud_bt_xck_cfg 	 = 0x7080;	 //DPLL, 256FS

	}
	else if(SAMPLE_RATE == 64000)
	{
		regs0->aud_hdmi_tx_mclk_cfg = 0x6883;  //PLLA, 256FS
		regs0->aud_ext_adc_xck_cfg = 0x6883;   //PLLA, 256FS
		regs0->aud_ext_dac_xck_cfg = 0x6883;   //PLLA, 256FS
		regs0->aud_int_dac_xck_cfg = 0x6887;   //PLLA, 128FS
		regs0->aud_int_adc_xck_cfg = 0x6883;   //PLLA, 256FS
		//regs0->aud_bt_xck_cfg 	 = 0x7080;	 //DPLL, 256FS

	}
	else if(SAMPLE_RATE == 128000)
	{
		regs0->aud_hdmi_tx_mclk_cfg = 0x6881;  //PLLA, 256FS
		regs0->aud_ext_adc_xck_cfg = 0x6881;   //PLLA, 256FS
		regs0->aud_ext_dac_xck_cfg = 0x6881;   //PLLA, 256FS
		regs0->aud_int_dac_xck_cfg = 0x6883;   //PLLA, 128FS
		regs0->aud_int_adc_xck_cfg = 0x6881;   //PLLA, 256FS
		//regs0->aud_bt_xck_cfg 	 = 0x7080;	 //DPLL, 256FS

	}
   regs0->aud_hdmi_tx_bck_cfg = 0x6003;   //64FS
   regs0->aud_ext_dac_bck_cfg = 0x6003;   //64FS
   regs0->aud_int_dac_bck_cfg = 0x6001;   //64FS
   regs0->aud_ext_adc_bck_cfg = 0x6003;   //64FS
   regs0->aud_bt_bck_cfg      = 0x6007;   //32FS, 16/16, 2 slot
   regs0->aud_iec0_bclk_cfg   = 0x6001;   //XCK from EXT_DAC_XCK, 128FS
   regs0->aud_iec1_bclk_cfg   = 0x6001;   //XCK from EXT_DAC_XCK, 128FS (HDMI SPDIF)
   regs0->aud_pcm_iec_bclk_cfg= 0x6001;   //XCK from EXT_DAC_XCK, 128FS

}









void  AUDHW_Set_PLL(void)
{
	volatile RegisterFile_G4 * regs0 = (volatile RegisterFile_G4 *)audio_plla_base;//(volatile RegisterFile_G4 *)REG(4,0);
    regs0->rf_pad_ctl7 = ((regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFF7FF); // PLLA Disable 4.7 [11]
    regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) &0xFFFFEFFF;	// Disable Bypass PLLA 4.7[12]
    regs0->rf_pad_ctl7 = (regs0->rf_pad_ctl7 | 0xffff0000) & 0xFFFFBFFF; // 4.7 [14]


    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFF3FF;	// DIVM 1.9 [10:9]
    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFFC0;	// DIVN 1.9 [5:0]
    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | 0x5;

    regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) & 0xFFFF87FF;	//PH_SEL 1.8[14:11]
    regs0->rf_pad_ctl8 = (regs0->rf_pad_ctl8 | 0xffff0000) | (0x4<<11);

    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFF9FFF;	// PH_STEP_SEL 1.9[14:13]
    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) | (0x1<<13);

    regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) & 0xFFFFF800;	// K_SDM 1.10[10:0]
    regs0->rf_pad_ctl10 = (regs0->rf_pad_ctl10 | 0xffff0000) | 0x273;

    regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) & 0xFFFFF800;	// M_SDM 1.11 [10:0]
    regs0->rf_pad_ctl11 = (regs0->rf_pad_ctl11 | 0xffff0000) | 0x3FF;

    regs0->rf_pad_ctl9 = (regs0->rf_pad_ctl9 | 0xffff0000) & 0xFFFFFF3F; // DIVR 1.9 [12:11]

    regs0->rf_pad_ctl7 |= (regs0->rf_pad_ctl7 | 0xffff0000) | (1<<11); // PLLA Enable 4.7[11]
    return;
}

void AUDHW_pin_mx(void)
{
	int i;
	volatile RegisterFile_G1 * regs0 = (volatile RegisterFile_G1 *)REG(1,0);
	volatile RegisterFile_G2 * regs1 = (volatile RegisterFile_G2 *)REG(2,0);

	regs0->rf_sft_cfg2 = 0xffff0001;
	for(i=0; i<32; i++)
	{
		regs1->G002_RESERVED[i] = 0xffff0000;
	}
}


void AUDHW_clk_cfg(void)
{
    volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
    // 147M Setting
    regs0->aud_hdmi_tx_mclk_cfg = 0x6883;  //PLLA, 256FS
    regs0->aud_ext_adc_xck_cfg = 0x6883;   //PLLA, 256FS
    regs0->aud_ext_dac_xck_cfg = 0x6883;   //PLLA, 256FS
    regs0->aud_int_dac_xck_cfg = 0x6887;   //PLLA, 128FS
    regs0->aud_int_adc_xck_cfg = 0x6883;   //PLLA, 256FS

    regs0->aud_hdmi_tx_bck_cfg = 0x6003;   //64FS
    regs0->aud_ext_dac_bck_cfg = 0x6003;   //64FS
    regs0->aud_int_dac_bck_cfg = 0x6001;   //64FS
    regs0->aud_ext_adc_bck_cfg = 0x6003;   //64FS
    regs0->aud_bt_bck_cfg      = 0x6007;   //32FS, 16/16, 2 slot
    regs0->aud_iec0_bclk_cfg   = 0x6001;   //XCK from EXT_DAC_XCK, 128FS
    regs0->aud_iec1_bclk_cfg   = 0x6001;   //XCK from EXT_DAC_XCK, 128FS (HDMI SPDIF)
    regs0->aud_pcm_iec_bclk_cfg= 0x6001;   //XCK from EXT_DAC_XCK, 128FS
}

void AUDHW_Mixer_Setting(void)
{
    UINT32 val;
    volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
    //67. 0~4
    regs0->aud_grm_master_gain = 0x80000000;    //aud_grm_master_gain
    regs0->aud_grm_gain_control_0 = 0x80808080; //aud_grm_gain_control_0
    regs0->aud_grm_gain_control_1 = 0x80808080; //aud_grm_gain_control_1
    regs0->aud_grm_gain_control_2 = 0x808000;   //aud_grm_gain_control_2
    regs0->aud_grm_gain_control_3 = 0x80808080; //aud_grm_gain_control_3
    regs0->aud_grm_gain_control_4 = 0x0000007f; //aud_grm_gain_control_4


    val = 0x204;                                //1=pcm, mix75, mix73
    val = val|0x08100000;                       //1=pcm, mix79, mix77
    regs0->aud_grm_mix_control_1 = val;         //aud_grm_mix_control_1
    val = 0;
    regs0->aud_grm_mix_control_2 = val;	        //aud_grm_mix_control_2

    //EXT DAC I2S
    regs0->aud_grm_switch_0 = 0x76543210;       //aud_grm_switch_0
    regs0->aud_grm_switch_1 = 0xBA98;		    //aud_grm_switch_1

    //INT DAC I2S
    regs0->aud_grm_switch_int = 0x76543210;     //aud_grm_switch_int
    regs0->aud_grm_delta_volume = 0x8000; 	    //aud_grm_delta_volume
    regs0->aud_grm_delta_ramp_pcm = 0x8000;	    //aud_grm_delta_ramp_pcm
    regs0->aud_grm_delta_ramp_risc = 0x8000;    //aud_grm_delta_ramp_risc
    regs0->aud_grm_delta_ramp_linein = 0x8000;  //aud_grm_delta_ramp_linein
    regs0->aud_grm_other = 0x4;				    //aud_grm_other for A20
    regs0->aud_grm_switch_hdmi_tx = 0x76543210; //aud_grm_switch_hdmi_tx
}


void AUDHW_int_dac_adc_Setting(void)
{
    volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
    regs0->int_dac_ctrl1 |= (0x1<<31);	//ADAC reset (normal mode)
    regs0->int_dac_ctrl0 = 0xC41B8F5F;							//power down DA0, DA1 & DA2, enable auto sleep
    regs0->int_dac_ctrl0 |= (0x7<<23);	//DAC op power on
    regs0->int_dac_ctrl0 &= 0xffffffe0;	//DAC power on
    regs0->int_dac_ctrl0 = 0xC0201010;  // enable ref voltage
    regs0->int_dac_ctrl1 |= 0x3f;			//demute DA0, DA1 & DA2
    regs0->int_adc_ctrl |= (1<<31);		//ACODEC RESET
    regs0->int_adc_ctrl = 0x4F064F1E;							//enable ADC0 & ADC1 VREF, ADC0L pga gain +6dB
    regs0->int_adc_ctrl3 = 0x3F244F06;							//enable ADC2 VREF and
    regs0->int_adc_ctrl &= 0xF3FFF3FF;	//ADC0 & ADC1 power on
    regs0->int_adc_ctrl3 &= 0xFFFFF3FF;	//ADC2 power on
}

void AUDHW_Cfg_AdcIn(void)
{
   int val;
   volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);


   regs0->adcp_ch_enable = 0x0;     //adcp_ch_enable
   regs0->adcp_fubypass = 0x7777;   //adcp_fubypass
   regs0->adcp_risc_gain = 0x1111;  //adcp_risc_gain, all gains are 1x
   regs0->G069_reserved_00 = 0x3;   //adcprc A16~18
   val=0x650100;                    //steplen0=0, Eth_off=0x65, Eth_on=0x100, steplen0=0
   regs0->adcp_agc_cfg = val;      //adcp_agc_cfg0
}

void AUDHW_SystemInit(void)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);

    //reset aud fifo
    regs0->audif_ctrl  = 0x1;      //aud_ctrl=1
    regs0->audif_ctrl  = 0x0;      //aud_ctrl=0

    regs0->pcm_cfg = 0x4d;
    regs0->hdmi_tx_i2s_cfg = 0x4d;
    regs0->hdmi_rx_i2s_cfg = 0x4d;	// 0x14d for extenal i2s-in and CLKGENA to be master mode, 0x1c for int-adc
    regs0->ext_adc_cfg = 0x1c;
    regs0->int_adc_dac_cfg = 0x001c004d;	//0x001c004d


    regs0->iec0_par0_out = 0x40009800;      //config PCM_IEC_TX, pcm_iec_par0_out
    regs0->iec0_par1_out = 0x00000000;      //pcm_iec_par1_out

    AUDHW_Cfg_AdcIn();

    regs0->iec_cfg = 0x4002;	            //iec_cfg

    // config playback timer //
    regs0->aud_apt_mode = 1;			   // aud_apt_mode, reset mode
    regs0->aud_apt_data = 0x00f0001e;      // aud_apt_parameter, parameter for 48khz

    regs0->adcp_ch_enable  = 0x3; 		   //adcp_ch_enable, Only enable ADCP ch0&1
    regs0->aud_apt_mode = 0; 		       //clear reset of PTimer before enable FIFO

    regs0->aud_fifo_enable = 0x0f; 	       //aud_fifo_enable
    regs0->aud_enable  = 0xffff;	       //aud_enable  [21]PWM 5f

    regs0->int_dac_ctrl1 &= 0x7fffffff;
    regs0->int_dac_ctrl1 |= (0x1<<31);
    regs0->aud_fifo_mode = 0;//0x20000;
}


INT32 AUD_Set_DacAnalogGain( AUD_ChannelIdx_e tag, int pgaGain)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);

	switch( tag )
	{
		case CHANNEL_LRF:
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xffffff00) | (pgaGain<<0);
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xffff00ff) | (pgaGain<<8);
			break;
		case CHANNEL_LRS:
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0x00ffffff) |(pgaGain<<24);
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xff00ffff) |(pgaGain<<16);

			break;
		case CHANNEL_LEF:
			regs0->int_dac_ctrl1 = (regs0->int_dac_ctrl1 & 0xffff00ff) |(pgaGain<<8);

			break;
		case CHANNEL_LINK:
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xffffff00) | (pgaGain<<0);
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xffff00ff) | (pgaGain<<8);
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0x00ffffff) |(pgaGain<<24);
			regs0->int_dac_ctrl2 = (regs0->int_dac_ctrl2 & 0xff00ffff) |(pgaGain<<16);
			regs0->int_dac_ctrl1 = (regs0->int_dac_ctrl1 & 0xffff00ff) |(pgaGain<<8);
			break;
		default:
			AUD_INFO("invalid dac channel index\n");

	}
	return 0;
}



void snd_aud_config( void)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
	regs0->aud_audhwya = aud_param.fifoInfo.pcmtx_physAddrBase;

	/****************************************
	*			FIFO Allocation
	****************************************/
	regs0->aud_a0_base = 0;
	regs0->aud_a1_base = 0;//regs0->aud_a0_base + DRAM_PCM_BUF_LENGTH;
	regs0->aud_a2_base = 0;//regs0->aud_a1_base + DRAM_PCM_BUF_LENGTH;
	regs0->aud_a3_base = 0;//0->aud_a2_base + DRAM_PCM_BUF_LENGTH;
	regs0->aud_a4_base = 0;//0->aud_a3_base + DRAM_PCM_BUF_LENGTH;

	return;
}


void AUD_hw_free(void)
{
	volatile RegisterFile_Audio * regs0 = (volatile RegisterFile_Audio*)audio_base;//(volatile RegisterFile_Audio *)REG(60,0);
	regs0->aud_grm_master_gain = 0;
}


