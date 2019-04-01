/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/clk.h>  
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
#include "sp-mipi.h"

static struct sp_fmt formats[] = {
	{
		.name     = "GREY",
		.fourcc   = V4L2_PIX_FMT_GREY,
		.depth    = 1,
	},
};

static struct sp_fmt *get_format(struct v4l2_format *f)
{
	struct sp_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}
//===================================================================================   
/* ------------------------------------------------------------------
	SP7021 function
   ------------------------------------------------------------------*/
static SP_MIPI_t stSPMIPIInfo;
static regs_mipicsi_t *pMIPICSIReg;
static regs_csiiw_t *pCSIIWReg;
static int sp_mipi_get_register_base(struct platform_device *pdev, unsigned int *membase, const char *res_name)
{
	struct resource *r;
	void __iomem *p;

	FUNC_DEBUG();
	DBG_INFO("Register name  : %s!!\n", res_name);

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if(r == NULL) {
		DBG_INFO("platform_get_resource_byname fail\n");
		return -ENODEV;
	}

	p = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(p)) {
		DBG_ERR("ioremap fail\n");
		return PTR_ERR(p);
	}

	DBG_INFO("ioremap addr : 0x%x!!\n", (unsigned int)p);
	*membase = (unsigned int)p;

	return 0;
}

