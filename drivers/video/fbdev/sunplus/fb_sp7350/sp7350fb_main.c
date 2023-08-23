/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Sunplus SP7350 SoC framebuffer main driver
 *
 * linux/drivers/video/sunplus/fb_sp7350/sp7350fb_main.c
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 *
 */
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

#include <media/sunplus/disp/sp7350/sp7350_disp_osd.h>
#include "sp7350fb_main.h"

struct fb_info *gsp_fbinfo;

int sp7350fb_swapbuf(u32 buf_id, int buf_max)
{
	int ret = 0;

	if (buf_id >= buf_max)
		return -1;

	ret = sp7350_osd_region_buf_fetch_done(buf_id);

	return ret;
}

static int sp7350fb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *fbinfo)
{
	struct sp7350fb_device *fbdev =
		(struct sp7350fb_device *)fbinfo->par;
	int ret = 0;

	fbinfo->var.xoffset = var->xoffset;
	fbinfo->var.yoffset = var->yoffset;

	ret = sp7350fb_swapbuf(fbinfo->var.yoffset / fbinfo->var.yres,
		fbdev->buf_num);
	if (ret)
		pr_err("swap buffer fails");

	return ret;
}

static int sp7350fb_ioctl(struct fb_info *fbinfo,
		unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		sp7350_osd_region_wait_sync();
		break;
	}

	return 0;
}

unsigned int sp7350fb_chan_by_field(unsigned char chan,
		struct fb_bitfield *bf)
{
	unsigned int ret_chan = (unsigned int)chan;

	ret_chan >>= 8 - bf->length;

	return ret_chan << bf->offset;
}

static int sp7350fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	struct sp7350fb_device *fbdev =
		(struct sp7350fb_device *)info->par;
	unsigned int *palette = (unsigned int *)info->pseudo_palette;
	unsigned short *red, *green, *blue, *transp;
	unsigned short trans = ~0;
	int i;

	if (!palette)
		return -1;

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;

	for (i = cmap->start; (i < (cmap->start + cmap->len)) ||
		(i < (SP7350_FB_PALETTE_LEN / sizeof(u32))); ++i) {
		if (transp)
			trans = *(transp++);

		if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_ARGB8888) {
			palette[i] = 0xff000000;
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
		} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_RGBA8888) {
			palette[i] = sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
			palette[i] |= 0x000000ff;
		} else if ((fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP) ||
			(fbdev->color_mode == SP7350_OSD_COLOR_MODE_RGB565)) {
			palette[i] = sp7350fb_chan_by_field((unsigned char)trans,
					&info->var.transp);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
		} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_ARGB4444) {
			palette[i] = 0xf000f000;
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
		} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_RGBA4444) {
			palette[i] = sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
			palette[i] |= 0x000f000f;
		} else if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_ARGB1555) {
			palette[i] = 0x80008000;
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(red++),
					&info->var.red);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(green++),
					&info->var.green);
			palette[i] |= sp7350fb_chan_by_field((unsigned char)*(blue++),
					&info->var.blue);
		}
	}

	if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_YUY2) {
		palette[0] = 0x80008000; //V Y U Y (black)
		palette[1] = 0x7213D513; //V Y U Y
		palette[2] = 0x38634763; //V Y U Y
		palette[3] = 0x2B779C77; //V Y U Y
		palette[4] = 0xD5326332; //V Y U Y
		palette[5] = 0xC746B846; //V Y U Y
		palette[6] = 0xB1644764; //V Y U Y
		palette[7] = 0x80AA80AA; //V Y U Y
		palette[8] = 0x80558055; //V Y U Y
		palette[9] = 0x7268D568; //V Y U Y
		palette[10] = 0x38B847B8; //V Y U Y
		palette[11] = 0x2BCC9CCC; //V Y U Y
		palette[12] = 0xD5876387; //V Y U Y
		palette[13] = 0xC79BB89B; //V Y U Y
		palette[14] = 0x8DEB2BEB; //V Y U Y
		palette[15] = 0x80FF80FF; //V Y U Y (white)
	}

	return 0;
}

