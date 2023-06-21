// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Sunplus VIN
 *
 * Copyright Sunplus Technology Co., Ltd.
 * 	  All rights reserved.
 *
 * Based on Renesas R-Car VIN driver
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>

#include <media/videobuf2-dma-contig.h>

#include "sp-vin.h"

/* -----------------------------------------------------------------------------
 * HW Functions
 */

/* MIPI-CSI-IW registers */
#define CSIIW_LATCH_MODE			0x00 	/* 171.0 CSIIW latch mode (csiiw_latch_mode) */
#define CSIIW_CONFIG0				0x04 	/* 171.1 CSIIW config register 0 (csiiw_config0) */
#define CSIIW_BASE_ADDR				0x08 	/* 171.2 CSIIW base address (csiiw_base_addr) */
#define CSIIW_STRIDE				0x0C 	/* 171.3 CSIIW line stride (csiiw_stride) */
#define CSIIW_FRAME_SIZE			0x10 	/* 171.4 CSIIW frame size (csiiw_frame_size) */
#define CSIIW_FRAME_BUF				0x14 	/* 171.5 CSIIW frame buffer rorate (csiiw_frame_buf) */
#define CSIIW_CONFIG1				0x18 	/* 171.6 CSIIW config register 1 (csiiw_config1) */
#define CSIIW_FRAME_SIZE_RO			0x1C 	/* 171.7 CSIIW frame size of HW automatic detection (csiiw_frame_size_ro) */
#define CSIIW_DEBUG_INFO			0x20 	/* 171.8 CSIIW debug info (csiiw_debug_info) */
#define CSIIW_CONFIG2				0x24 	/* 171.9 CSIIW config register 2 (csiiw_config2) */
#define CSIIW_INTERRUPT				0x28 	/* 171.10 CSIIW interrupt (csiiw_interrupt) */
#define CSIIW_RESERVED				0x2C 	/* 171.11 CSIIW reserved (csiiw_reserved) */
#define CSIIW_TIMESTAMP_FRAMECNT	0x30 	/* 171.12 CSIIW time stamp frame count (csiiw_timestamp_framecnt) */
#define CSIIW_TIMESTAMP_FRAME0		0x34 	/* 171.13 CSIIW time stamp frame 0 (csiiw_timestamp_frame0) */
#define CSIIW_TIMESTAMP_FRAME1		0x38 	/* 171.14 CSIIW time stamp frame 1 (csiiw_timestamp_frame1) */
#define CSIIW_TIMESTAMP_FRAME2		0x3C 	/* 171.15 CSIIW time stamp frame 2 (csiiw_timestamp_frame2) */
#define CSIIW_TIMESTAMP_FRAME3		0x40 	/* 171.16 CSIIW time stamp frame 3 (csiiw_timestamp_frame3) */
#define CSIIW_VERSION				0x44 	/* 171.17 CSIIW IP version (csiiw_version) */


struct vin_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

#define to_buf_list(vb2_buffer) (&container_of(vb2_buffer, \
					       struct vin_buffer, \
					       vb)->list)

#define mEXTENDED_ALIGNED(w, n)	((w%n) ? ((w/n)*n+n) : (w))


static inline void set_field(u32 *valp, u32 field, u32 mask)
{
	u32 val = *valp;

	val &= ~mask;
	val |= (field << __ffs(mask)) & mask;
	*valp = val;
}

static inline u32 csiiw_readl(struct vin_dev *vin, u32 offset)
{
	return readl(vin->base + offset);
}

static inline void csiiw_writel(struct vin_dev *vin, u32 offset, u32 value)
{
	writel(value, vin->base + offset);
}

