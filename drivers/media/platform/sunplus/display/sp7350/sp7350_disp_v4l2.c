// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for V4L2
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
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

#include "sp7350_disp_regs.h"
#include "sp7350_display.h"

extern int sp7350_vpp_restore(struct sp_disp_device *disp_dev);

static unsigned int allocator;
module_param(allocator, uint, 0444);
MODULE_PARM_DESC(allocator, " memory allocator selection, default is 0.\n"
			     "\t    0 == dma-contig\n"
			     "\t    1 == vmalloc");
#define DISP_NAME	"sp_disp"
#define DISP_OSD0_NAME	"sp_disp_osd0"
#define DISP_OSD1_NAME	"sp_disp_osd1"
#define DISP_OSD2_NAME	"sp_disp_osd2"
#define DISP_OSD3_NAME	"sp_disp_osd3"
#define DISP_VPP0_NAME	"sp_disp_vpp0"

//static void sp_print_list(struct list_head *head)
void sp_print_list(struct list_head *head)
{
	struct list_head *listptr;
	struct videobuf_buffer *entry;

	//pr_info("********************************************************\n");
	//pr_info("(HEAD addr =  %px, next = %px, prev = %px)\n", head, head->next, head->prev);
	list_for_each(listptr, head) {
		entry = list_entry(listptr, struct videobuf_buffer, stream);
		//pr_info("list addr = %px | next = %px | prev = %px\n", &entry->stream, entry->stream.next,
		//	 entry->stream.prev);
	}
	//pr_info("********************************************************\n");
}

static int sp_disp_open(struct file *file)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	int ret;

	mutex_lock(&disp_dev->lock);

	ret = v4l2_fh_open(file);
	if (ret)
		pr_err("v4l2_fh_open failed!\n");
	
	mutex_unlock(&disp_dev->lock);

	return ret;
}

static int sp_disp_release(struct file *file)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	int ret;

	mutex_lock(&disp_dev->lock);

	ret = _vb2_fop_release(file, NULL);

	mutex_unlock(&disp_dev->lock);

	return ret;
}

static const struct v4l2_file_operations sp_disp_fops = {
	.owner		= THIS_MODULE,
	.open		= sp_disp_open,
	.release	= sp_disp_release,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= vb2_fop_mmap,
	.poll		= vb2_fop_poll,
};

static const struct vb2_mem_ops *const sp_mem_ops[2] = {
	&vb2_dma_contig_memops,
	&vb2_vmalloc_memops,
};

static int sp_get_device_id(struct vb2_queue *vq)
{
	struct sp_disp_layer *layer = vb2_get_drv_priv(vq);
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;

	if (!(strcmp(DISP_OSD0_NAME, layer->video_dev.name)))
		dev_id = SP_DISP_DEVICE_0;
	else if (!(strcmp(DISP_OSD1_NAME, layer->video_dev.name)))
		dev_id = SP_DISP_DEVICE_1;
	else if (!(strcmp(DISP_OSD2_NAME, layer->video_dev.name)))
		dev_id = SP_DISP_DEVICE_2;
	else if (!(strcmp(DISP_OSD3_NAME, layer->video_dev.name)))
		dev_id = SP_DISP_DEVICE_3;
	else if (!(strcmp(DISP_VPP0_NAME, layer->video_dev.name)))
		dev_id = SP_DISP_DEVICE_4;
	else
		pr_err("unknown layer!\n");

	return dev_id;

}

static int sp_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
		unsigned int *nplanes, unsigned int sizes[],
		struct device *alloc_devs[])
{
	/* Get the file handle object and layer object */
	struct sp_disp_layer *layer = vb2_get_drv_priv(vq);
	struct sp_disp_device *disp_dev = layer->disp_dev;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	unsigned int size = 0;

	dev_id = sp_get_device_id(vq);

	if (disp_dev->dev[dev_id])
		size = layer->fmt.fmt.pix.sizeimage;

	if (*nplanes) {
		if (sizes[0] < size)
			return -EINVAL;

		size = sizes[0];
	}

	*nplanes = 1;
	sizes[0] = size;

	/* Store number of buffers allocated in numbuffer member */
	if ((vq->num_buffers + *nbuffers) < MIN_BUFFERS)
		*nbuffers = MIN_BUFFERS - vq->num_buffers;

	return 0;
}

