/*
 * spsoc-pcm.h - ALSA PCM interface for the Sunplus SoC.
 *
 *  Copyright (C) 2013 S+
 */

#ifndef _SPSOC_PCM_H
#define _SPSOC_PCM_H
#include <linux/hrtimer.h>
#include <linux/interrupt.h>


#define DRAM_PCM_BUF_LENGTH	(128 * 1024)

#define PERIOD_BYTES_MIN_CONS 	128
#define PERIOD_BYTES_MAX_CONS 	(64 * 1024)

#define NUM_FIFO_TX		8 // A0~A4, A20, A26~A27
#define NUM_FIFO_RX		8 // A22~A25, A14, A17~A18, A21	
#define NUM_FIFO		(NUM_FIFO_TX + NUM_FIFO_RX)	

#define SP_I2S_0                0
#define SP_TDM                  1
#define SP_I2S_1                2
#define SP_I2S_2                3
#define SP_SPDIF                4
#define SP_OTHER                4
#define I2S_P_INC0		(0x1 << 0)//0x1f
#define I2S_P_INC1		(0x1 << 6)
#define I2S_P_INC2		(0x1 << 19)
#define I2S_C_INC0    		(0x1 << 11)
#define I2S_C_INC1    		(0x1 << 16)
#define I2S_C_INC2    		(0x1 << 10)
#define TDMPDM_C_INC0 		((0x1 << 14) | (0x3 << 17) | (0x1 << 21) | (0xf << 22))
#define TDM_P_INC0    		((0x1 << 20) | (0x3 << 26) | 0x1f)
#define SPDIF_P_INC0  		(0x1 << 5)
#define SPDIF_C_INC0  		(0x1 << 13)

#define aud_enable_i2stdm_p	(0x01 | (0x1df << 16)) //??
#define aud_enable_i2s1_p	(0x01 << 15) 
#define aud_enable_i2s2_p	(0x01 << 13) 
#define aud_enable_i2s_c    	(0x01 << 11)
#define aud_enable_spdiftx0_p  	(0x01 << 1)
#define aud_enable_spdif_c  	(0x01 << 6)
//#define aud_enable_tdm_p    	(0x01 | (0x5f<<16))
#define aud_enable_tdmpdm_c 	(0x01 << 12)


#define aud_test_mode		(0)

#define DRAM_HDMI_BUF_LENGTH	(DRAM_PCM_BUF_LENGTH * 4)
#define MIC_Delay_Byte		(DRAM_PCM_BUF_LENGTH * 0.75)

struct spsoc_runtime_data {
	spinlock_t	lock;
	dma_addr_t 	dma_buffer;		/* physical address of dma buffer */
	dma_addr_t 	dma_buffer_end;	/* first address beyond DMA buffer */
	size_t 		period_size;
	
	struct 		hrtimer hrt;
	struct 		tasklet_struct tasklet;
	int 		poll_time_ns;
	struct snd_pcm_substream *substream;
	int 		period;
	int 		periods;
	unsigned int 	offset;
	unsigned int 	last_offset;
	unsigned int 	last_appl_ofs;
	unsigned int 	size;
	unsigned char 	trigger_flag;
	unsigned int 	start_threshold;
	unsigned char 	usemmap_flag;
	unsigned char 	start_oncetime;
	unsigned int 	last_remainder;
	unsigned int 	Oldoffset;
	unsigned int 	fifosize_from_user;
	atomic_t 	running;

};

#define AUDYA_NEW_MODE

