/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <mach/io_map.h>
#include <media/videobuf-dma-contig.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>
#include <media/v4l2-common.h>
#include <linux/i2c.h>
#include "sp-mipi.h"


/* ------------------------------------------------------------------
	Constants
   ------------------------------------------------------------------*/
static const struct sp_fmt ov9281_raw10_formats[] = {
	{
		.name     = "GREY, RAW10",
		.fourcc   = V4L2_PIX_FMT_GREY,
		.width    = 1280,
		.height   = 800,
		.depth    = 8,
	},
};

static const struct sp_fmt gc0310_bayer_raw8_formats[] = {
	{
		.name     = "BAYER, RAW8",
		.fourcc   = V4L2_PIX_FMT_SRGGB8,
		.width    = 640,
		.height   = 480,
		.depth    = 8,
	},
};

static const struct sp_fmt gc0310_yuy2_formats[] = {
	{
		.name     = "YUYV/YUY2, YUV422",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.width    = 640,
		.height   = 480,
		.depth    = 16,
	},
};

static const struct sp_sensor_format ov9281_sensor_raw10 = {
	.mipi_lane = 2,
	.sol_sync = SYNC_RAW10,
	.formats = ov9281_raw10_formats,
	.formats_size = ARRAY_SIZE(ov9281_raw10_formats),
};

static const struct sp_sensor_format gc0310_sensor_raw8 = {
	.mipi_lane = 1,
	.sol_sync = SYNC_RAW8,
	.formats = gc0310_bayer_raw8_formats,
	.formats_size = ARRAY_SIZE(gc0310_bayer_raw8_formats),
};

static const struct sp_sensor_format gc0310_sensor_yuy2 = {
	.mipi_lane = 1,
	.sol_sync = SYNC_YUY2,
	.formats = gc0310_yuy2_formats,
	.formats_size = ARRAY_SIZE(gc0310_yuy2_formats),
};

static struct sp_vout_subdev_info sp_vout_sub_devs[] = {
	{
		.name = "ov9281",
		.grp_id = 0,
		.board_info = {
			I2C_BOARD_INFO("ov9281", 0x60),
		},
		.sensor_fmt = {&ov9281_sensor_raw10},
	},
	{
		.name = "gc0310",
		.grp_id = 0,
		.board_info = {
			I2C_BOARD_INFO("gc0310", 0x21),
		},
		.sensor_fmt = {&gc0310_sensor_raw8, &gc0310_sensor_yuy2},
	}
};

static const struct sp_vout_config psp_vout_cfg = {
	.i2c_adapter_id = 1,
	.sub_devs       = sp_vout_sub_devs,
	.num_subdevs    = ARRAY_SIZE(sp_vout_sub_devs),
};


/* ------------------------------------------------------------------
	SP7021 function
   ------------------------------------------------------------------*/
static const struct sp_fmt *get_format(const struct sp_sensor_format *cur_sensor_fmt, struct v4l2_format *f)
{
	const struct sp_fmt *formats = cur_sensor_fmt->formats;
	int size = cur_sensor_fmt->formats_size;
	unsigned int k;

	for (k = 0; k < size; k++) {
		if (formats[k].fourcc == f->fmt.pix.pixelformat) {
			break;
		}
	}

	if (k == size) {
		return NULL;
	}

	return &formats[k];
}

static int sp_mipi_get_register_base(struct platform_device *pdev, void **membase, const char *res_name)
{
	struct resource *r;
	void __iomem *p;

	DBG_INFO("Resource name: %s\n", res_name);

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if (r == NULL) {
		MIP_ERR("platform_get_resource_byname failed!\n");
		return -ENODEV;
	}

	p = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(p)) {
		MIP_ERR("devm_ioremap_resource failed!\n");
		return PTR_ERR(p);
	}

	DBG_INFO("ioremap address = 0x%08x\n", (unsigned int)p);
	*membase = p;

	return 0;
}

