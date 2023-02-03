// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>

#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

#ifdef SP_DISP_V4L2_SUPPORT
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-dma-contig.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>
#include <media/v4l2-common.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#endif

#if defined(CONFIG_VIDEO_SP7350_DISP_DEBUG)
struct proc_dir_entry *entry;
#define MON_PROC_NAME	"disp_mon"
#endif

extern int sp7350_disp_state;
struct sp_disp_device *gdisp_dev;

extern int sp7350_vpp_resolution_init(struct sp_disp_device *disp_dev);
extern int sp7350_osd_resolution_init(struct sp_disp_device *disp_dev);
#ifdef SP_DISP_V4L2_SUPPORT
extern int sp7350_v4l2_initialize(struct device *dev,
		struct sp_disp_device *disp_dev);
extern int sp7350_v4l2_init_multi_layer(int i, struct platform_device *pdev,
		struct sp_disp_device *disp_dev);
extern int sp7350_v4l2_reg_multi_layer(int i, struct platform_device *pdev,
		struct sp_disp_device *disp_dev);
#endif

unsigned char vpp0_data_array[720*480*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/yuv422_YUY2_720x480_vpp.h"
};
unsigned char osd0_data_array[320*240*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_320x240.h"
};
unsigned char osd1_data_array[320*240*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_320x240.h"
};
unsigned char osd2_data_array[320*240*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_320x240.h"
};
unsigned char osd3_data_array[320*240*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_320x240.h"
};

static irqreturn_t sp7350_display_irq_fs(int irq, void *param)
{
	//pr_info("%s\n", __func__);
	#ifdef V4L2_TEST_DQBUF
	struct sp_disp_device *disp_dev = platform_get_drvdata(param);
	struct sp_disp_buffer *next_frm;
	struct sp_disp_layer *layer;
	struct sp7350fb_info info;
	unsigned long addr;
	int i, yuv_fmt;

	if (!param)
		return IRQ_HANDLED;

	for (i = 0; i < SP_DISP_MAX_DEVICES; i++) {
		layer = disp_dev->dev[i];

		if (!disp_dev->dev[i])
			continue;

		if (layer->skip_first_int) {
			layer->skip_first_int = 0;
			continue;
		}

		if (layer->streaming) {
			spin_lock(&disp_dev->dma_queue_lock);
			if (!list_empty(&layer->dma_queue)) {
				next_frm = list_entry(layer->dma_queue.next,
						struct  sp_disp_buffer, list);
				/* Remove that from the buffer queue */
				list_del_init(&next_frm->list);

				/* Mark state of the frame to active */
				next_frm->vb.vb2_buf.state = VB2_BUF_STATE_ACTIVE;

				addr = vb2_dma_contig_plane_dma_addr(&next_frm->vb.vb2_buf, 0);

				if ((i >= SP_DISP_DEVICE_0) && (i <= SP_DISP_DEVICE_3)) {
					if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ARGB32)
						info.color_mode = 0xe;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ABGR32)
						info.color_mode = 0xd;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ARGB444)
						info.color_mode = 0xb;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB444)
						info.color_mode = 0xa;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_ARGB555)
						info.color_mode = 0x9;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565)
						info.color_mode = 0x8;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
						info.color_mode = 0x4;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_PAL8)
						info.color_mode = 0x2;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
						info.color_mode = 0x2;

					info.width = layer->fmt.fmt.pix.width;
					info.height = layer->fmt.fmt.pix.height;
					info.buf_addr = addr;

					sp7350_osd_layer_set(&info, i);

				} else if (i == SP_DISP_DEVICE_4) {
					if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
						yuv_fmt = 0x7;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV16)
						yuv_fmt = 0x3;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV24)
						yuv_fmt = 0x6;
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
						yuv_fmt = 0x2;
					/*
					 * set vpp layer for imgread block
					 */
					sp7350_vpp_imgread_set((u32)virt_to_phys((unsigned long *)addr),
							disp_dev->vpp_res[0].x_ofs,
							disp_dev->vpp_res[0].y_ofs,
							layer->fmt.fmt.pix.width,
							layer->fmt.fmt.pix.height,
							yuv_fmt);
					/*
					 * set vpp layer for vscl block
					 */
					sp7350_vpp_vscl_set(disp_dev->vpp_res[0].x_ofs, disp_dev->vpp_res[0].y_ofs,
							disp_dev->vpp_res[0].crop_w, disp_dev->vpp_res[0].crop_h,
							layer->fmt.fmt.pix.width, layer->fmt.fmt.pix.height,
							disp_dev->out_res.width, disp_dev->out_res.height);
				}

				layer->cur_frm->vb.vb2_buf.timestamp = ktime_get_ns();
				layer->cur_frm->vb.field = layer->fmt.fmt.pix.field;
				layer->cur_frm->vb.sequence = layer->sequence++;
				vb2_buffer_done(&layer->cur_frm->vb.vb2_buf, VB2_BUF_STATE_DONE);
				/* Make cur_frm pointing to next_frm */
				layer->cur_frm = next_frm;
			}
			spin_unlock(&disp_dev->dma_queue_lock);

		}
	}
	#endif

	return IRQ_HANDLED;
}