static void csiiw_dt_config(struct vin_dev *vin)
{
	u32 config0, config2;

	config0 = csiiw_readl(vin, CSIIW_CONFIG0);
	config2 = csiiw_readl(vin, CSIIW_CONFIG2);


	dev_dbg(vin->dev, "%s, %d, config0: %08x, config2: %08x\n",
			__FUNCTION__, __LINE__, config0, config2);

	switch (vin->vin_fmt->bpc) {
	default:
	case 8:
		set_field(&config0, 0, 0x3<<4);     /* Source is 8 bits per pixel */
		set_field(&config0, 0, 0x1<<16);	/* Disable packed mode */
		break;

	case 10:
		set_field(&config0, 1, 0x3<<4);     /* Source is 10 bits per pixel */

		if (vin->vin_fmt->bpp == vin->vin_fmt->bpc)
			set_field(&config0, 1, 0x1<<16); 	/* Enable packed mode */
		else
			set_field(&config0, 0, 0x1<<16); 	/* Disable packed mode */
		break;

	case 12:
		set_field(&config0, 2, 0x3<<4);     /* Source is 12 bits per pixel */

		if (vin->vin_fmt->bpp == vin->vin_fmt->bpc)
			set_field(&config0, 1, 0x1<<16); 	/* Enable packed mode */
		else
			set_field(&config0, 0, 0x1<<16); 	/* Disable packed mode */
		break;
	}

	/* Packed mode does not support stride mode for DRAM DMA writes */
	if (config0 & 0x00010000)
		set_field(&config2, 1, 0x1<<0);		/* Set NO_STRIDE_EN to 1 for packed mode */
	else
		set_field(&config2, 0, 0x1<<0);		/* Set NO_STRIDE_EN to 0 for unpacked mode */

	dev_dbg(vin->dev, "%s, %d, config0: %08x, config2: %08x\n",
			__FUNCTION__, __LINE__, config0, config2);

	csiiw_writel(vin, CSIIW_CONFIG0, config0);
	csiiw_writel(vin, CSIIW_CONFIG2, config2);
}

static void csiiw_fs_config(struct vin_dev *vin)
{
	struct v4l2_pix_format *pix = &vin->format;
	u32 stride, frame_size, val;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(vin->dev, "%s, %dx%d, pix->bytesperline: %d\n",
		__func__, pix->width, pix->height, pix->bytesperline);

	stride = csiiw_readl(vin, CSIIW_STRIDE);
	frame_size = csiiw_readl(vin, CSIIW_FRAME_SIZE);

	/* Set LINE_STRIDE field of csiiw_stride */
	val = mEXTENDED_ALIGNED(pix->bytesperline, 16);
	set_field(&stride, val, 0x3fff<<0);

	/* Set XLEN field of csiiw_frame_size */
	switch (vin->mbus_code) {
	default:

	/* YUV */
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
		val = pix->bytesperline;
		break;

	/* RAW */
	case MEDIA_BUS_FMT_SBGGR8_1X8:		/* 8 bits */
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:	/* 10 bits */
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:	/* 12 bits */
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		val = pix->width;
		break;
	}

	set_field(&frame_size, val, 0x1fff<<0);

	/* Set YLEN field of csiiw_frame_size */
	set_field(&frame_size, pix->height, 0xfff<<16);

	dev_dbg(vin->dev, "%s, %d: stride: %08x, frame_size: %08x\n",
		 __func__, __LINE__, stride, frame_size);

	csiiw_writel(vin, CSIIW_STRIDE, stride);
	csiiw_writel(vin, CSIIW_FRAME_SIZE, frame_size);
}

static void csiiw_wr_dma_addr(struct vin_dev *vin, dma_addr_t addr)
{
	//dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);
	//dev_dbg(vin->dev, "%s, addr: 0x%x\n", __func__, addr);

	/* Set BASE_ADDR field of csiiw_base_addr */
	csiiw_writel(vin, CSIIW_BASE_ADDR, addr);
}

static void csiiw_init(struct vin_dev *vin)
{
	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	csiiw_writel(vin, CSIIW_LATCH_MODE, 0x1);		/* Enable before base addr setup */
	csiiw_writel(vin, CSIIW_STRIDE, 0x500);
	csiiw_writel(vin, CSIIW_FRAME_SIZE, 0x3200500);
	csiiw_writel(vin, CSIIW_FRAME_BUF, 0x00000100);	/* set offset to trigger DRAM write */
	csiiw_writel(vin, CSIIW_CONFIG0, 0xf02700);		/* Disable csiiw, unpacked mode */
	csiiw_writel(vin, CSIIW_INTERRUPT, 0x1111);		/* Disable and clean fs_irq and fe_irq */
	//csiiw_writel(vin, CSIIW_CONFIG2, 0x0001);		/* NO_STRIDE_EN = 1 */
}

