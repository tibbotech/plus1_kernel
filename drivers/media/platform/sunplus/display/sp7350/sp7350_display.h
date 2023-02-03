/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver define
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISPLAY_H__
#define __SP7350_DISPLAY_H__

/*
 * SP7350 Debug
 */
#if defined(CONFIG_VIDEO_SP7350_DISP_DEBUG)
extern struct proc_dir_entry *entry;
extern const struct proc_ops sp_disp_proc_ops;
#endif

#include <linux/types.h>
#include "sp7350_disp_dmix.h"
#include "sp7350_disp_tgen.h"
#include "sp7350_disp_tcon.h"
#include "sp7350_disp_mipitx.h"
#include "sp7350_disp_vpp.h"
#include <media/sunplus/disp/sp7350/sp7350_disp_osd.h>

#define SP_DISP_V4L2_SUPPORT
//#define SP_DISP_OSD_PARM
//#define V4L2_TEST_DQBUF

#ifdef SP_DISP_V4L2_SUPPORT
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-dma-contig.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-common.h>
#endif

#define ALIGNED(x, n)		((x) & (~(n - 1)))
#define EXTENDED_ALIGNED(x, n)	(((x) + ((n) - 1)) & (~(n - 1)))

#define SWAP32(x)	((((unsigned int)(x)) & 0x000000ff) << 24 \
			| (((unsigned int)(x)) & 0x0000ff00) << 8 \
			| (((unsigned int)(x)) & 0x00ff0000) >> 8 \
			| (((unsigned int)(x)) & 0xff000000) >> 24)
#define SWAP16(x)	(((x) & 0x00ff) << 8 | ((x) >> 8))

#define MIN_BUFFERS		2
#define	SP_DISP_MAX_DEVICES	(SP_DISP_MAX_OSD_LAYER + SP_DISP_MAX_VPP_LAYER)
#define	SP_DISP_MAX_OSD_LAYER	4
#define	SP_DISP_MAX_VPP_LAYER	1

#ifdef SP_DISP_V4L2_SUPPORT
enum sp_disp_device_id {
	SP_DISP_DEVICE_0,
	SP_DISP_DEVICE_1,
	SP_DISP_DEVICE_2,
	SP_DISP_DEVICE_3,
	SP_DISP_DEVICE_4
};
struct sp_disp_layer {
	/*for layer specific parameters */
	struct sp_disp_device	*disp_dev;		/* Pointer to the sp_disp_device */
	struct sp_disp_buffer   *cur_frm;		/* Pointer pointing to current v4l2_buffer */
	struct sp_disp_buffer   *next_frm;		/* Pointer pointing to next v4l2_buffer */
	struct vb2_queue	buffer_queue;		/* Buffer queue used in video-buf2 */
	struct list_head	dma_queue;		/* Queue of filled frames */
	spinlock_t		irqlock;		/* Used in video-buf */
	struct video_device	video_dev;

	struct v4l2_format	fmt;			/* Used to store pixel format */
	unsigned int		usrs;			/* number of open instances of the layer */
	struct mutex		opslock;		/* facilitation of ioctl ops lock by v4l2*/
	enum sp_disp_device_id	device_id;		/* Identifies device object */
	bool			skip_first_int;		/* skip first int */
	bool			streaming;		/* layer start_streaming */
	unsigned int		sequence;
};

struct sp_fmt {
	char    *name;
	u32     fourcc;		/* v4l2 format id */
	int     width;
	int     height;
	int     walign;
	int     halign;
	int     depth;
	int     sol_sync;	/* sync of start of line */
};

struct sp_vout_layer_info {
	char name[32];			/* Sub device name */
	const struct sp_fmt *formats;	/* pointer to video formats */
	int formats_size;		/* number of formats */
};

struct sp_disp_config {
	struct sp_vout_layer_info *layer_devs;	/* information about each layer */
	int num_layerdevs;			/* Number of layer devices */
};

/* File handle structure */
struct sp_disp_fh {
	struct v4l2_fh fh;
	struct sp_disp_device *disp_dev;
	u8 io_allowed;	/* Indicates whether this file handle is doing IO */
};

/* buffer for one video frame */
struct sp_disp_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer          vb;
	struct list_head                list;
};
#endif

/*
 * define for sp7350 display driver
 *   osd layer resolution info
 */
struct sp_disp_layer_osd_res {
	unsigned int x_ofs;
	unsigned int y_ofs;
	unsigned int width;
	unsigned int height;
	unsigned int color_mode;
};

/*
 * define for sp7350 display driver
 *   vpp layer resolution info
 */
struct sp_disp_layer_vpp_res {
	unsigned int x_ofs;
	unsigned int y_ofs;
	unsigned int crop_w;
	unsigned int crop_h;
	unsigned int width;
	unsigned int height;
	unsigned int color_mode;
};

/*
 * define for sp7350 display driver
 *   output resolution info
 */
struct sp_disp_out_res {
	unsigned int width;
	unsigned int height;
	unsigned int mipitx_mode;
};

/*
 * define for sp7350 display driver debug
 */
struct sp7350_debug {
	struct dentry *debugfs_dir;
	unsigned long test;
};

struct sp_disp_device {
	/*
	 * define for read/write register
	 */
	void __iomem *base;

	/* clock */
	struct clk		*disp_clk[16];
	/* reset */
	struct reset_control	*rstc;

	struct device *pdev;	/* parent device */

	/*
	 * define for OSD0/OSD1/OSD2/OSD3 layer resolution
	 * define for VPP0 layer resolution
	 * define for MIPITX output resolution
	 */
	struct sp_disp_layer_osd_res osd_res[SP_DISP_MAX_OSD_LAYER];
	struct sp_disp_layer_vpp_res vpp_res[SP_DISP_MAX_VPP_LAYER];
	struct sp_disp_out_res	out_res;

	/*
	 * define for OSD0 layer connect to sp7350fb driver
	 */
	void *osd_hdr[SP_DISP_MAX_OSD_LAYER];
	dma_addr_t osd_hdr_phy[SP_DISP_MAX_OSD_LAYER];

	spinlock_t		osd_lock;
	wait_queue_head_t	osd_wait;
	unsigned int		osd_fe_protect;

#ifdef SP_DISP_V4L2_SUPPORT
	/* for device */
	struct v4l2_device	v4l2_dev;	/* V4l2 device */
	struct mutex		lock;		/* lock used to access this structure */
	spinlock_t		dma_queue_lock;	/* IRQ lock for DMA queue */

	struct sp_disp_layer	*dev[SP_DISP_MAX_DEVICES];
#endif

#if defined(CONFIG_VIDEO_SP7350_DISP_DEBUG)
	struct sp7350_debug debug;
#endif
};

extern struct sp_disp_device *gdisp_dev;

#if IS_ENABLED(CONFIG_DEBUG_FS)
void sp7350_debug_init(struct sp_disp_device *disp_dev);
void sp7350_debug_cleanup(struct sp_disp_device *disp_dev);
#else
static inline void sp7350_debug_init(struct sp_disp_device *disp_dev)
{
}
static inline void sp7350_debug_cleanup(struct sp_disp_device *disp_dev)
{
}
#endif

#endif	//__SP7350_DISPLAY_H__
