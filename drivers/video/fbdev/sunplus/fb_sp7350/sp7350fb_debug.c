/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Sunplus SP7350 SoC framebuffer main driver
 *
 * linux/drivers/video/sunplus/fb_sp7350/sp7350fb_debug.c
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 *
 */
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/uaccess.h>
//#include <asm/cacheflush.h>
#include <media/sunplus/disp/sp7350/sp7350_disp_osd.h>
#include "sp7350fb_main.h"

#define SP7350_FB_LOAD_BMP
#define SP7350_FB_LOAD_FONT
#define SP7350_FB_DBG_CMD_LEN		256

#ifdef SP7350_FB_LOAD_FONT
#include "sp7350fb_font_8x8.h"
#include "sp7350fb_font_8x16.h"
#include "sp7350fb_font_16x32.h"
#endif

static unsigned int sp_color_bar[] = {
	0xffffffff,	/* white */
	0xffffff00,	/* yellow */
	0xff00ffff,	/* cyan */
	0xff00ff00,	/* green */
	0xffff00ff,	/* magenta */
	0xffff0000,	/* red */
	0xff0000ff,	/* blue */
	0xff000000,	/* black */
	0x80000000,	/* gray */
	0xffff0000,	/* red left side border */
	0xffffffff,	/* white right side border */
};

#ifdef SP7350_FB_LOAD_FONT
unsigned int sp_font_color = 0xffffffff; /* white */
unsigned int sp_font_color_bg = 0xff000000; /* black */
int sp_font_width = 8;
int sp_font_height = 16;
#endif

static void sp7350_device_info(struct fb_info *fbinfo)
{
	struct sp7350fb_device *fbdev =
		(struct sp7350fb_device *)fbinfo->par;

	if (!fbdev)
		return;

	pr_info("fbdev dump\n");
	pr_info("  fbdev %dx%d, cmod %d(%s)\n",
		fbdev->width,
		fbdev->height,
		fbdev->color_mode,
		fbdev->color_mode_name);
	pr_info("  fbdev page %d align to %d\n",
		fbdev->width *
		fbdev->height *
		(fbinfo->var.bits_per_pixel >> 3),
		fbdev->buf_size_page);
	pr_info("  fbdev buf %dx%d = %d\n",
		fbdev->buf_size_page,
		fbdev->buf_num,
		fbdev->buf_size_total);
	#ifdef SP7350_FB_RESERVED_MEM
	pr_info("  fbdev buf %px(%lx)\n",
		fbdev->buf_mem_total,
		fbinfo->fix.smem_start); //memory-region
	#else
	pr_info("  fbdev buf %px(%llx)\n",
		fbdev->buf_mem_total,
		virt_to_phys(fbdev->buf_mem_total)); //normal
	#endif
	pr_info("  fbdev pal %px(%llx)\n",
		fbdev->pal,
		virt_to_phys(fbdev->pal));
	pr_info("  fbdev osd_handle %d\n", fbdev->osd_handle);

	pr_info("fb_info fix \n");
	pr_info("  id [%s]\n", fbinfo->fix.id);
	pr_info("  smem addr/len 0x%lx %d\n",
		fbinfo->fix.smem_start,
		fbinfo->fix.smem_len);
	pr_info("  mmio addr/len 0x%lx %d\n",
		fbinfo->fix.mmio_start,
		fbinfo->fix.mmio_len);
	pr_info("  step xpan/ypan/ywrap %d %d %d\n",
		fbinfo->fix.xpanstep,
		fbinfo->fix.ypanstep,
		fbinfo->fix.ywrapstep);
	pr_info("  line len %d\n", fbinfo->fix.line_length);

	pr_info("fb_info var \n");
	pr_info("  reso (%d %d) %d %d %d %d\n",
		fbinfo->var.xoffset,
		fbinfo->var.yoffset,
		fbinfo->var.xres,
		fbinfo->var.yres,
		fbinfo->var.xres_virtual,
		fbinfo->var.yres_virtual);
	pr_info("  bit bpp %d nonstd %d\n",
		fbinfo->var.bits_per_pixel,
		fbinfo->var.nonstd);
	pr_info("  bit len/ofs (%d %d %d %d)(%d %d %d %d)\n",
		fbinfo->var.red.length,
		fbinfo->var.green.length,
		fbinfo->var.blue.length,
		fbinfo->var.transp.length,
		fbinfo->var.red.offset,
		fbinfo->var.green.offset,
		fbinfo->var.blue.offset,
		fbinfo->var.transp.offset);
	pr_info("  bit     msb (%d %d %d %d)\n",
		fbinfo->var.red.msb_right,
		fbinfo->var.green.msb_right,
		fbinfo->var.blue.msb_right,
		fbinfo->var.transp.msb_right);
	pr_info("  pixclk %d h/vsync_len %d %d\n",
		fbinfo->var.pixclock,
		fbinfo->var.hsync_len,
		fbinfo->var.vsync_len);
	pr_info("  margin lef/rig/upp/low %d %d %d %d\n",
		fbinfo->var.left_margin,
		fbinfo->var.right_margin,
		fbinfo->var.upper_margin,
		fbinfo->var.lower_margin);

	pr_info("fbinfo dump\n");
	pr_info("  screen base 0x%px\n", fbinfo->screen_base);
	pr_info("  screen buf  0x%px\n", fbinfo->screen_buffer);
	pr_info("  screen size %ld\n", fbinfo->screen_size);
	pr_info("  palette base 0x%px\n", fbinfo->pseudo_palette);

}