static void mipicsi_init(struct sp_vout_device *vout)
{
	u32 val, val2;

	val = 0x8104;   // Normal mode, MSB first, Auto

	switch (vout->cur_sensor_fmt->mipi_lane) {
	default:
	case 1: // 1 lane
		val |= 0;
		val2 = 0x11;
		break;
	case 2: // 2 lanes
		val |= (1<<20);
		val2 = 0x13;
		break;
	case 4: // 4 lanes
		val |= (2<<20);
		val2 = 0x1f;
		break;
	}

	switch (vout->cur_sensor_fmt->sol_sync) {
	default:
	case SYNC_RAW8:	        // 8 bits
	case SYNC_YUY2:
		val |= (2<<16); break;
	case SYNC_RAW10:        // 10 bits
		val |= (1<<16); break;
	}

	writel(val, &vout->mipicsi_regs->mipicsi_mix_cfg);
	writel(vout->cur_sensor_fmt->sol_sync, &vout->mipicsi_regs->mipicsi_sof_sol_syncword);
	writel(val2, &vout->mipicsi_regs->mipi_analog_cfg2);
	writel(0x110, &vout->mipicsi_regs->mipicsi_ecc_cfg);
	writel(0x1000, &vout->mipicsi_regs->mipi_analog_cfg1);
	writel(0x1001, &vout->mipicsi_regs->mipi_analog_cfg1);
	writel(0x1000, &vout->mipicsi_regs->mipi_analog_cfg1);
	writel(0x1, &vout->mipicsi_regs->mipicsi_enable);               // Enable MIPICSI
}

static void csiiw_init(struct sp_vout_device *vout)
{
	writel(0x1, &vout->csiiw_regs->csiiw_latch_mode);               // latch mode should be enable before base address setup

	writel(0x500, &vout->csiiw_regs->csiiw_stride);
	writel(0x3200500, &vout->csiiw_regs->csiiw_frame_size);

	// buffer_number (1:0x00000100, 2:0x011f4000, 3:0x021f4000)
	writel(0x00000100, &vout->csiiw_regs->csiiw_frame_buf);         // set offset to trigger DRAM write

	//raw8 (0x2701); raw10 (10bit two byte space:0x2731, 8bit one byte space:0x2701)
	writel(0x32700, &vout->csiiw_regs->csiiw_config0);              // Disable csiiw, fs_irq and fe_irq
}

static void sp_vout_schedule_next_buffer(struct sp_vout_device *vout)
{
	vout->next_frm = list_entry(vout->dma_queue.next, struct videobuf_buffer, queue);
	list_del(&vout->next_frm->queue);

	vout->next_frm->state = VIDEOBUF_ACTIVE;
	vout->baddr = videobuf_to_dma_contig(vout->next_frm);
	writel(vout->baddr, &vout->csiiw_regs->csiiw_base_addr);        // base address

	DBG_INFO("%s: baddr = %08x\n", __FUNCTION__, vout->baddr);
}

static void sp_vout_process_buffer_complete(struct sp_vout_device *vout)
{
	v4l2_get_timestamp(&vout->cur_frm->ts);
	vout->cur_frm->state = VIDEOBUF_DONE;
	vout->cur_frm->size = vout->fmt.fmt.pix.sizeimage;
	wake_up_interruptible(&vout->cur_frm->done);
	vout->cur_frm = vout->next_frm;
}

irqreturn_t csiiw_fs_isr(int irq, void *dev_instance)
{
	struct sp_vout_device *vout = dev_instance;

	if (vout->started) {

	}

	return IRQ_HANDLED;
}

irqreturn_t csiiw_fe_isr(int irq, void *dev_instance)
{
	struct sp_vout_device *vout = dev_instance;

	if (vout->started) {
		spin_lock(&vout->dma_queue_lock);

		if (!list_empty(&vout->dma_queue) && (vout->cur_frm == vout->next_frm))
			sp_vout_schedule_next_buffer(vout);

		if (vout->cur_frm != vout->next_frm)
			sp_vout_process_buffer_complete(vout);

		spin_unlock(&vout->dma_queue_lock);
	}

	return IRQ_HANDLED;
}

static int csiiw_irq_init(struct sp_vout_device *vout)
{
	int ret;

	vout->fs_irq = irq_of_parse_and_map(vout->pdev->of_node, 0);
	ret = devm_request_irq(vout->pdev, vout->fs_irq, csiiw_fs_isr, 0, "csiiw_fs", vout);
	if (ret) {
		goto err_fs_irq;
	}

	vout->fe_irq = irq_of_parse_and_map(vout->pdev->of_node, 1);
	ret = devm_request_irq(vout->pdev, vout->fe_irq, csiiw_fe_isr, 0, "csiiw_fe", vout);
	if (ret) {
		goto err_fe_irq;
	}

	MIP_INFO("Installed csiiw interrupts (fs_irq=%d, fe_irq=%d).\n", vout->fs_irq , vout->fe_irq);
	return 0;

err_fe_irq:
err_fs_irq:
	MIP_ERR("request_irq failed (%d)!\n", ret);
	return ret;
}

