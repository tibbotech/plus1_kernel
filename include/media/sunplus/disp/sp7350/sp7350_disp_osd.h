/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for OSD layer
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_OSD_H__
#define __SP7350_DISP_OSD_H__

#include <linux/fb.h>

#define SP7350_LAYER_OSD0				0x0
#define SP7350_LAYER_OSD1				0x1
#define SP7350_LAYER_OSD2				0x2
#define SP7350_LAYER_OSD3				0x3

/*OSD_CTRL*/
#define OSD_CTRL_COLOR_MODE_RGB			BIT(10)
#define OSD_CTRL_NOT_FETCH_EN			BIT(8)
#define OSD_CTRL_CLUT_FMT_ARGB			BIT(7)
#define OSD_CTRL_LATCH_EN				BIT(5)
#define OSD_CTRL_A32B32_EN				BIT(4)
#define OSD_CTRL_FIFO_DEPTH				GENMASK(2, 0)

/*OSD_BIST_CTRL*/
#define SP7350_OSD_BIST_MASK			GENMASK(7, 6)
#define SP7350_OSD_BIST_EN(en)			FIELD_PREP(GENMASK(7, 7), en)
#define SP7350_OSD_BIST_TYPE(sel)		FIELD_PREP(GENMASK(6, 6), sel)

/*OSD_EN*/
#define SP7350_OSD_EN_MASK				GENMASK(0, 0)
#define SP7350_OSD_EN(en)				FIELD_PREP(GENMASK(0, 0), en)

/*
 * OSD Header config[0]
 */
#define SP7350_OSD_HDR_CULT				BIT(31) /* En Color Look Up Table */
#define SP7350_OSD_HDR_BS				BIT(12) /* BYTE SWAP */
/*
 *   BL =1 define HDR_ALPHA as fix value
 *   BL2=1 define HDR_ALPHA as factor value
 */
#define SP7350_OSD_HDR_BL2				BIT(10)
#define SP7350_OSD_HDR_BL				BIT(8)
#define SP7350_OSD_HDR_ALPHA			GENMASK(7, 0)

/*
 * OSD Header config[5]
 */
#define SP7350_OSD_HDR_CSM				GENMASK(19, 16) /* Color Space Mode */
#define SP7350_OSD_HDR_CSM_SET(sel)		FIELD_PREP(GENMASK(19, 16), sel)
#define SP7350_OSD_CSM_RGB_BT601		0x1 /* RGB to BT601 */
#define SP7350_OSD_CSM_BYPASS			0x4 /* Bypass */

/*
 * OSD region dirty flag for SW latch
 */
#define REGION_ADDR_DIRTY				BIT(0) //(1 << 0)
#define REGION_GSCL_DIRTY				BIT(1) //(1 << 1)

/* for sp7350_osd_header*/
#define SP7350_OSD_COLOR_MODE_8BPP			0x2
#define SP7350_OSD_COLOR_MODE_YUY2			0x4
#define SP7350_OSD_COLOR_MODE_RGB565		0x8
#define SP7350_OSD_COLOR_MODE_ARGB1555		0x9
#define SP7350_OSD_COLOR_MODE_RGBA4444		0xa
#define SP7350_OSD_COLOR_MODE_ARGB4444		0xb
#define SP7350_OSD_COLOR_MODE_RGBA8888		0xd
#define SP7350_OSD_COLOR_MODE_ARGB8888		0xe

/*
 *  sp7350_osd_header
 */
struct sp7350_osd_header {
	u32 osd_header[8];
	u32 osd_rsv[24];
};

struct sp7350_osd_region_info {
	u32 buf_width;
	u32 buf_height;
	u32 start_x;
	u32 start_y;
	u32 act_width;
	u32 act_height;
};

/*
 *  sp7350 osd region include
 *  sp7350_osd_header + sp7350_osd_palette + bitmap
 */
struct sp7350_osd_region {
	struct sp7350_osd_region_info region_info;
	u32	color_mode;	/* osd color mode */
	u32	buf_num;	/* fix 2 */
	u32	buf_align;	/* fix 4096 */

	u32	buf_addr_phy;	/* buffer address physical */
	u8	*buf_addr_vir;	/* buffer address virtual */
	//palette addr in osd header
	u8	*hdr_pal;	/* palette address virtual */
	u32	buf_size;	/* buffer size */
	u32	buf_cur_id;	/* buffer current id */

	// SW latch
	u32	dirty_flag;	/* dirty flag */
	//other side palette addr, Gearing with swap buffer.
	u8	*pal;		/* palette address virtual, for swap buffer */

	struct sp7350_osd_header *hdr;
	//structure size should be 32 alignment.
	u32	reserved[4];
};

/* for part of fb_var_screeninfo in fb_info */
struct sp7350fb_colormode {
	char name[24];
	unsigned int bits_per_pixel;
	unsigned int nonstd;		/* != 0 Non standard pixel format */
	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/
};

struct sp7350fb_info {
	/*
	 * get width/height/color_mode 
	 * from dts
	 */
	unsigned int width;		/* buffer width */
	unsigned int height;		/* buffer height */
	u32 color_mode;			/* buffer color_mode */

	/*
	 * apply bbp argb len/ofs parameter
	 * depends on color mode
	 */
	struct sp7350fb_colormode cmod;	/* buffer color_mode details */
	unsigned int buf_num;		/* buffer number, fix 2 */
	unsigned int buf_align;		/* buffer align, fix 4096 */

	u32 buf_addr_phy;			/* buffer address for sp7350_osd_layer_set only */
	dma_addr_t buf_addr;		/* buffer address */
	void __iomem *buf_addr_pal;	/* buffer address for palette */
	unsigned int buf_size;		/* buffer size */
	unsigned int handle;		/* buffer handle */
};

/*
 * Init SP7350 OSD Setting
 */
void sp7350_osd_init(void);

/*
 * Show SP7350 OSD Info
 */
void sp7350_osd_decrypt_info(void);
void sp7350_osd_resolution_chk(void);

/*
 * SP7350 OSD BIST Settings
 */
void sp7350_osd_bist_info(void);
void sp7350_osd_bist_set(int osd_layer_sel, int bist_en, int osd_bist_type);

/*
 * SP7350 OSD Layer Settings
 */
void sp7350_osd_layer_onoff(int osd_layer_sel, int onoff);

/*
 * SP7350 OSD Layer header settings
 */
void sp7350_osd_header_init(void);
void sp7350_osd_header_clear(int osd_layer_sel);
void sp7350_osd_header_show(void);
void sp7350_osd_header_save(void);
void sp7350_osd_header_restore(int osd_layer_sel);
void sp7350_osd_header_update(struct sp7350fb_info *info, int osd_layer_sel);
void sp7350_osd_layer_set(struct sp7350fb_info *info, int osd_layer_sel);
//void sp7350_osd_layer_clear(void);
/*
 * SP7350 OSD REGION functions
 */
int sp7350_osd_get_fbinfo(struct sp7350fb_info *pinfo);
int sp7350_osd_region_create(struct sp7350fb_info *pinfo);
void sp7350_osd_region_release(void);

void sp7350_osd_region_update(void);
u32 sp7350_osd_region_buf_fetch_done(u32 buf_id);
void sp7350_osd_region_wait_sync(void);

void sp7350_osd_region_irq_disable(void);
void sp7350_osd_region_irq_enable(void);

/*
 * SP7350 OSD register store/restore
 */
void sp7350_osd_store(void);
void sp7350_osd_restore(void);

#endif	//__SP7350_DISP_OSD_H__