void sp7350_rgb2yuv(int argb, int *y, int *u, int *v)
{
	int r, g, b;

	r = (argb >> 16) & 0xff;
	g = (argb >> 8) & 0xff;
	b = (argb >> 0) & 0xff;
	*y = ((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16;
	*u = ((-r * 38 - g * 74 + b * 112 + 128) >> 8) + 128;
	*v = ((r * 112 - g * 94 - b * 18 + 128) >> 8) + 128;
	*y = (*y < 0) ? 0 : ((*y > 255) ? 255 : *y);
	*u = (*u < 0) ? 0 : ((*u > 255) ? 255 : *u);
	*v = (*v < 0) ? 0 : ((*v > 255) ? 255 : *v);
}

static void sp7350_gen_colorbar_data(struct fb_info *info, char *ptr,
	int id, int x, int y)
{
	struct sp7350fb_device *fbdev =
		(struct sp7350fb_device *)info->par;
	u32 color[sizeof(sp_color_bar)/sizeof(u32)];
	int level, idx;
	int yy, uu, vv;
	u32 w, h;
	int k;

	w = info->var.xres;
	h = info->var.yres;
	level = w >> 3;

	/* Re-assign argb data based on osd color mode */
	if (info->var.nonstd) {
		for (k = 0; k < sizeof(color) / sizeof(u32); ++k) {
			sp7350_rgb2yuv(sp_color_bar[k], &yy, &uu, &vv);
			color[k] = (yy << 16) | (uu << 8) | (vv << 0);
			//pr_info("color[%d] = 0x%08x\n", k, color[k]);
		}
	} else {
		for (k = 0; k < sizeof(color) / sizeof(u32); ++k) {
			color[k] = sp7350fb_chan_by_field(
				sp_color_bar[k] >> 24,
				&info->var.transp);
			color[k] |= sp7350fb_chan_by_field(
				sp_color_bar[k] >> 16,
				&info->var.red);
			color[k] |= sp7350fb_chan_by_field(
				sp_color_bar[k] >> 8,
				&info->var.green);
			color[k] |= sp7350fb_chan_by_field(
				sp_color_bar[k] >> 0,
				&info->var.blue);
			//pr_info("color[%d] = 0x%08x\n", k, color[k]);
		}
	}

	if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP) {
		if ((y == 0) || (y == (h - 1)) || (x == 0) || (x == (w - 1))) {
			*(ptr++) = 9 + ((id & 0x1) ^ (x < (w >> 1)));
		} else {
			if (id & 0x1)
				*(ptr++) = (x / level);
			else
				*(ptr++) = ((w - 1 - x) / level);
		}
		memcpy(info->pseudo_palette, color, sizeof(color));
	} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2) {
		if ((y == 0) || (y == (h - 1)) || (x == 0) || (x == (w - 1))) {
			idx = ((id & 0x1) ^ (x < (w >> 1)));

			if (x & 0x1) {
				ptr[0] = (color[9 + idx] >> 16) & 0xff;
				ptr[-1] += ((color[9 + idx] >> 8) & 0xff) >> 1;
				ptr[1] += ((color[9 + idx] >> 0) & 0xff) >> 1;
			} else {
				ptr[0] =(color[9 + idx] >> 16) & 0xff;
				ptr[1] = ((color[9 + idx] >> 8) & 0xff) >> 1;
				ptr[3] = ((color[9 + idx] >> 0) & 0xff) >> 1;
			}
		} else {
			if (id & 0x1)
				k = color[x / level];
			else
				k = color[(w - 1 - x) / level];

			if (x & 0x1) {
				ptr[0] = (k >> 16) & 0xff;
				ptr[-1] += ((k >> 8) & 0xff) >> 1;
				ptr[1] += ((k >> 0) & 0xff) >> 1;
			} else {
				ptr[0] = (k >> 16) & 0xff;
				ptr[1] = ((k >> 8) & 0xff) >> 1;
				ptr[3] = ((k >> 0) & 0xff) >> 1;
			}
		}
	} else {
		for (k = 0; k < info->var.bits_per_pixel; k += 8) {
			if ((y == 0) || (y == (h - 1)) || (x == 0)
					|| (x == (w - 1))) {
				idx = ((id & 0x1) ^ (x < (w >> 1)));
				*(ptr++) = color[9 + idx] >> k;
			} else {
				if (id & 0x1)
					*(ptr++) = color[x / level] >> k;
				else
					*(ptr++) = color[(w - 1 - x) / level]
						>> k;
			}
		}
	}
}

