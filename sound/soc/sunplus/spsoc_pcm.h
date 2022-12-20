/*
 * spsoc-pcm.h - ALSA PCM interface for the Sunplus SoC.
 *
 *  Copyright (C) 2022 S+
 */

#ifndef _SPSOC_PCM_H
#define _SPSOC_PCM_H

#include <linux/interrupt.h>

#define DRAM_PCM_BUF_LENGTH	(128 * 1024)

#define PERIOD_BYTES_MIN_CONS 	128
#define PERIOD_BYTES_MAX_CONS 	(64 * 1024)

#define NUM_FIFO_TX		6 // A0~A4, A20
#define NUM_FIFO_RX		4 // A22~A25
#define NUM_FIFO		(NUM_FIFO_TX + NUM_FIFO_RX)

#define SP_I2S                  0
#define SP_TDM                  1
#define SP_PDM                  2
#define SP_SPDIF                3
#define SP_I2SHDMI              4
#define SP_SPDIFHDMI            5
#define SP_OTHER                5
#define I2S_P_INC0		0x1f
#define I2S_C_INC0    		((0x7<<16) | (0x1<<21))
#define TDMPDM_C_INC0 		(0xf<<22)
#define TDM_P_INC0    		((0x1<<20) | 0x1f)
#define SPDIF_P_INC0  		(0x1<<5)
#define SPDIF_C_INC0  		(0x1<<13)
#define SPDIF_HDMI_P_INC0  	(0x1<<6)

#define aud_enable_i2stdm_p	(0x01 | (0x5f<<16))
#define aud_enable_i2s_c    	(0x1<<11)
#define aud_enable_spdiftx0_p  	(0x1<<1)
#define aud_enable_spdif_c  	(0x1<<6)
//#define aud_enable_tdm_p    	(0x01 | (0x5f<<16))
#define aud_enable_tdmpdm_c 	(0x01<<12)
#define aud_enable_spdiftx1_p  	(0x1<<8)

#define aud_test_mode		(0)

#define DRAM_HDMI_BUF_LENGTH	(DRAM_PCM_BUF_LENGTH * 4)

struct spsoc_runtime_data {
	spinlock_t	lock;
	dma_addr_t dma_buffer;		/* physical address of dma buffer */
	dma_addr_t dma_buffer_end;	/* first address beyond DMA buffer */
	size_t period_size;

	struct hrtimer hrt;
	struct tasklet_struct tasklet;
	int poll_time_ns;
	struct snd_pcm_substream *substream;
	int period;
	int periods;
	unsigned long offset;
	unsigned long last_offset;
	unsigned long last_appl_ofs;
	unsigned long size;
	unsigned char trigger_flag;
	unsigned long start_threshold;
	unsigned char usemmap_flag;
	unsigned char start_oncetime;
	unsigned int last_remainder;
	unsigned int Oldoffset;
	unsigned int fifosize_from_user;
	atomic_t running;

};

typedef struct  t_AUD_FIFO_PARAMS{
	unsigned int en_flag;		// enable or disable
	unsigned int fifo_status;
	// A0~A4
	unsigned int pcmtx_virtAddrBase;		// audhw_ya (virtual address)
	unsigned int pcmtx_physAddrBase;		// audhw_ya (physical address)
	unsigned int pcmtx_length;
	// IEC0
	unsigned int iec0tx_virtAddrBase;
	unsigned int iec0tx_physAddrBase;
	unsigned int iec0tx_length;
	// IEC1
	unsigned int iec1tx_virtAddrBase;
	unsigned int iec1tx_physAddrBase;
	unsigned int iec1tx_length;
	// A7/A10
	unsigned int mic_virtAddrBase;
	unsigned int mic_physAddrBase;
	unsigned int mic_length;
	// total length
	unsigned int TxBuf_TotalLen;
	unsigned int RxBuf_TotalLen;
	unsigned int Buf_TotalLen;
}AUD_FIFO_PARAMS;

typedef struct t_AUD_GAIN_PARAMS {
	unsigned int rampflag;		// Ramp up or down
	unsigned int muteflag;
	unsigned int volumeScale;	// master gain
	unsigned int volumeVal;	//master gain
	unsigned int fifoNum;
	unsigned int fifoGain;
}AUD_GAIN_PARAMS;

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

#endif /* _SPSOC_PCM_H */