static void csiiw_enable(struct vin_dev *vin)
{
	u32 val;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Clean CMD_ERROR and LB_FATAL fields of csiiw_debug_info register */
	val = 0;
	set_field(&val, 0x1, 0x1<<3);		/* Clean CMD_ERROR flag */
	set_field(&val, 0x1, 0x1<<2);		/* Clean LB_FATAL flag */
	csiiw_writel(vin, CSIIW_DEBUG_INFO, val);

	dev_dbg(vin->dev, "%s, %d: debug_info: %08x\n", __FUNCTION__, __LINE__, val);

	/* Configure csiiw_interrupt register */
	val = 0;
	set_field(&val, 0x0, 0x1<<12);		/* Disable Frame End IRQ mask */
	set_field(&val, 0x1, 0x1<<8);		/* Clean Frame End interrupt */
	set_field(&val, 0x1, 0x1<<4);		/* Enable Frame Start IRQ mask */
	set_field(&val, 0x1, 0x1<<0);		/* Clean Frame Start interrupt */
	csiiw_writel(vin, CSIIW_INTERRUPT, val);

	dev_dbg(vin->dev, "%s, %d: interrupt: %08x\n", __FUNCTION__, __LINE__, val);

	/* Configure csiiw_config0 register */
	val = csiiw_readl(vin, CSIIW_CONFIG0);
	set_field(&val, 0x2, 0xf<<12);		/* Bus urgent command threshold */
	set_field(&val, 0x7, 0x7<<8);		/* Bus command queue for rate control */
	set_field(&val, 0x1, 0x1<<0);		/* Enable CSIIW function */
	csiiw_writel(vin, CSIIW_CONFIG0, val);

	dev_dbg(vin->dev, "%s, %d: config0: 0x%08x\n", __func__, __LINE__, val);
}

static void csiiw_disable(struct vin_dev *vin)
{
	u32 val;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Configure csiiw_interrupt register */
	val = csiiw_readl(vin, CSIIW_INTERRUPT);
	val = val & 0x00001111;
	set_field(&val, 0x1, 0x1<<12);	/* Enable Frame End IRQ mask */
	set_field(&val, 0x1, 0x1<<4);	/* Enable Frame Start IRQ mask */
	csiiw_writel(vin, CSIIW_INTERRUPT, val);

	dev_dbg(vin->dev, "%s, %d: interrupt: %08x\n", __FUNCTION__, __LINE__, val);

	/* Configure csiiw_config0 register */
	val = csiiw_readl(vin, CSIIW_CONFIG0);
	set_field(&val, 0x0, 0x1<<0);	/* Disable CSIIW function */
	csiiw_writel(vin, CSIIW_CONFIG0, val);

	dev_dbg(vin->dev, "%s, %d: config0: 0x%08x\n", __func__, __LINE__, val);
}

static void vin_crop_scale_comp_gen2(struct vin_dev *vin)
{
	unsigned int crop_height;
	u32 xs, ys;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);
	//dev_dbg(vin->dev, "%s, vin->format.field: %d\n", __func__, vin->format.field);
	//dev_dbg(vin->dev, "%s, vin->crop.height: 0x%x, vin->crop.width: 0x%x\n",__func__, vin->crop.height, vin->crop.width);
	//dev_dbg(vin->dev, "%s, vin->compose.height: 0x%x, vin->compose.width: 0x%x\n",__func__, vin->compose.height, vin->compose.width);

	/* Set scaling coefficient */
	crop_height = vin->crop.height;
	if (V4L2_FIELD_HAS_BOTH(vin->format.field))
		crop_height *= 2;

	ys = 0;
	if (crop_height != vin->compose.height)
		ys = (4096 * crop_height) / vin->compose.height;

	// Configure Y Scale Register
	//dev_dbg(vin->dev, "%s, ys: %d, crop_height: 0x%x\n", __func__, ys, crop_height);

	xs = 0;
	if (vin->crop.width != vin->compose.width)
		xs = (4096 * vin->crop.width) / vin->compose.width;

	/* Horizontal upscaling is up to double size */
	if (xs > 0 && xs < 2048)
		xs = 2048;

	// Configure X Scale Register
	//dev_dbg(vin->dev, "%s, xs: %d\n", __func__, xs);

	/* Horizontal upscaling is done out by scaling down from double size */
	if (xs < 4096)
		xs *= 2;

	// Config registers for coefficient

	/* Set Start/End Pixel/Line Post-Clip */
	// Config registers for Post-Clip

	vin_dbg(vin,
		"Pre-Clip: %ux%u@%u:%u YS: %d XS: %d Post-Clip: %ux%u@%u:%u\n",
		vin->crop.width, vin->crop.height, vin->crop.left,
		vin->crop.top, ys, xs, vin->format.width, vin->format.height,
		0, 0);
}