static void sp7350_gen_colorbar(struct fb_info *info, char *ptr, int id)
{
	int i, j;
	u32 w, h;

	w = info->var.xres;
	h = info->var.yres;
	for (i = 0; i < h; ++i) {
		for (j = 0; j < w; ++j) {
			sp7350_gen_colorbar_data(info, ptr, id, j, i);
			ptr += info->var.bits_per_pixel >> 3;
		}
	}
}

static void sp7350_fill_color(char *ptr, int size, int argb, struct fb_info *info)
{
	struct sp7350fb_device *fbdev =
		(struct sp7350fb_device *)info->par;
	unsigned int color = 0;
	int i, k;

	if (info->var.nonstd) {
		int y, u, v;

		sp7350_rgb2yuv(argb, &y, &u, &v);
		color = (y << 16) | (u << 8) | (v << 0);
	} else {
		color = sp7350fb_chan_by_field((argb >> 24) & 0xff,
				&info->var.transp);
		color |= sp7350fb_chan_by_field((argb >> 16) & 0xff,
				&info->var.red);
		color |= sp7350fb_chan_by_field((argb >> 8) & 0xff,
				&info->var.green);
		color |= sp7350fb_chan_by_field((argb >> 0) & 0xff,
				&info->var.blue);
	}

	if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP) {
		memset(ptr, 0, size);
		memcpy(info->pseudo_palette, &color, sizeof(color));
	} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2) {
		for (i = 0; i < (size << 1); i += 4) {
			*(ptr++) = (color >> 16) & 0xff;
			*(ptr++) = (color >> 8) & 0xff;
			*(ptr++) = (color >> 16) & 0xff;
			*(ptr++) = (color >> 0) & 0xff;
		}
	} else {
		for (i = 0; i < size; ++i) {
			for (k = 0; k < info->var.bits_per_pixel; k += 8)
				*(ptr++) = color >> k;
		}
	}
}