/*--------------------------------------------------------------------------
*							IOCTL Command
*--------------------------------------------------------------------------*/
typedef enum
{
	/* fifo */
	AUDDRV_IOCTL1_FIFO_ENABLE	= 0x9001,
	AUDDRV_IOCTL1_FIFO_DISABLE,
	AUDDRV_IOCTL1_FIFO_PAUSE,
	AUDDRV_IOCTL1_FIFO_RESET,
	AUDDRV_IOCTL1_GET_FIFOFLAG,
	AUDDRV_IOCTL1_GETINFO_FIFO,
	/* gain */
	AUDDRV_IOCTL1_RAMPUP         	= 0x9101,
	AUDDRV_IOCTL1_RAMPDOWN,
	AUDDRV_IOCTL1_Mute,
	AUDDRV_IOCTL1_Demute,
	AUDDRV_IOCTL1_SET_VOLUME,
	AUDDRV_IOCTL1_GETINTO_GAIN,
	AUDDRV_IOCTL1_SET_FIFOGAIN,
	AUDDRV_IOCTL1_GET_FIFOGAIN,
	/* clock */
	AUDDRV_IOCTL1_SET_SAMPLERATE 	= 0x9201,
	AUDDRV_IOCTL1_SET_IntDAC_FS,
	AUDDRV_IOCTL1_SET_ExtDAC_FS,
	AUDDRV_IOCTL1_SET_SPDIF_FS,
	AUDDRV_IOCTL1_SET_HDMI_IEC_FS,
	AUDDRV_IOCTL1_SET_HDMI_I2S_FS,
	AUDDRV_IOCTL1_SET_FreqMask,
	AUDDRV_IOCTL1_GETINFO_FS,
	/* data format */
	AUDDRV_IOCTL1_GET_I2SCFG   	= 0x9301,
	AUDDRV_IOCTL1_SET_PCMFMT,
	/* audio pts */
	AUDDRV_IOCTL1_SET_APT        	= 0x9401,
	AUDDRV_IOCTL1_SET_PTS,
	AUDDRV_IOCTL1_GET_PTS,
	/* spdif/hdmi */
	AUDDRV_IOCTL1_Set_CMGS		= 0x9501,
	AUDDRV_IOCTL1_UpdateIEC0_ChannelStatus,
	AUDDRV_IOCTL1_UpdateIEC1_ChannelStatus,
	AUDDRV_IOCTL1_SET_SPDIF_MODE,
	AUDDRV_IOCTL1_GET_SPDIF_MODE,
	AUDDRV_IOCTL1_SET_HDMI_MODE,
	AUDDRV_IOCTL1_GET_HDMI_MODE,
	/* bluetooth */
	AUDDRV_IOCTL1_SET_BT_CFG 	= 0x9601,
	/* GRM switch */
	AUDDRV_IOCTL1_SET_VCDMIX 	= 0x9701,
	AUDDRV_IOCTL1_SET_OUTPUT_MODE,
}IOCTL_command;

typedef enum
{
	FIFO_Enable = 0,
	FIFO_Disable,
	FIFO_Pause,	
}FIFO_STATE;

typedef struct  t_AUD_FIFO_PARAMS {
	unsigned int en_flag;		// enable or disable
	unsigned int fifo_status;
	// A0~A4
	unsigned long pcmtx_virtAddrBase;		// audhw_ya (virtual address)
	dma_addr_t pcmtx_physAddrBase;		// audhw_ya (physical address)
	unsigned int pcmtx_length;
	// IEC0
	unsigned long iec0tx_virtAddrBase;
	dma_addr_t iec0tx_physAddrBase;
	unsigned int iec0tx_length;
	// IEC1
	unsigned long iec1tx_virtAddrBase;
	dma_addr_t iec1tx_physAddrBase;
	unsigned int iec1tx_length;
	// A7/A10
	unsigned long mic_virtAddrBase;
	dma_addr_t mic_physAddrBase;
	unsigned int mic_length;
	// total length
	unsigned int TxBuf_TotalLen;
	unsigned int RxBuf_TotalLen;
	unsigned int Buf_TotalLen;
}AUD_FIFO_PARAMS;

#define PCM_RAMPUP	1
#define PCM_RAMPDOWN	0

typedef struct t_AUD_GAIN_PARAMS {
	unsigned int rampflag;		// Ramp up or down
	unsigned int muteflag;
	unsigned int volumeScale;	// master gain
	unsigned int volumeVal;	//master gain
	unsigned int fifoNum;
	unsigned int fifoGain;	
}AUD_GAIN_PARAMS;

