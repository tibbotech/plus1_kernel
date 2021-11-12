/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __REG_HDMITX_H__
#define __REG_HDMITX_H__

#include <media/sunplus/hdmi/hdmitx.h>

// GROUP 380 HDMI G0
#define PWR_CTRL				0x14
#define SW_RESET				0x18
#define SYSTEM_STATUS				0x1c
#define SYSTEM_CTRL1				0x20

// GROUP 381 HDMI G1
#define VIDEO_CTRL1				0xc0
#define VIDEO_PAT_GEN1				0xd0
#define VIDEO_PAT_GEN2				0xd4
#define VIDEO_PAT_GEN3				0xd8
#define VIDEO_PAT_GEN4				0xdc
#define VIDEO_PAT_GEN5				0xe0
#define VIDEO_PAT_GEN6				0xe4
#define VIDEO_PAT_GEN7				0xe8
#define VIDEO_PAT_GEN8				0xec
#define VIDEO_PAT_GEN9				0xf0

// GROUP 382 HDMI G2
#define VIDEO_FORMAT				0x130
#define AUDIO_CTRL1				0x140
#define AUDIO_CTRL2				0x144
#define AUDIO_SPDIF_CTRL			0x148
#define AUDIO_CHNL_STS2				0x160
#define ACR_CONFIG1				0x168
#define ACR_N_VALUE1				0x16c

// GROUP 383 HDMI G3
#define INTR0_UNMASK				0x184
#define INTR1_UNMASK				0x188
#define INTR0_STS				0x190
#define INTR1_STS				0x194
#define DDC_SLV_DEVIDE_ADDR			0x1a0
#define DDC_SLV_REG_OFFSET			0x1a8
#define DDC_DATA_CNT				0x1ac
#define DDC_CMD					0x1b0
#define DDC_STS					0x1b4
#define DDC_DATA				0x1b8
#define DDC_FIFO_DATA_CNT			0x1bc

// GROUP 385 HDMI G5
#define INFO_FRAME_CTRL1			0x280
#define INFO_FRAME_CTRL2			0x284
#define AVI_INFO_FRAME01			0x298
#define AVI_INFO_FRAME23			0x29c
#define AVI_INFO_FRAME45			0x2a0
#define AVI_INFO_FRAME67			0x2a4
#define AVI_INFO_FRAME89			0x2a8
#define AVI_INFO_FRAME1011			0x2ac
#define AVI_INFO_FRAME1213			0x2b0
#define AUDIO_INFO_FRAME01			0x2b4
#define AUDIO_INFO_FRAME23			0x2b8
#define AUDIO_INFO_FRAME45			0x2bc
#define AUDIO_INFO_FRAME67			0x2c0
#define AUDIO_INFO_FRAME89			0x2c4
#define AUDIO_INFO_FRAME1011			0x2c8

// GROUP 384 HDMI G7
#define TMDSTX_CTRL1				0x3e8
#define TMDSTX_CTRL2				0x3ec
#define TMDSTX_CTRL3				0x3f0
#define TMDSTX_CTRL4				0x3f4
#define TMDSTX_CTRL5				0x3f8

// GROUP 4 MOON 4
#define PLLTV_CONTROL_REGISTER0			0x38
#define PLLTV_CONTROL_REGISTER1			0x3c
#define PLLTV_CONTROL_REGISTER2			0x40

// GROUP 5 MOON 5
#define TMDS_L2SW_CONTROL_REGISTER		0x10

#endif /* __REG_HDMITX_H__ */