#ifdef SP7350_FB_LOAD_BMP
static void sp7350_draw_bmpfile(char *filename,
		unsigned char *fb_ptr,
		struct fb_info *fbinfo,
		int start_x,
		int start_y)
{
	int pixel_len = fbinfo->var.bits_per_pixel >> 3;
	struct sp7350_bmp_header bmp = {0};
	struct file *fp = NULL;
	char *tmpbuf = NULL;
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(filename, O_RDONLY, 0);

	if (!IS_ERR(fp)) {
		int line_pitch = 0, bpp = 0;
		int x, y, k;
		int line_index, readlen;
		unsigned int color;

		readlen = sizeof(struct sp7350_bmp_header) -
			sizeof(unsigned short);
		ret = kernel_read(fp, (char *)&bmp.identifier,
			readlen, &fp->f_pos);
		if (ret <= 0) {
			pr_err("read file:%s error\n", filename);
			goto err_init;
		}

		pr_info("BMP info\n");
		pr_info("  Filesize %d offset %d compression %d\n",
			bmp.file_size, bmp.data_offset, bmp.compression);
		pr_info("  WxH(%dx%d), bpp %d\n",
			bmp.width, bmp.height, bmp.bpp);

		if (!(bmp.bpp == 24 || bmp.bpp == 32)) {
			pr_err("only support 24 / 32 bit bitmap\n");
			goto err_init;
		}

		if (bmp.bpp == 24)
			line_pitch = SP7350_DISP_ALIGN(bmp.width, 2);
		else
			line_pitch = bmp.width;

		if ((bmp.width + start_x) > fbinfo->var.xres) {
			pr_err("bmp_width + start_x >= device width, %d + %d >= %d\n",
				bmp.width, start_x, fbinfo->var.xres);
			goto err_init;
		}

		if ((bmp.height + start_y) > fbinfo->var.yres) {
			pr_err("bmp_height + start_y >= device height, %d + %d >= %d\n",
				bmp.height, start_y, fbinfo->var.yres);
			goto err_init;
		}

		bpp = bmp.bpp >> 3;

		tmpbuf = kmalloc(line_pitch * bmp.height * bpp, GFP_KERNEL);
		if (IS_ERR(tmpbuf)) {
			pr_err("kmalloc error\n");
			goto err_init;
		}

		fp->f_op->llseek(fp, bmp.data_offset, 0);

		ret = kernel_read(fp, tmpbuf,
			line_pitch * bmp.height * bpp, &fp->f_pos);
		if (ret <= 0) {
			pr_err("read file:%s error\n", filename);
			goto err_init;
		}

		fb_ptr += (start_y * fbinfo->var.xres + start_x) * pixel_len;

		for (y = bmp.height - 1; y >= 0; y--) {
			for (x = 0; x < bmp.width; x++) {
				line_index = (line_pitch * y + x) * bpp;

				color = sp7350fb_chan_by_field(
					(bmp.bpp == 24) ?
					0xff : tmpbuf[line_index + 3],
					&fbinfo->var.transp);
				color |= sp7350fb_chan_by_field(
					tmpbuf[line_index + 2],
					&fbinfo->var.red);
				color |= sp7350fb_chan_by_field(
					tmpbuf[line_index + 1],
					&fbinfo->var.green);
				color |= sp7350fb_chan_by_field(
					tmpbuf[line_index],
					&fbinfo->var.blue);

				//pr_info("B:%x G:%x R:%x A:%x",
				//	tmpbuf[line_index],
				//	tmpbuf[line_index + 1],
				//	tmpbuf[line_index + 2],
				//	(bmp.bpp == 24) ?
				//	0xff : tmpbuf[line_index + 3]);

				for (k = 0; k < fbinfo->var.bits_per_pixel;
						k += 8)
					*(fb_ptr++) = color >> k;
			}
			fb_ptr += (fbinfo->var.xres - bmp.width) * pixel_len;
		}
	} else {
		pr_err("Can't open \"%s\"\n", filename);
		goto err_init;
	}

err_init:
	if (!IS_ERR(fp)) {
		if (filp_close(fp, NULL))
			pr_err("Close file error\n");
	}

	if (!IS_ERR(tmpbuf) && tmpbuf != NULL)
		kfree(tmpbuf);

	set_fs(oldfs);
}
#endif