static int sp_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *q = vb->vb2_queue;
	struct sp_disp_layer *layer = vb2_get_drv_priv(q);
	unsigned long size = layer->fmt.fmt.pix.sizeimage;
	unsigned long addr;

	vb2_set_plane_payload(vb, 0, layer->fmt.fmt.pix.sizeimage);

	if (vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
		pr_err("Buffer is too small (%lu < %lu)!\n", vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	addr = vb2_dma_contig_plane_dma_addr(vb, 0);

	vbuf->field = layer->fmt.fmt.pix.field;

	return 0;
}

static void sp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *q = vb->vb2_queue;
	struct sp_disp_layer *layer = vb2_get_drv_priv(q);
	struct sp_disp_device *disp = layer->disp_dev;
	struct sp_disp_buffer *buf = container_of(vbuf, struct sp_disp_buffer, vb);
	unsigned long flags = 0;

	// Add the buffer to the DMA queue.
	spin_lock_irqsave(&disp->dma_queue_lock, flags);
	list_add_tail(&buf->list, &layer->dma_queue);
	sp_print_list(&layer->dma_queue);
	spin_unlock_irqrestore(&disp->dma_queue_lock, flags);

}

static int sp_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct sp_disp_layer *layer = vb2_get_drv_priv(vq);
	struct sp_disp_device *disp_dev = layer->disp_dev;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct sp7350fb_info info;
	unsigned long flags;
	unsigned long addr;
	int yuv_fmt;

	if (layer->streaming) {
		pr_info("Device has started streaming!\n");
		return -EBUSY;
	}

	layer->sequence = 0;

	dev_id = sp_get_device_id(vq);

	spin_lock_irqsave(&disp_dev->dma_queue_lock, flags);

	/* Get the next frame from the buffer queue */
	layer->cur_frm = list_entry(layer->dma_queue.next,
				struct sp_disp_buffer, list);

	// Remove buffer from the dma queue.
	list_del_init(&layer->cur_frm->list);

	addr = vb2_dma_contig_plane_dma_addr(&layer->cur_frm->vb.vb2_buf, 0);

	spin_unlock_irqrestore(&disp_dev->dma_queue_lock, flags);

	if ((dev_id >= SP_DISP_DEVICE_0) && (dev_id <= SP_DISP_DEVICE_4))
		sp7350_dmix_layer_cfg_set(dev_id);

	if ((dev_id >= SP_DISP_DEVICE_0) && (dev_id <= SP_DISP_DEVICE_3)) {
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
		else
			info.color_mode = 0xe;

		info.width = layer->fmt.fmt.pix.width;
		info.height = layer->fmt.fmt.pix.height;
		info.buf_addr_phy = (u32)addr;

		sp7350_osd_layer_set(&info, dev_id);

	} else if (dev_id == SP_DISP_DEVICE_4) {
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

	layer->skip_first_int = 1;
	layer->streaming = 1;

	return 0;
}