static irqreturn_t sp7350_display_irq_fe(int irq, void *param)
{
	//pr_info("%s\n", __func__);
	if (sp7350_disp_state == 0) {
		sp7350_osd_region_update();
	}

	return IRQ_HANDLED;
}

static irqreturn_t sp7350_display_irq_int1(int irq, void *param)
{
	//pr_info("%s\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t sp7350_display_irq_int2(int irq, void *param)
{
	//pr_info("%s\n", __func__);

	return IRQ_HANDLED;
}

static int sp7350_resolution_get(struct sp_disp_device *disp_dev)
{
	u32 osd0_res[5], osd1_res[5], osd2_res[5], osd3_res[5];
	u32 vpp0_res[7];
	u32 out_res[3];
	int ret;

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
			"sp7350,osd0_layer", osd0_res, 5);
	if (ret) {
		disp_dev->osd_res[0].x_ofs = 0;
		disp_dev->osd_res[0].y_ofs = 0;
		disp_dev->osd_res[0].width = 0;//720;
		disp_dev->osd_res[0].height = 0;//480;
		disp_dev->osd_res[0].color_mode = SP7350_OSD_COLOR_MODE_ARGB8888;
	} else {
		disp_dev->osd_res[0].x_ofs = osd0_res[0];
		disp_dev->osd_res[0].y_ofs = osd0_res[1];
		disp_dev->osd_res[0].width = osd0_res[2];
		disp_dev->osd_res[0].height = osd0_res[3];
		disp_dev->osd_res[0].color_mode = osd0_res[4];
	}
	//pr_info("  osd0 width/height/cmod = (%d %d) %d %d %d\n",
	//	disp_dev->osd_res[0].x_ofs,
	//	disp_dev->osd_res[0].y_ofs,
	//	disp_dev->osd_res[0].width,
	//	disp_dev->osd_res[0].height,
	//	disp_dev->osd_res[0].color_mode);

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
		"sp7350,osd1_layer", osd1_res, 5);
	if (ret) {
		disp_dev->osd_res[1].x_ofs = 0;
		disp_dev->osd_res[1].y_ofs = 0;
		disp_dev->osd_res[1].width = 0;//720;
		disp_dev->osd_res[1].height = 0;//480;
		disp_dev->osd_res[1].color_mode = SP7350_OSD_COLOR_MODE_ARGB8888;
	} else {
		disp_dev->osd_res[1].x_ofs = osd1_res[0];
		disp_dev->osd_res[1].y_ofs = osd1_res[1];
		disp_dev->osd_res[1].width = osd1_res[2];
		disp_dev->osd_res[1].height = osd1_res[3];
		disp_dev->osd_res[1].color_mode = osd1_res[4];
	}
	//pr_info("  osd1 width/height/cmod = (%d %d) %d %d %d\n",
	//	disp_dev->osd_res[1].x_ofs,
	//	disp_dev->osd_res[1].y_ofs,
	//	disp_dev->osd_res[1].width,
	//	disp_dev->osd_res[1].height,
	//	disp_dev->osd_res[1].color_mode);

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
		"sp7350,osd2_layer", osd2_res, 5);
	if (ret) {
		disp_dev->osd_res[2].x_ofs = 0;
		disp_dev->osd_res[2].y_ofs = 0;
		disp_dev->osd_res[2].width = 0;//720;
		disp_dev->osd_res[2].height = 0;//480;
		disp_dev->osd_res[2].color_mode = SP7350_OSD_COLOR_MODE_ARGB8888;
	} else {
		disp_dev->osd_res[2].x_ofs = osd2_res[0];
		disp_dev->osd_res[2].y_ofs = osd2_res[1];
		disp_dev->osd_res[2].width = osd2_res[2];
		disp_dev->osd_res[2].height = osd2_res[3];
		disp_dev->osd_res[2].color_mode = osd2_res[4];
	}
	//pr_info("  osd2 width/height/cmod = (%d %d) %d %d %d\n",
	//	disp_dev->osd_res[2].x_ofs,
	//	disp_dev->osd_res[2].y_ofs,
	//	disp_dev->osd_res[2].width,
	//	disp_dev->osd_res[2].height,
	//	disp_dev->osd_res[2].color_mode);

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
		"sp7350,osd3_layer", osd3_res, 5);
	if (ret) {
		disp_dev->osd_res[3].x_ofs = 0;
		disp_dev->osd_res[3].y_ofs = 0;
		disp_dev->osd_res[3].width = 0;//720;
		disp_dev->osd_res[3].height = 0;//480;
		disp_dev->osd_res[3].color_mode = SP7350_OSD_COLOR_MODE_ARGB8888;
	} else {
		disp_dev->osd_res[3].x_ofs = osd3_res[0];
		disp_dev->osd_res[3].y_ofs = osd3_res[1];
		disp_dev->osd_res[3].width = osd3_res[2];
		disp_dev->osd_res[3].height = osd3_res[3];
		disp_dev->osd_res[3].color_mode = osd3_res[4];
	}
	//pr_info("  osd3 width/height/cmod = (%d %d) %d %d %d\n",
	//	disp_dev->osd_res[3].x_ofs,
	//	disp_dev->osd_res[3].y_ofs,
	//	disp_dev->osd_res[3].width,
	//	disp_dev->osd_res[3].height,
	//	disp_dev->osd_res[3].color_mode);

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
		"sp7350,vpp0_layer", vpp0_res, 7);
	if (ret) {
		disp_dev->vpp_res[0].x_ofs = 0;
		disp_dev->vpp_res[0].y_ofs = 0;
		disp_dev->vpp_res[0].crop_w = 0;
		disp_dev->vpp_res[0].crop_h = 0;
		disp_dev->vpp_res[0].width = 0;//720;
		disp_dev->vpp_res[0].height = 0;//480;
		disp_dev->vpp_res[0].color_mode = SP7350_VPP_IMGREAD_DATA_FMT_YUY2;
	} else {
		disp_dev->vpp_res[0].x_ofs = vpp0_res[0];
		disp_dev->vpp_res[0].y_ofs = vpp0_res[1];
		disp_dev->vpp_res[0].crop_w = vpp0_res[2];
		disp_dev->vpp_res[0].crop_h = vpp0_res[3];
		disp_dev->vpp_res[0].width = vpp0_res[4];
		disp_dev->vpp_res[0].height = vpp0_res[5];
		disp_dev->vpp_res[0].color_mode = vpp0_res[6];

		if ((vpp0_res[0] + vpp0_res[2]) > vpp0_res[4])
			pr_info("  warning (x_ofs + crop_w) > input_w!\n");
		if ((vpp0_res[1] + vpp0_res[3]) > vpp0_res[5])
			pr_info("  warning (y_ofs + crop_h) > input_h!\n");
	}
	//pr_info("  vpp0 (x,y) (xlen,ylen) = (%d %d)(%d %d)\n",
	//	disp_dev->vpp_res[0].x_ofs,
	//	disp_dev->vpp_res[0].y_ofs,
	//	disp_dev->vpp_res[0].crop_w,
	//	disp_dev->vpp_res[0].crop_h);
	//pr_info("  vpp0 width/height/cmod = %d %d %d\n",
	//	disp_dev->vpp_res[0].width,
	//	disp_dev->vpp_res[0].height,
	//	disp_dev->vpp_res[0].color_mode);

	ret = of_property_read_u32_array(disp_dev->pdev->of_node,
		"sp7350,disp_output", out_res, 3);
	if (ret) {
		disp_dev->out_res.width = 0;//720;
		disp_dev->out_res.height = 0;//480;
		disp_dev->out_res.mipitx_mode = SP7350_MIPITX_DSI;
	} else {
		disp_dev->out_res.width = out_res[0];
		disp_dev->out_res.height = out_res[1];
		disp_dev->out_res.mipitx_mode = out_res[2];
	}
	pr_info("  mipitx width/height/mod = %d %d MIPITX %s\n",
		disp_dev->out_res.width,
		disp_dev->out_res.height,
		disp_dev->out_res.mipitx_mode?"CSI":"DSI");

	return ret;
}