#ifdef SP7350_FB_LOAD_FONT
static void sp7350_draw_font(unsigned char *fb_ptr, struct fb_info *fbinfo,
		int start_x, int start_y, int font_idx)
{
	int bits_per_pixel, byte_per_pixel;
	unsigned int font_color, bg_color;
	int x = 0, y = 0, k, font_offset;
	unsigned short ch;
	int val = 0;

	bits_per_pixel = fbinfo->var.bits_per_pixel;
	byte_per_pixel = fbinfo->var.bits_per_pixel >> 3;

	/* set font color */
	/* re-arrange font color by osd color_mode */
	font_color = sp7350fb_chan_by_field((sp_font_color >> 24) & 0xff,
		&fbinfo->var.transp);
	font_color |= sp7350fb_chan_by_field((sp_font_color >> 16) & 0xff,
		&fbinfo->var.red);
	font_color |= sp7350fb_chan_by_field((sp_font_color >> 8) & 0xff,
		&fbinfo->var.green);
	font_color |= sp7350fb_chan_by_field((sp_font_color >> 0) & 0xff,
		&fbinfo->var.blue);
	/* re-arrange font back ground color by osd color_mode */
	bg_color = sp7350fb_chan_by_field((sp_font_color_bg >> 24) & 0xff,
		&fbinfo->var.transp);
	bg_color |= sp7350fb_chan_by_field((sp_font_color_bg >> 16) & 0xff,
		&fbinfo->var.red);
	bg_color |= sp7350fb_chan_by_field((sp_font_color_bg >> 8) & 0xff,
		&fbinfo->var.green);
	bg_color |= sp7350fb_chan_by_field((sp_font_color_bg >> 0) & 0xff,
		&fbinfo->var.blue);

	/* show font info for debug */
	pr_info("font info\n");
	pr_info("  draw font at (x,y)(%d,%d)\n", start_x, start_y);
	pr_info("  draw font size (w,h)(%dx%d) idx %d\n",
		sp_font_width, sp_font_height, font_idx);
	pr_info("  color (fg,bg)(0x%08x,0x%08x)\n", font_color, bg_color);

	/*  cal data buf address for font draw */
	fb_ptr += (start_y * fbinfo->var.xres + start_x) * byte_per_pixel;

	/*  cal font array offset for byte unit */
	font_offset = font_idx * sp_font_width * sp_font_height / 8;

	/* fill font area */
	for (y = 0; y < sp_font_height; y++) {

		if ((sp_font_width == 8) && (sp_font_height == 8))
			/* in case of font 8x8 */
			ch = SP7350_FB_FONT_ASCII_8X8[font_offset + y + 0];
		else if ((sp_font_width == 8) && (sp_font_height == 16))
			/* in case of font 8x16 */
			ch = SP7350_FB_FONT_ASCII_8X16[font_offset + y + 0];
		else if ((sp_font_width == 16) && (sp_font_height == 32))
			/* in case of font 16x32 */
			ch = SP7350_FB_FONT_ASCII_16X32[font_offset + y * 2 + 0] << 8 |
				SP7350_FB_FONT_ASCII_16X32[font_offset + y * 2 + 1];

		for (x = 0; x < sp_font_width; x++) {
			val = (ch >> (sp_font_width - x - 1)) & 0x0001;

			for (k = 0; k < bits_per_pixel; k += 8) {
				if (val)
					*(fb_ptr++) = font_color >> k;
				else
					*(fb_ptr++) = bg_color >> k;
			}
		}
		fb_ptr += (fbinfo->var.xres - sp_font_width) * byte_per_pixel;
	}

}
#endif

static char *spmon_skipspace(char *p)
{
	if (p == NULL)
		return NULL;
	while (1) {
		int c = p[0];

		if (c == ' ' || c == '\t' || c == '\v')
			p++;
		else
			break;
	}
	return p;
}

static char *spmon_readint(char *p, int *x)
{
	int base = 10;
	int cnt, retval;

	if (x == NULL)
		return p;

	*x = 0;

	if (p == NULL)
		return NULL;

	p = spmon_skipspace(p);

	if (p[0] == '0' && p[1] == 'x') {
		base = 16;
		p += 2;
	}
	if (p[0] == '0' && p[1] == 'o') {
		base = 8;
		p += 2;
	}
	if (p[0] == '0' && p[1] == 'b') {
		base = 2;
		p += 2;
	}

	retval = 0;
	for (cnt = 0; 1; cnt++) {
		int c = *p++;
		int val;

		/* translate 0~9, a~z, A~Z */
		if (c >= '0' && c <= '9')
			val = c - '0';
		else if (c >= 'a' && c <= 'z')
			val = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			val = c - 'A' + 10;
		else
			break;
		if (val >= base)
			break;

		retval = retval * base + val;
	}

	if (cnt == 0)
		return p;	/* no translation is done?? */

	*(unsigned int *) x = retval;	/* store it */
	return p;
}

