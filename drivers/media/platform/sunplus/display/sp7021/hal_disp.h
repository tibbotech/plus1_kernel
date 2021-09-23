/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __HAL_DISP_H__
#define __HAL_DISP_H__

#include "disp_dmix.h"
#include "disp_tgen.h"
#include "disp_dve.h"
#include <media/sunplus/disp/sp7021/disp_osd.h>
#include <media/sunplus/disp/sp7021/disp_vpp.h>

#define SP_DISP_V4L2_SUPPORT
//#define SP_DISP_VPP_FIXED_ADDR
#define	SP_DISP_OSD_PARM
#define V4L2_TEST_DQBUF

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
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/
//#define SP_DISP_DEBUG

#define sp_disp_err(fmt, args...)		pr_err("[DISP][Err]"fmt, ##args)
#define sp_disp_info(fmt, args...)		pr_info("[DISP]"fmt, ##args)
#ifdef SP_DISP_DEBUG
#define sp_disp_dbg(fmt, args...)		pr_info("[DISP]"fmt, ##args)
#else
#define sp_disp_dbg(fmt, args...)
#endif

#define ALIGNED(x, n)				((x) & (~(n - 1)))
#define EXTENDED_ALIGNED(x, n)		(((x) + ((n) - 1)) & (~(n - 1)))

#define SWAP32(x)	((((unsigned int)(x)) & 0x000000ff) << 24 \
					| (((unsigned int)(x)) & 0x0000ff00) << 8 \
					| (((unsigned int)(x)) & 0x00ff0000) >> 8 \
					| (((unsigned int)(x)) & 0xff000000) >> 24)
#define SWAP16(x)	(((x) & 0x00ff) << 8 | ((x) >> 8))

#ifndef ENABLE
	#define ENABLE			1
#endif

#ifndef DISABLE
	#define DISABLE			0
#endif

#ifdef SP_DISP_V4L2_SUPPORT
#define MIN_BUFFERS				2
#define	SP_DISP_MAX_DEVICES			3
#endif
/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
struct display_size_t {
	unsigned int width;
	unsigned int height;
};

struct ttl_spec_t {
	unsigned int dts_exist;
	unsigned int clk;
	unsigned int divm, divn, divr;
	unsigned int hfp;
	unsigned int hsync;
	unsigned int hbp;
	unsigned int hactive;
	unsigned int vfp;
	unsigned int vsync;
	unsigned int vbp;
	unsigned int vactive;
	unsigned int ttl_out_enable;
	unsigned int ttl_rgb_swap;
	unsigned int ttl_clock_pol;
	unsigned int set_user_mode;
	unsigned int ttl_vpp_layer;
	unsigned int ttl_vpp_adj;
	unsigned int ttl_osd_adj;
	unsigned int ttl_parm_adj;
	unsigned int hdmi_skip_plltv;
};

#ifdef SP_DISP_V4L2_SUPPORT
enum sp_disp_device_id {
	SP_DISP_DEVICE_0,
	SP_DISP_DEVICE_1,
	SP_DISP_DEVICE_2
};
struct sp_disp_layer {
	/*for layer specific parameters */
	struct sp_disp_device	*disp_dev;		/* Pointer to the sp_disp_device */
	struct sp_disp_buffer   *cur_frm;		/* Pointer pointing to current v4l2_buffer */
	struct sp_disp_buffer   *next_frm;		/* Pointer pointing to next v4l2_buffer */
	struct vb2_queue		buffer_queue;	/* Buffer queue used in video-buf2 */
	struct list_head	    dma_queue;		/* Queue of filled frames */
	spinlock_t				irqlock;		/* Used in video-buf */
	struct video_device		video_dev;

	struct v4l2_format		fmt;			/* Used to store pixel format */
	unsigned int			usrs;			/* number of open instances of the layer */
	struct mutex			opslock;		/* facilitation of ioctl ops lock by v4l2*/
	enum sp_disp_device_id	device_id;		/* Identifies device object */
	bool					skip_first_int;	/* skip first int */
	bool					streaming;		/* layer start_streaming */
	unsigned int			sequence;

};
struct sp_fmt {
	char    *name;
	u32     fourcc;                                         /* v4l2 format id */
	int     width;
	int     height;
	int     walign;
	int     halign;
	int     depth;
	int     sol_sync;                                       /* sync of start of line */
};

struct sp_vout_layer_info {
	char                            name[32];               /* Sub device name */
	const struct sp_fmt             *formats;               /* pointer to video formats */
	int                             formats_size;           /* number of formats */
};

struct sp_disp_config {
	struct sp_vout_layer_info      *layer_devs;              /* information about each layer */
	int                             num_layerdevs;            /* Number of layer devices */
};

/* File handle structure */
struct sp_disp_fh {
	struct v4l2_fh fh;
	struct sp_disp_device *disp_dev;
	u8 io_allowed;							/* Indicates whether this file handle is doing IO */
};

/* buffer for one video frame */
struct sp_disp_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer          vb;
	struct list_head                list;
};
#endif

struct sp_disp_device {
	void *pHWRegBase;

	struct display_size_t	UIRes;
	unsigned int			UIFmt;

	struct ttl_spec_t	TTLPar;

	//OSD
	spinlock_t osd_lock;
	wait_queue_head_t osd_wait;
	unsigned int osd_field_end_protect;

	//clk
	struct clk *tgen_clk;
	struct clk *dmix_clk;
	struct clk *osd0_clk;
	struct clk *gpost0_clk;
	struct clk *vpost_clk;
	struct clk *ddfch_clk;
	struct clk *dve_clk;
	struct clk *hdmi_clk;

	struct reset_control *rstc;

	struct display_size_t	panelRes;

#ifdef SP_DISP_V4L2_SUPPORT

	#ifdef	SP_DISP_OSD_PARM
	u8		*Osd0Header;
	u8		*Osd1Header;
	u32		Osd0Header_phy;
	u32		Osd1Header_phy;
	#endif

	/* for device */
	struct v4l2_device	v4l2_dev;		/* V4l2 device */
	struct device		*pdev;			/* parent device */
	struct mutex		lock;			/* lock used to access this structure */
	spinlock_t			dma_queue_lock;	/* IRQ lock for DMA queue */

	struct sp_disp_layer	*dev[SP_DISP_MAX_DEVICES];
#endif
};

extern struct sp_disp_device *gDispWorkMem;

#endif	//__HAL_DISP_H__