#define FS_6K   0x8000
#define FS_8K   0x5000
#define FS_11K  0x6000
#define FS_12K  0x7000
#define FS_16K  0x1000
#define FS_22K  0x2000
#define FS_24K  0x4000
#define FS_32K  0x0001
#define FS_44K  0x0002
#define FS_48K  0x0004
#define FS_64K  0x0010
#define FS_88K  0x0020
#define FS_96K  0x0040
#define FS_128K 0x0100
#define FS_176K 0x0200
#define FS_192K 0x0400
#define FS_MAX  0x8000

typedef struct t_AUD_FSCLK_PARAMS {
	unsigned int IntDAC_clk;
	unsigned int ExtDAC_clk;
	unsigned int IEC0_clk;
	unsigned int hdmi_iec_clk;
	unsigned int hdmi_i2s_clk;
	unsigned int PCM_FS;	// the sample ratre of original content stream
	unsigned int freq_mask;	// {0x0007(48K), 0x0067, 0x0667(192K)}
}AUD_FSCLK_PARAMS;

typedef enum
{
	extdac = 0x0,
	intdac,
	intadc,
	extadc,
	hdmitx,
	hdmirx,
}i2sfmt_e;

typedef struct t_AUD_I2SCFG_PARAMS {
	unsigned int path_type;	// i2sfmt_e
	unsigned int input_cfg;
	unsigned int extdac;
	unsigned int intdac;
	unsigned int intadc;
	unsigned int extadc;
	unsigned int hdmitx;
	unsigned int hdmirx;	
}AUD_I2SCFG_PARAMS;

typedef struct t_auddrv_param
{
	AUD_FIFO_PARAMS	fifoInfo;
	AUD_GAIN_PARAMS	gainInfo;
	AUD_FSCLK_PARAMS	fsclkInfo;
	AUD_I2SCFG_PARAMS	i2scfgInfo;	
	unsigned int spdif_mode;
	unsigned int hdmi_mode;
	unsigned int CGMS_mode;
} auddrv_param;

extern auddrv_param aud_param;

typedef enum
{
	FIFOSmpCnt_Init = 0,
	FIFOSmpCnt_Enable,
	FIFOSmpCnt_Disable,
	FIFOSmpCnt_Reset,
}AUDFIFOSmpCnt_e;

typedef struct t_AUD_APT_PARAMS {
	unsigned int apt_mode;
	unsigned int fs;
}AUD_APT_PARAMS;

typedef struct t_AUD_PTS_PARAMS {
	unsigned int w_pts;	// write a PTS
	unsigned int r_pts;	// read the current pts reproted by audhw
	unsigned int pcm0_ptr;	
}AUD_PTS_PARAMS;

typedef struct t_AUD_CGMS_PARAMS {
	unsigned char CGMS_type;
	unsigned char iec0_bitwidth;
	unsigned char disc_type;	// CD(0) or DVD(1)
}AUD_CGMS_PARAMS;

typedef struct t_AUD_channelstatus_PARAMS {
	unsigned int user_disc_fmt;
	unsigned int fs_id;
	unsigned short CodecCapability;
}AUD_channelstatus_PARAMS;

typedef enum
{
	bt_i2s_mode = 0,	
	bt_pcm_mode1,	// slave, short sync, LSB, 16-bit with 16-cycle, 2 slot, pcmbck=256K
	bt_pcm_mode2,	// slave, short sync, LSB, 16-bit with 16-cycle, 1 slot, pcmbck=128K
	bt_pcm_mode3,	// master, short sync, LSB, 16-bit with 16-cycle, 16 slot, pcmbck=2048K
	bt_pcm_mode4,	// master, long sync, MSB, 16-bit with 16-cycle, 2 slot, pcmbck=256K
	bt_pcm_mode5,	// master, long sync, MSB, 16-bit with 16-cycle, 1 slot, pcmbck=128K
}bluetooth_mode_e;

typedef struct t_AUD_VCDMIX_PARAMS {
	unsigned char en_mode;	// 1:enable, 0:disable
	unsigned char fifoNum;		// ex: FIFO_PCM0
}AUD_VCDMIX_PARAMS;

typedef enum
{
	Output_Stereo = 0,	// (L/R)
	Output_LL,
	Output_RR,
	Output_LRswitch,	// LR exchange	
}Output_mode_e;


#endif /* _SPSOC_PCM_H */