static struct fb_ops framebuffer_ops = {
	.owner			= THIS_MODULE,
	.fb_pan_display		= sp7350fb_pan_display,
	.fb_setcmap		= sp7350fb_setcmap,
	.fb_ioctl		= sp7350fb_ioctl,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
};

static int sp7350fb_create_device(struct platform_device *pdev,
		struct sp7350fb_info *sp_fbinfo)
{
	struct device *dev = &pdev->dev;
	struct sp7350fb_device *fbdev;
	struct fb_info *info;
	u64 htotal, vtotal, fps;
	u32 tmp_value;
	dma_addr_t fb_phymem;
	#ifdef SP7350_FB_RESERVED_MEM
	int ret;

	ret = of_reserved_mem_device_init(dev);
	if (ret && ret != -ENODEV)
		return ret;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_fb_init;

	#endif

	info = framebuffer_alloc(sizeof(*fbdev), dev);
	if (!info)
		return -ENOMEM;

	fbdev = info->par;
	memset(fbdev, 0, sizeof(*fbdev));
	fbdev->fb = info;

	/* init sp7350fb context */
	fbdev->color_mode = sp_fbinfo->color_mode;
	snprintf(fbdev->color_mode_name,
		sizeof(fbdev->color_mode_name) - 1,
		"%s",
		sp_fbinfo->cmod.name);
	fbdev->width = sp_fbinfo->width;
	fbdev->height = sp_fbinfo->height;

	if (sp_fbinfo->buf_align % PAGE_SIZE) {
		pr_err("err buf align: %d(%d)\n",
			sp_fbinfo->buf_align, (int)PAGE_SIZE);
		sp_fbinfo->buf_align =
			DIV_ROUND_UP(sp_fbinfo->buf_align, PAGE_SIZE) * PAGE_SIZE;
		pr_err("cal buf align: %d(%d)\n",
			sp_fbinfo->buf_align, (int)PAGE_SIZE);
		//goto err_fb_init;
	}

	/* cal sp7350fb size */
	tmp_value = fbdev->width * fbdev->height *
		(sp_fbinfo->cmod.bits_per_pixel >> 3);
	fbdev->buf_size_page = SP7350_DISP_ALIGN((fbdev->width
			* fbdev->height
			* (sp_fbinfo->cmod.bits_per_pixel >> 3)),
			sp_fbinfo->buf_align);
	//pr_info("fbdev: buf_size %d align %d to %d\n",
	//	tmp_value, sp_fbinfo->buf_align, fbdev->buf_size_page);
	fbdev->buf_num = sp_fbinfo->buf_num; //fix 2
	fbdev->buf_size_total = fbdev->buf_size_page * fbdev->buf_num;

	/* alloc sp7350fb buf */
	fbdev->buf_mem_total = dma_alloc_coherent(&pdev->dev,
			fbdev->buf_size_total, &fb_phymem, GFP_KERNEL | __GFP_ZERO);

	if (!fbdev->buf_mem_total) {
		pr_err("malloc failed, size %d(%dx%dx(%d/8)*%d)\n",
			fbdev->buf_size_total,
			fbdev->width,
			fbdev->height,
			sp_fbinfo->cmod.bits_per_pixel,
			fbdev->buf_num);
		goto err_fb_init;
	}

	/* alloc sp7350fb palette */
	fbdev->pal = kzalloc(SP7350_FB_PALETTE_LEN, GFP_KERNEL);
	if (!fbdev->pal)
		goto err_fb_init;

	/* assign fb info for sp7350fb */
	info->fbops = &framebuffer_ops;
	info->flags = FBINFO_FLAG_DEFAULT;

	info->pseudo_palette	= fbdev->pal;
	info->screen_base	= fbdev->buf_mem_total;
	info->screen_size	= fbdev->buf_size_total;

	/* assign fb info for resolution */
	/* assign fb info for var info */
	info->var.xres = fbdev->width;
	info->var.yres = fbdev->height;
	info->var.xres_virtual = fbdev->width;
	info->var.yres_virtual = fbdev->height * fbdev->buf_num;
	//info->var.xoffset = 0;
	//info->var.yoffset = 0;

