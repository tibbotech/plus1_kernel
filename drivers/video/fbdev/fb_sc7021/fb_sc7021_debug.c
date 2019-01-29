/**************************************************************************
 *                                                                        *
 *         Copyright (c) 2019 by Sunplus Inc.                             *
 *                                                                        *
 *  This software is copyrighted by and is the property of Sunplus        *
 *  Inc. All rights are reserved by Sunplus Inc.                          *
 *  This software may only be used in accordance with the                 *
 *  corresponding license agreement. Any unauthorized use, duplication,   *
 *  distribution, or disclosure of this software is expressly forbidden.  *
 *                                                                        *
 *  This Copyright notice MUST not be removed or modified without prior   *
 *  written consent of Sunplus Technology Co., Ltd.                       *
 *                                                                        *
 *  Sunplus Inc. reserves the right to modify this software               *
 *  without notice.                                                       *
 *                                                                        *
 *  Sunplus Inc.                                                          *
 *  19, Innovation First Road, Hsinchu Science Park                       *
 *  Hsinchu City 30078, Taiwan, R.O.C.                                    *
 *                                                                        *
 **************************************************************************/

/**
 * @file fb_sc7021_debug.c
 * @brief linux kernel framebuffer debug driver
 * @author PoChou Chen
 */
/**************************************************************************
 *                         H E A D E R   F I L E S
 **************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>

#include "mach/display/disp_osd.h"
#include "fb_sc7021_main.h"

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define SUPPORT_LOAD_BMP

/* test pattern gen */
#define MAKE_ARGB(A, R, G, B)	((B) | ((G) << 8) | ((R) << 16) | ((A) << 24))
#define ARGB_GETA(pixel)		(((pixel) >> 24) & 0xff)
#define ARGB_GETR(pixel)		(((pixel) >> 16) & 0xff)
#define ARGB_GETG(pixel)		(((pixel) >> 8) & 0xff)
#define ARGB_GETB(pixel)		(((pixel) >> 0) & 0xff)

#define CMD_LEN				(256)

#ifdef DEBUG_MSG
	#define mod_dbg(fmt, arg...)	pr_debug("[%s:%d] "fmt, \
			__func__, \
			__LINE__, \
			##arg)
#else
	#define mod_dbg(...)
#endif

#define mod_err(fmt, arg...)		pr_err("[%s:%d] Error! "fmt, \
		__func__, \
		__LINE__, \
		##arg)
#define mod_warn(fmt, arg...)		pr_warn("[%s:%d] Warning! "fmt, \
		__func__, \
		__LINE__, \
		##arg)
#define mod_info(fmt, arg...)		pr_info("[%s:%d] "fmt, \
		__func__, \
		__LINE__, \
		##arg)

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
#ifdef SUPPORT_LOAD_BMP
struct BMP_HEADER {
	unsigned short reserved0;
	unsigned short identifier;
	unsigned int file_size;
	unsigned int reserved1;
	unsigned int data_offset;
	unsigned int header_size;
	unsigned int width;
	unsigned int height;
	unsigned short planes;
	unsigned short bpp;
	unsigned int compression;
	unsigned short reserved;
};
#endif

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static void _fb_debug_cmd(char *tmpbuf, struct fb_info *fbinfo);
static int _set_debug_cmd(const char *val, const struct kernel_param *kp);

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static unsigned int gColorbar[] = {
	MAKE_ARGB(0xff, 0xff, 0xff, 0xff),	/* white */
	MAKE_ARGB(0xff, 0xff, 0xff, 0x00),	/* yellow */
	MAKE_ARGB(0xff, 0x00, 0xff, 0xff),	/* cyan */
	MAKE_ARGB(0xff, 0x00, 0xff, 0x00),	/* green */
	MAKE_ARGB(0xff, 0xff, 0x00, 0xff),	/* magenta */
	MAKE_ARGB(0xff, 0xff, 0x00, 0x00),	/* red */
	MAKE_ARGB(0xff, 0x00, 0x00, 0xff),	/* blue */
	MAKE_ARGB(0xff, 0x00, 0x00, 0x00),	/* black */
	MAKE_ARGB(0x80, 0x00, 0x00, 0x00),	/* gray */
	MAKE_ARGB(0xff, 0xff, 0x00, 0x00),	/* red left side border */
	MAKE_ARGB(0xff, 0xff, 0xff, 0xff),	/* white right side border */
};