static int sp_mipi_get_resources(struct platform_device *pdev, SP_MIPI_t *pstSpMIPIInfo)
{
	int ret;
	unsigned int membase = 0;

	FUNC_DEBUG();

	ret = sp_mipi_get_register_base(pdev, &membase, MIPICSI_REG_NAME);
	if (ret) {
		DBG_ERR(" %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	}
	else {
		pstSpMIPIInfo->mipicsi_regs = (void __iomem *)membase;
	}

	ret = sp_mipi_get_register_base(pdev, &membase, CSIIW_REG_NAME);
	if (ret) {
		DBG_ERR(" %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	}
	else {
		pstSpMIPIInfo->csiiw_regs = (void __iomem *)membase;
	}
	return 0;
}

static void MIPI_CSI_init(void)
{
	writel(0x1f,
			&(pMIPICSIReg->mipi_analog_cfg2)); 
	writel(0x118104,
			&(pMIPICSIReg->mipicsi_mix_cfg));			//raw10:0x118104, raw8:0x128104
	writel(0X1000,
			&(pMIPICSIReg->mipi_analog_cfg1)); 
	writel(0X1001,
			&(pMIPICSIReg->mipi_analog_cfg1)); 
	writel(0X1000,
			&(pMIPICSIReg->mipi_analog_cfg1)); 	
	writel(0X2B,
			&(pMIPICSIReg->mipicsi_sof_sol_syncword));	//raw10:0x2B, raw8:0x2A
	writel(0X01,
			&(pMIPICSIReg->mipicsi_enable));			//Enable MIPICSI	
}

static void MIPI_CSIIW_init(void)
{
	writel(0x1,
			&(pCSIIWReg->csiiw_latch_mode));			// latch mode should be enable before base address setup
	writel(0x500,
			&(pCSIIWReg->csiiw_stride)); 
	writel(0x3200500,
			&(pCSIIWReg->csiiw_frame_size)); 
	// buffer_number (1:0x00000100, 2:0x011f4000, 3:0x021f4000)
	writel(0x00000100,
			&(pCSIIWReg->csiiw_frame_buf));				// set offset to trigger dram writer
	//raw8 (0x2701); raw10 (10bit two byte space:0x2731, 8bit one byte space:0x2701)
	writel(0x00032700,
			&(pCSIIWReg->csiiw_config0));				// Disable CSIIW and mask fs_irq & fe_irq
}

static void sp_vout_schedule_next_buffer(struct sp_vout_device *vout)
{
	vout->next_frm = list_entry(vout->dma_queue.next,
					struct videobuf_buffer, queue);
	list_del(&vout->next_frm->queue);

	vout->next_frm->state = VIDEOBUF_ACTIVE;
	vout->baddr = videobuf_to_dma_contig(vout->next_frm);
	writel(vout->baddr, &(pCSIIWReg->csiiw_base_addr)); // base address
	DBG_INFO(" %s : baddr=%x\n", __FUNCTION__, vout->baddr);
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
	struct sp_vout_device *vout  = dev_instance;

	if (vout->started) {
		spin_lock(&vout->dma_queue_lock);
		if (!list_empty(&vout->dma_queue) &&
			vout->cur_frm == vout->next_frm)
			sp_vout_schedule_next_buffer(vout);

		if (vout->cur_frm != vout->next_frm)
			sp_vout_process_buffer_complete(vout);

		spin_unlock(&vout->dma_queue_lock);
	}

	return IRQ_HANDLED;
}

static int CSIIW0_irq_init(struct sp_vout_device *vout)
{ 
	int ret; 
	struct device_node *np = NULL; 

	np = of_find_compatible_node(NULL, NULL, "sunplus,sp7021-mipicsi"); 
	vout->fs_irq = irq_of_parse_and_map(np, 0);  			
	ret = request_irq(vout->fs_irq, csiiw_fs_isr, IRQF_SHARED, "CSIIW0_FS", vout); 	
	if (ret) { 
		DBG_ERR("request_irq() failed (%d)\n", ret);
		return (ret); 
	} 

	vout->fe_irq = irq_of_parse_and_map(np, 1); 
	ret = request_irq(vout->fe_irq, csiiw_fe_isr, IRQF_SHARED, "CSIIW0_FE", vout); 	
	if (ret) { 
		DBG_ERR("request_irq() failed (%d)\n", ret); 
		return (ret); 
	} 
	DBG_INFO("MIPICSIIW interrupt installed. (fs_irq=%d, fe_irq=%d)\n", vout->fs_irq , vout->fe_irq);

	return 0; 
}

static int buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	//struct sp_vout_fh *fh = vq->priv_data;
	//struct sp_vout_device *vout = fh->vout;

	*size = VOUT_WIDTH * VOUT_HEIGHT;

	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	DBG_INFO(" %s : count=%d, size =%d\n", __FUNCTION__,  *count, *size);

	return 0;
}

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct sp_vout_fh *fh = vq->priv_data;
	struct sp_vout_device *vout = fh->vout;
	unsigned long addr;
	int ret;

	/* If buffer is not initialized, initialize it */
	if (VIDEOBUF_NEEDS_INIT == vb->state) {
		vb->width = vout->fmt.fmt.pix.width;
		vb->height = vout->fmt.fmt.pix.height;
		vb->size = vout->fmt.fmt.pix.sizeimage;
		vb->field = field;

		ret = videobuf_iolock(vq, vb, NULL);
		if (ret < 0)
			return ret;

		addr = videobuf_to_dma_contig(vb);
		/* Make sure user addresses are aligned to 32 bytes */
		if (!ALIGN(addr, 32))
			return -EINVAL;

		vb->state = VIDEOBUF_PREPARED;
	}
	DBG_INFO(" %s (video_buffer): width=%d, height=%d, size=%lx, field=%d, state=%d\n",
			 __FUNCTION__,  vb->width, vb->height, vb->size, vb->field, vb->state);

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
	DBG_INFO(" %s : list_add\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	spin_unlock_irqrestore(&vout->dma_queue_lock, flags);

	/* Change state of the buffer */
	vb->state = VIDEOBUF_QUEUED;
	DBG_INFO(" %s (video_buffer): state=%d\n", __FUNCTION__, vb->state);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
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
	DBG_INFO(" %s : init_list\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	spin_unlock_irqrestore(&vout->dma_queue_lock, flags);

	videobuf_dma_contig_free(vq, vb);
	vb->state = VIDEOBUF_NEEDS_INIT;
	DBG_INFO(" %s (video_buffer): state=%d\n", __FUNCTION__, vb->state);
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
static int vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *vcap)
{
	DBG_INFO(" %s \n", __FUNCTION__);
	strlcpy(vcap->driver, "SP Video Driver", sizeof(vcap->driver));
	strlcpy(vcap->card, "SP MIPI Camera Card", sizeof(vcap->card));
	strlcpy(vcap->bus_info, "SP MIPI Camera BUS", sizeof(vcap->bus_info));
	vcap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE; 
	vcap->capabilities = vcap->device_caps| V4L2_CAP_DEVICE_CAPS ; // report capabilities
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *fmtdesc)
{
	struct sp_fmt *fmt;

	if (fmtdesc->index >= ARRAY_SIZE(formats)) {
		DBG_INFO(" %s stop\n", __FUNCTION__);
		return -EINVAL;
	}

	fmt = &formats[fmtdesc->index];
	strlcpy(fmtdesc->description, fmt->name, sizeof(fmtdesc->description));
	fmtdesc->pixelformat = fmt->fourcc;

	DBG_INFO(" %s : index=%d\n", __FUNCTION__, fmtdesc->index);
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct sp_fmt *fmt;
	enum v4l2_field field;

	DBG_INFO(" %s \n", __FUNCTION__);
	fmt = get_format(f);

	field = f->fmt.pix.field;
	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} 
	f->fmt.pix.field = field;

	v4l_bound_align_image(&f->fmt.pix.width, 48, norm_maxw(), 2,
			      &f->fmt.pix.height, 32, norm_maxh(), 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (fmt->fourcc == V4L2_PIX_FMT_YUYV ||
	    fmt->fourcc == V4L2_PIX_FMT_UYVY)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	int ret;
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO(" %s \n", __FUNCTION__);
	ret = vidioc_try_fmt_vid_cap(file, vout, f);

	vout->fmt.type = f->type;
	vout->fmt.fmt.pix.width = f->fmt.pix.width;
	vout->fmt.fmt.pix.height = f->fmt.pix.height;
	vout->fmt.fmt.pix.pixelformat = f->fmt.pix.pixelformat; // from vidioc_try_fmt_vid_cap
	vout->fmt.fmt.pix.field = f->fmt.pix.field;	
	vout->fmt.fmt.pix.bytesperline = f->fmt.pix.bytesperline;
	vout->fmt.fmt.pix.sizeimage = f->fmt.pix.sizeimage ;
	vout->fmt.fmt.pix.colorspace = f->fmt.pix.colorspace;

	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct sp_vout_device *vout = video_drvdata(file);

	memcpy(f, &vout->fmt, sizeof(*f));

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *req_buf)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	int ret;

	DBG_INFO(" %s \n", __FUNCTION__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != req_buf->type) {
		v4l2_err(&vout->v4l2_dev, "Invalid buffer type\n");
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&vout->lock);
	if (ret)
		return ret;

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
	DBG_INFO(" %s : init_list\n", __FUNCTION__);
	print_List(&vout->dma_queue);
	ret = videobuf_reqbufs(&vout->buffer_queue, req_buf);
	mutex_unlock(&vout->lock);

	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv,
			 struct v4l2_buffer *buf)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO(" %s \n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		DBG_ERR("Invalid buf type\n");
		return  -EINVAL;
	}

	if (vout->memory != V4L2_MEMORY_MMAP) {
		DBG_ERR("Invalid memory\n");
		return -EINVAL;
	}
	/* Call videobuf_querybuf to get information */
	return videobuf_querybuf(&vout->buffer_queue, buf);
}

static int vidioc_qbuf(struct file *file, void *priv,
			 struct v4l2_buffer *buf)

{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;

	DBG_INFO(" %s \n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		DBG_ERR("Invalid buf type\n");
		return -EINVAL;
	}

	/*
	 * If this file handle is not allowed to do IO,
	 * return error
	 */
	if (!fh->io_allowed) {
		DBG_ERR("fh->io_allowed\n");
		return -EACCES;
	}
	return videobuf_qbuf(&vout->buffer_queue, buf);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO(" %s \n", __FUNCTION__);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		DBG_ERR("Invalid buf type\n");
		return -EINVAL;
	}
	return videobuf_dqbuf(&vout->buffer_queue,
				      buf, file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type buf_type)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	//struct sp_vout_subdev_info *sdinfo;
	int ret;

	DBG_INFO(" %s \n", __FUNCTION__);

	if (buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		DBG_ERR("Invalid buf type\n");
		return -EINVAL;
	}

	/* If file handle is not allowed IO, return error */
	if (!fh->io_allowed) {
		DBG_ERR("fh->io_allowed\n");
		return -EACCES;
	}

#if 0 //phtsai
	sdinfo = vout->current_subdev;
	ret = v4l2_device_call_until_err(&vout->v4l2_dev, sdinfo->grp_id,
					video, s_stream, 1);
	if (ret && (ret != -ENOIOCTLCMD)) {
		DBG_ERR("stream on failed in subdev\n");
		return -EINVAL;
	}
#endif

	/* If buffer queue is empty, return error */
	if (list_empty(&vout->buffer_queue.stream)) {
		DBG_ERR("buffer queue is empty\n");
		return -EIO;
	}

	//writel(0x00002700, &(pCSIIWReg->csiiw_config0));	// Disable CSIIW and enable fs_irq & fe_irq

	/* Call videobuf_streamon to start streaming * in videobuf */
	ret = videobuf_streamon(&vout->buffer_queue);
	if (ret)
		return ret;

	ret = mutex_lock_interruptible(&vout->lock);
	if (ret)
		goto streamoff;
	/* Get the next frame from the buffer queue */
	vout->next_frm = list_entry(vout->dma_queue.next,
					struct videobuf_buffer, queue);

	vout->cur_frm = vout->next_frm;

	/* Remove buffer from the buffer queue */
	list_del(&vout->cur_frm->queue);
	DBG_INFO(" %s : list_del\n", __FUNCTION__);
	print_List(&vout->dma_queue);

	/* Mark state of the current frame to active */
	vout->cur_frm->state = VIDEOBUF_ACTIVE;
	vout->baddr = videobuf_to_dma_contig(vout->cur_frm);

	writel(vout->baddr, &(pCSIIWReg->csiiw_base_addr)); // base address
	writel(mEXTENDED_ALIGNED(vout->fmt.fmt.pix.width, 16), 
			&(pCSIIWReg->csiiw_stride));		// line pitch
	writel((vout->fmt.fmt.pix.height<<16)|vout->fmt.fmt.pix.width, 
			&(pCSIIWReg->csiiw_frame_size));	// hv size
	writel(0x00002701, 
			&(pCSIIWReg->csiiw_config0));		// Enable CSIIW and fs_irq & ge_irq


	DBG_INFO(" %s : cur_frm=%p, next_frm=%p, baddr=%x\n", 
				__FUNCTION__, vout->cur_frm, vout->next_frm, vout->baddr);

	vout->started = 1;	
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
	//struct vout *sdinfo;
	int ret;

	DBG_INFO(" %s \n", __FUNCTION__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != buf_type) {
		DBG_ERR("Invalid buf type\n");
		return -EINVAL;
	}

	/* If io is allowed for this file handle, return error */
	if (!fh->io_allowed) {
		DBG_ERR("fh->io_allowed\n");
		return -EACCES;
	}

	/* If streaming is not started, return error */
	if (!vout->started) {
		DBG_ERR("device started\n");
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&vout->lock);
	if (ret)
		return ret;

#if 0 //phtsai
	vpfe_stop_ccdc_capture(vpfe_dev);
	vpfe_detach_irq(vpfe_dev);

	sdinfo = vpfe_dev->current_subdev;
	ret = v4l2_device_call_until_err(&vpfe_dev->v4l2_dev, sdinfo->grp_id,
					video, s_stream, 0);
	if (ret && (ret != -ENOIOCTLCMD))
		v4l2_err(&vpfe_dev->v4l2_dev, "stream off failed in subdev\n");
#endif
	vout->started = 0;
	// FW must mask irq to avoid unmap issue (for test code)
	writel(0x00032700,
			&(pCSIIWReg->csiiw_config0));				// Disable CSIIW and mask fs_irq & fe_irq
	ret = videobuf_streamoff(&vout->buffer_queue);
	mutex_unlock(&vout->lock);
	return ret;
}

static const struct v4l2_ioctl_ops sp_mipi_ioctl_ops = {
	.vidioc_querycap			= vidioc_querycap,
	.vidioc_enum_fmt_vid_cap 	= vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   	= vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     	= vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= vidioc_g_fmt_vid_cap,
	.vidioc_reqbufs				= vidioc_reqbufs,
	.vidioc_querybuf			= vidioc_querybuf,
	.vidioc_qbuf          		= vidioc_qbuf,
	.vidioc_dqbuf         		= vidioc_dqbuf,
	.vidioc_streamon      		= vidioc_streamon,
	.vidioc_streamoff     		= vidioc_streamoff,
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

	DBG_INFO(" %s \n", __FUNCTION__);
#if 0
	if (!vpfe_dev->cfg->num_subdevs) {
		DBG_ERR("No decoder registered\n");
		return -ENODEV;
	}
#endif

	/* Allocate memory for the file handle object */
	fh = kmalloc(sizeof(struct sp_vout_fh), GFP_KERNEL);
	if (!fh) {
		DBG_ERR("Allocate memory fail\n");
		return -ENOMEM;
	}

	/* store pointer to fh in private_data member of file */
	file->private_data = fh;
	fh->vout = vout;
	v4l2_fh_init(&fh->fh, vdev);

	mutex_lock(&vout->lock);
	/* Set io_allowed member to false */
	fh->io_allowed = 0;
	v4l2_fh_add(&fh->fh);
	mutex_unlock(&vout->lock);
	return 0;
}

static int sp_vout_release(struct file *file)
{
	struct sp_vout_device *vout = video_drvdata(file);
	struct sp_vout_fh *fh = file->private_data;
	//struct vpfe_subdev_info *sdinfo;
	//int ret;

	DBG_INFO(" %s \n", __FUNCTION__);

	/* Get the device lock */
	mutex_lock(&vout->lock);
	/* if this instance is doing IO */
	if (fh->io_allowed) {
		if (vout->started) {
#if 0
			sdinfo = vout->current_subdev;
			ret = v4l2_device_call_until_err(&vpfe_dev->v4l2_dev,
							 sdinfo->grp_id,
							 video, s_stream, 0);
			if (ret && (ret != -ENOIOCTLCMD))
				v4l2_err(&vout->v4l2_dev,
				"stream off failed in subdev\n");
			vpfe_stop_ccdc_capture(vout);
			vpfe_detach_irq(vout);
#endif
			vout->started = 0;
			writel(0x00032700,
					&(pCSIIWReg->csiiw_config0));	// Disable CSIIW and mask fs_irq & fe_irq

			//free_irq(vout->fs_irq, vout);
			//free_irq(vout->fe_irq, vout);
			videobuf_streamoff(&vout->buffer_queue);
		}
		videobuf_stop(&vout->buffer_queue);
		videobuf_mmap_free(&vout->buffer_queue);
	}
	/* Decrement device usrs counter */
	v4l2_fh_del(&fh->fh);
	v4l2_fh_exit(&fh->fh);
	mutex_unlock(&vout->lock);

	file->private_data = NULL;
	/* Free memory allocated to file handle object */
	kfree(fh);
	return 0;
}

static int sp_vout_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sp_vout_device *vout = video_drvdata(file);

	DBG_INFO(" %s \n", __FUNCTION__);
	return videobuf_mmap_mapper(&vout->buffer_queue, vma);
}