void vin_crop_scale_comp(struct vin_dev *vin)
{
	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Set Start/End Pixel/Line Pre-Clip */
	// Configure registers to crop image

	/* TODO: Add support for the UDS scaler. */
	//if (vin->info->model != RCAR_GEN3)
		vin_crop_scale_comp_gen2(vin);

	// Configure image stride register
}

/* -----------------------------------------------------------------------------
 * Hardware setup
 */

static int vin_setup(struct vin_dev *vin)
{
	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * Input interface
	 */
	csiiw_dt_config(vin);

	/*
	 * Output format
	 */
	csiiw_fs_config(vin);

	/* Always update on field change */
	/* If input and output use the same colorspace, use bypass mode */
	/* Progressive or interlaced mode */
	/* Ack interrupts */
	/* Enable interrupts */
	/* Start capturing */
	/* Enable module */
	csiiw_enable(vin);

	return 0;
}

static void vin_set_slot_addr(struct vin_dev *vin, int slot, dma_addr_t addr)
{
	const struct vin_video_format *fmt;
	int offsetx, offsety;
	dma_addr_t offset;

	//dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	fmt = vin_format_from_pixel(vin, vin->format.pixelformat);

	/*
	 * There is no HW support for composition do the beast we can
	 * by modifying the buffer offset
	 */
	offsetx = (vin->compose.left * fmt->bpp) / 8;
	offsety = vin->compose.top * vin->format.bytesperline;
	offset = addr + offsetx + offsety;

	//dev_dbg(vin->dev, "%s, offsetx: 0x%x, offsety: 0x%x\n", __func__, offsetx, offsety);
	//dev_dbg(vin->dev, "%s, offset: 0x%x, addr: 0x%x\n", __func__, offset, addr);

	/*
	 * The address needs to be 128 bytes aligned. Driver should never accept
	 * settings that do not satisfy this in the first place...
	 */
	if (WARN_ON((offsetx | offsety | offset) & HW_BUFFER_MASK))
		return;

	csiiw_wr_dma_addr(vin, (dma_addr_t)offset);
}

/*
 * Moves a buffer from the queue to the HW slot. If no buffer is
 * available use the scratch buffer. The scratch buffer is never
 * returned to userspace, its only function is to enable the capture
 * loop to keep running.
 */
static void vin_fill_hw_slot(struct vin_dev *vin, int slot)
{
	struct vin_buffer *buf;
	struct vb2_v4l2_buffer *vbuf;
	dma_addr_t phys_addr;
	int prev;

	//dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* A already populated slot shall never be overwritten. */
	if (WARN_ON(vin->buf_hw[slot].buffer))
		return;

	prev = (slot == 0 ? HW_BUFFER_NUM : slot) - 1;

	//dev_dbg(vin->dev, "%s, slot: %d, prev: %d\n", __func__, slot, prev);
	//dev_dbg(vin->dev, "%s, vin->buf_hw[prev].type: %d\n", __func__, vin->buf_hw[prev].type);

	if (vin->buf_hw[prev].type == HALF_TOP) {
		vbuf = vin->buf_hw[prev].buffer;
		vin->buf_hw[slot].buffer = vbuf;
		vin->buf_hw[slot].type = HALF_BOTTOM;
		switch (vin->format.pixelformat) {
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV16:
			phys_addr = vin->buf_hw[prev].phys +
				vin->format.sizeimage / 4;
			break;
		default:
			phys_addr = vin->buf_hw[prev].phys +
				vin->format.sizeimage / 2;
			break;
		}
	} else if (list_empty(&vin->buf_list)) {
		vin->buf_hw[slot].buffer = NULL;
		vin->buf_hw[slot].type = FULL;
		phys_addr = vin->scratch_phys;
	} else {
		/* Keep track of buffer we give to HW */
		buf = list_entry(vin->buf_list.next, struct vin_buffer, list);
		vbuf = &buf->vb;
		list_del_init(to_buf_list(vbuf));
		vin->buf_hw[slot].buffer = vbuf;

		vin->buf_hw[slot].type =
			V4L2_FIELD_IS_SEQUENTIAL(vin->format.field) ?
			HALF_TOP : FULL;

		/* Setup DMA */
		phys_addr = vb2_dma_contig_plane_dma_addr(&vbuf->vb2_buf, 0);
	}

	vin_dbg(vin, "Filling HW slot: %d type: %d buffer: %p\n",
		slot, vin->buf_hw[slot].type, vin->buf_hw[slot].buffer);

	vin->buf_hw[slot].phys = phys_addr;
	vin_set_slot_addr(vin, slot, phys_addr);
}