static struct kernel_param_ops fb_debug_param_ops = {
	.set = _set_debug_cmd,
};

module_param_cb(debug, &fb_debug_param_ops, NULL, 0644);

/**************************************************************************
 *             F U N C T I O N    I M P L E M E N T A T I O N S           *
 **************************************************************************/
static int _set_debug_cmd(const char *val, const struct kernel_param *kp)
{
	_fb_debug_cmd((char *)val, gFB_INFO);

	return 0;
}

#ifdef SUPPORT_LOAD_BMP
static void _initKernelEnv(mm_segment_t *oldfs)
{
	*oldfs = get_fs();
	set_fs(KERNEL_DS);
}

static void _deinitKernelEnv(mm_segment_t *oldfs)
{
	set_fs(*oldfs);
}

static int _readFile(struct file *fp, char *buf, int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->read(fp, buf, readlen, &fp->f_pos);
	else
		return -1;
}

static int _closeFile(struct file *fp)
{
	return filp_close(fp, NULL);
}

static void _Draw_Bmpfile(char *filename,
		unsigned char *fb_ptr,
		struct fb_info *fbinfo,
		int start_x,
		int start_y)
{
	struct BMP_HEADER bmp = {0};
	char			*tmpbuf = NULL;
	struct file		*fp = NULL;
	mm_segment_t	oldfs;
	int				pixel_len =
		fbinfo->var.bits_per_pixel >> 3;

	_initKernelEnv(&oldfs);
	fp = filp_open(filename, O_RDONLY, 0);

	if (!IS_ERR(fp)) {
		int line_pitch = 0, bpp = 0;
		int x, y, k;
		int line_index;
		unsigned int color;

		if (_readFile(fp, (char *)&bmp.identifier,
					sizeof(struct BMP_HEADER)
					- sizeof(unsigned short)) <= 0) {
			mod_err("read file:%s error\n", filename);
			goto Failed;
		}

		mod_dbg("Image info: size %dx%d, bpp %d, compression %d, offset %d, filesize %d\n",
				bmp.width,
				bmp.height,
				bmp.bpp,
				bmp.compression,
				bmp.data_offset,
				bmp.file_size);

		if (!(bmp.bpp == 24 || bmp.bpp == 32)) {
			mod_err("only support 24 / 32 bit bitmap\n");
			goto Failed;
		}

		if (bmp.bpp == 24)
			line_pitch = DISP_ALIGN(bmp.width, 2);
		else
			line_pitch = bmp.width;

		if ((bmp.width + start_x) > fbinfo->var.xres) {
			mod_err("bmp_width + start_x >= device width, %d + %d >= %d\n",
					bmp.width,
					start_x,
					fbinfo->var.xres);
			goto Failed;
		}

		if ((bmp.height + start_y) > fbinfo->var.yres) {
			mod_err("bmp_height + start_y >= device height, %d + %d >= %d\n",
					bmp.height,
					start_y,
					fbinfo->var.yres);
			goto Failed;
		}

		bpp = bmp.bpp >> 3;

		tmpbuf = kmalloc(line_pitch * bmp.height * bpp, GFP_KERNEL);
		if (IS_ERR(tmpbuf)) {
			mod_err("kmalloc error\n");
			goto Failed;
		}

		fp->f_op->llseek(fp, bmp.data_offset, 0);
		if (_readFile(fp, tmpbuf, line_pitch * bmp.height * bpp) <= 0) {
			mod_err("read file:%s error\n", filename);
			goto Failed;
		}

		fb_ptr += (start_y * fbinfo->var.xres + start_x) * pixel_len;

		for (y = bmp.height - 1; y >= 0; y--) {
			for (x = 0; x < bmp.width; x++) {
				line_index = (line_pitch * y + x) * bpp;

				color = sc7021_fb_chan_by_field(
						(bmp.bpp == 24) ?
						0xff : tmpbuf[line_index + 3],
						&fbinfo->var.transp);
				color |= sc7021_fb_chan_by_field(
						tmpbuf[line_index + 2],
						&fbinfo->var.red);
				color |= sc7021_fb_chan_by_field(
						tmpbuf[line_index + 1],
						&fbinfo->var.green);
				color |= sc7021_fb_chan_by_field(
						tmpbuf[line_index],
						&fbinfo->var.blue);

#if 0
				mod_dbg("B:%x G:%x R:%x A:%x",
					tmpbuf[line_index],
					tmpbuf[line_index + 1],
					tmpbuf[line_index + 2],
					(bmp.bpp == 24) ?
					0xff : tmpbuf[line_index + 3]);
#endif
				for (k = 0; k < fbinfo->var.bits_per_pixel;
						k += 8)
					*(fb_ptr++) = color >> k;
			}
			fb_ptr += (fbinfo->var.xres - bmp.width) * pixel_len;
		}
	} else {
		mod_err("Can't open %s\n", filename);
		goto Failed;
	}

Failed:
	if (!IS_ERR(fp)) {
		if (_closeFile(fp))
			mod_err("Close file error\n");
	}

	if (!IS_ERR(tmpbuf) && tmpbuf != NULL)
		kfree(tmpbuf);

	_deinitKernelEnv(&oldfs);
}
#endif