static int sp7350_display_probe(struct platform_device *pdev)
{
	int num_irq = of_irq_count(pdev->dev.of_node);
	struct device *dev = &pdev->dev;
	struct sp_disp_device *disp_dev;
	struct resource *res;
	int i, irq, ret;

	pr_info("%s: disp probe ...\n", __func__);

	disp_dev = devm_kzalloc(dev, sizeof(*disp_dev), GFP_KERNEL);
	if (!disp_dev)
		return -ENOMEM;

	disp_dev->pdev = dev;
	gdisp_dev = disp_dev;

	/*
	 * request irq
	 */
	pr_info("%s: disp probe %d irq\n", __func__, num_irq);
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, sp7350_display_irq_fs,
			IRQF_TRIGGER_RISING, "sp_disp_irq_fs", pdev);
	if (ret)
		pr_err("request_irq fail\n");

	irq = platform_get_irq(pdev, 1);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, sp7350_display_irq_fe,
			IRQF_TRIGGER_RISING, "sp_disp_irq_fe", pdev);
	if (ret)
		pr_err("request_irq fail\n");

	irq = platform_get_irq(pdev, 2);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, sp7350_display_irq_int1,
			IRQF_TRIGGER_RISING, "sp_disp_irq_int1", pdev);
	if (ret)
		pr_err("request_irq fail\n");

	irq = platform_get_irq(pdev, 3);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, sp7350_display_irq_int2,
			IRQF_TRIGGER_RISING, "sp_disp_irq_int2", pdev);
	if (ret)
		pr_err("request_irq fail\n");

	/*
	 * get reg base resource
	 */
	//pr_info("%s: disp probe reg base\n", __func__);
	//disp_dev->base = devm_platform_ioremap_resource(pdev, 0);
	//if (IS_ERR(disp_dev->base))
	//	return PTR_ERR(disp_dev->base);

	disp_dev->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(disp_dev->base))
		return dev_err_probe(&pdev->dev, PTR_ERR(disp_dev->base), "reg base not found\n");

	/*
	 * init clk
	 */
	//pr_info("%s: disp probe clk\n", __func__);

	for (i = 0; i < 16; i++) {
		disp_dev->disp_clk[i] = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(disp_dev->disp_clk[i]))
			return PTR_ERR(disp_dev->disp_clk[i]);
		ret = clk_prepare_enable(disp_dev->disp_clk[i]);
		if (ret)
			return ret;
	}

	//dmix must first init
	//pr_info("%s: init dmix ...\n", __func__);
	sp7350_dmix_init();
	//pr_info("%s: init tgen ...\n", __func__);
	sp7350_tgen_init();
	//pr_info("%s: init tcon ...\n", __func__);
	sp7350_tcon_init();
	//pr_info("%s: init mipitx ...\n", __func__);
	sp7350_mipitx_init();
	//pr_info("%s: init osd ...\n", __func__);
	sp7350_osd_init();
	//pr_info("%s: init vpp ...\n", __func__);
	sp7350_vpp_init();

	//pr_info("%s: init dmix layer ...\n", __func__);
	/* dmix setting
	 * L6   L5   L4   L3   L2   L1   BG
	 * OSD0 OSD1 OSD2 OSD3 ---- VPP0 PTG
	 */
	sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_BLENDING);
	sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_BLENDING);
	sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_BLENDING);
	sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_BLENDING);
	sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);

	/*
	 * get all necessary resolution from dts
	 *
	 */
	sp7350_resolution_get(disp_dev);

	/*
	 * v4l2 init for osd/vpp layers
	 *
	 */
	//pr_info("%s: register v4l2 ...\n", __func__);
	#ifdef SP_DISP_V4L2_SUPPORT
	ret = sp7350_v4l2_initialize(&pdev->dev, disp_dev);
	if (ret)
		return ret;

	for (i = 0; i < SP_DISP_MAX_DEVICES; i++) {
		if (sp7350_v4l2_init_multi_layer(i, pdev, disp_dev)) {
			ret = -ENODEV;
			return ret;
		}
		if (sp7350_v4l2_reg_multi_layer(i, pdev, disp_dev)) {
			ret = -ENODEV;
			return ret;
		}
	}
	#endif

	//pr_info("%s: set osd layer ...\n", __func__);
	//TBD , osd_res[x] != out_res is acceptable
	//disp_dev->out_res.width = disp_dev->osd_res[0].width;
	//disp_dev->out_res.height = disp_dev->osd_res[0].height;
	sp7350_osd_resolution_init(disp_dev);

	//pr_info("%s: set vpp layer ...\n", __func__);
	sp7350_vpp_resolution_init(disp_dev);

	//pr_info("%s: set mipitx ...\n", __func__);

	platform_set_drvdata(pdev, disp_dev);