static unsigned int sp_vout_poll(struct file *file, poll_table *wait)
{
	struct sp_vout_device *vout = video_drvdata(file);
	
	DBG_INFO(" %s \n", __FUNCTION__);
	if (vout->started)
		return videobuf_poll_stream(file,
					    &vout->buffer_queue, wait);
	return 0;
}

static const struct v4l2_file_operations sp_mipi_fops = {
	.owner				= THIS_MODULE,
	.open				= sp_vout_open,
	.release			= sp_vout_release,
	.unlocked_ioctl		= video_ioctl2,
	.mmap 				= sp_vout_mmap,
	.poll 				= sp_vout_poll,
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
	int ret;
	SP_MIPI_t *pstSpMIPIInfo = NULL;

	DBG_INFO(" %s \n", __FUNCTION__);
	pstSpMIPIInfo = &stSPMIPIInfo;
	memset(pstSpMIPIInfo, 0, sizeof(SP_MIPI_t));

	DBG_INFO("Get resources (CLK & Reg.) start.\n");
	ret = sp_mipi_get_resources(pdev, pstSpMIPIInfo);
	if (ret) {
		DBG_ERR("get resources fail !\n");
		return ret;
	}

	pstSpMIPIInfo->clk_mipicsi_0 = devm_clk_get(dev, "clk_mipicsi_0");
	if (IS_ERR(pstSpMIPIInfo->clk_mipicsi_0)) {
		ret = PTR_ERR(pstSpMIPIInfo->clk_mipicsi_0);
		DBG_ERR("failed to retrieve clk_mipicsi_0\n");
		goto err_clk_disable;
	}
	ret = clk_prepare_enable(pstSpMIPIInfo->clk_mipicsi_0);
	if (ret) {
		DBG_ERR("failed to enable clk_mipicsi_0\n");
		goto err_clk_disable;
	}

	pstSpMIPIInfo->clk_csiiw_0 = devm_clk_get(dev, "clk_csiiw_0");
	if (IS_ERR(pstSpMIPIInfo->clk_csiiw_0)) {
		ret = PTR_ERR(pstSpMIPIInfo->clk_csiiw_0);
		DBG_ERR("failed to retrieve clk_csiiw_0\n");
		goto err_clk_disable;
	}

	ret = clk_prepare_enable(pstSpMIPIInfo->clk_csiiw_0);
	if (ret) {
		DBG_ERR("failed to enable clk_csiiw_0\n");
		goto err_clk_disable;
	}	
	pMIPICSIReg = (regs_mipicsi_t *)pstSpMIPIInfo->mipicsi_regs;
	pCSIIWReg = (regs_csiiw_t *)pstSpMIPIInfo->csiiw_regs;
	DBG_INFO("mipicsi_regs= 0x%p, csiiw_regs= 0x%p\n", 
			pstSpMIPIInfo->mipicsi_regs, pstSpMIPIInfo->csiiw_regs);
	DBG_INFO("Get resources (CLK & Reg.) end.\n");

	vout = kzalloc(sizeof(struct sp_vout_device), GFP_KERNEL);
	if (!vout) {
		DBG_ERR("Failed to allocate memory for vpfe_dev\n");
		return -ENOMEM;
	}

	vout->pdev = &pdev->dev;
	vfd = &vout->video_dev;

	/* Initialize field of video device */
	vfd->release		= video_device_release;
	vfd->fops			= &sp_mipi_fops;
	vfd->ioctl_ops		= &sp_mipi_ioctl_ops;
	//vfd->tvnorms		= 0;
	vfd->v4l2_dev 		= &vout->v4l2_dev;
	strlcpy(vfd->name, VOUT_NAME, sizeof(vfd->name));

	ret = v4l2_device_register(&pdev->dev, &vout->v4l2_dev);
	if (ret) {
		DBG_ERR("Unable to register v4l2 device.\n");
		goto probe_err0;
	}

	// init V4L2_format
	vout->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vout->fmt.fmt.pix.width = norm_maxw();
	vout->fmt.fmt.pix.height = norm_maxh();	
	vout->fmt.fmt.pix.pixelformat = formats[0].fourcc;
	vout->fmt.fmt.pix.field = V4L2_FIELD_NONE;
	vout->fmt.fmt.pix.bytesperline = (vout->fmt.fmt.pix.width * formats[0].depth) >> 3;
	vout->fmt.fmt.pix.sizeimage = vout->fmt.fmt.pix.height * vout->fmt.fmt.pix.bytesperline ;
	vout->fmt.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
	vout->fmt.fmt.pix.priv = 0;

	DBG_INFO("v4l2 device registered\n");
	spin_lock_init(&vout->irqlock);
	spin_lock_init(&vout->dma_queue_lock);
	mutex_init(&vout->lock);
	vfd->minor = -1;

	ret = video_register_device(&vout->video_dev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		DBG_ERR("Unable to register video device.\n");
		vfd->minor = -1;
		ret = -ENODEV;
		goto probe_err1;
	}

	/* set the driver data in platform device */
	platform_set_drvdata(pdev, vout);
	/* set driver private data */
	video_set_drvdata(&vout->video_dev, vout);
	DBG_INFO("registered and initialized"
				" video device %d\n", vfd->minor);

	MIPI_CSI_init();
	MIPI_CSIIW_init();
	CSIIW0_irq_init(vout);
	return 0;

probe_err0:
	kfree(vout);
	return ret;
probe_err1:
	video_device_release(vfd);
	v4l2_device_unregister(&vout->v4l2_dev);
	return ret;

err_clk_disable:
	clk_disable_unprepare(pstSpMIPIInfo->clk_mipicsi_0);
	clk_disable_unprepare(pstSpMIPIInfo->clk_csiiw_0);
	return ret;
}

static int sp_mipi_remove(struct platform_device *pdev)
{
	struct sp_vout_device *vout = platform_get_drvdata(pdev);

	DBG_INFO(" %s \n", __FUNCTION__);
	//kfree(vpfe_dev->sd);
	v4l2_device_unregister(&vout->v4l2_dev);
	video_unregister_device(&vout->video_dev);
	kfree(vout);

	free_irq(vout->fs_irq, vout); 
	free_irq(vout->fe_irq, vout); 

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

MODULE_DESCRIPTION("SP_MIPI driver");
MODULE_LICENSE("GPL");