static char *_mon_skipspace(char *p)
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

static char *_mon_readint(char *p, int *x)
{
	int base = 10;
	int cnt, retval;

	if (x == NULL)
		return p;

	*x = 0;

	if (p == NULL)
		return NULL;

	p = _mon_skipspace(p);

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
		return p;			/* no translation is done?? */

	*(unsigned int *) x = retval;		/* store it */
	return p;
}

static void _gen_colorbar_data(struct fb_info *info,
		int id,
		int w,
		int h,
		int x,
		int y,
		char *ptr)
{
	struct framebuffer_t *fb_par = (struct framebuffer_t *)info->par;
	int k;
	int level = w >> 3;
	int idx;
	unsigned int color[sizeof(gColorbar) / sizeof(unsigned int)];

	for (k = 0; k < sizeof(color) / sizeof(unsigned int); ++k) {
		color[k] = sc7021_fb_chan_by_field(ARGB_GETA(gColorbar[k]),
				&info->var.transp);
		color[k] |= sc7021_fb_chan_by_field(ARGB_GETR(gColorbar[k]),
				&info->var.red);
		color[k] |= sc7021_fb_chan_by_field(ARGB_GETG(gColorbar[k]),
				&info->var.green);
		color[k] |= sc7021_fb_chan_by_field(ARGB_GETB(gColorbar[k]),
				&info->var.blue);
	}

	if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
		if ((y == 0) || (y == (h - 1)) || (x == 0) || (x == (w - 1))) {
			*(ptr++) = 9 + ((id & 0x1) ^ (x < (w >> 1)));
		} else {
			if (id & 0x1)
				*(ptr++) = (x / level);
			else
				*(ptr++) = ((w - 1 - x) / level);
		}
		memcpy(info->pseudo_palette, color, sizeof(color));
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

static void _gen_colorbar(char *ptr, int w, int h, int id, struct fb_info *info)
{
	int i, j;

	for (i = 0; i < h; ++i) {
		for (j = 0; j < w; ++j) {
			_gen_colorbar_data(info, id, w, h, j, i, ptr);
			ptr += info->var.bits_per_pixel >> 3;
		}
	}
}

static void _fill_color(char *ptr, int size, int argb, struct fb_info *info)
{
	struct framebuffer_t *fb_par = (struct framebuffer_t *)info->par;
	int i, k;
	unsigned int color;

	color = sc7021_fb_chan_by_field(ARGB_GETA(argb), &info->var.transp);
	color |= sc7021_fb_chan_by_field(ARGB_GETR(argb), &info->var.red);
	color |= sc7021_fb_chan_by_field(ARGB_GETG(argb), &info->var.green);
	color |= sc7021_fb_chan_by_field(ARGB_GETB(argb), &info->var.blue);

	if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
		memset(ptr, 0, size);
		memcpy(info->pseudo_palette, &color, sizeof(color));
	} else {
		for (i = 0; i < size; ++i) {
			for (k = 0; k < info->var.bits_per_pixel; k += 8)
				*(ptr++) = color >> k;
		}
	}
}