#if defined(CONFIG_VIDEO_SP7350_DISP_DEBUG)
	/*
	 * init debug fs
	 */
	sp7350_debug_init(disp_dev);
	/*
	 * create debug proc
	 */
	entry = proc_create(MON_PROC_NAME, 0444, NULL, &sp_disp_proc_ops);
#endif

	pr_info("%s: disp probe done\n", __func__);

	return 0;
}

static int sp7350_display_remove(struct platform_device *pdev)
{
	struct sp_disp_device *disp_dev = platform_get_drvdata(pdev);
	int i, irq;
	#ifdef SP_DISP_V4L2_SUPPORT
	struct sp_disp_layer *disp_layer = NULL;
	#endif

	pr_info("%s\n", __func__);

	/*
	 * free irq
	 */
	for (i = 0; i < 4; i++) {
		irq = platform_get_irq(pdev, i);
		devm_free_irq(&pdev->dev, irq, pdev);
	}

	/*
	 * disable clk
	 */
	for (i = 0; i < 16; i++)
		clk_disable_unprepare(disp_dev->disp_clk[i]);

#ifdef SP_DISP_V4L2_SUPPORT
	/*
	 * unregister v4l2
	 */
	for (i = 0; i < SP_DISP_MAX_DEVICES; i++) {
		/* Get the pointer to the layer object */
		disp_layer = disp_dev->dev[i];
		/* Unregister video device */
		video_unregister_device(&disp_layer->video_dev);
	}
	for (i = 0; i < SP_DISP_MAX_DEVICES; i++) {
		kfree(disp_dev->dev[i]);
		disp_dev->dev[i] = NULL;
	}

	v4l2_device_unregister(&disp_dev->v4l2_dev);
	kfree(disp_dev);
#endif

	#if defined(CONFIG_VIDEO_SP7350_DISP_DEBUG)
	/*
	 * release debug fs
	 */
	sp7350_debug_cleanup(disp_dev);
	/*
	 * release debug proc
	 */
	if (entry)
		remove_proc_entry(MON_PROC_NAME, NULL);
	#endif

	return 0;
}