static int vin_capture_start(struct vin_dev *vin)
{
	int slot, ret;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	for (slot = 0; slot < HW_BUFFER_NUM; slot++) {
		vin->buf_hw[slot].buffer = NULL;
		vin->buf_hw[slot].type = FULL;
	}

	for (slot = 0; slot < HW_BUFFER_NUM; slot++)
		vin_fill_hw_slot(vin, slot);

	vin_crop_scale_comp(vin);

	ret = vin_setup(vin);
	if (ret)
		return ret;

	vin_dbg(vin, "Starting to capture\n");


	vin->state = STARTING;

	return 0;
}

static void vin_capture_stop(struct vin_dev *vin)
{
	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Disable module */
	csiiw_disable(vin);
}

/* -----------------------------------------------------------------------------
 * DMA Functions
 */

#define VIN_TIMEOUT_MS 100
#define VIN_RETRIES 10

static irqreturn_t vin_fs_irq(int irq, void *data)
{
	struct vin_dev *vin = data;
	u32 interupt = 0;

	/* Clean Frame Start interrupt */
	interupt = csiiw_readl(vin, CSIIW_INTERRUPT);
	interupt = interupt & 0x00001011;		/* Clean FIELD_START_INT */
	csiiw_writel(vin, CSIIW_INTERRUPT, interupt);

	return IRQ_HANDLED;
}

static irqreturn_t vin_fe_irq(int irq, void *data)
{
	struct vin_dev *vin = data;
	int slot;
	unsigned int handled = 0;
	unsigned long flags;
	u32 interupt = 0, debbug_info = 0;

	dev_dbg(vin->dev, "%s, vin->id: %d, vin->state: %d\n", __func__, vin->id, vin->state);

	interupt = csiiw_readl(vin, CSIIW_INTERRUPT);
	debbug_info = csiiw_readl(vin, CSIIW_DEBUG_INFO);

	spin_lock_irqsave(&vin->qlock, flags);

	handled = 1;

	/* Nothing to do if capture status is 'STOPPED' */
	if (vin->state == STOPPED) {
		vin_dbg(vin, "IRQ while state stopped\n");
		goto done;
	}

	/* Nothing to do if capture status is 'STOPPING' */
	if (vin->state == STOPPING) {
		vin_dbg(vin, "IRQ while state stopping\n");
		goto done;
	}

	/* Prepare for capture and update state */
	// vnms = rvin_read(vin, VNMS_REG);
	// slot = (vnms & VNMS_FBS_MASK) >> VNMS_FBS_SHIFT;
	/* For Sunplus CSI-IW, there is only one DMA slot. */
	slot = 0;		/* It means DMA slot */

	/*
	 * To hand buffers back in a known order to userspace start
	 * to capture first from slot 0.
	 */
	if (vin->state == STARTING) {
		if (slot != 0) {
			vin_dbg(vin, "Starting sync slot: %d\n", slot);
			goto done;
		}

		vin_dbg(vin, "Capture start synced!\n");
		vin->state = RUNNING;
	}

	/* Capture frame */
	if (vin->buf_hw[slot].buffer) {
		/*
		 * Nothing to do but refill the hardware slot if
		 * capture only filled first half of vb2 buffer.
		 */
		if (vin->buf_hw[slot].type == HALF_TOP) {
			vin->buf_hw[slot].buffer = NULL;
			vin_fill_hw_slot(vin, slot);
			goto done;
		}

		vin->buf_hw[slot].buffer->sequence = vin->sequence;
		vin->buf_hw[slot].buffer->vb2_buf.timestamp = ktime_get_ns();
		vb2_buffer_done(&vin->buf_hw[slot].buffer->vb2_buf,
				VB2_BUF_STATE_DONE);
		vin->buf_hw[slot].buffer = NULL;
	} else {
		/* Scratch buffer was used, dropping frame. */
		vin_dbg(vin, "Dropping frame %u\n", vin->sequence);
	}

	vin->sequence++;

	/* Prepare for next frame */
	vin_fill_hw_slot(vin, slot);

done:
	spin_unlock_irqrestore(&vin->qlock, flags);

	/* Clean Frame End interrupt */
	interupt = interupt & 0x00001110;		/* Clean FIELD_END_INT */
	csiiw_writel(vin, CSIIW_INTERRUPT, interupt);

	/* Clean csiiw_debug_info register */
	debbug_info = 0x0000000c;				/* Clean CMD_ERROR and LB_FATAL */
	csiiw_writel(vin, CSIIW_DEBUG_INFO, debbug_info);

	return IRQ_RETVAL(handled);
}

