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

/*
 * VPP / OSD data fetch test
 */
unsigned char vpp0_data_array[720*480*2] __attribute__((aligned(1024))) = {
	#include "test_pattern/yuv422_YUY2_720x480_vpp.h"
};
unsigned char osd0_data_array[200*200*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_200x200_osd0.h"
};
unsigned char osd1_data_array[200*200*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_200x200_osd1.h"
};
unsigned char osd2_data_array[200*200*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_200x200_osd2.h"
};
unsigned char osd3_data_array[200*200*4] __attribute__((aligned(1024))) = {
	#include "test_pattern/ARGB8888_200x200_osd3.h"
};

static irqreturn_t sp7350_display_irq_fs(int irq, void *param)
{
	#ifdef V4L2_TEST_DQBUF
	struct sp_disp_device *disp_dev = platform_get_drvdata(param);
	struct sp_disp_buffer *next_frm;
	struct sp_disp_layer *layer;
	struct sp7350fb_info info;
	unsigned long addr;
	int i, yuv_fmt;

	if (!param)
		return IRQ_HANDLED;

	if ((!disp_dev->dev[0]) && (!disp_dev->dev[1]) && (!disp_dev->dev[2]) &&
			(!disp_dev->dev[3]) && (!disp_dev->dev[4]))
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
					info.buf_addr_phy = (u32)addr;

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
					else if (layer->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
						yuv_fmt = 0x1;
					else
						yuv_fmt = 0x2;
					/*
					 * set vpp layer for imgread block
					 */
					sp7350_vpp_imgread_set((u32)addr,
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
	if (sp7350_disp_state == 0) {
		sp7350_osd_region_update();
	}

	return IRQ_HANDLED;
}

static irqreturn_t sp7350_display_irq_int1(int irq, void *param)
{
	return IRQ_HANDLED;
}

static irqreturn_t sp7350_display_irq_int2(int irq, void *param)
{
	return IRQ_HANDLED;
}

static const char * const mipitx_format_str_dsi[] = {
	"DSI-RGB565-16bits", "DSI-RGB666-18bits", "DSI-RGB666-24bits",
	"DSI-RGB888-24bits", "none", "none", "none", "none"};

static const char * const mipitx_format_str_csi[] = {
	"CSI-RGB565-16bits", "CSI-YUV422-16bits", "CSI-YUV422-20bits",
	"CSI-RGB888-24bits", "none", "none", "none", "none"};

static int sp7350_resolution_get(struct sp_disp_device *disp_dev)
{
	u32 osd0_res[5], osd1_res[5], osd2_res[5], osd3_res[5];
	u32 vpp0_res[7];
	u32 out_res[3];
	u32 mipitx_lane, mipitx_clk_edge;
	u32 mipitx_sync_timing, mipitx_format;
	//int ret, i;
	int ret;
	const char *connect_dev_name = "null_dev";

	/*
	 * get connect_dev_name
	 */
	ret = of_property_read_string_index(disp_dev->pdev->of_node, "sp7350,dev_name", 0, &connect_dev_name);
	if(!strcmp("null_dev", connect_dev_name)) {
		disp_dev->mipitx_dev_id = 0x88888888;
		pr_err("video out not set\n");
	}
	if(!strcmp("HXM0686TFT-001", connect_dev_name)) /* 480x1280 */
		disp_dev->mipitx_dev_id = 0x00001000;
	else if(!strcmp("TCXD024IBLON-2", connect_dev_name)) /* 240x320 */
		disp_dev->mipitx_dev_id = 0x00001001;
	else
		disp_dev->mipitx_dev_id = 0x00000000;

	pr_info("connect_dev_name %s\n", connect_dev_name);

	/*
	 * set osd0_layer (offset & resolution & format)
	 */
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

	/*
	 * set osd1_layer (offset & resolution & format)
	 */
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

	/*
	 * set osd2_layer (offset & resolution & format)
	 */
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

	/*
	 * set osd3_layer (offset & resolution & format)
	 */
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

	#if 0
	for (i = 0; i < SP_DISP_MAX_OSD_LAYER; i++) {
		pr_info("  osd%d width/height/cmod = (%d %d) %d %d %d\n",
			i,
			disp_dev->osd_res[i].x_ofs,
			disp_dev->osd_res[i].y_ofs,
			disp_dev->osd_res[i].width,
			disp_dev->osd_res[i].height,
			disp_dev->osd_res[i].color_mode);
	}
	#endif

	/*
	 * set vpp0_layer (offset & resolution & format)
	 */
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
	#if 0
	pr_info("  vpp0 (x,y) (xlen,ylen) = (%d %d)(%d %d)\n",
		disp_dev->vpp_res[0].x_ofs,
		disp_dev->vpp_res[0].y_ofs,
		disp_dev->vpp_res[0].crop_w,
		disp_dev->vpp_res[0].crop_h);
	pr_info("  vpp0 width/height/cmod = %d %d %d\n",
		disp_dev->vpp_res[0].width,
		disp_dev->vpp_res[0].height,
		disp_dev->vpp_res[0].color_mode);
	#endif

	/*
	 * set mipitx output type & resolution
	 */
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
	#if 0
	pr_info("  mipitx width/height/mod = %d %d MIPITX %s\n",
		disp_dev->out_res.width,
		disp_dev->out_res.height,
		disp_dev->out_res.mipitx_mode?"CSI":"DSI");
	#endif

	/*
	 * set mipitx_lane
	 */
	ret = of_property_read_u32(disp_dev->pdev->of_node,
		"sp7350,disp_mipitx_lane", &mipitx_lane);
	if (ret)
		disp_dev->mipitx_lane = 4;//4 lane;
	else
		disp_dev->mipitx_lane = mipitx_lane;

	if ((mipitx_lane != 0x1) && (mipitx_lane != 0x2) && (mipitx_lane != 0x4))
		disp_dev->mipitx_lane = 4;//4 lane;

	/*
	 * set mipitx_clk_edge
	 */
	ret = of_property_read_u32(disp_dev->pdev->of_node,
		"sp7350,disp_mipitx_clk_edge", &mipitx_clk_edge);
	if (ret)
		disp_dev->mipitx_clk_edge = 0; //Raising Edge
	else
		disp_dev->mipitx_clk_edge = mipitx_clk_edge;

	/*
	 * set mipitx_sync_timing
	 */
	ret = of_property_read_u32(disp_dev->pdev->of_node,
		"sp7350,disp_mipitx_sync_timing", &mipitx_sync_timing);
	if (ret)
		disp_dev->mipitx_sync_timing = 0; //Sync Pulse
	else
		disp_dev->mipitx_sync_timing = mipitx_sync_timing;

	if (mipitx_sync_timing >= 0x2)
		disp_dev->mipitx_sync_timing = 1; //Sync event

	/*
	 * set mipitx_format
	 */
	ret = of_property_read_u32(disp_dev->pdev->of_node,
		"sp7350,disp_mipitx_format", &mipitx_format);
	if (ret)
		disp_dev->mipitx_format = 3;//DSI RGB888 or CSI_24BITS
	else
		disp_dev->mipitx_format = mipitx_format;

	if (mipitx_format >= 0x4)
		disp_dev->mipitx_format = 3;//DSI RGB888 or CSI_24BITS

	/*
	 * decide mipitx_data_bit based on mipitx_format
	 */
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {
		if (disp_dev->mipitx_format == 0x0)
			disp_dev->mipitx_data_bit = 16;
		else if (disp_dev->mipitx_format == 0x1)
			disp_dev->mipitx_data_bit = 18;
		else if (disp_dev->mipitx_format == 0x2)
			disp_dev->mipitx_data_bit = 24;
		else if (disp_dev->mipitx_format == 0x3)
			disp_dev->mipitx_data_bit = 24;
		else
			disp_dev->mipitx_data_bit = 24;
	} else if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {
		if (disp_dev->mipitx_format == 0x0)
			disp_dev->mipitx_data_bit = 16;
		else if (disp_dev->mipitx_format == 0x1)
			disp_dev->mipitx_data_bit = 16;
		else if (disp_dev->mipitx_format == 0x2)
			disp_dev->mipitx_data_bit = 20;
		else if (disp_dev->mipitx_format == 0x3)
			disp_dev->mipitx_data_bit = 24;
		else
			disp_dev->mipitx_data_bit = 24;
	}

	#if 0
	pr_info("  mipitx set (%s)&(%s)\n",
		disp_dev->mipitx_clk_edge?"Falling Edge":"Raising Edge",
		disp_dev->mipitx_sync_timing?"Sync Event":"Sync Pulse");

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		pr_info("  mipitx %d lane %s data_bits %d\n",
			disp_dev->mipitx_lane,
			mipitx_format_str_dsi[disp_dev->mipitx_format],
			disp_dev->mipitx_data_bit);
	else if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI)
		pr_info("  mipitx %d lane %s data_bits %d\n",
			disp_dev->mipitx_lane,
			mipitx_format_str_csi[disp_dev->mipitx_format],
			disp_dev->mipitx_data_bit);
	#endif

	return ret;
}

static const char * const sp7350_disp_clkc[] = {
	"clkc_dispsys", "clkc_dmix", "clkc_gpost0", "clkc_gpost1",
	"clkc_gpost2", "clkc_gpost3", "clkc_imgread0", "clkc_mipitx",
	"clkc_osd0", "clkc_osd1", "clkc_osd2", "clkc_osd3",
	"clkc_tcon", "clkc_tgen", "clkc_vpost0", "clkc_vscl0"
};

static const char * const sp7350_disp_rtsc[] = {
	"rstc_dispsys", "rstc_dmix", "rstc_gpost0", "rstc_gpost1",
	"rstc_gpost2", "rstc_gpost3", "rstc_imgread0", "rstc_mipitx",
	"rstc_osd0", "rstc_osd1", "rstc_osd2", "rstc_osd3",
	"rstc_tcon", "rstc_tgen", "rstc_vpost0", "rstc_vscl0"
};

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

	// Set the driver data in platform device.
	platform_set_drvdata(pdev, disp_dev);

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
	 * get reset gpio for MIPITX DSI (optional)
	 */
	disp_dev->reset_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
	//if (IS_ERR(disp_dev->reset_gpio))
	//	pr_err("reset_gpio not found\n");

	/*
	 * get reg base resource
	 */
	disp_dev->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(disp_dev->base))
		return dev_err_probe(&pdev->dev, PTR_ERR(disp_dev->base), "reg base not found\n");

	disp_dev->ao_moon3 = devm_platform_get_and_ioremap_resource(pdev, 1, &res);
	if (IS_ERR(disp_dev->ao_moon3))
		return dev_err_probe(&pdev->dev, PTR_ERR(disp_dev->ao_moon3), "reg ao_moon3 not found\n");

	/*
	 * init clk & reset
	 */
	for (i = 0; i < 16; i++) {
		disp_dev->disp_clk[i] = devm_clk_get(&pdev->dev, sp7350_disp_clkc[i]);
		//pr_info("default clk[%d] %ld\n", i, clk_get_rate(disp_dev->disp_clk[i]));
		if (IS_ERR(disp_dev->disp_clk[i]))
			return PTR_ERR(disp_dev->disp_clk[i]);

		disp_dev->disp_rstc[i] = devm_reset_control_get_exclusive(&pdev->dev, sp7350_disp_rtsc[i]);
		if (IS_ERR(disp_dev->disp_rstc[i]))
			return dev_err_probe(&pdev->dev, PTR_ERR(disp_dev->disp_rstc[i]), "err get reset\n");

		ret = reset_control_deassert(disp_dev->disp_rstc[i]);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "failed to deassert reset\n");

		ret = clk_prepare_enable(disp_dev->disp_clk[i]);
		if (ret)
			return ret;

		//if (i == 7) {
		//	clk_set_rate(disp_dev->disp_clk[i], 60000000);//set 60MHz
		//	pr_info("set clk[%d] %ld\n", i, clk_get_rate(disp_dev->disp_clk[i]));
		//}
	}

	/*
	 * get all necessary resolution from dts
	 */
	sp7350_resolution_get(disp_dev);

	//dmix must first init
	sp7350_dmix_init();

	sp7350_tgen_init();
	
	sp7350_tcon_init();

	sp7350_osd_init();

	sp7350_vpp_init();

	/* dmix setting
	 * L6   L5   L4   L3   L2   L1   BG
	 * OSD0 OSD1 OSD2 OSD3 ---- VPP0 PTG
	 */
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {
		sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_BLENDING);
		sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_BLENDING);
		sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_BLENDING);
		sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_BLENDING);
		sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
	} else {
		sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
	}
	//sp7350_dmix_all_layer_info();
	sp7350_dmix_layer_cfg_store();

	/*
	 * v4l2 init for osd/vpp layers
	 */
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

	/*
	 * init resolution setting for osd layers
	 */
	sp7350_osd_resolution_init(disp_dev);

	/*
	 * init resolution setting for vpp layers
	 */
	sp7350_vpp_resolution_init(disp_dev);

	#ifdef SP_DISP_V4L2_SUPPORT
	/*
	 * init layer setting for v4l2
	 */
	for (i = 0; i < SP_DISP_MAX_DEVICES; i++) {
		if (i == 4) {
			/* for vpp layer */
			disp_dev->dev[i]->fmt.fmt.pix.width = disp_dev->vpp_res[0].width;
			disp_dev->dev[i]->fmt.fmt.pix.height = disp_dev->vpp_res[0].height;

			if (disp_dev->vpp_res[0].color_mode == SP7350_VPP_IMGREAD_DATA_FMT_UYVY)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
			else if (disp_dev->vpp_res[0].color_mode == SP7350_VPP_IMGREAD_DATA_FMT_YUY2)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
			else if (disp_dev->vpp_res[0].color_mode == SP7350_VPP_IMGREAD_DATA_FMT_NV16)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV16;
			else if (disp_dev->vpp_res[0].color_mode == SP7350_VPP_IMGREAD_DATA_FMT_NV24)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV24;
			else if (disp_dev->vpp_res[0].color_mode == SP7350_VPP_IMGREAD_DATA_FMT_NV12)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
			else
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		} else {
			/* for osd layers */
			disp_dev->dev[i]->fmt.fmt.pix.width = disp_dev->osd_res[i].width;
			disp_dev->dev[i]->fmt.fmt.pix.height = disp_dev->osd_res[i].height;

			if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_8BPP)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_YUY2)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_RGB565)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_ARGB1555)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ARGB555;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_RGBA4444)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB444;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_ARGB4444)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ARGB444;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_RGBA8888)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ABGR32;
			else if (disp_dev->osd_res[i].color_mode == SP7350_OSD_COLOR_MODE_ARGB8888)
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ARGB32;
			else
				disp_dev->dev[i]->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ARGB32;
		}
		disp_dev->dev[i]->fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		disp_dev->dev[i]->fmt.fmt.pix.field = V4L2_FIELD_NONE;
		disp_dev->dev[i]->fmt.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
		disp_dev->dev[i]->fmt.fmt.pix.priv = 0;
	}
	#endif

	/*
	 * init MIPITX DSI or CSI output setting
	 */
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		sp7350_mipitx_phy_init_dsi();
	else
		sp7350_mipitx_phy_init_csi();

	
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
	 * disable clk & enable reset
	 */
	for (i = 0; i < 16; i++) {
		clk_disable_unprepare(disp_dev->disp_clk[i]);
		reset_control_assert(disp_dev->disp_rstc[i]);
	}

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
	int i;

	pr_info("%s\n", __func__);
	/*
	 * store display settings
	 */
	sp7350_dmix_layer_cfg_store();
	sp7350_tgen_store();
	sp7350_tcon_store();
	sp7350_mipitx_store();
	sp7350_osd_store();
	sp7350_osd_header_save();
	sp7350_vpp0_store();

	/*
	 * disable clk & enable reset
	 */
	for (i = 0; i < 16; i++) {
		if (i == 7) continue; //skip set mipitx module
		clk_disable_unprepare(disp_dev->disp_clk[i]);
		reset_control_assert(disp_dev->disp_rstc[i]);
	}

	return 0;
}

static int sp7350_display_resume(struct platform_device *pdev)
{
	struct sp_disp_device *disp_dev = platform_get_drvdata(pdev);
	int i;

	pr_info("%s\n", __func__);
	/*
	 * disable reset & enable clk
	 */
	for (i = 0; i < 16; i++) {
		if (i == 7) continue; //skip set mipitx module
		reset_control_deassert(disp_dev->disp_rstc[i]);
		clk_prepare_enable(disp_dev->disp_clk[i]);
	}

	/*
	 * restore display settings
	 */
	sp7350_dmix_layer_cfg_restore();
	sp7350_tgen_restore();
	sp7350_tcon_restore();
	sp7350_mipitx_restore();
	sp7350_osd_restore();
	for (i = 0; i < SP_DISP_MAX_OSD_LAYER; i++)
		sp7350_osd_header_restore(i);

	sp7350_vpp0_restore();
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		sp7350_mipitx_phy_init_dsi();
	else
		sp7350_mipitx_phy_init_csi();

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