static void sp_stop_streaming(struct vb2_queue *vq)
{
	struct sp_disp_layer *layer = vb2_get_drv_priv(vq);
	struct sp_disp_device *disp_dev = layer->disp_dev;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct sp_disp_buffer *buf;
	unsigned long flags;

	if (!layer->streaming) {
		pr_info("Device has stopped already!\n");
		return;
	}

	dev_id = sp_get_device_id(vq);

	if ((dev_id >= SP_DISP_DEVICE_0) && (dev_id <= SP_DISP_DEVICE_4))
		sp7350_dmix_layer_cfg_restore();

	if (dev_id <= SP_DISP_DEVICE_3)
		sp7350_osd_header_clear(dev_id);

	sp7350_vpp_restore(disp_dev);

	layer->streaming = 0;

	// Release all active buffers.
	spin_lock_irqsave(&disp_dev->dma_queue_lock, flags);

	if (layer->cur_frm != NULL)
		vb2_buffer_done(&layer->cur_frm->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	while (!list_empty(&layer->dma_queue)) {
		buf = list_entry(layer->dma_queue.next, struct sp_disp_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	spin_unlock_irqrestore(&disp_dev->dma_queue_lock, flags);
}

static const struct vb2_ops sp_video_qops = {
	.queue_setup            = sp_queue_setup,
	.buf_prepare            = sp_buf_prepare,
	.buf_queue              = sp_buf_queue,
	.start_streaming        = sp_start_streaming,
	.stop_streaming         = sp_stop_streaming,
	.wait_prepare           = vb2_ops_wait_prepare,
	.wait_finish            = vb2_ops_wait_finish
};

static int sp_get_dev_id(struct sp_disp_fh *fh)
{
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;

	if (!(strcmp(DISP_OSD0_NAME, fh->fh.vdev->name)))
		dev_id = SP_DISP_DEVICE_0;
	else if (!(strcmp(DISP_OSD1_NAME, fh->fh.vdev->name)))
		dev_id = SP_DISP_DEVICE_1;
	else if (!(strcmp(DISP_OSD2_NAME, fh->fh.vdev->name)))
		dev_id = SP_DISP_DEVICE_2;
	else if (!(strcmp(DISP_OSD3_NAME, fh->fh.vdev->name)))
		dev_id = SP_DISP_DEVICE_3;
	else if (!(strcmp(DISP_VPP0_NAME, fh->fh.vdev->name)))
		dev_id = SP_DISP_DEVICE_4;
	else
		pr_err("unknown layer!\n");

	return dev_id;

}

static int sp_vidioc_querycap(struct file *file, void *priv,
			struct v4l2_capability *vcap)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	struct sp_disp_fh *fh = priv;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;

	dev_id = sp_get_dev_id(fh);

	if (disp_dev->dev[dev_id]) {
		strlcpy(vcap->driver, "SP Video Driver", sizeof(vcap->driver));
		strlcpy(vcap->card, "SP DISPLAY Card", sizeof(vcap->card));
		vcap->device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;
		vcap->capabilities = vcap->device_caps | V4L2_CAP_DEVICE_CAPS;
		if (!(strcmp(DISP_OSD0_NAME, disp_dev->dev[dev_id]->video_dev.name)))
			strlcpy(vcap->bus_info, "SP DISP Device BUS OSD0", sizeof(vcap->bus_info));
		else if (!(strcmp(DISP_OSD1_NAME, disp_dev->dev[dev_id]->video_dev.name)))
			strlcpy(vcap->bus_info, "SP DISP Device BUS OSD1", sizeof(vcap->bus_info));
		else if (!(strcmp(DISP_OSD2_NAME, disp_dev->dev[dev_id]->video_dev.name)))
			strlcpy(vcap->bus_info, "SP DISP Device BUS OSD2", sizeof(vcap->bus_info));
		else if (!(strcmp(DISP_OSD3_NAME, disp_dev->dev[dev_id]->video_dev.name)))
			strlcpy(vcap->bus_info, "SP DISP Device BUS OSD3", sizeof(vcap->bus_info));
		else if (!(strcmp(DISP_VPP0_NAME, disp_dev->dev[dev_id]->video_dev.name)))
			strlcpy(vcap->bus_info, "SP DISP Device BUS VPP0", sizeof(vcap->bus_info));
	}

	return 0;
}

static int sp_display_g_fmt(struct file *file, void *priv,
			struct v4l2_format *fmt)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	struct sp_disp_fh *fh = priv;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct v4l2_format *fmt1 = NULL;

	/* If buffer type is video output */
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[%s:%d] invalid type\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_id = sp_get_dev_id(fh);

	if (disp_dev->dev[dev_id]) {
		fmt1 = &disp_dev->dev[dev_id]->fmt;

		/* Fill in the information about format */
		fmt->fmt.pix = fmt1->fmt.pix;
	}

	return 0;
}

static struct sp_fmt osd_formats[] = {
	{
		.name     = "GREY", /* osd_format 0x2 = 8bpp (ARGB) , with grey paletteb */
		.fourcc   = V4L2_PIX_FMT_GREY,
		.width    = 720,
		.height   = 480,
		.depth    = 8,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "PAL8", /* osd_format 0x2 = 8bpp (ARGB) , with color palette */
		.fourcc   = V4L2_PIX_FMT_PAL8,
		.width    = 720,
		.height   = 480,
		.depth    = 8,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "YUYV", /* osd_format 0x4 = YUY2 */
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "RGBP", /* osd_format 0x8 = RGB565 */
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "AR15", /* osd_format 0x9 = ARGB1555 */
		.fourcc   = V4L2_PIX_FMT_ARGB555,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "R444", /* osd_format 0xa = RGBA4444 */
		.fourcc   = V4L2_PIX_FMT_RGB444,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "AR12", /* osd_format 0xb = ARGB4444 */
		.fourcc   = V4L2_PIX_FMT_ARGB444,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "AR24", /* osd_format 0xd = RGBA8888 */
		.fourcc   = V4L2_PIX_FMT_ABGR32,
		.width    = 720,
		.height   = 480,
		.depth    = 32,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "BA24", /* osd_format 0xe = ARGB8888 */
		.fourcc   = V4L2_PIX_FMT_ARGB32,
		.width    = 720,
		.height   = 480,
		.depth    = 32,
		.walign   = 1,
		.halign   = 1,
	},
};
static struct sp_fmt vpp_formats[] = {
	{
		.name     = "NV12", /* vpp_format = NV12 */
		.fourcc   = V4L2_PIX_FMT_NV12,
		.width    = 720,
		.height   = 480,
		.depth    = 12,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "NV16", /* vpp_format = NV16 */
		.fourcc   = V4L2_PIX_FMT_NV16,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "NV24", /* vpp_format = NV24 */
		.fourcc   = V4L2_PIX_FMT_NV24,
		.width    = 720,
		.height   = 480,
		.depth    = 24,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "YUYV", /* vpp_format = YUY2 */
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
	{
		.name     = "UYVY", /* vpp0_format = UYVY */
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.width    = 720,
		.height   = 480,
		.depth    = 16,
		.walign   = 1,
		.halign   = 1,
	},
};

static struct sp_vout_layer_info sp_vout_layer[] = {
	{
		.name = "osd0",
		.formats = osd_formats,
		.formats_size = ARRAY_SIZE(osd_formats),
	},
	{
		.name = "osd1",
		.formats = osd_formats,
		.formats_size = ARRAY_SIZE(osd_formats),
	},
	{
		.name = "osd2",
		.formats = osd_formats,
		.formats_size = ARRAY_SIZE(osd_formats),
	},
	{
		.name = "osd3",
		.formats = osd_formats,
		.formats_size = ARRAY_SIZE(osd_formats),
	},
	{
		.name = "vpp0",
		.formats = vpp_formats,
		.formats_size = ARRAY_SIZE(vpp_formats),
	},
};

static int sp_display_enum_fmt(struct file *file, void  *priv,
			struct v4l2_fmtdesc *fmtdesc)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	struct sp_disp_fh *fh = priv;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct sp_fmt *fmt;

	dev_id = sp_get_dev_id(fh);

	if (disp_dev->dev[dev_id]) {
		if (!(strcmp(DISP_OSD0_NAME, disp_dev->dev[dev_id]->video_dev.name))) {
			if (fmtdesc->index >= ARRAY_SIZE(osd_formats))
				return -EINVAL;

			fmt = &osd_formats[fmtdesc->index];
			strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
			fmtdesc->pixelformat = fmt->fourcc;
		} else if (!(strcmp(DISP_OSD1_NAME, disp_dev->dev[dev_id]->video_dev.name))) {
			if (fmtdesc->index >= ARRAY_SIZE(osd_formats))
				return -EINVAL;

			fmt = &osd_formats[fmtdesc->index];
			strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
			fmtdesc->pixelformat = fmt->fourcc;
		} else if (!(strcmp(DISP_OSD2_NAME, disp_dev->dev[dev_id]->video_dev.name))) {
			if (fmtdesc->index >= ARRAY_SIZE(osd_formats))
				return -EINVAL;

			fmt = &osd_formats[fmtdesc->index];
			strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
			fmtdesc->pixelformat = fmt->fourcc;
		} else if (!(strcmp(DISP_OSD3_NAME, disp_dev->dev[dev_id]->video_dev.name))) {
			if (fmtdesc->index >= ARRAY_SIZE(osd_formats))
				return -EINVAL;

			fmt = &osd_formats[fmtdesc->index];
			strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
			fmtdesc->pixelformat = fmt->fourcc;
		} else if (!(strcmp(DISP_VPP0_NAME, disp_dev->dev[dev_id]->video_dev.name))) {
			if (fmtdesc->index >= ARRAY_SIZE(vpp_formats))
				return -EINVAL;

			fmt = &vpp_formats[fmtdesc->index];
			strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
			fmtdesc->pixelformat = fmt->fourcc;
		} else
			pr_info("[%s:%d] unknown layer !!\n", __func__, __LINE__);
	}

	return 0;
}

static int sp_try_format(struct sp_disp_device *disp_dev,
			struct v4l2_pix_format *pixfmt, int layer_id)
{
	struct sp_vout_layer_info *pixfmt0 = &sp_vout_layer[layer_id];
	struct sp_fmt *pixfmt1 = NULL;
	int i, match = 0;

	if (pixfmt0->formats_size <= 0) {
		pr_err("[%s:%d] invalid formats\n", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < (pixfmt0->formats_size) ; i++) {
		if (layer_id <= SP_DISP_DEVICE_3)
			pixfmt1 = &osd_formats[i];
		else if (layer_id == SP_DISP_DEVICE_4)
			pixfmt1 = &vpp_formats[i];

		if (pixfmt->pixelformat == pixfmt1->fourcc) {
			match = 1;
			break;
		}
	}
	if (!match) {
		return -EINVAL;
	}

	return 0;
}

static int sp_display_try_fmt(struct file *file, void *priv,
			struct v4l2_format *fmt)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	struct sp_disp_fh *fh = priv;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct v4l2_pix_format *pixfmt = &fmt->fmt.pix;
	int ret;

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[%s:%d] Invalid V4L2 buffer type!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_id = sp_get_dev_id(fh);

	/* Check for valid pixel format */
	ret = sp_try_format(disp_dev, pixfmt, dev_id);
	if (ret)
		return ret;

	return 0;
}

static int sp_display_s_fmt(struct file *file, void *priv,
			struct v4l2_format *fmt)
{
	struct sp_disp_device *disp_dev = video_drvdata(file);
	struct sp_disp_fh *fh = priv;
	enum sp_disp_device_id dev_id = SP_DISP_DEVICE_0;
	struct v4l2_pix_format *pixfmt = &fmt->fmt.pix;
	int ret;

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[%s:%d] invalid type\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_id = sp_get_dev_id(fh);

	/* Check for valid pixel format */
	ret = sp_try_format(disp_dev, pixfmt, dev_id);
	if (ret)
		return ret;

	if (disp_dev->dev[dev_id]) {
		disp_dev->dev[dev_id]->fmt.type                 = fmt->type;
		disp_dev->dev[dev_id]->fmt.fmt.pix.width        = pixfmt->width;
		disp_dev->dev[dev_id]->fmt.fmt.pix.height       = pixfmt->height;
		disp_dev->dev[dev_id]->fmt.fmt.pix.pixelformat  = pixfmt->pixelformat; // from sp_try_format
		disp_dev->dev[dev_id]->fmt.fmt.pix.field        = pixfmt->field;
		disp_dev->dev[dev_id]->fmt.fmt.pix.bytesperline = pixfmt->bytesperline;
		//disp_dev->dev[dev_id]->fmt.fmt.pix.bytesperline = (pixfmt->width)*4;
		disp_dev->dev[dev_id]->fmt.fmt.pix.sizeimage    = pixfmt->sizeimage;
		//disp_dev->dev[dev_id]->fmt.fmt.pix.sizeimage    = (pixfmt->width)*(pixfmt->height)*4;
		disp_dev->dev[dev_id]->fmt.fmt.pix.colorspace   = pixfmt->colorspace;
	}

	return 0;
}

static const struct v4l2_ioctl_ops sp_disp_ioctl_ops = {
	.vidioc_querycap		= sp_vidioc_querycap,
	.vidioc_g_fmt_vid_out		= sp_display_g_fmt,
	.vidioc_enum_fmt_vid_out	= sp_display_enum_fmt,
	.vidioc_s_fmt_vid_out		= sp_display_s_fmt,
	.vidioc_try_fmt_vid_out		= sp_display_try_fmt,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

int sp7350_v4l2_initialize(struct device *dev,
		struct sp_disp_device *disp_dev)
{
	int ret;

	if (!disp_dev || !dev) {
		pr_err("Null device pointers.\n");
		return -ENODEV;
	}

	// Register V4L2 device.
	ret = v4l2_device_register(dev, &disp_dev->v4l2_dev);
	if (ret) {
		pr_err("Unable to register v4l2 device!\n");
		goto err_v4l2_register;
	}

	// Initialize locks.
	spin_lock_init(&disp_dev->dma_queue_lock);
	mutex_init(&disp_dev->lock);

	return 0;

err_v4l2_register:
	v4l2_device_unregister(&disp_dev->v4l2_dev);
	kfree(disp_dev);

	return ret;
}
EXPORT_SYMBOL(sp7350_v4l2_initialize);

int sp7350_v4l2_init_multi_layer(int i, struct platform_device *pdev,
		struct sp_disp_device *disp_dev)
{
	struct sp_disp_layer *disp_layer = NULL;
	struct video_device *vbd = NULL;

	/* Allocate memory for four plane display objects */

	disp_dev->dev[i] =
		kzalloc(sizeof(struct sp_disp_layer), GFP_KERNEL);

	/* If memory allocation fails, return error */
	if (!disp_dev->dev[i]) {
		pr_err("ran out of memory\n");
		return  -ENOMEM;
	}
	spin_lock_init(&disp_dev->dev[i]->irqlock);
	mutex_init(&disp_dev->dev[i]->opslock);

	/* Get the pointer to the layer object */
	disp_layer = disp_dev->dev[i];
	vbd = &disp_layer->video_dev;
	/* Initialize field of video device */
	vbd->release	= video_device_release_empty;
	vbd->fops	= &sp_disp_fops;
	vbd->ioctl_ops	= &sp_disp_ioctl_ops;
	vbd->minor	= -1;
	vbd->v4l2_dev   = &disp_dev->v4l2_dev;
	vbd->lock	= &disp_layer->opslock;
	vbd->vfl_dir	= VFL_DIR_TX;
	vbd->device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;

	if (i == 0)
		strlcpy(vbd->name, DISP_OSD0_NAME, sizeof(vbd->name));
	else if (i == 1)
		strlcpy(vbd->name, DISP_OSD1_NAME, sizeof(vbd->name));
	else if (i == 2)
		strlcpy(vbd->name, DISP_OSD2_NAME, sizeof(vbd->name));
	else if (i == 3)
		strlcpy(vbd->name, DISP_OSD3_NAME, sizeof(vbd->name));
	else if (i == 4)
		strlcpy(vbd->name, DISP_VPP0_NAME, sizeof(vbd->name));

	disp_layer->device_id = i;

	return 0;
}
EXPORT_SYMBOL(sp7350_v4l2_init_multi_layer);

int sp7350_v4l2_reg_multi_layer(int i, struct platform_device *pdev,
		struct sp_disp_device *disp_dev)
{
	struct vb2_queue *q;
	int ret;
	int vid_num = 10;

	/* Allocate memory for four plane display objects */

	/* initialize vb2 queue */
	q = &disp_dev->dev[i]->buffer_queue;
	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = disp_dev->dev[i];
	q->ops = &sp_video_qops;
	q->mem_ops = sp_mem_ops[allocator];
	q->buf_struct_size = sizeof(struct sp_disp_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = MIN_BUFFERS;
	q->lock = &disp_dev->dev[i]->opslock;
	q->dev = disp_dev->pdev;
	ret = vb2_queue_init(q);
	if (ret) {
		pr_err("vb2_queue_init() failed!\n");
		goto err_vb2_queue_init;
	}

	INIT_LIST_HEAD(&disp_dev->dev[i]->dma_queue);

	sp_print_list(&disp_dev->dev[i]->dma_queue);

	disp_dev->dev[i]->video_dev.queue = &disp_dev->dev[i]->buffer_queue;

	// Register video device.
	ret = video_register_device(&disp_dev->dev[i]->video_dev, VFL_TYPE_VIDEO, (vid_num + i));
	if (ret) {
		pr_err("Unable to register video device!\n");
		ret = -ENODEV;
		goto err_video_register;
	}

	//vb2_dma_contig_set_max_seg_size(disp_dev->pdev, DMA_BIT_MASK(32));

	disp_dev->dev[i]->disp_dev = disp_dev;
	/* set driver private data */
	video_set_drvdata(&disp_dev->dev[i]->video_dev, disp_dev);

	return 0;

err_vb2_queue_init:
err_video_register:
	/* Unregister video device */
	video_unregister_device(&disp_dev->dev[i]->video_dev);
	kfree(disp_dev->dev[i]);
	disp_dev->dev[i] = NULL;

	v4l2_device_unregister(&disp_dev->v4l2_dev);
	kfree(disp_dev);

	return ret;
}
EXPORT_SYMBOL(sp7350_v4l2_reg_multi_layer);