	/* assign fb info for color format */
	info->var.bits_per_pixel= sp_fbinfo->cmod.bits_per_pixel;
	//info->var.grayscale	= 0; /* 0: color 1: grayscale >1: FOURCC */
	info->var.red		= sp_fbinfo->cmod.red;
	info->var.green		= sp_fbinfo->cmod.green;
	info->var.blue		= sp_fbinfo->cmod.blue;
	info->var.transp	= sp_fbinfo->cmod.transp;

	info->var.nonstd	= sp_fbinfo->cmod.nonstd;

	info->var.activate	= FB_ACTIVATE_NXTOPEN;
	info->var.accel_flags	= 0;
	info->var.vmode		= FB_VMODE_NONINTERLACED;

	/* assign fb info for timing */
	/* fake setting fps 59.94 */
	info->var.left_margin	= 60;	/* HBProch */
	info->var.right_margin	= 16;	/* HFPorch */
	info->var.upper_margin	= 30;	/* VBPorch */
	info->var.lower_margin	= 10;	/* VFPorch */
	/* assign fb info for hsync */
	info->var.hsync_len	= 10 - ((fbdev->width
		+ info->var.left_margin
		+ info->var.right_margin) % 10);
	/* assign fb info for vsync */
	info->var.vsync_len	= 10 - ((fbdev->height
		+ info->var.upper_margin
		+ info->var.lower_margin) % 10);
	/* assign fb info for pixclk */
	/*
	 * htotal = width + hbp + hfp + hsync
	 * vtotal = height + vbp + vfp + vsync
	 * total clk = htotal * vtotal * fps
	 * pixclock = 10^12 / (total clk)
	 */
	htotal = (u64)(fbdev->width + info->var.left_margin +
		info->var.right_margin + info->var.hsync_len)/10;
	vtotal = (u64)(fbdev->height + info->var.upper_margin +
		info->var.lower_margin + info->var.vsync_len)/10;
	fps = (u64)5994; /* timing calculate for 59.94Hz */
	info->var.pixclock = (u32)DIV64_U64_ROUND_CLOSEST(NSEC_PER_SEC,
		htotal * vtotal * fps / 1000);

	/* assign fb info for fixed info */
	snprintf(info->fix.id,
		sizeof(info->fix.id) - 1,
		"FB-%dx%d",
		fbdev->width,
		fbdev->height);
	info->fix.smem_start	= (unsigned long)fb_phymem;
	info->fix.smem_len	= fbdev->buf_size_total;
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux	= 0;
	if (fbdev->color_mode == SP7350_OSD_COLOR_MODE_8BPP)
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.xpanstep	= 0;
	info->fix.ypanstep	= 1;
	info->fix.ywrapstep	= 0;
	info->fix.line_length	= info->var.xres_virtual
		* (sp_fbinfo->cmod.bits_per_pixel >> 3);
	info->fix.mmio_start	= 0;
	info->fix.mmio_len	= 0;
	info->fix.accel		= FB_ACCEL_NONE;

	if (register_framebuffer(info)) {
		pr_err("register framebuffer failed\n");
		goto err_fb_init;
	}

	/* update sp7350fb_info for osd region */
	sp_fbinfo->buf_addr = fb_phymem;
	sp_fbinfo->buf_addr_pal = info->pseudo_palette;
	sp_fbinfo->buf_size = fbdev->buf_size_total;

	//pr_info("fbdev: fb buf VA 0x%px(PA 0x%llx) len %d\n",
	//	(void __iomem *)fbdev->buf_mem_total,
	//	sp_fbinfo->buf_addr,
	//	fbdev->buf_size_total);
	//pr_info("fbdev: fb pal VA 0x%px(PA 0x%llx) len %d\n",
	//	(void __iomem *)fbdev->pal,
	//	__pa(fbdev->pal),
	//	SP7350_FB_PALETTE_LEN);
	pr_info("fbdev: fb res %dx%d, size %d + %d\n",
		fbdev->width,
		fbdev->height,
		fbdev->buf_size_total,
		SP7350_FB_PALETTE_LEN);