static int buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct sp_vout_fh *fh = vq->priv_data;
	struct sp_vout_device *vout = fh->vout;

	*size = vout->fmt.fmt.pix.sizeimage;

	if (0 == (*count)) {
		*count = 32;
	}

	while (((*size)*(*count)) > (vid_limit * 1024 * 1024))
		(*count)--;

	DBG_INFO("%s: count = %d, size = %d\n", __FUNCTION__, *count, *size);

	return 0;
}

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb, enum v4l2_field field)
{
	struct sp_vout_fh *fh = vq->priv_data;
	struct sp_vout_device *vout = fh->vout;
	unsigned long addr;
	int ret;

	/* If buffer is not initialized, initialize it */
	if (VIDEOBUF_NEEDS_INIT == vb->state) {
		vb->width  = vout->fmt.fmt.pix.width;
		vb->height = vout->fmt.fmt.pix.height;
		vb->size   = vout->fmt.fmt.pix.sizeimage;
		vb->field  = field;

		ret = videobuf_iolock(vq, vb, NULL);
		if (ret < 0) {
			return ret;
		}

		addr = videobuf_to_dma_contig(vb);

		/* Make sure user addresses are aligned to 32 bytes */
		if (!ALIGN(addr, FRAME_BUFFER_ALIGN)) {
			MIP_ERR("Physical base address of frame buffer should be %d-byte align!\n", FRAME_BUFFER_ALIGN);
			return -EINVAL;
		}

		vb->state = VIDEOBUF_PREPARED;
	}
	DBG_INFO("%s (video_buffer): width = %d, height = %d, size = %lx, field = %d, state = %d\n",
		 __FUNCTION__, vb->width, vb->height, vb->size, vb->field, vb->state);

	return 0;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	/* Get the file handle object and device object */
	struct sp_vout_fh *fh = vq->priv_data;
	struct sp_vout_device *vout = fh->vout;
	unsigned long flags;

	/* add the buffer to the DMA queue */
	spin_lock_irqsave(&vout->dma_queue_lock, flags);
	list_add_tail(&vb->queue, &vout->dma_queue);
	DBG_INFO("%s: list_add\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	spin_unlock_irqrestore(&vout->dma_queue_lock, flags);

	/* Change state of the buffer */
	vb->state = VIDEOBUF_QUEUED;
	DBG_INFO("%s (video_buffer): state = %d\n", __FUNCTION__, vb->state);
}

static void buffer_release(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct sp_vout_fh *fh = vq->priv_data;
	struct sp_vout_device *vout = fh->vout;
	unsigned long flags;

	/*
	 * We need to flush the buffer from the dma queue since
	 * they are de-allocated
	 */
	spin_lock_irqsave(&vout->dma_queue_lock, flags);
	INIT_LIST_HEAD(&vout->dma_queue);
	DBG_INFO("%s: init_list\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	spin_unlock_irqrestore(&vout->dma_queue_lock, flags);

	videobuf_dma_contig_free(vq, vb);
	vb->state = VIDEOBUF_NEEDS_INIT;
	DBG_INFO("%s (video_buffer): state = %d\n", __FUNCTION__, vb->state);
}

static struct videobuf_queue_ops sp_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

//===================================================================================
/* ------------------------------------------------------------------
	V4L2 ioctrl operations
   ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *vcap)
{
	DBG_INFO("%s\n", __FUNCTION__);

	strlcpy(vcap->driver, "SP Video Driver", sizeof(vcap->driver));
	strlcpy(vcap->card, "SP MIPI Camera Card", sizeof(vcap->card));
	strlcpy(vcap->bus_info, "SP MIPI Camera BUS", sizeof(vcap->bus_info));

	// report capabilities
	vcap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	vcap->capabilities = vcap->device_caps| V4L2_CAP_DEVICE_CAPS ;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv, struct v4l2_fmtdesc *fmtdesc)
{
	struct sp_vout_device *vout = video_drvdata(file);
	const struct sp_fmt *fmt;

	DBG_INFO("%s: index = %d\n", __FUNCTION__, fmtdesc->index);

	if (fmtdesc->index >= vout->cur_sensor_fmt->formats_size) {
		return -EINVAL;
	}

	fmt = &vout->cur_sensor_fmt->formats[fmtdesc->index];
	strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
	fmtdesc->pixelformat = fmt->fourcc;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	struct sp_vout_device *vout = video_drvdata(file);
	const struct sp_fmt *fmt;
	enum v4l2_field field;

	DBG_INFO("%s\n", __FUNCTION__);

	fmt = get_format(vout->cur_sensor_fmt, f);

	field = f->fmt.pix.field;
	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	}
	f->fmt.pix.field = field;

	v4l_bound_align_image(&f->fmt.pix.width, 48, fmt->width, 2,
			      &f->fmt.pix.height, 32, fmt->height, 2, 0);

	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage    = f->fmt.pix.height * f->fmt.pix.bytesperline;

	if ((fmt->fourcc == V4L2_PIX_FMT_YUYV) || (fmt->fourcc == V4L2_PIX_FMT_UYVY)) {
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	} else {
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	}

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	struct sp_vout_device *vout = video_drvdata(file);
	int ret;

	DBG_INFO("%s\n", __FUNCTION__);

	ret = vidioc_try_fmt_vid_cap(file, vout, f);

	vout->fmt.type                 = f->type;
	vout->fmt.fmt.pix.width        = f->fmt.pix.width;
	vout->fmt.fmt.pix.height       = f->fmt.pix.height;
	vout->fmt.fmt.pix.pixelformat  = f->fmt.pix.pixelformat; // from vidioc_try_fmt_vid_cap
	vout->fmt.fmt.pix.field        = f->fmt.pix.field;
	vout->fmt.fmt.pix.bytesperline = f->fmt.pix.bytesperline;
	vout->fmt.fmt.pix.sizeimage    = f->fmt.pix.sizeimage ;
	vout->fmt.fmt.pix.colorspace   = f->fmt.pix.colorspace;

	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO("%s\n", __FUNCTION__);

	memcpy(f, &vout->fmt, sizeof(*f));

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *req_buf)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	int ret;

	DBG_INFO("%s\n", __FUNCTION__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != req_buf->type) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	/* Lock for entering critical section */
	ret = mutex_lock_interruptible(&vout->lock);
	if (ret) {
		return ret;
	}

	vout->memory = req_buf->memory;
	videobuf_queue_dma_contig_init(&vout->buffer_queue,
				&sp_video_qops,
				vout->pdev,
				&vout->irqlock,
				req_buf->type,
				vout->fmt.fmt.pix.field,
				sizeof(struct videobuf_buffer),
				fh, NULL);

	fh->io_allowed = 1;
	INIT_LIST_HEAD(&vout->dma_queue);
	DBG_INFO("%s: init_list\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	ret = videobuf_reqbufs(&vout->buffer_queue, req_buf);

	/* Unlock after leaving critical section */
	mutex_unlock(&vout->lock);

	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO("%s\n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	if (vout->memory != V4L2_MEMORY_MMAP) {
		MIP_ERR("Invalid V4L2 memory type!\n");
		return -EINVAL;
	}

	/* Call videobuf_querybuf to get information */
	return videobuf_querybuf(&vout->buffer_queue, buf);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;

	DBG_INFO("%s\n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	/*
	 * If this file handle is not allowed to do IO, return error.
	 */
	if (!fh->io_allowed) {
		MIP_ERR("IO is not allowed!\n");
		return -EACCES;
	}

	return videobuf_qbuf(&vout->buffer_queue, buf);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO("%s\n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	return videobuf_dqbuf(&vout->buffer_queue, buf, file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type buf_type)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	struct sp_vout_subdev_info *sdinfo;
	int ret;

	DBG_INFO("%s\n", __FUNCTION__);

	if (buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	/* If file handle is not allowed IO, return error. */
	if (!fh->io_allowed) {
		MIP_ERR("IO is not allowed!\n");
		return -EACCES;
	}

	sdinfo = vout->current_subdev;
	ret = v4l2_device_call_until_err(&vout->v4l2_dev, sdinfo->grp_id,
					video, s_stream, 1);
	if (ret && (ret != -ENOIOCTLCMD)) {
		MIP_ERR("streamon failed in subdevice!\n");
		return -EINVAL;
	}

	/* If buffer queue is empty, return error. */
	if (list_empty(&vout->buffer_queue.stream)) {
		MIP_ERR("Buffer queue is empty!\n");
		return -EIO;
	}

	//writel(0x02700, &vout->csiiw_regs->csiiw_config0);      // Disable csiiw, but enable fs_irq & fe_irq

	/* Call videobuf_streamon to start streaming in videobuf */
	ret = videobuf_streamon(&vout->buffer_queue);
	if (ret) {
		return ret;
	}

	/* Lock for entering critical section */
	ret = mutex_lock_interruptible(&vout->lock);
	if (ret) {
		goto streamoff;
	}

	/* Get the next frame from the buffer queue */
	vout->next_frm = list_entry(vout->dma_queue.next, struct videobuf_buffer, queue);
	vout->cur_frm = vout->next_frm;

	/* Remove buffer from the buffer queue */
	list_del(&vout->cur_frm->queue);
	DBG_INFO("%s: list_del\n", __FUNCTION__);
	print_List(&vout->dma_queue);

	/* Mark state of the current frame to active */
	vout->cur_frm->state = VIDEOBUF_ACTIVE;
	vout->baddr = videobuf_to_dma_contig(vout->cur_frm);

	writel(vout->baddr, &vout->csiiw_regs->csiiw_base_addr);
	writel(mEXTENDED_ALIGNED(vout->fmt.fmt.pix.bytesperline, 16), &vout->csiiw_regs->csiiw_stride);
	writel((vout->fmt.fmt.pix.height<<16)|vout->fmt.fmt.pix.bytesperline, &vout->csiiw_regs->csiiw_frame_size);

	writel(0x02701, &vout->csiiw_regs->csiiw_config0);      // Enable csiiw, fs_irq and fe_irq

	DBG_INFO("%s: cur_frm = %p, next_frm = %p, baddr = %x\n",
		__FUNCTION__, vout->cur_frm, vout->next_frm, vout->baddr);

	vout->started = 1;

	/* Unlock after leaving critical section */
	mutex_unlock(&vout->lock);
	return ret;

streamoff:
	ret = videobuf_streamoff(&vout->buffer_queue);
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type buf_type)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	struct sp_vout_subdev_info *sdinfo;
	int ret;

	DBG_INFO("%s\n", __FUNCTION__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != buf_type) {
		MIP_ERR("Invalid V4L2 buffer type!\n");
		return -EINVAL;
	}

	/* If io is allowed for this file handle, return error */
	if (!fh->io_allowed) {
		MIP_ERR("IO is not allowed!\n");
		return -EACCES;
	}

	/* If streaming is not started, return error */
	if (!vout->started) {
		MIP_ERR("Device has started already!\n");
		return -EINVAL;
	}

	/* Lock for entering critical section */
	ret = mutex_lock_interruptible(&vout->lock);
	if (ret) {
		return ret;
	}

	sdinfo = vout->current_subdev;
	ret = v4l2_device_call_until_err(&vout->v4l2_dev, sdinfo->grp_id,
					video, s_stream, 0);
	if (ret && (ret != -ENOIOCTLCMD)) {
		MIP_ERR("streamon failed in subdevice!\n");
		return -EINVAL;
	}

	vout->started = 0;

	// FW must mask irq to avoid unmap issue (for test code)
	writel(0x32700, &vout->csiiw_regs->csiiw_config0);      // Disable csiiw, fs_irq and fe_irq

	ret = videobuf_streamoff(&vout->buffer_queue);

	/* Unlock after leaving critical section */
	mutex_unlock(&vout->lock);
	return ret;
}

static const struct v4l2_ioctl_ops sp_mipi_ioctl_ops = {
	.vidioc_querycap                = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap        = vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap         = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap           = vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap           = vidioc_g_fmt_vid_cap,
	.vidioc_reqbufs                 = vidioc_reqbufs,
	.vidioc_querybuf                = vidioc_querybuf,
	.vidioc_qbuf                    = vidioc_qbuf,
	.vidioc_dqbuf                   = vidioc_dqbuf,
	.vidioc_streamon                = vidioc_streamon,
	.vidioc_streamoff               = vidioc_streamoff,
};

//===================================================================================
/* ------------------------------------------------------------------
	V4L2 file operations
   ------------------------------------------------------------------*/
static int sp_vout_open(struct file *file)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct video_device *vdev = video_devdata(file);
	struct sp_vout_fh *fh;

	DBG_INFO("%s\n", __FUNCTION__);

	/* Allocate memory for the file handle object */
	fh = kmalloc(sizeof(struct sp_vout_fh), GFP_KERNEL);
	if (!fh) {
		MIP_ERR("Failed to allocate memory for file handle object!\n");
		return -ENOMEM;
	}

	/* store pointer to fh in private_data member of file */
	file->private_data = fh;
	fh->vout = vout;
	v4l2_fh_init(&fh->fh, vdev);

	/* Get the device lock */
	mutex_lock(&vout->lock);

	/* Set io_allowed member to false */
	fh->io_allowed = 0;
	v4l2_fh_add(&fh->fh);

	/* Get the device unlock */
	mutex_unlock(&vout->lock);
	return 0;
}

static int sp_vout_release(struct file *file)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	struct sp_vout_subdev_info *sdinfo;
	int ret;

	DBG_INFO("%s\n", __FUNCTION__);

	/* Get the device lock */
	mutex_lock(&vout->lock);

	/* if this instance is doing IO */
	if (fh->io_allowed) {
		if (vout->started) {
			sdinfo = vout->current_subdev;
			ret = v4l2_device_call_until_err(&vout->v4l2_dev, sdinfo->grp_id,
							 video, s_stream, 0);
			if (ret && (ret != -ENOIOCTLCMD)) {
				MIP_ERR("streamon failed in subdevice!\n");
				return -EINVAL;
			}

			vout->started = 0;
			writel(0x32700, &vout->csiiw_regs->csiiw_config0);      // Disable csiiw, fs_irq and fe_irq

			videobuf_streamoff(&vout->buffer_queue);
		}
		videobuf_stop(&vout->buffer_queue);
		videobuf_mmap_free(&vout->buffer_queue);
	}

	/* Decrement device usrs counter */
	v4l2_fh_del(&fh->fh);
	v4l2_fh_exit(&fh->fh);

	/* Get the device unlock */
	mutex_unlock(&vout->lock);

	file->private_data = NULL;

	/* Free memory allocated to file handle object */
	kfree(fh);
	return 0;
}

static int sp_vout_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO("%s\n", __FUNCTION__);

	return videobuf_mmap_mapper(&vout->buffer_queue, vma);
}

static unsigned int sp_vout_poll(struct file *file, poll_table *wait)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO("%s\n", __FUNCTION__);

	if (vout->started)
		return videobuf_poll_stream(file, &vout->buffer_queue, wait);

	return 0;
}

