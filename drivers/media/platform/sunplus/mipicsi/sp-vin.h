// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Sunplus VIN
 *
 * Copyright Sunplus Technology Co., Ltd.
 * 	  All rights reserved.
 *
 * Based on Renesas R-Car VIN driver
 */

#ifndef __SP_VIN__
#define __SP_VIN__

#include <linux/kref.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>


/* Compliler switch */
//#define MC_MODE_DEFAULT	/* Set link by default to test MC mode */
//#define MIPI_CSI_BIST		/* Enable MIPI-CSI BIST function */
//#define MIPI_CSI_XTOR		/* Import RAW10 pattern from MIPI XTOR */
//#define MIPI_CSI_4VC		/* Switch MIPI-CSI2 to 4-VC mode */

/* Number of HW buffers */
#define HW_BUFFER_NUM 1

/* Address alignment mask for HW buffers */
#define HW_BUFFER_MASK 0x7f

/* Max number on VIN instances that can be in a system */
#define VIN_MAX_NUM 14

struct vin_group;

enum model_id {
	SP_Q628,
	SP_Q645,
	SP_Q654,
};

enum vin_csi_id {
	VIN_CSI0,		/* CSIRX0_CSIIW0 */
	VIN_CSI1,		/* CSIRX0_CSIIW1 */
	VIN_CSI2,		/* CSIRX1_CSIIW0 */
	VIN_CSI3,		/* CSIRX1_CSIIW1 */
	VIN_CSI4,		/* CSIRX23_CSIIW0 */
	VIN_CSI5,		/* CSIRX23_CSIIW1 */
	VIN_CSI6,		/* CSIRX23_CSIIW2 */
	VIN_CSI7,		/* CSIRX23_CSIIW3 */
	VIN_CSI8,		/* CSIRX4_CSIIW0 */
	VIN_CSI9,		/* CSIRX4_CSIIW1 */
	VIN_CSI10,		/* CSIRX5_CSIIW0 */
	VIN_CSI11,		/* CSIRX5_CSIIW1 */
	VIN_CSI12,		/* CSIRX5_CSIIW2 */
	VIN_CSI13,		/* CSIRX5_CSIIW3 */
	VIN_CSI_MAX,
};

/**
 * STOPPED  - No operation in progress
 * STARTING - Capture starting up
 * RUNNING  - Operation in progress have buffers
 * STOPPING - Stopping operation
 */
enum vin_dma_state {
	STOPPED = 0,
	STARTING,
	RUNNING,
	STOPPING,
};

/**
 * enum vin_buffer_type
 *
 * Describes how a buffer is given to the hardware. To be able
 * to capture SEQ_TB/BT it's needed to capture to the same vb2
 * buffer twice so the type of buffer needs to be kept.
 *
 * FULL - One capture fills the whole vb2 buffer
 * HALF_TOP - One capture fills the top half of the vb2 buffer
 * HALF_BOTTOM - One capture fills the bottom half of the vb2 buffer
 */
enum vin_buffer_type {
	FULL,
	HALF_TOP,
	HALF_BOTTOM,
};

/**
 * struct vin_video_format - Data format stored in memory
 * @fourcc:	Pixelformat
 * @bpp:	Bits per pixel
 * @bpc:	Bits per color channel
 */
struct vin_video_format {
	u32 fourcc;
	u32 mbus_code;
	u8 bpp;			/* Bits per pixel */
	u8 bpc;			/* Bits per color channel */
};

struct vin_video_framesize {
	u32 width;
	u32 height;
};

/**
 * struct vin_group_route - describes a route from a channel of a
 *	CSI-2 receiver to a VIN
 *
 * @csi:	CSI-2 receiver ID.
 * @channel:	Output channel of the CSI-2 receiver.
 * @vin:	VIN ID.
 * @mask:	Bitmask of the different CHSEL register values that
 *		allow for a route from @csi + @chan to @vin.
 *
 * .. note::
 *	Each R-Car CSI-2 receiver has four output channels facing the VIN
 *	devices, each channel can carry one CSI-2 Virtual Channel (VC).
 *	There is no correlation between channel number and CSI-2 VC. It's
 *	up to the CSI-2 receiver driver to configure which VC is output
 *	on which channel, the VIN devices only care about output channels.
 *
 *	There are in some cases multiple CHSEL register settings which would
 *	allow for the same route from @csi + @channel to @vin. For example
 *	on R-Car H3 both the CHSEL values 0 and 3 allow for a route from
 *	CSI40/VC0 to VIN0. All possible CHSEL values for a route need to be
 *	recorded as a bitmask in @mask, in this example bit 0 and 3 should
 *	be set.
 */
struct vin_group_route {
	enum vin_csi_id csi;
	unsigned int channel;
	unsigned int vin;
	unsigned int mask;
};

/**
 * struct vin_info - Information about the particular VIN implementation
 * @model:		VIN model
 * @use_mc:		use media controller instead of controlling subdevice
 * @nv12:		support outputing NV12 pixel format
 * @max_width:		max input width the VIN supports
 * @max_height:		max input height the VIN supports
 * @routes:		list of possible routes from the CSI-2 recivers to
 *			all VINs. The list mush be NULL terminated.
 */