	gsp_fbinfo = info;

	return 0;

err_fb_init:
	if (sp_fbinfo)
		pr_err("create dev err\n");

	if (fbdev->pal)
		kfree(fbdev->pal);

	#ifdef SP7350_FB_RESERVED_MEM
	if (fbdev->buf_mem_total)
		of_reserved_mem_device_release(dev);
	#else
	if (fbdev->buf_mem_total)
		dma_free_coherent(&pdev->dev,
			fbdev->buf_size_total,
			fbdev->buf_mem_total,
			fb_phymem);
	#endif

	if (info)
		framebuffer_release(info);

	return -1;
}

static int sp7350fb_probe(struct platform_device *pdev)
{
	struct sp7350fb_device *fbdev;
	struct sp7350fb_info *sp_fbinfo;
	int ret;

	if (!pdev->dev.of_node) {
		pr_err("invalid dt node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(pdev->dev.of_node)) {
		pr_err("dt is not available\n");
		return -ENODEV;
	}

	sp_fbinfo = kzalloc(sizeof(*sp_fbinfo), GFP_KERNEL);
	if (!sp_fbinfo)
		return -ENOMEM;

	ret = sp7350_osd_get_fbinfo(sp_fbinfo);
	if (ret) {
		kfree(sp_fbinfo);
		pr_err("fbdev: sp_fbinfo not present");
		return -EPROBE_DEFER;
	}

	if (!sp_fbinfo->width || !sp_fbinfo->height) {
		ret = -EINVAL;
		pr_err("invalid width or height\n");
		goto err_fb_probe;
	}

	//pr_info("create fbdev %dx%d, cmod %d(%s)\n",
	pr_info("fbdev: fb create %dx%d, cmod %d(%s)\n",
		sp_fbinfo->width,
		sp_fbinfo->height,
		sp_fbinfo->color_mode,
		sp_fbinfo->cmod.name);
	ret = sp7350fb_create_device(pdev, sp_fbinfo);
	if (ret)
		goto err_fb_probe;

	ret = sp7350_osd_region_create(sp_fbinfo);
	if (ret)
		goto err_fb_probe;

	fbdev = (struct sp7350fb_device *)gsp_fbinfo->par;

	fbdev->osd_handle = sp_fbinfo->handle;

	platform_set_drvdata(pdev, fbdev);

	sp7350_osd_region_irq_enable();

	//pr_info("fb probe done\n");

	return 0;

err_fb_probe:
	if (sp_fbinfo)
		kfree(sp_fbinfo);

	if (gsp_fbinfo) {
		unregister_framebuffer(fbdev->fb);
		framebuffer_release(fbdev->fb);
		gsp_fbinfo = NULL;
	}

	sp7350_osd_region_irq_disable();
	sp7350_osd_region_release();

	return ret;
}

static int sp7350fb_remove(struct platform_device *pdev)
{
	struct sp7350fb_device *fbdev = platform_get_drvdata(pdev);

	//pr_info("%s\n", __func__);

	if (gsp_fbinfo) {
		//pr_info("fb unreg and release\n");
		unregister_framebuffer(fbdev->fb);
		framebuffer_release(fbdev->fb);	
		gsp_fbinfo = NULL;
	}
	sp7350_osd_region_irq_disable();
	sp7350_osd_region_release();
	//pr_info("%s done\n", __func__);

	return 0;
}

static const struct of_device_id sp7350fb_dt_ids[] = {
	{ .compatible = "sunplus,sp7350-fb", },
	{}
};
MODULE_DEVICE_TABLE(of, sp7350fb_dt_ids);

static struct platform_driver sp7350fb_driver = {
	.probe		= sp7350fb_probe,
	.remove		= sp7350fb_remove,
	.driver		= {
		.name	= "sp7350-fb",
		.of_match_table = sp7350fb_dt_ids,
	},
};
module_platform_driver(sp7350fb_driver);

MODULE_DESCRIPTION("SP7350 Framebuffer Driver");
MODULE_AUTHOR("Hammer Hsieh <hammer.hsieh@sunplus.com>");
MODULE_LICENSE("GPL v2");