static const struct v4l2_file_operations sp_mipi_fops = {
	.owner          = THIS_MODULE,
	.open           = sp_vout_open,
	.release        = sp_vout_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = sp_vout_mmap,
	.poll           = sp_vout_poll,
};

//===================================================================================
/* ------------------------------------------------------------------
	SP-MIPI driver probe
   ------------------------------------------------------------------*/
static int sp_mipi_probe(struct platform_device *pdev)
{
	struct sp_vout_device *vout;
	struct video_device *vfd;
	struct device *dev = &pdev->dev;
	struct sp_vout_config *sp_vout_cfg;
	struct sp_vout_subdev_info *sdinfo;
	struct i2c_adapter *i2c_adap;
	struct v4l2_subdev *subdev;
	struct sp_subdev_sensor_data *sensor_data;
	const struct sp_fmt *cur_fmt;
	int num_subdevs = 0;
	int ret, i;

	DBG_INFO("%s\n", __FUNCTION__);

	// Allocate memory for 'sp_vout_device'.
	vout = kzalloc(sizeof(struct sp_vout_device), GFP_KERNEL);
	if (!vout) {
		MIP_ERR("Failed to allocate memory for \'sp_vout_dev\'!\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	vout->pdev = &pdev->dev;

	/* set the driver data in platform device */
	platform_set_drvdata(pdev, vout);

	/* set driver private data */
	video_set_drvdata(&vout->video_dev, vout);

	// Get and set 'mipicsi' register base.
	ret = sp_mipi_get_register_base(pdev, (void**)&vout->mipicsi_regs, MIPICSI_REG_NAME);
	if (ret) {
		return ret;
	}

	// Get and set 'csiiw' register base.
	ret = sp_mipi_get_register_base(pdev, (void**)&vout->csiiw_regs, CSIIW_REG_NAME);
	if (ret) {
		return ret;
	}
	MIP_INFO("%s: mipicsi_regs = 0x%p, csiiw_regs = 0x%p\n",
		__FUNCTION__, vout->mipicsi_regs, vout->csiiw_regs);

	// Get clock resource 'clk_mipicsi'.
	vout->mipicsi_clk = devm_clk_get(dev, "clk_mipicsi");
	if (IS_ERR(vout->mipicsi_clk)) {
		ret = PTR_ERR(vout->mipicsi_clk);
		MIP_ERR("Failed to retrieve clock resource \'clk_mipicsi\'!\n");
		goto err_get_mipicsi_clk;
	}

	// Get clock resource 'clk_csiiw'.
	vout->csiiw_clk = devm_clk_get(dev, "clk_csiiw");
	if (IS_ERR(vout->csiiw_clk)) {
		ret = PTR_ERR(vout->csiiw_clk);
		MIP_ERR("Failed to retrieve clock resource \'clk_csiiw\'!\n");
		goto err_get_csiiw_clk;
	}

	// Get reset controller resource 'rstc_mipicsi'.
	vout->mipicsi_rstc = devm_reset_control_get(&pdev->dev, "rstc_mipicsi");
	if (IS_ERR(vout->mipicsi_rstc)) {
		ret = PTR_ERR(vout->mipicsi_rstc);
		dev_err(&pdev->dev, "Failed to retrieve reset controller 'rstc_mipicsi\'!\n");
		goto err_get_mipicsi_rstc;
	}

	// Get reset controller resource 'rstc_csiiw'.
	vout->csiiw_rstc = devm_reset_control_get(&pdev->dev, "rstc_csiiw");
	if (IS_ERR(vout->csiiw_rstc)) {
		ret = PTR_ERR(vout->csiiw_rstc);
		dev_err(&pdev->dev, "Failed to retrieve reset controller 'rstc_csiiw\'!\n");
		goto err_get_csiiw_rstc;
	}

	// Get i2c id.
	ret = of_property_read_u32(pdev->dev.of_node, "i2c-id", &vout->i2c_id);
	if (ret) {
		MIP_ERR("Failed to retrieve \'i2c-id\'!\n");
		goto err_get_i2c_id;
	}

	// Enable 'mipicsi' clock.
	ret = clk_prepare_enable(vout->mipicsi_clk);
	if (ret) {
		MIP_ERR("Failed to enable \'mipicsi\' clock!\n");
		goto err_en_mipicsi_clk;
	}

	// Enable 'csiiw' clock.
	ret = clk_prepare_enable(vout->csiiw_clk);
	if (ret) {
		MIP_ERR("Failed to enable \'csiiw\' clock!\n");
		goto err_en_csiiw_clk;
	}

	// De-assert 'mipicsi' reset controller.
	ret = reset_control_deassert(vout->mipicsi_rstc);
	if (ret) {
		MIP_ERR("Failed to deassert 'mipicsi' reset controller!\n");
		goto err_deassert_mipicsi_rstc;
	}

	// De-assert 'csiiw' reset controller.
	ret = reset_control_deassert(vout->csiiw_rstc);
	if (ret) {
		MIP_ERR("Failed to deassert 'csiiw' reset controller!\n");
		goto err_deassert_csiiw_rstc;
	}

	// Register V4L2 device.
	ret = v4l2_device_register(&pdev->dev, &vout->v4l2_dev);
	if (ret) {
		MIP_ERR("Unable to register V4L2 device!\n");
		goto err_v4l2_register;
	}
	MIP_INFO("Registered V4L2 device.\n");

	/* Initialize field of video device */
	vfd = &vout->video_dev;
	vfd->release    = video_device_release_empty;
	vfd->fops       = &sp_mipi_fops;
	vfd->ioctl_ops  = &sp_mipi_ioctl_ops;
	//vfd->tvnorms  = 0;
	vfd->v4l2_dev   = &vout->v4l2_dev;
	strlcpy(vfd->name, VOUT_NAME, sizeof(vfd->name));

	spin_lock_init(&vout->irqlock);
	spin_lock_init(&vout->dma_queue_lock);
	mutex_init(&vout->lock);
	vfd->minor = -1;

	// Register video device.
	ret = video_register_device(&vout->video_dev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		MIP_ERR("Unable to register video device!\n");
		vfd->minor = -1;
		ret = -ENODEV;
		goto err_video_register;
	}
	MIP_INFO("Registered video device \'/dev/video%d\'.\n", vfd->minor);

	// Get i2c_info for sub-device.
	sp_vout_cfg = kmalloc(sizeof(*sp_vout_cfg), GFP_KERNEL);
	if (!sp_vout_cfg) {
		MIP_ERR("Failed to allocate memory for \'sp_vout_cfg\'!\n");
		ret = -ENOMEM;
		goto err_alloc_vout_cfg;
	}
	memcpy (sp_vout_cfg, &psp_vout_cfg, sizeof(*sp_vout_cfg));
	sp_vout_cfg->i2c_adapter_id = vout->i2c_id;
	num_subdevs = sp_vout_cfg->num_subdevs;
	vout->cfg = sp_vout_cfg;

	// Get i2c adapter.
	i2c_adap = i2c_get_adapter(sp_vout_cfg->i2c_adapter_id);
	if (!i2c_adap) {
		MIP_ERR("Failed to get i2c adapter #%d!\n", sp_vout_cfg->i2c_adapter_id);
		ret = -ENODEV;
		goto err_i2c_get_adapter;
	}
	vout->i2c_adap = i2c_adap;
	MIP_INFO("Got i2c adapter #%d.\n", sp_vout_cfg->i2c_adapter_id);

	for (i = 0; i < num_subdevs; i++) {
		sdinfo = &sp_vout_cfg->sub_devs[i];

		/* Load up the subdevice */
		subdev = v4l2_i2c_new_subdev_board(&vout->v4l2_dev,
						    i2c_adap,
						    &sdinfo->board_info,
						    NULL);
		if (subdev) {
			MIP_INFO("Registered V4L2 subdevice \'%s\'.\n", sdinfo->name);
			break;
		}
	}
	if (i == num_subdevs) {
		MIP_ERR("Failed to register V4L2 subdevice!\n");
		ret = -ENXIO;
		goto err_subdev_register;
	}

	/* set current sub device */
	vout->current_subdev = &sp_vout_cfg->sub_devs[i];
	vout->v4l2_dev.ctrl_handler = subdev->ctrl_handler;
	sensor_data = v4l2_get_subdevdata(subdev);
	vout->cur_sensor_mode = sensor_data->mode;
	vout->cur_sensor_fmt = vout->current_subdev->sensor_fmt[vout->cur_sensor_mode];

	mipicsi_init(vout);
	csiiw_init(vout);

	ret = csiiw_irq_init(vout);
	if (ret) {
		goto err_csiiw_irq_init;
	}

	// Initialize video format (V4L2_format).
	cur_fmt = &vout->cur_sensor_fmt->formats[0];
	vout->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vout->fmt.fmt.pix.width = cur_fmt->width;
	vout->fmt.fmt.pix.height = cur_fmt->height;
	vout->fmt.fmt.pix.pixelformat = cur_fmt->fourcc;
	vout->fmt.fmt.pix.field = V4L2_FIELD_NONE;
	vout->fmt.fmt.pix.bytesperline = (vout->fmt.fmt.pix.width * cur_fmt->depth) >> 3;
	vout->fmt.fmt.pix.sizeimage = vout->fmt.fmt.pix.height * vout->fmt.fmt.pix.bytesperline;
	vout->fmt.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
	vout->fmt.fmt.pix.priv = 0;
	return 0;

err_csiiw_irq_init:
err_subdev_register:
	i2c_put_adapter(i2c_adap);

err_i2c_get_adapter:
	kfree(sp_vout_cfg);

err_alloc_vout_cfg:
	video_unregister_device(&vout->video_dev);

err_video_register:
	v4l2_device_unregister(&vout->v4l2_dev);

err_v4l2_register:
err_deassert_csiiw_rstc:
err_deassert_mipicsi_rstc:
	clk_disable_unprepare(vout->csiiw_clk);

err_en_csiiw_clk:
	clk_disable_unprepare(vout->mipicsi_clk);

err_en_mipicsi_clk:
err_get_i2c_id:
err_get_csiiw_rstc:
err_get_mipicsi_rstc:
err_get_csiiw_clk:
err_get_mipicsi_clk:
err_alloc:
	kfree(vout);
	return ret;
}

static int sp_mipi_remove(struct platform_device *pdev)
{
	struct sp_vout_device *vout = platform_get_drvdata(pdev);

	DBG_INFO("%s\n", __FUNCTION__);

	i2c_put_adapter(vout->i2c_adap);
	kfree(vout->cfg);

	video_unregister_device(&vout->video_dev);
	v4l2_device_unregister(&vout->v4l2_dev);

	clk_disable_unprepare(vout->csiiw_clk);
	clk_disable_unprepare(vout->mipicsi_clk);

	kfree(vout);
	return 0;
}

static const struct of_device_id sp_mipi_of_match[] = {
	{ .compatible = "sunplus,sp7021-mipicsi", },
	{}
};

static struct platform_driver sp_mipi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = VOUT_NAME,
		.of_match_table = sp_mipi_of_match,
	},
	.probe = sp_mipi_probe,
	.remove = sp_mipi_remove,
};

module_platform_driver(sp_mipi_driver);

MODULE_DESCRIPTION("Sunplus MIPI-CSI driver");
MODULE_LICENSE("GPL");