static int sp7350_display_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sp_disp_device *disp_dev = platform_get_drvdata(pdev);

	pr_info("%s\n", __func__);
	reset_control_assert(disp_dev->rstc);

	return 0;
}

static int sp7350_display_resume(struct platform_device *pdev)
{
	struct sp_disp_device *disp_dev = platform_get_drvdata(pdev);

	pr_info("%s\n", __func__);
	reset_control_deassert(disp_dev->rstc);

	return 0;
}

static const struct of_device_id sp7350_display_ids[] = {
	{ .compatible = "sunplus,sp7350-display"},
	{}
};
MODULE_DEVICE_TABLE(of, sp7350_display_ids);

static struct platform_driver sp7350_display_driver = {
	.probe		= sp7350_display_probe,
	.remove		= sp7350_display_remove,
	.suspend	= sp7350_display_suspend,
	.resume		= sp7350_display_resume,
	.driver		= {
		.name	= "sp7350_display",
		.of_match_table	= sp7350_display_ids,
	},
};
module_platform_driver(sp7350_display_driver);

MODULE_DESCRIPTION("Sunplus SP7350 Display Driver");
MODULE_AUTHOR("Hammer Hsieh <hammer.hsieh@sunplus.com>");
MODULE_LICENSE("GPL v2");