/*
 * Dump fb info
 *   echo "fbinfo" > /sys/module/sp7350fb/parameter/debug
 *
 * Fill buf with colorbar
 *   echo "set bar" > /sys/module/sp7350fb/parameter/debug
 * Fill buf with one color
 *   echo "set color 0xffff0000" > /sys/module/sp7350fb/parameter/debug
 *   echo "set color 0xff00ff00" > /sys/module/sp7350fb/parameter/debug
 *   echo "set color 0xff0000ff" > /sys/module/sp7350fb/parameter/debug
 * Fill buf palette (range 0 ~ 255)
 *   echo "set pal 0 0xffff0000" > /sys/module/sp7350fb/parameter/debug
 *   echo "set pal 255 0xffff0000" > /sys/module/sp7350fb/parameter/debug
 * Change buf id (range 0 ~ 1)
 *   echo "set buf 0" > /sys/module/sp7350fb/parameter/debug
 *   echo "set buf 1" > /sys/module/sp7350fb/parameter/debug
 *
 * Show buf palette
 *   echo "get pal" > /sys/module/sp7350fb/parameter/debug
 * Get buf id (range 0 ~ 1)
 *   echo "get buf" > /sys/module/sp7350fb/parameter/debug
 *
 * Load bmp file
 *   echo "bmp 0 0 0 /usr/share/argb8888.bmp" > /sys/module/sp7350fb/parameter/debug
 *     file path /usr/share/argb8888.bmp depends on your bmp file location.
 *     only support 24bit/32bit bmp file.
 *
 * Load font
 *   echo "font color 0xffff0000 0xff000000" > /sys/module/sp7350fb/parameter/debug
 *      font_color, font_color_bg
 *		setting value : 0xaa-rr-gg-bb (aa=alpha, rr=RED, gg=Green, bb=BLUE)
 *   echo "font size 8 16" > /sys/module/sp7350fb/parameter/debug
 *      font_width, font_height, support 8x8 8x16 16x32 ascii code
 *
 *   echo "font draw 0 10 20 test" > /sys/module/sp7350fb/parameter/debug
 *      buf_id=0, x=10, y=20 and display string
 */