static void _device_info(void)
{
	struct framebuffer_t *fb_par = NULL;

	fb_par = (struct framebuffer_t *)gFB_INFO->par;
	if (!fb_par)
		return;
	pr_err("\n=======================\n");
	pr_err("ID: %16s, ColorFormat: %s(%d), page size:%d buffer page:%d",
			gFB_INFO->fix.id,
			fb_par->ColorFmtName,
			fb_par->ColorFmt,
			fb_par->fbpagesize,
			fb_par->fbpagenum);
	pr_err("%dx%d, size=%d",
			fb_par->fbwidth,
			fb_par->fbheight,
			fb_par->fbsize);
	pr_err("Base addr: 0x%x, Phy addr: 0x%x, now show buf id=%d\n",
			(u32)fb_par->fbmem,
			__pa(fb_par->fbmem),
			gFB_INFO->var.yoffset
			/ gFB_INFO->var.yres);
	if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
		pr_err("palette Base addr: 0x%x, Phy addr: 0x%x\n",
				(u32)fb_par->fbmem_palette,
				__pa(fb_par->fbmem_palette));
	}
}

static void _print_UI_info(struct UI_FB_Info_t *info)
{
	pr_err("\n%dx%d, bufNum:%d, Align:%d, fmt:%d(%dbit)\n",
			info->UI_width,
			info->UI_height,
			info->UI_bufNum,
			info->UI_bufAlign,
			info->UI_ColorFmt,
			info->UI_Colormode.bits_per_pixel);

	pr_err("name:%s nonstd:%d\n",
			info->UI_Colormode.name,
			info->UI_Colormode.nonstd);
	pr_err("red    offset:%2d length:%2d msb_right:%2d\n",
			info->UI_Colormode.red.offset,
			info->UI_Colormode.red.length,
			info->UI_Colormode.red.msb_right);
	pr_err("green  offset:%2d length:%2d msb_right:%2d\n",
			info->UI_Colormode.green.offset,
			info->UI_Colormode.green.length,
			info->UI_Colormode.green.msb_right);
	pr_err("blue   offset:%2d length:%2d msb_right:%2d\n",
			info->UI_Colormode.blue.offset,
			info->UI_Colormode.blue.length,
			info->UI_Colormode.blue.msb_right);
	pr_err("transp offset:%2d length:%2d msb_right:%2d\n",
			info->UI_Colormode.transp.offset,
			info->UI_Colormode.transp.length,
			info->UI_Colormode.transp.msb_right);
}

