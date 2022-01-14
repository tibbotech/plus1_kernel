/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SP_MIPI_H__
#define __SP_MIPI_H__

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>

#define CSI_DRV_NAME		"sp_mipi_csi-rx"
#define MIPICSI_REG_NAME	"mipicsi"
#define CSIIW_REG_NAME		"csiiw"

#define mEXTENDED_ALIGNED(w, n)	((w%n) ? ((w/n)*n+n) : (w))
#define MIN_BUFFERS		2

/* SOL Sync */
#define SYNC_RGB565		0x22
#define SYNC_RGB888		0x24
#define SYNC_RAW8		0x2A
#define SYNC_RAW10		0x2B
#define SYNC_YUY2		0x1E

#define MIN_WIDTH	16U
#define MAX_WIDTH	2592U
#define MIN_HEIGHT	16U
#define MAX_HEIGHT	2592U

/* 
 * ------------------------------------------------------------------
 *	Basic structures
 * ------------------------------------------------------------------
 */
struct sp_mipi_format {
	u32 fourcc;
	u32 mbus_code;
	u8	bpp;
	u8	sol_sync;	/* sync of start of line */
};

struct sp_mipi_framesize {
	u32 width;
	u32 height;
};

/* buffer for one video frame */
struct sp_mipi_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;
};

struct sp_mipi_device {
	/* Protects the access of variables shared within the interrupt */
	spinlock_t		dma_queue_lock;		/* IRQ lock for DMA queue */
	struct device	*dev;		/* parent device */
	void __iomem	*mipicsi_regs;
	void __iomem	*csiiw_regs;
	struct clk		*mipicsi_clk;
	struct clk		*csiiw_clk;
	struct reset_control	*mipicsi_rstc;
	struct reset_control	*csiiw_rstc;
	unsigned int	sequence;
	struct list_head		dma_queue;		/* Queue of filled frames */
	struct sp_mipi_buffer	*cur_frm;		/* Pointer pointing to current v4l2_buffer */

	struct v4l2_device		v4l2_dev;
	struct video_device 	*vdev;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*source;
	struct v4l2_format		fmt;
	struct v4l2_rect		crop;
	bool				do_crop;

	const struct sp_mipi_format	**sd_formats;
	unsigned int			num_of_sd_formats;
	const struct sp_mipi_format	*sd_format;
	struct sp_mipi_framesize	*sd_framesizes;
	unsigned int			num_of_sd_framesizes;
	struct sp_mipi_framesize	sd_framesize;
	struct v4l2_rect		sd_bounds;

	/* Protect this data structure */
	struct mutex			lock;
	struct vb2_queue	buffer_queue;	/* Buffer queue used in video-buf2 */

	struct v4l2_fwnode_bus_mipi_csi2	bus;
	enum v4l2_mbus_type 	bus_type;

	struct media_device 	mdev;
	struct media_pad		vid_cap_pad;
	struct media_pipeline	pipeline;

	int 	fs_irq;
	int 	fe_irq;
	bool	streaming;		/* Indicates whether streaming started */
	bool	skip_first_int;
	struct gpio_desc	*cam_gpio0;
	struct gpio_desc	*cam_gpio1;
};

/* MIPI-CSI registers */
#define SP_MIPICSI_STATUS		0x00
#define SP_MIPI_DEBUG0			0x04
#define SP_MIPI_WC_LPF			0x08
#define SP_MIPI_ANALOG_CFG0		0x0c
#define SP_MIPI_ANALOG_CFG1		0x10
#define SP_MIPICSI_FSM_RST		0x14
#define SP_MIPI_ANALOG_CFG2		0x18
#define SP_MIPICSI_ENABLE		0x1c
#define SP_MIPICSI_MIX_CFG		0x20
#define SP_MIPICSI_DELAY_CTL	0x24
#define SP_MIPICSI_PACKET_SIZE		0x28
#define SP_MIPICSI_SOT_SYNCWORD		0x2c
#define SP_MIPICSI_SOF_SOL_SYNCWORD		0x30
#define SP_MIPICSI_EOF_EOL_SYNCWORD		0x34
#define SP_MIPICSI_RESERVED_A14		0x38
#define SP_MIPICSI_RESERVED_A15		0x3c
#define SP_MIPICSI_ECC_ERROR	0x40
#define SP_MIPICSI_CRC_ERROR	0x44
#define SP_MIPICSI_ECC_CFG		0x48
#define SP_MIPI_ANALOG_CFG3		0x4c
#define SP_MIPI_ANALOG_CFG4		0x50

/* MIPI-CSI-IW registers */
#define SP_CSIIW_LATCH_MODE		0x00
#define SP_CSIIW_CONFIG0		0x04
#define SP_CSIIW_BASE_ADDR		0x08
#define SP_CSIIW_STRIDE			0x0c
#define SP_CSIIW_FRAME_SIZE		0x10
#define SP_CSIIW_FRAME_BUF		0x14
#define SP_CSIIW_CONFIG1		0x18
#define SP_CSIIW_FRAME_SIZE_RO	0x1c
#endif