/* Need to hold qlock before calling */
static void return_all_buffers(struct vin_dev *vin,
			       enum vb2_buffer_state state)
{
	struct vin_buffer *buf, *node;
	struct vb2_v4l2_buffer *freed[HW_BUFFER_NUM];
	unsigned int i, n;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	for (i = 0; i < HW_BUFFER_NUM; i++) {
		freed[i] = vin->buf_hw[i].buffer;
		vin->buf_hw[i].buffer = NULL;

		for (n = 0; n < i; n++) {
			if (freed[i] == freed[n]) {
				freed[i] = NULL;
				break;
			}
		}

		if (freed[i])
			vb2_buffer_done(&freed[i]->vb2_buf, state);
	}

	list_for_each_entry_safe(buf, node, &vin->buf_list, list) {
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}
}

static int vin_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
			    unsigned int *nplanes, unsigned int sizes[],
			    struct device *alloc_devs[])

{
	struct vin_dev *vin = vb2_get_drv_priv(vq);

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Make sure the image size is large enough. */
	if (*nplanes)
		return sizes[0] < vin->format.sizeimage ? -EINVAL : 0;

	*nplanes = 1;
	sizes[0] = vin->format.sizeimage;

	dev_dbg(vin->dev, "nbuffers=%d, size=%d\n", *nbuffers, sizes[0]);

	return 0;
};

static int vin_buffer_prepare(struct vb2_buffer *vb)
{
	struct vin_dev *vin = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size = vin->format.sizeimage;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	if (vb2_plane_size(vb, 0) < size) {
		vin_err(vin, "buffer too small (%lu < %lu)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void vin_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vin_dev *vin = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	spin_lock_irqsave(&vin->qlock, flags);

	list_add_tail(to_buf_list(vbuf), &vin->buf_list);

	spin_unlock_irqrestore(&vin->qlock, flags);
}

static int vin_mc_validate_format(struct vin_dev *vin, struct v4l2_subdev *sd,
				   struct media_pad *pad)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(vin->dev, "%s, pad->index: %d\n", __func__, pad->index);

	fmt.pad = pad->index;
	if (v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt))
		return -EPIPE;

	dev_dbg(vin->dev, "%s, fmt.format.code: 0x%04x, fmt.format.field: %d\n",
		__func__, fmt.format.code, fmt.format.field);

	switch (fmt.format.code) {
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_UYVY10_2X10:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		if (vin->format.pixelformat != V4L2_PIX_FMT_SBGGR8)
			return -EPIPE;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		if (vin->format.pixelformat != V4L2_PIX_FMT_SGBRG8)
			return -EPIPE;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		if (vin->format.pixelformat != V4L2_PIX_FMT_SGRBG8)
			return -EPIPE;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		if (vin->format.pixelformat != V4L2_PIX_FMT_SRGGB8)
			return -EPIPE;
		break;
	default:
		return -EPIPE;
	}
	vin->mbus_code = fmt.format.code;

	switch (fmt.format.field) {
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_NONE:
	case V4L2_FIELD_INTERLACED_TB:
	case V4L2_FIELD_INTERLACED_BT:
	case V4L2_FIELD_INTERLACED:
	case V4L2_FIELD_SEQ_TB:
	case V4L2_FIELD_SEQ_BT:
		/* Supported natively */
		break;
	case V4L2_FIELD_ALTERNATE:
		switch (vin->format.field) {
		case V4L2_FIELD_TOP:
		case V4L2_FIELD_BOTTOM:
		case V4L2_FIELD_NONE:
		case V4L2_FIELD_ALTERNATE:
			break;
		case V4L2_FIELD_INTERLACED_TB:
		case V4L2_FIELD_INTERLACED_BT:
		case V4L2_FIELD_INTERLACED:
		case V4L2_FIELD_SEQ_TB:
		case V4L2_FIELD_SEQ_BT:
			/* Use VIN hardware to combine the two fields */
			fmt.format.height *= 2;
			break;
		default:
			return -EPIPE;
		}
		break;
	default:
		return -EPIPE;
	}

	if (fmt.format.width != vin->format.width ||
	    fmt.format.height != vin->format.height ||
	    fmt.format.code != vin->mbus_code)
		return -EPIPE;

	return 0;
}