static void _fb_debug_cmd(char *tmpbuf, struct fb_info *fbinfo)
{
	int i;
	struct framebuffer_t *fb_par;

	if (IS_ERR(fbinfo)) {
		mod_err("can't get fbinfo\n");
		return;
	}

	fb_par = (struct framebuffer_t *)fbinfo->par;

	tmpbuf = _mon_skipspace(tmpbuf);

	if (!strncasecmp(tmpbuf, "tpg", 3)) {
		for (i = 0; i < fb_par->fbpagenum; ++i)
			_gen_colorbar(fbinfo->screen_base
					+ (fb_par->fbpagesize * i),
					fbinfo->var.xres,
					fbinfo->var.yres,
					i,
					fbinfo);

		/* update palette */
		if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
			int ret;

			ret = sc7021_fb_swapbuf(
					fbinfo->var.yoffset / fbinfo->var.yres,
					fb_par->fbpagenum);
			pr_err("update palette by swapbuf ret:%d\n",
					ret);
		}
		flush_cache_all();
	} else if (!strncasecmp(tmpbuf, "fill", 4)) {
		unsigned int argb = 0;

		tmpbuf = _mon_readint(tmpbuf + 4, (int *)&argb);

		_fill_color(fbinfo->screen_base,
				fbinfo->screen_size
				/ (fbinfo->var.bits_per_pixel >> 3),
				argb,
				fbinfo);

		/* update palette */
		if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
			int ret;


			ret = sc7021_fb_swapbuf(
					fbinfo->var.yoffset / fbinfo->var.yres,
					fb_par->fbpagenum);
			pr_err("update palette by swapbuf ret:%d\n",
					ret);
		}
		pr_err("fill all by color(A:0x%02x, R:0x%02x, G:0x%02x, B:0x%02x)\n",
				ARGB_GETA(argb),
				ARGB_GETR(argb),
				ARGB_GETG(argb),
				ARGB_GETB(argb));
	} else if (!strncasecmp(tmpbuf, "palette", 7)) {
		int *palette_ptr = fbinfo->pseudo_palette;
		int index = 0;
		unsigned int argb = 0;
		unsigned int *palette = (unsigned int *)fbinfo->pseudo_palette;

		if ((fb_par->ColorFmt != DRV_OSD_REGION_FORMAT_8BPP)
				|| (!palette_ptr)) {
			pr_err("your color format unsupported.\n");
			return;
		}

		tmpbuf = _mon_readint(tmpbuf + 7, &index);
		tmpbuf = _mon_readint(tmpbuf, (int *)&argb);

		if (index >= (FB_PALETTE_LEN / sizeof(unsigned int))) {
			pr_err("your color format unsupported.\n");
			return;
		}

		palette[index] = sc7021_fb_chan_by_field(ARGB_GETA(argb),
				&fbinfo->var.transp);
		palette[index] |= sc7021_fb_chan_by_field(ARGB_GETR(argb),
				&fbinfo->var.red);
		palette[index] |= sc7021_fb_chan_by_field(ARGB_GETG(argb),
				&fbinfo->var.green);
		palette[index] |= sc7021_fb_chan_by_field(ARGB_GETB(argb),
				&fbinfo->var.blue);

		pr_err("set palette[%d] = 0x%x(A:0x%02x, R:0x%02x, G:0x%02x, B:0x%02x)\n",
				index,
				argb,
				ARGB_GETA(argb),
				ARGB_GETR(argb),
				ARGB_GETG(argb),
				ARGB_GETB(argb));
	} else if (!strncasecmp(tmpbuf, "sp", 2)) {
		int *palette_ptr = fbinfo->pseudo_palette;

		if ((fb_par->ColorFmt != DRV_OSD_REGION_FORMAT_8BPP)
				|| (!palette_ptr)) {
			pr_err("your color format unsupported.\n");
			return;
		}

		for (i = 0; i < (FB_PALETTE_LEN / sizeof(unsigned int)); ++i) {
			if (!(i % 16))
				pr_err("%3d\n", i);
			pr_err(" 0x%08x\n", *(palette_ptr++));
		}
	} else if (!strncasecmp(tmpbuf, "sb", 2)) {
		int ret = 0;
		u32 buf_id;

		tmpbuf = _mon_readint(tmpbuf + 2, (int *)&buf_id);

		ret = sc7021_fb_swapbuf(buf_id, fb_par->fbpagenum);

		pr_err("force show buffer_ID:%d, ret:%d\n",
				buf_id,
				ret);
	} else if (!strncasecmp(tmpbuf, "sw", 2)) {
		int ret = 0;

		if (!fbinfo->var.yoffset)
			fbinfo->var.yoffset = fbinfo->var.yres;
		else
			fbinfo->var.yoffset = 0;


		ret = sc7021_fb_swapbuf(
				fbinfo->var.yoffset / fbinfo->var.yres,
				fb_par->fbpagenum);
		pr_err("swap buffer now use ID:%d, ret:%d\n",
				fbinfo->var.yoffset / fbinfo->var.yres,
				ret);
	} else if (!strncasecmp(tmpbuf, "info", 4)) {
		_device_info();
	} else if (!strncasecmp(tmpbuf, "getUI", 5)) {
		struct UI_FB_Info_t Info;
		int ret;

		ret = DRV_OSD_Get_UI_Res(&Info);

		if (ret)
			mod_err("Get UI error\n");
		else
			_print_UI_info(&Info);
	}
#ifdef SUPPORT_LOAD_BMP
	else if (!strncasecmp(tmpbuf, "bmp", 3)) {
		int x, y, buf_id;

		tmpbuf = _mon_readint(tmpbuf + 3, &buf_id);
		tmpbuf = _mon_readint(tmpbuf, &x);
		tmpbuf = _mon_readint(tmpbuf, &y);
		tmpbuf = _mon_skipspace(tmpbuf);

		if (buf_id >= fb_par->fbpagenum) {
			mod_err("buffer id error: %d >= %d\n",
					buf_id,
					fb_par->fbpagenum);
			return;
		}

		if (fb_par->ColorFmt == DRV_OSD_REGION_FORMAT_8BPP) {
			mod_err("this color format unsupported.\n");
			return;
		}

		_Draw_Bmpfile(tmpbuf,
			(unsigned char *)((int)fbinfo->screen_base
				+ (fb_par->fbpagesize * buf_id)),
			fbinfo,
			x,
			y);
	}
#endif
	else
		mod_err("unknown command:%s\n", tmpbuf);

	(void)tmpbuf;
}

