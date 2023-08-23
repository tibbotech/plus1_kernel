/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Sunplus SP7350 SoC framebuffer main driver
 *
 * linux/drivers/video/sunplus/fb_sp7350/sp7350fb_main.h
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 *
 */
#ifndef __FB_SP7350_H__
#define __FB_SP7350_H__

#define SP7350_FB_RESERVED_MEM

#define SP7350_FB_PALETTE_LEN	1024

#define SP7350_DISP_ALIGN(x, n)	(((x) + ((n) - 1)) & ~((n) - 1))

struct sp7350fb_device {
	/*
	 * for framebuffer alloc/register/release
	 */	
	struct fb_info	*fb;
	/*
	 * buf_size_page = width * height * bbp >> 3
	 * color_mode : support bpp = 8, 16, 32
	 */
	int		width;
	int		height;
	int		color_mode;
	char	color_mode_name[24];
	/*
	 * buf_size_total = buf_num * buf_size_page
	 */
	int		buf_num;
	int		buf_size_page;
	int		buf_size_total;
	/*
	 * buf_mem_total: physical address for data buffer
	 * pal : physical address for palette buffer
	 */
	void __iomem	*buf_mem_total;
	void __iomem	*pal;
	/*
	 * osd_handle : for sp7350 display osd handle
	 */
	u32		osd_handle;
};

/* for sp7350fb_debug.c */
struct sp7350_bmp_header {
	/* unused define */
	unsigned short reserved0;

	/* bmp file header */
	unsigned short identifier;
	unsigned int file_size;
	unsigned int reserved1;
	unsigned int data_offset;

	/* bmp info header */
	unsigned int header_size;
	unsigned int width;
	unsigned int height;
	unsigned short planes;
	unsigned short bpp;
	unsigned int compression;
	
	/* unused define */
	unsigned short reserved;
};

/* for sp7350fb_debug.c */
unsigned int sp7350fb_chan_by_field(unsigned char chan,
	struct fb_bitfield *bf);
int sp7350fb_swapbuf(u32 buf_id, int buf_max);

extern struct fb_info *gsp_fbinfo;

#endif	/* __FB_SP7350_H__ */