static int vin_set_stream(struct vin_dev *vin, int on)
{
	struct media_pipeline *pipe;
	struct media_device *mdev;
	struct v4l2_subdev *sd;
	struct media_pad *pad;
	int ret;

	dev_dbg(vin->dev, "%s, %d, on: %d\n", __func__, __LINE__, on);

	pad = media_entity_remote_pad(&vin->pad);
	if (!pad)
		return -EPIPE;

	sd = media_entity_to_v4l2_subdev(pad->entity);

	if (!on) {
		media_pipeline_stop(&vin->vdev.entity);
		return v4l2_subdev_call(sd, video, s_stream, 0);
	}

	ret = vin_mc_validate_format(vin, sd, pad);
	if (ret)
		return ret;

	dev_dbg(vin->dev, "%s, sd->entity.pipe: 0x%px\n", __func__, sd->entity.pipe);

	/*
	 * The graph lock needs to be taken to protect concurrent
	 * starts of multiple VIN instances as they might share
	 * a common subdevice down the line and then should use
	 * the same pipe.
	 */
	mdev = vin->vdev.entity.graph_obj.mdev;
	mutex_lock(&mdev->graph_mutex);
	pipe = sd->entity.pipe ? sd->entity.pipe : &vin->vdev.pipe;
	ret = __media_pipeline_start(&vin->vdev.entity, pipe);
	mutex_unlock(&mdev->graph_mutex);
	if (ret)
		return ret;

	dev_dbg(vin->dev, "%s, %d, ret: %d\n", __func__, __LINE__, ret);

	ret = v4l2_subdev_call(sd, video, s_stream, 1);
	if (ret == -ENOIOCTLCMD)
		ret = 0;
	if (ret)
		media_pipeline_stop(&vin->vdev.entity);

	dev_dbg(vin->dev, "%s, %d, ret: %d\n", __func__, __LINE__, ret);

	return ret;
}

static int vin_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct vin_dev *vin = vb2_get_drv_priv(vq);
	unsigned long flags;
	int ret;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Allocate scratch buffer. */
	vin->scratch = dma_alloc_coherent(vin->dev, vin->format.sizeimage,
					  &vin->scratch_phys, GFP_KERNEL);
	if (!vin->scratch) {
		spin_lock_irqsave(&vin->qlock, flags);
		return_all_buffers(vin, VB2_BUF_STATE_QUEUED);
		spin_unlock_irqrestore(&vin->qlock, flags);
		vin_err(vin, "Failed to allocate scratch buffer\n");
		return -ENOMEM;
	}

	ret = vin_set_stream(vin, 1);
	if (ret) {
		spin_lock_irqsave(&vin->qlock, flags);
		return_all_buffers(vin, VB2_BUF_STATE_QUEUED);
		spin_unlock_irqrestore(&vin->qlock, flags);
		goto out;
	}

	spin_lock_irqsave(&vin->qlock, flags);

	vin->sequence = 0;

	ret = vin_capture_start(vin);
	if (ret) {
		return_all_buffers(vin, VB2_BUF_STATE_QUEUED);
		vin_set_stream(vin, 0);
	}

	spin_unlock_irqrestore(&vin->qlock, flags);
out:
	dev_dbg(vin->dev, "%s, %d, ret: %d\n", __func__, __LINE__, ret);

	if (ret)
		dma_free_coherent(vin->dev, vin->format.sizeimage, vin->scratch,
				  vin->scratch_phys);

	return ret;
}