struct vin_info {
	enum model_id model;
	bool use_mc;
	bool nv12;

	unsigned int max_width;
	unsigned int max_height;
	const struct vin_group_route *routes;
};

/**
 * struct vin_dev - Sunplus VIN device structure
 * @dev:		(OF) device
 * @base:		device I/O register space remapped to virtual memory
 * @info:		info about VIN instance
 *
 * @vdev:		V4L2 video device associated with VIN
 * @v4l2_dev:		V4L2 device
 * @ctrl_handler:	V4L2 control handler
 * @notifier:		V4L2 asynchronous subdevs notifier
 *
 * @group:		Gen3 CSI group
 * @id:			Gen3 group id for this VIN
 * @pad:		media pad for the video device entity
 *
 * @lock:		protects @queue
 * @queue:		vb2 buffers queue
 * @scratch:		cpu address for scratch buffer
 * @scratch_phys:	physical address of the scratch buffer
 *
 * @qlock:		protects @buf_hw, @buf_list, @sequence and @state
 * @buf_hw:		Keeps track of buffers given to HW slot
 * @buf_list:		list of queued buffers
 * @sequence:		V4L2 buffers sequence number
 * @state:		keeps track of operation state
 *
 * @is_csi:		flag to mark the VIN as using a CSI-2 subdevice
 *
 * @mbus_code:		media bus format code
 * @format:		active V4L2 pixel format
 *
 * @crop:		active cropping
 * @compose:		active composing
 * @src_rect:		active size of the video source
 * @std:		active video standard of the video source
 *
 * @alpha:		Alpha component to fill in for supported pixel formats
 */
struct vin_dev {
	struct device *dev;
	void __iomem *base;
	const struct vin_info *info;
	struct clk *clk;
	struct reset_control *rstc;

	struct video_device vdev;
	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler ctrl_handler;

	struct vin_group *group;
	unsigned int id;
	struct media_pad pad;

	struct mutex lock;
	struct vb2_queue queue;
	void *scratch;
	dma_addr_t scratch_phys;

	spinlock_t qlock;
	struct {
		struct vb2_v4l2_buffer *buffer;
		enum vin_buffer_type type;
		dma_addr_t phys;
	} buf_hw[HW_BUFFER_NUM];
	struct list_head buf_list;
	unsigned int sequence;
	enum vin_dma_state state;

	const struct vin_video_format **sd_formats;
	unsigned int num_of_sd_formats;
	const struct vin_video_format *sd_format;
	struct vin_video_framesize *sd_framesizes;
	unsigned int num_of_sd_framesizes;
	struct vin_video_framesize sd_framesize;

	u32 mbus_code;
	struct v4l2_format fmt;
	struct v4l2_pix_format format;
	const struct vin_video_format *vin_fmt;

	struct v4l2_rect crop;
	struct v4l2_rect compose;
	struct v4l2_rect src_rect;

	unsigned int alpha;
};

/* Debug */
#define vin_dbg(d, fmt, arg...)		dev_dbg(d->dev, fmt, ##arg)
#define vin_info(d, fmt, arg...)	dev_info(d->dev, fmt, ##arg)
#define vin_warn(d, fmt, arg...)	dev_warn(d->dev, fmt, ##arg)
#define vin_err(d, fmt, arg...)		dev_err(d->dev, fmt, ##arg)

/**
 * struct vin_group - VIN CSI2 group information
 * @refcount:		number of VIN instances using the group
 *
 * @mdev:		media device which represents the group
 *
 * @lock:		protects the count, notifier, vin and csi members
 * @count:		number of enabled VIN instances found in DT
 * @notifier:		group notifier for CSI-2 async subdevices
 * @vin:		VIN instances which are part of the group
 * @csi:		array of pairs of fwnode and subdev pointers
 *			to all CSI-2 subdevices.
 */
struct vin_group {
	struct kref refcount;

	struct media_device mdev;

	struct mutex lock;
	unsigned int count;
	struct v4l2_async_notifier notifier;
	struct vin_dev *vin[VIN_MAX_NUM];

	struct {
		struct fwnode_handle *fwnode;
		struct v4l2_subdev *subdev;
	} csi[VIN_CSI_MAX];
};

void vin_dma_init(struct vin_dev *vin);
int vin_dma_register(struct vin_dev *vin, int fs_irq, int fe_irq);
void vin_dma_unregister(struct vin_dev *vin);

int vin_v4l2_formats_init(struct vin_dev *vin);
int vin_v4l2_framesizes_init(struct vin_dev *vin);
int vin_v4l2_set_default_fmt(struct vin_dev *vin);
int vin_v4l2_register(struct vin_dev *vin);
void vin_v4l2_unregister(struct vin_dev *vin);

const struct vin_video_format *vin_format_from_pixel(struct vin_dev *vin,
						       u32 pixelformat);


/* Cropping, composing and scaling */
void vin_crop_scale_comp(struct vin_dev *vin);

int vin_set_channel_routing(struct vin_dev *vin, u8 chsel);
void vin_set_alpha(struct vin_dev *vin, unsigned int alpha);

#endif