static void sp7350_fb_dbg_cmd(char *str, struct fb_info *fbinfo)
{
	struct sp7350fb_device *fbdev;
	u32 buf_num, buf_size_page;
	int i;

	if (!fbinfo) {
		pr_err("can't get fbinfo\n");
		return;
	}

	fbdev = (struct sp7350fb_device *)fbinfo->par;

	buf_num = fbdev->buf_num;
	buf_size_page = fbdev->buf_size_page;

	str = spmon_skipspace(str);

	if (!strncasecmp(str, "fbinfo", 4)) {
		sp7350_device_info(fbinfo);
	} else if (!strncasecmp(str, "get", 3)) {
		str = spmon_skipspace(str + 3);
		if (!strncasecmp(str, "pal", 3)) {
			int *palette_ptr = fbinfo->pseudo_palette;

			if ((fbdev->color_mode != SP7350_OSD_COLOR_MODE_8BPP)
				|| (!palette_ptr)) {
				pr_err("color format unsupported.\n");
				return;
			}

			//for (i = 0; i < (SP7350_FB_PALETTE_LEN / sizeof(u32)); ++i) {
			//	if (!(i % 16))
			//		pr_info("palette %03d -- %03d\n", i, i + 15);
			//	pr_info(" 0x%08x\n", *(palette_ptr++));
			//}
			for (i = 0; i < (SP7350_FB_PALETTE_LEN / sizeof(u32) / 4); i++) {
				if (!(i % 4))
					pr_info("palette %03d -- %03d\n", i * 4, i * 4 + 15);
				pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
					*(palette_ptr + i * 4 + 0), *(palette_ptr + i * 4 + 1),
					*(palette_ptr + i * 4 + 2), *(palette_ptr + i * 4 + 3));
			}

		} else if (!strncasecmp(str, "buf", 3)) {
			int ret = 0;

			if (!fbinfo->var.yoffset)
				fbinfo->var.yoffset = fbinfo->var.yres;
			else
				fbinfo->var.yoffset = 0;

			ret = sp7350fb_swapbuf(
				fbinfo->var.yoffset / fbinfo->var.yres,
				fbdev->buf_num);
			pr_info("swap buffer now use ID:%d, ret:%d\n",
				fbinfo->var.yoffset / fbinfo->var.yres,
				ret);
		}
	} else if (!strncasecmp(str, "set", 3)) {
		str = spmon_skipspace(str + 3);
		if (!strncasecmp(str, "bar", 3)) {
			/* update buffer data */
			for (i = 0; i < buf_num; ++i) {
				sp7350_gen_colorbar(fbinfo,
					fbinfo->screen_base + (buf_size_page * i), i);
			}
			/* update palette */
			if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP) {
				int ret;

				ret = sp7350fb_swapbuf(
					fbinfo->var.yoffset / fbinfo->var.yres,
					buf_num);
				pr_info("update palette by swapbuf ret:%d\n", ret);
			}
		} else if (!strncasecmp(str, "color", 5)) {
			unsigned int argb = 0;

			str = spmon_readint(str + 5, (int *)&argb);

			sp7350_fill_color(fbinfo->screen_base,
				fbinfo->screen_size / (fbinfo->var.bits_per_pixel >> 3),
				argb, fbinfo);

			/* update palette */
			if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP) {
				int ret;

				ret = sp7350fb_swapbuf(fbinfo->var.yoffset / fbinfo->var.yres,
					buf_num);
				pr_info("update palette by swapbuf ret:%d\n", ret);
			}
			pr_info("fill all by color(A:0x%02x, R:0x%02x, G:0x%02x, B:0x%02x)\n",
				(argb >> 24) & 0xff, (argb >> 16) & 0xff,
				(argb >> 8) & 0xff, (argb >> 0) & 0xff);
			if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2) {
				int r, g, b;
				int y, u, v;

				r = (argb >> 16) & 0xff;
				g = (argb >> 8) & 0xff;
				b = (argb >> 0) & 0xff;
				y = ((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16;
				u = ((-r * 38 - g * 74 + b * 112 + 128) >> 8) + 128;
				v = ((r * 112 - g * 94 - b * 18 + 128) >> 8) + 128;
				y = (y < 0) ? 0 : ((y > 255) ? 255 : y);
				u = (u < 0) ? 0 : ((u > 255) ? 255 : u);
				v = (v < 0) ? 0 : ((v > 255) ? 255 : v);
				pr_info("fill all by color(Y:0x%02x, U:0x%02x, V:0x%02x)\n",
					y, u, v);
			}

		} else if (!strncasecmp(str, "pal", 3)) {
			int *palette_ptr = fbinfo->pseudo_palette;
			int index = 0;
			unsigned int argb = 0;
			unsigned int *palette = (unsigned int *)fbinfo->pseudo_palette;

			if ((fbdev->color_mode != SP7350_OSD_COLOR_MODE_8BPP)
					|| (!palette_ptr)) {
				pr_err("color format unsupported.\n");
				return;
			}

			str = spmon_readint(str + 3, &index);
			str = spmon_readint(str, (int *)&argb);

			if (index >= (SP7350_FB_PALETTE_LEN / sizeof(u32))) {
				pr_err("index %d > %ld\n", index,
					SP7350_FB_PALETTE_LEN / sizeof(u32));
				return;
			}

			palette[index] = sp7350fb_chan_by_field((argb >> 24) & 0xff,
				&fbinfo->var.transp);
			palette[index] |= sp7350fb_chan_by_field((argb >> 16) & 0xff,
				&fbinfo->var.red);
			palette[index] |= sp7350fb_chan_by_field((argb >> 8) & 0xff,
				&fbinfo->var.green);
			palette[index] |= sp7350fb_chan_by_field((argb >> 0) & 0xff,
				&fbinfo->var.blue);

			pr_info("set palette[%d] = 0x%x(A:0x%02x, R:0x%02x, G:0x%02x, B:0x%02x)\n",
				index, argb,
				(argb >> 24) & 0xff, (argb >> 16) & 0xff,
				(argb >> 8) & 0xff, (argb >> 0) & 0xff);
		} else if (!strncasecmp(str, "buf", 3)) {
			int ret = 0;
			u32 buf_id;

			str = spmon_readint(str + 3, (int *)&buf_id);
			ret = sp7350fb_swapbuf(buf_id, fbdev->buf_num);
			pr_info("force show buffer_ID:%d, ret:%d\n", buf_id, ret);
		}
#ifdef SP7350_FB_LOAD_BMP
	} else if (!strncasecmp(str, "bmp", 3)) {
		int x, y, buf_id;

		str = spmon_readint(str + 3, &buf_id);
		str = spmon_readint(str, &x);
		str = spmon_readint(str, &y);
		str = spmon_skipspace(str);

		if (buf_id >= fbdev->buf_num) {
			pr_err("buffer id error: %d >= %d\n", buf_id,
				fbdev->buf_num);
			return;
		}

		if ((fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP)
			|| (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2)) {
			pr_err("color format unsupported.\n");
			return;
		}

		sp7350_draw_bmpfile(str,
			(unsigned char *)(fbinfo->screen_base
				+ (fbdev->buf_size_page * buf_id)),
			fbinfo, x, y);
#endif
#ifdef SP7350_FB_LOAD_FONT
	} else if (!strncasecmp(str, "font", 4)) {
		str = spmon_skipspace(str + 4);

		if (!strncasecmp(str, "color", 5)) {
			int fg, bg;
			str = spmon_readint(str + 5, &fg);
			str = spmon_readint(str, &bg);

			pr_info("  cur font color (fg,bg)(0x%08x,0x%08x)\n",
				sp_font_color, sp_font_color_bg);

			sp_font_color = fg;
			sp_font_color_bg = bg;

			pr_info("  new font color (fg,bg)(0x%08x,0x%08x)\n",
				sp_font_color, sp_font_color_bg);

		} else if (!strncasecmp(str, "size", 4)) {
			pr_info("  cur font size (w,h)(%dx%d)\n",
				sp_font_width, sp_font_height);
			str = spmon_readint(str + 4, &sp_font_width);
			str = spmon_readint(str, &sp_font_height);
			pr_info("  new font size (w,h)(%dx%d)\n",
				sp_font_width, sp_font_height);
		} else if (!strncasecmp(str, "draw", 4)) {
			int x, y, buf_id;
			char str_index;

			x = y = buf_id = 0;

			str = spmon_readint(str + 4, &buf_id);
			str = spmon_readint(str, &x);
			str = spmon_readint(str, &y);

			if (buf_id >= fbdev->buf_num) {
				pr_err("buffer id error: %d >= %d\n",
					buf_id, fbdev->buf_num);
				return;
			}

			if ((fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP)
				|| (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2)) {
				pr_err("color format unsupported.\n");
				return;
			}

			if ((x >= fbinfo->var.xres) || (y >= fbinfo->var.yres)) {
				pr_err("out of range set(%d,%d) max(%d,%d)\n",
					x, y, fbinfo->var.xres, fbinfo->var.yres);
				return;
			}

			str = spmon_skipspace(str);
			if (IS_ERR(str) || (str == NULL) || (strlen(str) == 0)) {
				pr_err("no string to show\n");
				return;
			}

			for (i = 0; i < strlen(str); i++) {
				str_index = *(str + i);
				sp7350_draw_font((unsigned char *)(fbinfo->screen_base
					+ (fbdev->buf_size_page * buf_id)), fbinfo,
					x + sp_font_width * i, y, str_index);
			}
		}
#endif
	} else
		pr_err("unknown command:%s\n", str);

	(void)str;
}

static int sp7350_set_dbg_cmd(const char *val, const struct kernel_param *kp)
{
	sp7350_fb_dbg_cmd(strstrip((char *)val), gsp_fbinfo);

	return 0;
}

static struct kernel_param_ops fb_debug_param_ops = {
	.set = sp7350_set_dbg_cmd,
};
module_param_cb(debug, &fb_debug_param_ops, NULL, 0644);