static void vin_stop_streaming(struct vb2_queue *vq)
{
	struct vin_dev *vin = vb2_get_drv_priv(vq);
	unsigned long flags;
	int retries = 0;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	spin_lock_irqsave(&vin->qlock, flags);

	vin->state = STOPPING;

	/* Wait for streaming to stop */
	while (retries++ < VIN_RETRIES) {

		vin_capture_stop(vin);

		/* Check if HW is stopped */
		vin->state = STOPPED;
		break;

		spin_unlock_irqrestore(&vin->qlock, flags);
		msleep(VIN_TIMEOUT_MS);
		spin_lock_irqsave(&vin->qlock, flags);
	}

	if (vin->state != STOPPED) {
		/*
		 * If this happens something have gone horribly wrong.
		 * Set state to stopped to prevent the interrupt handler
		 * to make things worse...
		 */
		vin_err(vin, "Failed stop HW, something is seriously broken\n");
		vin->state = STOPPED;
	}

	/* Release all active buffers */
	return_all_buffers(vin, VB2_BUF_STATE_ERROR);

	spin_unlock_irqrestore(&vin->qlock, flags);

	vin_set_stream(vin, 0);

	/* disable interrupts */

	/* Free scratch buffer. */
	dma_free_coherent(vin->dev, vin->format.sizeimage, vin->scratch,
			  vin->scratch_phys);
}

static const struct vb2_ops vin_qops = {
	.queue_setup		= vin_queue_setup,
	.buf_prepare		= vin_buffer_prepare,
	.buf_queue		= vin_buffer_queue,
	.start_streaming	= vin_start_streaming,
	.stop_streaming		= vin_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

void vin_dma_unregister(struct vin_dev *vin)
{
	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	mutex_destroy(&vin->lock);

	v4l2_device_unregister(&vin->v4l2_dev);
}

int vin_dma_register(struct vin_dev *vin, int fs_irq, int fe_irq)
{
	struct vb2_queue *q = &vin->queue;
	int i, ret;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);

	/* Initialize the top-level structure */
	ret = v4l2_device_register(vin->dev, &vin->v4l2_dev);
	if (ret)
		return ret;

	mutex_init(&vin->lock);
	INIT_LIST_HEAD(&vin->buf_list);

	spin_lock_init(&vin->qlock);

	vin->state = STOPPED;

	for (i = 0; i < HW_BUFFER_NUM; i++)
		vin->buf_hw[i].buffer = NULL;

	/* buffer queue */
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_READ | VB2_DMABUF;
	q->lock = &vin->lock;
	q->drv_priv = vin;
	q->buf_struct_size = sizeof(struct vin_buffer);
	q->ops = &vin_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 4;
	q->dev = vin->dev;

	ret = vb2_queue_init(q);
	if (ret < 0) {
		vin_err(vin, "failed to initialize VB2 queue\n");
		goto error;
	}

	/* irq */
	ret = devm_request_irq(vin->dev, fs_irq, vin_fs_irq, IRQF_SHARED,
				   KBUILD_MODNAME, vin);
	if (ret) {
		vin_err(vin, "failed to request fs irq\n");
		goto error;
	}

	ret = devm_request_irq(vin->dev, fe_irq, vin_fe_irq, IRQF_SHARED,
				   KBUILD_MODNAME, vin);
	if (ret) {
		vin_err(vin, "failed to request fe irq\n");
		goto error;
	}

	csiiw_init(vin);

	return 0;
error:
	vin_dma_unregister(vin);

	return ret;
}

/* -----------------------------------------------------------------------------
 * Gen3 CHSEL manipulation
 */

/*
 * There is no need to have locking around changing the routing
 * as it's only possible to do so when no VIN in the group is
 * streaming so nothing can race with the VNMC register.
 */
int vin_set_channel_routing(struct vin_dev *vin, u8 chsel)
{
	int ret;

	dev_dbg(vin->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(vin->dev, "%s, chsel: %d\n", __func__, chsel);

	ret = pm_runtime_get_sync(vin->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(vin->dev);
		return ret;
	}

	// Configure registers for routing

	pm_runtime_put(vin->dev);

	return 0;
}

void vin_set_alpha(struct vin_dev *vin, unsigned int alpha)
{
	unsigned long flags;

	spin_lock_irqsave(&vin->qlock, flags);

	vin->alpha = alpha;

	if (vin->state == STOPPED)
		goto out;

	switch (vin->format.pixelformat) {
	case V4L2_PIX_FMT_ARGB555:
		// Congirure registers for alpha
		break;
	case V4L2_PIX_FMT_ABGR32:
		// Congirure registers for alpha
		break;
	default:
		goto out;
	}

out:
	spin_unlock_irqrestore(&vin->qlock, flags);
}
