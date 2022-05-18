// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 *
 * Sunplus MIPI/CSI RX Driver
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#ifdef CONFIG_PM_RUNTIME_MIPI
#include <linux/pm_runtime.h>
#endif
#include <linux/random.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-rect.h>
#include <media/videobuf-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#include "sp-mipi-q645n.h"

#define bytes_per_line(pixel, bpp)	(ALIGN(pixel * bpp, 16))

static unsigned int allocator;
module_param(allocator, uint, 0444);
MODULE_PARM_DESC(allocator, " memory allocator selection, default is 0.\n"
							"\t    0 == dma-contig\n"
							"\t    1 == vmalloc");

static int video_nr = -1;
module_param(video_nr, int, 0444);
MODULE_PARM_DESC(video_nr, " videoX start number, -1 is autodetect");

static inline struct sp_mipi_device *notifier_to_mipi(struct v4l2_async_notifier *n)
{
	return container_of(n, struct sp_mipi_device, notifier);
}

/* Register accessors */
static inline u32 csi_readl(struct sp_mipi_device *mipi, u32 reg)
{
	return readl(mipi->mipicsi_regs + reg);
}

static inline void csi_writel(struct sp_mipi_device *mipi, u32 reg, u32 value)
{
	writel(value, mipi->mipicsi_regs + reg);
}

static inline u32 iw_readl(struct sp_mipi_device *mipi, u32 reg)
{
	return readl(mipi->csiiw_regs + reg);
}

static inline void iw_writel(struct sp_mipi_device *mipi, u32 reg, u32 value)
{
	writel(value, mipi->csiiw_regs + reg);
}

static inline void set_field(u32 *valp, u32 field, u32 mask)
{
	u32 val = *valp;

	val &= ~mask;
	val |= (field << __ffs(mask)) & mask;
	*valp = val;
}

static int sp_get_register_base(struct platform_device *pdev,
				     void **membase, const char *res_name, u8 share)
{
	struct resource *r;
	void __iomem *p;

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if (r == NULL) {
		dev_err(&pdev->dev, "platform_get_resource_byname failed!\n");
		return -ENODEV;
	}

	if (share) {
		p = devm_ioremap(&pdev->dev, r->start, (r->end - r->start + 1));
		if (IS_ERR(p)) {
			dev_err(&pdev->dev, "devm_ioremap failed!\n");
			return PTR_ERR(p);
		}
	} else {
		p = devm_ioremap_resource(&pdev->dev, r);
		if (IS_ERR(p)) {
			dev_err(&pdev->dev, "devm_ioremap_resource failed!\n");
			return PTR_ERR(p);
		}
	}

	*membase = p;

	return 0;
}

static void sp_ana_macro_cfg(struct sp_mipi_device *mipi, u8 on)
{
	void __iomem *reg_addr = mipi->moon3_regs + 0x64;
	u32 val;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/* MIPI-CSI0/2 ports share analog macros with CPIO. Set G3.25
	 * register to swich the analog macros for MIPI-CSI0/2 ports.
	 */
	if ((mipi->id == 0) || (mipi->id == 2)) {
		val = readl(reg_addr);
		if (mipi->id == 0)
			set_field(&val, on, 0x1<<14);		/* MO_EN_MIPI0_RX */
		else
			set_field(&val, on, 0x1<<15);		/* MO_EN_MIPI2_RX */
		val = val | 0xffff0000;
		writel(val, reg_addr);
		dev_info(mipi->dev, "Enable ANA macro for MIPI-CSI%d\n", mipi->id);
	}
}

static void sp_mipicsi_lane_config(struct sp_mipi_device *mipi)
{
	u32 mix_cfg, dphy_cfg0;
	u8 num_lanes = mipi->bus.num_data_lanes;

	mix_cfg = csi_readl(mipi, MIPICSI_MIX_CFG);
	dphy_cfg0 = csi_readl(mipi, MIPICSI_DPHY_CFG0);

	set_field(&mix_cfg, 0x1, 0x1<<15);		/* When detect EOF control word, generate EOF */
#if defined(MIPI_CSI_XTOR)
	set_field(&mix_cfg, 0x0, 0x1<<8);		/* For bit sequence of a ward, transfer LSB bit first */
#else
	set_field(&mix_cfg, 0x1, 0x1<<8);		/* For bit sequence of a ward, transfer MSB bit first */
#endif

	switch (num_lanes) {
	default:
	case 1: /* 1 lane */
		set_field(&mix_cfg, 0x0, 0x3<<20);	/* 0x0: 1 lane */
		set_field(&dphy_cfg0, 0x1, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0 */
		break;

	case 2: /* 2 lanes */
		set_field(&mix_cfg, 0x1, 0x3<<20);	/* 0x1: 2 lanes */
		set_field(&dphy_cfg0, 0x3, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0/1 */
		break;

	case 4: /* 4 lanes */
		set_field(&mix_cfg, 0x2, 0x3<<20);	/* 0x2: 4 lanes */
		set_field(&dphy_cfg0, 0xf, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0/1/2/3 */
		break;
	}

	set_field(&dphy_cfg0, 0x1, 0x1<<2);		/* Enable clock lane of LP mode circuit */
	set_field(&dphy_cfg0, 0x1, 0x1<<0);		/* Set PHY to normal mode */

	dev_dbg(mipi->dev, "%s, %d: mix_cfg: %08x, dphy_cfg0: %08x\n",
		__func__, __LINE__, mix_cfg, dphy_cfg0);

	csi_writel(mipi, MIPICSI_MIX_CFG, mix_cfg);
	csi_writel(mipi, MIPICSI_DPHY_CFG0, dphy_cfg0);
}

static void sp_mipicsi_dt_config(struct sp_mipi_device *mipi)
{
	u32 sol_sync = mipi->sd_format->sol_sync;
	u32 mix_cfg, sync_word;

	mix_cfg = csi_readl(mipi, MIPICSI_MIX_CFG);
	sync_word = csi_readl(mipi, MIPICSI_SOF_SOL_SYNCWORD);

	switch (sol_sync) {
	default:
	case SYNC_RAW8: 		/* 8 bits */
	case SYNC_YUY2:
		set_field(&mix_cfg, 2, 0x3<<16);	/* Raw 8 */
		break;

	case SYNC_RAW10:		/* 10 bits */
		set_field(&mix_cfg, 1, 0x3<<16);	/* Raw 10 */
		break;

	case SYNC_RAW12:		/* 12 bits */
		set_field(&mix_cfg, 0, 0x3<<16);	/* Raw 12 */
		break;
	}

	/* Set SOL_SYNCOWRD field */
#if defined(MIPI_CSI_XTOR)
	set_field(&sync_word, 0x00, 0xffff<<16);	/* SOF_SYNCWORD */
	//set_field(&sync_word, 0x2B, 0xffff<<0);	/* SOL_SYNCWORD */
#endif
	set_field(&sync_word, sol_sync, 0xffff<<0);

	dev_dbg(mipi->dev, "%s, %d: mix_cfg: %08x, sync_word: %08x\n",
		__func__, __LINE__, mix_cfg, sync_word);

	csi_writel(mipi, MIPICSI_MIX_CFG, mix_cfg);
	csi_writel(mipi, MIPICSI_SOF_SOL_SYNCWORD, sync_word);
}

static void sp_mipicsi_reset_s2p_ff(struct sp_mipi_device *mipi)
{
	u32 val;

	val = csi_readl(mipi, MIPICSI_DPHY_CFG0);

	dev_dbg(mipi->dev, "%s, %d: dphy_cfg0: %08x\n", __func__, __LINE__, val);

	/* Reset Serial-to-parallel Flip-flop */
	set_field(&val, 0x1, 0x1<<1);		/* RSTS2P = 1: Reset mode */
	udelay(1);
	csi_writel(mipi, MIPICSI_DPHY_CFG0, val);
	udelay(1);
	set_field(&val, 0x0, 0x1<<1);		/* RSTS2P = 0: Normal mode (default) */
	csi_writel(mipi, MIPICSI_DPHY_CFG0, val);

}

#if defined(MIPI_CSI_BIST)
/* BIST_Mode field: Internal pattern format selection
 * 0: Color bar for YUY2 format
 * 1: Border for YUY2 format
 * 2: Gray bar for RAW12 format
 */
#define BIST_MODE_YUY2_PATTERN	0

static void sp_mipicsi_bist_config(struct sp_mipi_device *mipi, u8 dt)
{
	struct v4l2_pix_format *pix = &mipi->fmt.fmt.pix;
	u32 height = pix->height;
	u32 width = pix->width;
	u32 val;
	u8 mode = BIST_MODE_YUY2_PATTERN;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	val = 0;
	if (dt == 0x2b) width = width/2;		/* One pixel is output twice for RAW12 */
	val = csi_readl(mipi, MIPICSI_BIST_CFG1);
	set_field(&val, height, 0xfff<<16);		/* Vertical size of internal pattern. 1080 */
	set_field(&val, width, 0x1fff<<0);		/* Horizontal size of internal pattern. 1920 */
	csi_writel(mipi, MIPICSI_BIST_CFG1, val);

	dev_dbg(mipi->dev, "%s, %d, bist_cfg1: %08x\n", __func__, __LINE__, val);

	val = 0;
	if (dt == 0x2b) mode = 2;				/* Gray bar for RAW12 */	
	val = csi_readl(mipi, MIPICSI_BIST_CFG0);
	set_field(&val, mode, 0x3<<4);		/* Gray bar for RAW12 format */
	set_field(&val, 0x1, 0x1<<1);		/* Use internal clock for BIST */
	set_field(&val, 0x0, 0x1<<0);		/* Disable MIPI internal pattern */
	csi_writel(mipi, MIPICSI_BIST_CFG0, val);

	dev_dbg(mipi->dev, "%s, %d, bist_cfg0: %08x\n", __func__, __LINE__, val);

}

static void sp_mipicsi_bist_control(struct sp_mipi_device *mipi, u8 on)
{
	u32 val = 0;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	val = csi_readl(mipi, MIPICSI_BIST_CFG0);
	set_field(&val, on, 0x1<<0); 			/* Enable BIST function */
	csi_writel(mipi, MIPICSI_BIST_CFG0, val);
}
#endif

static void sp_mipicsi_init(struct sp_mipi_device *mipi)
{
	u32 val;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	val = 0;
	set_field(&val, 0x1, 0x1<<15);		/* When detect EOF control word, generate EOF */
	set_field(&val, 0x1, 0x1<<8);		/* For bit sequence of a ward, transfer MSB bit first */
	csi_writel(mipi, MIPICSI_MIX_CFG, val);

	val = 0;
	set_field(&val, SYNC_YUY2, 0xffff<<0);		/* Set SOL_SYNCOWRD */
	csi_writel(mipi, MIPICSI_SOF_SOL_SYNCWORD, val);

    val = 0;
	set_field(&val, 0x1, 0x1<<8);		/* Enable ECC correction */
	set_field(&val, 0x1, 0x1<<4);		/* Enable ECC check */
	csi_writel(mipi, MIPICSI_ECC_CFG, val);

	val = 0;							/* The other fields are set to default values. */
	set_field(&val, 0x1, 0xf<<4);		/* Enable data lane of LP mode circuit for data lane 0 */
	set_field(&val, 0x1, 0x1<<3);		/* Enable HS mode circuit of PHY according to the power state singal */
	set_field(&val, 0x1, 0x1<<2);		/* Enable clock lane of LP mode circuit */
	set_field(&val, 0x0, 0x1<<1);		/* Set serial-to-parallel flip-flop to normal mode */
	set_field(&val, 0x1, 0x1<<0);		/* Set PHY to normal mode */
	csi_writel(mipi, MIPICSI_DPHY_CFG0, val);

	set_field(&val, 0x1, 0x1<<1);		/* Reset serial-to-parallel flip-flop */
	csi_writel(mipi, MIPICSI_DPHY_CFG0, val);
	udelay(1);
	set_field(&val, 0x0, 0x1<<1);		/* Set serial-to-parallel flip-flop to normal mode */
	csi_writel(mipi, MIPICSI_DPHY_CFG0, val);

	val = 0;
	set_field(&val, 0x1, 0x1<<0);		/* Enable MIPICSI */
	csi_writel(mipi, MIPICSI_ENABLE, val);

#if defined(MIPI_CSI_XTOR)
	val = 0;
	val = csi_readl(mipi, MIPICSI_EOF_EOL_SYNCWORD);
	set_field(&val, 0x03, 0x1fff<<0);		/* EOF_SYNCWORD */
	csi_writel(mipi, MIPICSI_EOF_EOL_SYNCWORD, val);
#endif
}

static void sp_csiiw_dt_config(struct sp_mipi_device *mipi)
{
	u32 sol_sync = mipi->sd_format->sol_sync;
	u32 config0;

	config0 = iw_readl(mipi, CSIIW_CONFIG0);

	switch (sol_sync) {
	default:
	case SYNC_RAW8: 		/* 8 bits */
	case SYNC_YUY2:
		set_field(&config0, 0, 0x3<<4);		/* Source is 8 bits per pixel */
		break;

	case SYNC_RAW10:		/* 10 bits */
		set_field(&config0, 1, 0x3<<4);		/* Source is 10 bits per pixel */
		break;

	case SYNC_RAW12:		/* 12 bits */
#if defined(MIPI_CSI_BIST) && defined(BIST_RAW12_TO_RAW10)
		set_field(&config0, 1, 0x3<<4);		/* Source is 10 bits per pixel */
#else
		set_field(&config0, 2, 0x3<<4);		/* Source is 12 bits per pixel */
#endif
		break;
	}

	dev_dbg(mipi->dev, "%s, %d, config0: %08x\n",
		__func__, __LINE__, config0);

	iw_writel(mipi, CSIIW_CONFIG0, config0);
}

static void sp_csiiw_fs_config(struct sp_mipi_device *mipi)
{
	struct v4l2_pix_format *pix = &mipi->fmt.fmt.pix;
	u32 sol_sync = mipi->sd_format->sol_sync;
	u32 stride, frame_size, val;

	stride = iw_readl(mipi, CSIIW_STRIDE);
	frame_size = iw_readl(mipi, CSIIW_FRAME_SIZE);

	/* Set LINE_STRIDE field of csiiw_stride */
	val = mEXTENDED_ALIGNED(pix->bytesperline, 16);
	set_field(&stride, val, 0x3fff<<0);


	/* Set XLEN field of csiiw_frame_size */
	switch (sol_sync) {
	default:
	case SYNC_YUY2:
		val = pix->bytesperline;
		break;

	case SYNC_RAW8: 		/* 8 bits */
	case SYNC_RAW10:		/* 10 bits */
	case SYNC_RAW12:		/* 12 bits */
		val = pix->width;
		break;
	}
	set_field(&frame_size, val, 0x1fff<<0);

	/* Set YLEN field of csiiw_frame_size */
	set_field(&frame_size, pix->height, 0xfff<<16);

	dev_dbg(mipi->dev, "%s, %d: stride: %08x, frame_size: %08x\n",
		 __func__, __LINE__, stride, frame_size);

	iw_writel(mipi, CSIIW_STRIDE, stride);
	iw_writel(mipi, CSIIW_FRAME_SIZE, frame_size);
}

static void sp_csiiw_wr_dma_addr(struct sp_mipi_device *mipi, dma_addr_t addr)
{
	/* Set BASE_ADDR field of csiiw_base_addr */
	iw_writel(mipi, CSIIW_BASE_ADDR, addr);

}

static void sp_csiiw_enable(struct sp_mipi_device *mipi)
{
	u32 val;

	/* Clean CMD_ERROR and LB_FATAL fields */
	val = 0;
	set_field(&val, 0x1, 0x1<<3);		/* Clean CMD_ERROR flag */
	set_field(&val, 0x1, 0x1<<2);		/* Clean LB_FATAL flag */
	iw_writel(mipi, CSIIW_DEBUG_INFO, val);

	dev_dbg(mipi->dev, "%s, %d: debug_info: %08x\n", __func__, __LINE__, val);

	/* Configure csiiw_interrupt register */
	val = iw_readl(mipi, CSIIW_INTERRUPT);
	val = val & 0x00001111;
	set_field(&val, 0x0, 0x1<<12);		/* Disable Frame End IRQ mask */
	set_field(&val, 0x1, 0x1<<8);		/* Clean Frame End interrupt */
	set_field(&val, 0x1, 0x1<<4);		/* Enable Frame Start IRQ mask */
	set_field(&val, 0x1, 0x1<<0);		/* Clean Frame Start interrupt */
	iw_writel(mipi, CSIIW_INTERRUPT, val);

	dev_dbg(mipi->dev, "%s, %d: interrupt: %08x\n", __func__, __LINE__, val);

	/* Configure csiiw_config0 register */
	val = iw_readl(mipi, CSIIW_CONFIG0);
	set_field(&val, 0x2, 0xf<<12);		/* Bus urgent command threshold */
	set_field(&val, 0x7, 0x7<<8);		/* Bus command queue for rate control */
	set_field(&val, 0x1, 0x1<<0);		/* Enable CSIIW function */
	iw_writel(mipi, CSIIW_CONFIG0, val);

	dev_dbg(mipi->dev, "%s, %d: config0: %08x\n", __func__, __LINE__, val);
}

static void sp_csiiw_disable(struct sp_mipi_device *mipi)
{
	u32 val;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/* Configure csiiw_interrupt register */
	val = iw_readl(mipi, CSIIW_INTERRUPT);
	val = val & 0x00001111;
	set_field(&val, 0x1, 0x1<<12);		/* Enable frame end IRQ mask */
	set_field(&val, 0x1, 0x1<<4);		/* Enable frame start IRQ mask */
	iw_writel(mipi, CSIIW_INTERRUPT, val);

	/* Configure csiiw_config0 register */
	val = iw_readl(mipi, CSIIW_CONFIG0);
	set_field(&val, 0x0, 0x1<<0);		/* Disable CSIIW function */
	iw_writel(mipi, CSIIW_CONFIG0, val);
}

static void sp_csiiw_init(struct sp_mipi_device *mipi)
{
	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	iw_writel(mipi, CSIIW_LATCH_MODE, 0x1);			/* latch mode should be enable before base address setup */
	iw_writel(mipi, CSIIW_STRIDE, 0x500);
	iw_writel(mipi, CSIIW_FRAME_SIZE, 0x3200500);
	iw_writel(mipi, CSIIW_FRAME_BUF, 0x00000100);		/* set offset to trigger DRAM write */

#if defined(PACKED_MODE) /* Enable pack mode. Temporary code. */
	iw_writel(mipi, CSIIW_CONFIG0, 0x12700);		/* Disable csiiw, use packed mode */
#else
	iw_writel(mipi, CSIIW_CONFIG0, 0x02700);		/* Disable csiiw, use unpacked mode */
#endif
	iw_writel(mipi, CSIIW_INTERRUPT, 0x1111);		/* Disable and clean fs_irq and fe_irq */
	iw_writel(mipi, CSIIW_DEBUG_INFO, 0x000c);		/* Clean CMD_ERROR and LB_FATAL */
	iw_writel(mipi, CSIIW_CONFIG2, 0x0001);			/* NO_STRIDE_EN = 1 */
}

static irqreturn_t sp_csiiw_fs_isr(int irq, void *dev_instance)
{
	struct sp_mipi_device *mipi = dev_instance;
	u32 interupt = 0;

	/* Clean Frame Start interrupt */
	interupt = iw_readl(mipi, CSIIW_INTERRUPT);
	interupt = interupt & 0x00001011;		/* Clean Frame Start interrupt */
	iw_writel(mipi, CSIIW_INTERRUPT, interupt);

	if (mipi->streaming) {
	}

	return IRQ_HANDLED;
}

static irqreturn_t sp_csiiw_fe_isr(int irq, void *dev_instance)
{
	struct sp_mipi_device *mipi = dev_instance;
	struct sp_mipi_buffer *next_frm;
	int addr;
	u32 interupt = 0, debbug_info = 0;

	interupt = iw_readl(mipi, CSIIW_INTERRUPT);
	debbug_info = iw_readl(mipi, CSIIW_DEBUG_INFO);

	if (mipi->skip_first_int) {
		mipi->skip_first_int = false;
	}
	else if (mipi->streaming) {
		spin_lock(&mipi->dma_queue_lock);

		/* One frame is just being captured, get the next frame-buffer
		 * from the queue. If no frame-buffer is available in queue,
		 * hold on to the current buffer.
		 */
		if (!list_empty(&mipi->dma_queue))
		{
			/* One video frame is just being captured, if next frame
			 * is available, delete the frame from queue.
			 */
			next_frm = list_entry(mipi->dma_queue.next, struct sp_mipi_buffer, list);
			list_del_init(&next_frm->list);

			/* Set active-buffer to 'next frame'. */
			addr = vb2_dma_contig_plane_dma_addr(&next_frm->vb.vb2_buf, 0);
			sp_csiiw_wr_dma_addr(mipi, addr);		/* base address */

			/* Then, release current frame. */
			mipi->cur_frm->vb.vb2_buf.timestamp = ktime_get_ns();
			mipi->cur_frm->vb.field = mipi->fmt.fmt.pix.field;
			mipi->cur_frm->vb.sequence = mipi->sequence++;
			vb2_buffer_done(&mipi->cur_frm->vb.vb2_buf, VB2_BUF_STATE_DONE);

			/* Finally, move on. */
			mipi->cur_frm = next_frm;
		}

		spin_unlock(&mipi->dma_queue_lock);
	}

	/* Clean Frame End interrupt */
	interupt = interupt & 0x00001110;		/* Clean Frame End interrupt */
	iw_writel(mipi, CSIIW_INTERRUPT, interupt);

	/* Clean csiiw_debug_info register */
	debbug_info = 0x0000000c;		/* Clean CMD_ERROR and LB_FATAL */
	iw_writel(mipi, CSIIW_DEBUG_INFO, debbug_info);

	return IRQ_HANDLED;
}

static int sp_csiiw_irq_init(struct sp_mipi_device *mipi)
{
	int ret;

	mipi->fs_irq = irq_of_parse_and_map(mipi->dev->of_node, 0);
	ret = devm_request_irq(mipi->dev, mipi->fs_irq, sp_csiiw_fs_isr, 0, "csiiw_fs", mipi);
	if (ret)
		goto err_fs_irq;

	mipi->fe_irq = irq_of_parse_and_map(mipi->dev->of_node, 1);
	ret = devm_request_irq(mipi->dev, mipi->fe_irq, sp_csiiw_fe_isr, 0, "csiiw_fe", mipi);
	if (ret)
		goto err_fe_irq;

	return 0;

err_fe_irq:
err_fs_irq:
	dev_err(mipi->dev, "request_irq failed (%d)!\n", ret);
	return ret;
}

static int sp_mipi_queue_setup(struct vb2_queue *vq,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct sp_mipi_device *mipi = vb2_get_drv_priv(vq);
	unsigned int size = mipi->fmt.fmt.pix.sizeimage;

	if (vq->num_buffers + *nbuffers < MIN_BUFFERS)
		*nbuffers = MIN_BUFFERS - vq->num_buffers;

	if (*nplanes) {
		if (sizes[0] < size)
			return -EINVAL;
		size = sizes[0];
	}

	*nplanes = 1;
	sizes[0] = size;

	dev_dbg(mipi->dev, "nbuffers=%d, size=%d\n", *nbuffers, sizes[0]);

	return 0;
}

static int sp_mipi_buf_prepare(struct vb2_buffer *vb)
{
	struct sp_mipi_device *mipi =  vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sp_mipi_buffer *buf = container_of(vbuf, struct sp_mipi_buffer, vb);
	unsigned long size;

	size = mipi->fmt.fmt.pix.sizeimage;
	if (vb2_plane_size(vb, 0) < size) {
		dev_err(mipi->dev, "%s data will not fit into plane (%lu < %lu)\n",
			__func__, vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, size);
	return 0;
}

static void sp_mipi_buf_queue(struct vb2_buffer *vb)
{
	struct sp_mipi_device *mipi =  vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sp_mipi_buffer *buf = container_of(vbuf, struct sp_mipi_buffer, vb);
	unsigned long flags = 0;

	/* Add the buffer to the DMA queue. */
	spin_lock_irqsave(&mipi->dma_queue_lock, flags);
	list_add_tail(&buf->list, &mipi->dma_queue);
	spin_unlock_irqrestore(&mipi->dma_queue_lock, flags);
}


static struct media_entity *sp_mipi_find_source(struct sp_mipi_device *mipi)
{
	struct media_entity *entity = &mipi->vdev->entity;
	struct media_pad *pad;

	/* Walk searching for entity having no sink */
	while (1) {
		pad = &entity->pads[0];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;

		pad = media_entity_remote_pad(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		entity = pad->entity;
	}

	return entity;
}

static int sp_mipi_pipeline_s_fmt(struct sp_mipi_device *mipi,
			       struct v4l2_subdev_pad_config *pad_cfg,
			       struct v4l2_subdev_format *format)
{
	struct media_entity *entity = &mipi->source->entity;
	struct v4l2_subdev *subdev;
	struct media_pad *sink_pad = NULL;
	struct media_pad *src_pad = NULL;
	struct media_pad *pad = NULL;
	struct v4l2_subdev_format fmt = *format;
	bool found = false;
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

#if defined(MIPI_CSI_BIST)
	*format = fmt;
	return 0;
#endif

	/*
	 * Starting from sensor subdevice, walk within
	 * pipeline and set format on each subdevice
	 */
	while (1) {
		unsigned int i;

		/* Search if current entity has a source pad */
		for (i = 0; i < entity->num_pads; i++) {
			pad = &entity->pads[i];
			if (pad->flags & MEDIA_PAD_FL_SOURCE) {
				src_pad = pad;
				found = true;
				break;
			}
		}
		if (!found)
			break;

		subdev = media_entity_to_v4l2_subdev(entity);

		/* Propagate format on sink pad if any, otherwise source pad */
		if (sink_pad)
			pad = sink_pad;

		dev_dbg(mipi->dev, "\"%s\":%d pad format set to 0x%x %ux%u\n",
			subdev->name, pad->index, format->format.code,
			format->format.width, format->format.height);

		fmt.pad = pad->index;
		ret = v4l2_subdev_call(subdev, pad, set_fmt, pad_cfg, &fmt);
		if (ret < 0) {
			dev_err(mipi->dev, "%s: Failed to set format 0x%x %ux%u on \"%s\":%d pad (%d)\n",
				__func__, format->format.code,
				format->format.width, format->format.height,
				subdev->name, pad->index, ret);
			return ret;
		}

		if (fmt.format.code != format->format.code ||
		    fmt.format.width != format->format.width ||
		    fmt.format.height != format->format.height) {
			dev_dbg(mipi->dev, "\"%s\":%d pad format has been changed to 0x%x %ux%u\n",
				subdev->name, pad->index, fmt.format.code,
				fmt.format.width, fmt.format.height);
		}

		/* Walk to next entity */
		sink_pad = media_entity_remote_pad(src_pad);
		if (!sink_pad || !is_media_entity_v4l2_subdev(sink_pad->entity))
			break;

		entity = sink_pad->entity;
	}
	*format = fmt;

	return 0;
}

static int sp_mipi_pipeline_s_stream(struct sp_mipi_device *mipi, int state)
{
	struct media_entity *entity = &mipi->vdev->entity;
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	int ret;

#if defined(MIPI_CSI_BIST)
	sp_mipicsi_bist_control(mipi, state);
	return 0;
#endif

	/* Start/stop all entities within pipeline */
	while (1) {
		pad = &entity->pads[0];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;

		pad = media_entity_remote_pad(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		entity = pad->entity;
		subdev = media_entity_to_v4l2_subdev(entity);

		ret = v4l2_subdev_call(subdev, video, s_stream, state);
		if (ret < 0 && ret != -ENOIOCTLCMD) {
			dev_err(mipi->dev, "%s: \"%s\" failed to %s streaming (%d)\n",
				__func__, subdev->name,
				state ? "start" : "stop", ret);
			return ret;
		}

		dev_dbg(mipi->dev, "\"%s\" is %s\n",
			subdev->name, state ? "started" : "stopped");
	}

	return 0;
}

static int sp_mipi_pipeline_start(struct sp_mipi_device *mipi)
{
	return sp_mipi_pipeline_s_stream(mipi, 1);
}

static void sp_mipi_pipeline_stop(struct sp_mipi_device *mipi)
{
	sp_mipi_pipeline_s_stream(mipi, 0);
}

static void sp_mipi_release_buffers(struct sp_mipi_device *mipi,
				enum vb2_buffer_state state)
{
	struct sp_mipi_buffer *buf, *tmp;

	/* Release all queued buffers. */
	spin_lock_irq(&mipi->dma_queue_lock);

	list_for_each_entry_safe(buf, tmp, &mipi->dma_queue, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}

	if (mipi->cur_frm) {
		vb2_buffer_done(&mipi->cur_frm->vb.vb2_buf, state);
		mipi->cur_frm = NULL;
	}

	spin_unlock_irq(&mipi->dma_queue_lock);
}

static int sp_mipi_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct sp_mipi_device *mipi = vb2_get_drv_priv(vq);
	unsigned long flags;
	dma_addr_t addr;
	int ret;

	if (mipi->streaming) {
		dev_warn(mipi->dev, "Device has started streaming!\n");
		return -EBUSY;
	}

	ret = media_pipeline_start(&mipi->vdev->entity, &mipi->pipeline);
	if (ret < 0) {
		dev_err(mipi->dev, "%s: Failed to start streaming, media pipeline start error (%d)\n",
			__func__, ret);
		goto error_release_buffers;
	}

	mipi->sequence = 0;

	spin_lock_irqsave(&mipi->dma_queue_lock, flags);

	/* Get the next frame from the dma queue. */
	mipi->cur_frm = list_entry(mipi->dma_queue.next, struct sp_mipi_buffer, list);

	/* Remove buffer from the dma queue. */
	list_del_init(&mipi->cur_frm->list);

	addr = vb2_dma_contig_plane_dma_addr(&mipi->cur_frm->vb.vb2_buf, 0);

	spin_unlock_irqrestore(&mipi->dma_queue_lock, flags);

	sp_mipicsi_lane_config(mipi);
	sp_mipicsi_dt_config(mipi);
#if defined(MIPI_CSI_BIST)
	sp_mipicsi_bist_config(mipi, mipi->sd_format->sol_sync);
#endif
	sp_csiiw_wr_dma_addr(mipi, addr);
	sp_csiiw_fs_config(mipi);
	sp_csiiw_dt_config(mipi);

	/* Reset Serial-to-parallel Flip-flop */
	sp_mipicsi_reset_s2p_ff(mipi);
	sp_csiiw_enable(mipi);

	ret = sp_mipi_pipeline_start(mipi);
	if (ret)
		goto err_media_pipeline_stop;

	mipi->streaming = true;
	mipi->skip_first_int = true;

	dev_dbg(mipi->dev, "%s: cur_frm = %p, addr = %08llx\n", __func__, mipi->cur_frm, addr);

	return 0;

err_media_pipeline_stop:
	media_pipeline_stop(&mipi->vdev->entity);
error_release_buffers:
	sp_mipi_release_buffers(mipi, VB2_BUF_STATE_QUEUED);

	return ret;
}

static void sp_mipi_stop_streaming(struct vb2_queue *vq)
{
	struct sp_mipi_device *mipi = vb2_get_drv_priv(vq);
	struct sp_mipi_buffer *buf;
	unsigned long flags;

	if (!mipi->streaming) {
		dev_warn(mipi->dev, "Device has stopped already!\n");
		return;
	}

	sp_mipi_pipeline_stop(mipi);

	/* FW must mask irq to avoid unmap issue (for test code) */
	sp_csiiw_disable(mipi);

	mipi->streaming = false;

	/* Release all active buffers. */
	spin_lock_irqsave(&mipi->dma_queue_lock, flags);

	if (mipi->cur_frm != NULL)
		vb2_buffer_done(&mipi->cur_frm->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	while (!list_empty(&mipi->dma_queue)) {
		buf = list_entry(mipi->dma_queue.next, struct sp_mipi_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		dev_dbg(mipi->dev, "video buffer #%d is done!\n", buf->vb.vb2_buf.index);
	}

	spin_unlock_irqrestore(&mipi->dma_queue_lock, flags);
}

static const struct vb2_ops sp_video_qops = {
	.queue_setup	= sp_mipi_queue_setup,
	.buf_prepare	= sp_mipi_buf_prepare,
	.buf_queue		= sp_mipi_buf_queue,
	.start_streaming	= sp_mipi_start_streaming,
	.stop_streaming		= sp_mipi_stop_streaming,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish
};

static int sp_mipi_g_fmt_vid_cap(struct file *file, void *priv,
			      struct v4l2_format *fmt)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	*fmt = mipi->fmt;

	return 0;
}

static const struct sp_mipi_format *find_format_by_fourcc(struct sp_mipi_device *mipi,
						       unsigned int fourcc)
{
	unsigned int num_formats = mipi->num_of_sd_formats;
	const struct sp_mipi_format *fmt;
	unsigned int i;

	for (i = 0; i < num_formats; i++) {
		fmt = mipi->sd_formats[i];
		if (fmt->fourcc == fourcc)
			return fmt;
	}

	return NULL;
}

static void __find_outer_frame_size(struct sp_mipi_device *mipi,
				    struct v4l2_pix_format *pix,
				    struct sp_mipi_framesize *framesize)
{
	struct sp_mipi_framesize *match = NULL;
	unsigned int i;
	unsigned int min_err = UINT_MAX;

	for (i = 0; i < mipi->num_of_sd_framesizes; i++) {
		struct sp_mipi_framesize *fsize = &mipi->sd_framesizes[i];
		int w_err = (fsize->width - pix->width);
		int h_err = (fsize->height - pix->height);
		int err = w_err + h_err;

		if (w_err >= 0 && h_err >= 0 && err < min_err) {
			min_err = err;
			match = fsize;
		}
	}
	if (!match)
		match = &mipi->sd_framesizes[0];

	*framesize = *match;
}

static int sp_mipi_try_fmt(struct sp_mipi_device *mipi, struct v4l2_format *f,
			const struct sp_mipi_format **sd_format,
			struct sp_mipi_framesize *sd_framesize)
{
	const struct sp_mipi_format *sd_fmt;
	struct sp_mipi_framesize sd_fsize;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_pad_config pad_cfg;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
	};
	bool do_crop;
	int ret = 0;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(mipi->dev, "mipi->do_crop: %d\n", mipi->do_crop);

	sd_fmt = find_format_by_fourcc(mipi, pix->pixelformat);
	if (!sd_fmt) {
		if (!mipi->num_of_sd_formats)
			return -ENODATA;

		sd_fmt = mipi->sd_formats[mipi->num_of_sd_formats - 1];
		pix->pixelformat = sd_fmt->fourcc;
	}

	/* Limit to hardware capabilities */
	pix->width = clamp(pix->width, MIN_WIDTH, MAX_WIDTH);
	pix->height = clamp(pix->height, MIN_HEIGHT, MAX_HEIGHT);

	/* No crop if JPEG is requested */
	do_crop = mipi->do_crop && (pix->pixelformat != V4L2_PIX_FMT_JPEG);

	if (do_crop && mipi->num_of_sd_framesizes) {
		struct sp_mipi_framesize outer_sd_fsize;
		/*
		 * If crop is requested and sensor have discrete frame sizes,
		 * select the frame size that is just larger than request
		 */
		__find_outer_frame_size(mipi, pix, &outer_sd_fsize);
		pix->width = outer_sd_fsize.width;
		pix->height = outer_sd_fsize.height;
	}

	v4l2_fill_mbus_format(&format.format, pix, sd_fmt->mbus_code);

	ret = v4l2_subdev_call(mipi->source, pad, set_fmt,
			       &pad_cfg, &format);
#if defined(MIPI_CSI_BIST) 
	ret = 0;
#else
	if (ret < 0)
		return ret;
#endif

	/* Update pix regarding to what sensor can do */
	v4l2_fill_pix_format(pix, &format.format);

	/* Save resolution that sensor can actually do */
	sd_fsize.width = pix->width;
	sd_fsize.height = pix->height;

	if (do_crop) {
		struct v4l2_rect c = mipi->crop;
		struct v4l2_rect max_rect;

		/*
		 * Adjust crop by making the intersection between
		 * format resolution request and crop request
		 */
		max_rect.top = 0;
		max_rect.left = 0;
		max_rect.width = pix->width;
		max_rect.height = pix->height;
		v4l2_rect_map_inside(&c, &max_rect);
		c.top  = clamp_t(s32, c.top, 0, pix->height - c.height);
		c.left = clamp_t(s32, c.left, 0, pix->width - c.width);
		mipi->crop = c;

		/* Adjust format resolution request to crop */
		pix->width = mipi->crop.width;
		pix->height = mipi->crop.height;
	}

	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = ALIGN((pix->width * sd_fmt->bpp)/8, 16);
	pix->sizeimage = pix->bytesperline * pix->height;

	if (sd_format)
		*sd_format = sd_fmt;
	if (sd_framesize)
		*sd_framesize = sd_fsize;

	return 0;
}

static int sp_mipi_set_fmt(struct sp_mipi_device *mipi, struct v4l2_format *f)
{
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	const struct sp_mipi_format *sd_format;
	struct sp_mipi_framesize sd_framesize;
	struct v4l2_mbus_framefmt *mf = &format.format;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * Try format, fmt.width/height could have been changed
	 * to match sensor capability or crop request
	 * sd_format & sd_framesize will contain what subdev
	 * can do for this request.
	 */
	ret = sp_mipi_try_fmt(mipi, f, &sd_format, &sd_framesize);
	if (ret)
		return ret;

	/* Disable crop if JPEG is requested or BT656 bus is selected */
	if (pix->pixelformat == V4L2_PIX_FMT_JPEG &&
	    mipi->bus_type != V4L2_MBUS_BT656)
		mipi->do_crop = false;

	/* pix to mbus format */
	v4l2_fill_mbus_format(mf, pix,
			      sd_format->mbus_code);
	mf->width = sd_framesize.width;
	mf->height = sd_framesize.height;

	ret = sp_mipi_pipeline_s_fmt(mipi, NULL, &format);
	if (ret < 0)
		return ret;

	dev_dbg(mipi->dev, "Sensor format set to 0x%x %ux%u\n",
		mf->code, mf->width, mf->height);
	dev_dbg(mipi->dev, "Buffer format set to %4.4s %ux%u\n",
		(char *)&pix->pixelformat, pix->width, pix->height);

	mipi->fmt = *f;
	mipi->sd_format = sd_format;
	mipi->sd_framesize = sd_framesize;

	return 0;
}

static int sp_mipi_s_fmt_vid_cap(struct file *file, void *priv,
			      struct v4l2_format *f)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	if (mipi->streaming) {
		dev_warn(mipi->dev, "Device has started streaming!\n");
		return -EBUSY;
	}

	return sp_mipi_set_fmt(mipi, f);
}

static int sp_mipi_try_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	return sp_mipi_try_fmt(mipi, f, NULL, NULL);
}


static int sp_mipi_enum_fmt_vid_cap(struct file *file, void  *priv,
				 struct v4l2_fmtdesc *f)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	if (f->index >= mipi->num_of_sd_formats)
		return -EINVAL;

	f->pixelformat = mipi->sd_formats[f->index]->fourcc;
	return 0;
}

static int sp_mipi_get_sensor_format(struct sp_mipi_device *mipi,
				  struct v4l2_pix_format *pix)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	int ret;

	ret = v4l2_subdev_call(mipi->source, pad, get_fmt, NULL, &fmt);
	if (ret)
		return ret;

	v4l2_fill_pix_format(pix, &fmt.format);

	return 0;
}

static int sp_mipi_get_sensor_bounds(struct sp_mipi_device *mipi,
				  struct v4l2_rect *r)
{
	struct v4l2_subdev_selection bounds = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.target = V4L2_SEL_TGT_CROP_BOUNDS,
	};
	unsigned int max_width, max_height, max_pixsize;
	struct v4l2_pix_format pix;
	unsigned int i;
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * Get sensor bounds first
	 */
	ret = v4l2_subdev_call(mipi->source, pad, get_selection,
			       NULL, &bounds);
	if (!ret)
		*r = bounds.r;
	if (ret != -ENOIOCTLCMD)
		return ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * If selection is not implemented,
	 * fallback by enumerating sensor frame sizes
	 * and take the largest one
	 */
	max_width = 0;
	max_height = 0;
	max_pixsize = 0;
	for (i = 0; i < mipi->num_of_sd_framesizes; i++) {
		struct sp_mipi_framesize *fsize = &mipi->sd_framesizes[i];
		unsigned int pixsize = fsize->width * fsize->height;

		if (pixsize > max_pixsize) {
			max_pixsize = pixsize;
			max_width = fsize->width;
			max_height = fsize->height;
		}
	}
	if (max_pixsize > 0) {
		r->top = 0;
		r->left = 0;
		r->width = max_width;
		r->height = max_height;
		return 0;
	}

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * If frame sizes enumeration is not implemented,
	 * fallback by getting current sensor frame size
	 */
	ret = sp_mipi_get_sensor_format(mipi, &pix);
	if (ret)
		return ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	r->top = 0;
	r->left = 0;
	r->width = pix.width;
	r->height = pix.height;

	return 0;
}

static int sp_mipi_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	strscpy(cap->driver, CSI_DRV_NAME, sizeof(cap->driver));
	strscpy(cap->card, CSI_DRV_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s", CSI_DRV_NAME);

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/* report capabilities */
	cap->device_caps = mipi->vdev->device_caps;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int sp_mipi_enum_input(struct file *file, void *priv,
			   struct v4l2_input *i)
{
	if (i->index != 0)
		return -EINVAL;

	i->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(i->name, "Camera", sizeof(i->name));
	return 0;
}

static int sp_mipi_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int sp_mipi_s_input(struct file *file, void *priv, unsigned int i)
{
	if (i > 0)
		return -EINVAL;
	return 0;
}

static int sp_mipi_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct sp_mipi_device *mipi = video_drvdata(file);
	const struct sp_mipi_format *sd_fmt;
	struct v4l2_subdev_frame_size_enum fse = {
		.index = fsize->index,
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	int ret;

	sd_fmt = find_format_by_fourcc(mipi, fsize->pixel_format);
	if (!sd_fmt)
		return -EINVAL;

	fse.code = sd_fmt->mbus_code;

	ret = v4l2_subdev_call(mipi->source, pad, enum_frame_size,
			       NULL, &fse);
	if (ret)
		return ret;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = fse.max_width;
	fsize->discrete.height = fse.max_height;

	return 0;
}

static int sp_mipi_g_parm(struct file *file, void *priv,
		       struct v4l2_streamparm *p)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	return v4l2_g_parm_cap(video_devdata(file), mipi->source, p);
}

static int sp_mipi_s_parm(struct file *file, void *priv,
		       struct v4l2_streamparm *p)
{
	struct sp_mipi_device *mipi = video_drvdata(file);

	return v4l2_s_parm_cap(video_devdata(file), mipi->source, p);
}

static int sp_mipi_enum_frameintervals(struct file *file, void *fh,
				    struct v4l2_frmivalenum *fival)
{
	struct sp_mipi_device *mipi = video_drvdata(file);
	const struct sp_mipi_format *sd_fmt;
	struct v4l2_subdev_frame_interval_enum fie = {
		.index = fival->index,
		.width = fival->width,
		.height = fival->height,
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	int ret;

	sd_fmt = find_format_by_fourcc(mipi, fival->pixel_format);
	if (!sd_fmt)
		return -EINVAL;

	fie.code = sd_fmt->mbus_code;

	ret = v4l2_subdev_call(mipi->source, pad,
			       enum_frame_interval, NULL, &fie);
	if (ret)
		return ret;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete = fie.interval;

	return 0;
}

static int sp_mipi_open(struct file *file)
{
	struct sp_mipi_device *mipi = video_drvdata(file);
	int ret;

#ifdef CONFIG_PM_RUNTIME_MIPI
	if (pm_runtime_get_sync(mipi->dev) < 0)
		goto out;
#endif

	mutex_lock(&mipi->lock);

	ret = v4l2_fh_open(file);
	if (ret)
		dev_err(mipi->dev, "v4l2_fh_open failed!\n");

	mutex_unlock(&mipi->lock);
	return ret;

#ifdef CONFIG_PM_RUNTIME_MIPI
out:
	pm_runtime_mark_last_busy(mipi->dev);
	pm_runtime_put_autosuspend(mipi->dev);
	return -ENOMEM;
#endif
}

static int sp_mipi_release(struct file *file)
{
	struct sp_mipi_device *mipi = video_drvdata(file);
	int ret;

	mutex_lock(&mipi->lock);

	/* The release helper will cleanup any on-going streaming. */
	ret = _vb2_fop_release(file, NULL);

	mutex_unlock(&mipi->lock);

#ifdef CONFIG_PM_RUNTIME_MIPI
	pm_runtime_put(mipi->dev);		/* Starting count timeout. */
#endif

	return ret;
}

static const struct v4l2_ioctl_ops sp_mipi_ioctl_ops = {
	.vidioc_querycap		= sp_mipi_querycap,

	.vidioc_try_fmt_vid_cap		= sp_mipi_try_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= sp_mipi_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= sp_mipi_s_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap	= sp_mipi_enum_fmt_vid_cap,

	.vidioc_enum_input		= sp_mipi_enum_input,
	.vidioc_g_input			= sp_mipi_g_input,
	.vidioc_s_input			= sp_mipi_s_input,

	.vidioc_g_parm			= sp_mipi_g_parm,
	.vidioc_s_parm			= sp_mipi_s_parm,

	.vidioc_enum_framesizes		= sp_mipi_enum_framesizes,
	.vidioc_enum_frameintervals	= sp_mipi_enum_frameintervals,

	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_prepare_buf 	= vb2_ioctl_prepare_buf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static const struct v4l2_file_operations sp_mipi_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl 	= video_ioctl2,
	.open		= sp_mipi_open,
	.release	= sp_mipi_release,
	.poll		= vb2_fop_poll,
	.mmap		= vb2_fop_mmap,
	.read		= vb2_fop_read,
};

static const struct vb2_mem_ops *const sp_mem_ops[2] = {
	&vb2_dma_contig_memops,
	&vb2_vmalloc_memops,
};

static int sp_mipi_set_default_fmt(struct sp_mipi_device *mipi)
{
	struct v4l2_format f = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt.pix = {
			.width		= CIF_WIDTH,
			.height		= CIF_HEIGHT,
			.field		= V4L2_FIELD_NONE,
			.pixelformat	= mipi->sd_formats[0]->fourcc,
		},
	};
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	ret = sp_mipi_try_fmt(mipi, &f, NULL, NULL);
	if (ret)
		return ret;
	mipi->sd_format = mipi->sd_formats[0];
	mipi->fmt = f;
	return 0;
}

static const struct sp_mipi_format sp_mipi_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_YUYV,
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.bpp = 16,
		.sol_sync = SYNC_YUY2,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.bpp = 16,
		.sol_sync = SYNC_YUY2,
	}, {
		.fourcc = V4L2_PIX_FMT_YVYU,
		.mbus_code = MEDIA_BUS_FMT_YVYU8_2X8,
		.bpp = 16,
		.sol_sync = SYNC_YUY2,
	}, {
		.fourcc = V4L2_PIX_FMT_VYUY,
		.mbus_code = MEDIA_BUS_FMT_VYUY8_2X8,
		.bpp = 16,
		.sol_sync = SYNC_YUY2,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565,	/* gggbbbbb rrrrrggg */
		.mbus_code = MEDIA_BUS_FMT_RGB565_2X8_LE,
		.bpp = 16,
		.sol_sync = SYNC_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565X,	/* rrrrrggg gggbbbbb */
		.mbus_code = MEDIA_BUS_FMT_RGB565_2X8_BE,
		.bpp = 16,
		.sol_sync = SYNC_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB24,	/* rgb */
		.mbus_code = MEDIA_BUS_FMT_RGB888_2X12_LE,
		.bpp = 24,
		.sol_sync = SYNC_RGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_BGR24,	/* bgr */
		.mbus_code = MEDIA_BUS_FMT_RGB888_2X12_BE,
		.bpp = 24,
		.sol_sync = SYNC_RGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_JPEG,
		.mbus_code = MEDIA_BUS_FMT_JPEG_1X8,
		.bpp = 8,
		.sol_sync = SYNC_RAW8,
	}, {
		.fourcc = V4L2_PIX_FMT_SBGGR8,
		.mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.bpp = 8,
		.sol_sync = SYNC_RAW8,
	}, {
		.fourcc = V4L2_PIX_FMT_SGBRG8,
		.mbus_code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.bpp = 8,
		.sol_sync = SYNC_RAW8,
	}, {
		.fourcc = V4L2_PIX_FMT_SGRBG8,
		.mbus_code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.bpp = 8,
		.sol_sync = SYNC_RAW8,
	}, {
		.fourcc = V4L2_PIX_FMT_SRGGB8,
		.mbus_code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.bpp = 8,
		.sol_sync = SYNC_RAW8,
	}, {
		.fourcc = V4L2_PIX_FMT_SBGGR10,
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.bpp = 10,
		.sol_sync = SYNC_RAW10,
	}, {
		.fourcc = V4L2_PIX_FMT_SGBRG10,
		.mbus_code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.bpp = 10,
		.sol_sync = SYNC_RAW10,
	}, {
		.fourcc = V4L2_PIX_FMT_SGRBG10,
		.mbus_code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.bpp = 10,
		.sol_sync = SYNC_RAW10,
	}, {
		.fourcc = V4L2_PIX_FMT_SRGGB10,
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.bpp = 10,
		.sol_sync = SYNC_RAW10,
	},
};

#if defined(MIPI_CSI_BIST)
static int sp_mipi_formats_init_bist(struct sp_mipi_device *mipi)
{
	const struct sp_mipi_format *sd_fmts[ARRAY_SIZE(sp_mipi_formats)];
	unsigned int num_fmts = 0, i, j;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(sp_mipi_formats); i++) {
		/* Code supported, have we got this fourcc yet? */
		for (j = 0; j < num_fmts; j++)
			if (sd_fmts[j]->fourcc ==
					sp_mipi_formats[i].fourcc) {
				/* Already available */
				dev_dbg(mipi->dev, "Skipping fourcc/code: %4.4s/0x%x\n",
					(char *)&sd_fmts[j]->fourcc,
					sd_fmts[j]->mbus_code);
				break;
			}
		if (j == num_fmts) {
			/* New */
			sd_fmts[num_fmts++] = sp_mipi_formats + i;
			dev_dbg(mipi->dev, "Supported fourcc/code: %4.4s/0x%x\n",
				(char *)&sd_fmts[num_fmts - 1]->fourcc,
				sd_fmts[num_fmts - 1]->mbus_code);
		}
	}

	dev_dbg(mipi->dev, "%s, num_fmts: %d\n", __func__, num_fmts);

	if (!num_fmts)
		return -ENXIO;

	mipi->num_of_sd_formats = num_fmts;
	mipi->sd_formats = devm_kcalloc(mipi->dev,
					num_fmts, sizeof(struct sp_mipi_format *),
					GFP_KERNEL);
	if (!mipi->sd_formats) {
		dev_err(mipi->dev, "Could not allocate memory\n");
		return -ENOMEM;
	}

	memcpy(mipi->sd_formats, sd_fmts,
	       num_fmts * sizeof(struct sp_mipi_format *));
	mipi->sd_format = mipi->sd_formats[0];

	return 0;
}
#endif

static int sp_mipi_formats_init(struct sp_mipi_device *mipi)
{
	const struct sp_mipi_format *sd_fmts[ARRAY_SIZE(sp_mipi_formats)];
	unsigned int num_fmts = 0, i, j;
	struct v4l2_subdev *subdev = mipi->source;
	struct v4l2_subdev_mbus_code_enum mbus_code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	while (!v4l2_subdev_call(subdev, pad, enum_mbus_code,
				 NULL, &mbus_code)) {
		for (i = 0; i < ARRAY_SIZE(sp_mipi_formats); i++) {
			if (sp_mipi_formats[i].mbus_code != mbus_code.code)
				continue;

			/* Exclude JPEG if BT656 bus is selected */
			if (sp_mipi_formats[i].fourcc == V4L2_PIX_FMT_JPEG &&
			    mipi->bus_type == V4L2_MBUS_BT656)
				continue;

			/* Code supported, have we got this fourcc yet? */
			for (j = 0; j < num_fmts; j++)
				if (sd_fmts[j]->fourcc ==
						sp_mipi_formats[i].fourcc) {
					/* Already available */
					dev_dbg(mipi->dev, "Skipping fourcc/code: %4.4s/0x%x\n",
						(char *)&sd_fmts[j]->fourcc,
						mbus_code.code);
					break;
				}
			if (j == num_fmts) {
				/* New */
				sd_fmts[num_fmts++] = sp_mipi_formats + i;
				dev_dbg(mipi->dev, "Supported fourcc/code: %4.4s/0x%x\n",
					(char *)&sd_fmts[num_fmts - 1]->fourcc,
					sd_fmts[num_fmts - 1]->mbus_code);
			}
		}
		mbus_code.index++;
	}

	if (!num_fmts)
		return -ENXIO;

	mipi->num_of_sd_formats = num_fmts;
	mipi->sd_formats = devm_kcalloc(mipi->dev,
					num_fmts, sizeof(struct sp_mipi_format *),
					GFP_KERNEL);
	if (!mipi->sd_formats) {
		dev_err(mipi->dev, "Could not allocate memory\n");
		return -ENOMEM;
	}

	memcpy(mipi->sd_formats, sd_fmts,
	       num_fmts * sizeof(struct sp_mipi_format *));
	mipi->sd_format = mipi->sd_formats[0];

	return 0;
}

static int sp_mipi_framesizes_init(struct sp_mipi_device *mipi)
{
	unsigned int num_fsize = 0;
	struct v4l2_subdev *subdev = mipi->source;
	struct v4l2_subdev_frame_size_enum fse = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.code = mipi->sd_format->mbus_code,
	};
	unsigned int ret;
	unsigned int i;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/* Allocate discrete framesizes array */
	while (!v4l2_subdev_call(subdev, pad, enum_frame_size,
				 NULL, &fse))
		fse.index++;

	num_fsize = fse.index;
	if (!num_fsize)
		return 0;

	mipi->num_of_sd_framesizes = num_fsize;
	mipi->sd_framesizes = devm_kcalloc(mipi->dev, num_fsize,
					   sizeof(struct sp_mipi_framesize),
					   GFP_KERNEL);
	if (!mipi->sd_framesizes) {
		dev_err(mipi->dev, "Could not allocate memory\n");
		return -ENOMEM;
	}

	/* Fill array with sensor supported framesizes */
	dev_dbg(mipi->dev, "Sensor supports %u frame sizes:\n", num_fsize);
	for (i = 0; i < mipi->num_of_sd_framesizes; i++) {
		fse.index = i;
		ret = v4l2_subdev_call(subdev, pad, enum_frame_size,
				       NULL, &fse);
		if (ret)
			return ret;
		mipi->sd_framesizes[fse.index].width = fse.max_width;
		mipi->sd_framesizes[fse.index].height = fse.max_height;
		dev_dbg(mipi->dev, "%ux%u\n", fse.max_width, fse.max_height);
	}

	return 0;
}

static int sp_mipi_graph_notify_complete(struct v4l2_async_notifier *notifier)
{
	struct sp_mipi_device *mipi = notifier_to_mipi(notifier);
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

	/*
	 * Now that the graph is complete,
	 * we search for the source subdevice
	 * in order to expose it through V4L2 interface
	 */
	mipi->source = media_entity_to_v4l2_subdev(sp_mipi_find_source(mipi));
	if (!mipi->source) {
		dev_err(mipi->dev, "Source subdevice not found\n");
		return -ENODEV;
	}

	dev_dbg(mipi->dev, "mipi->source->name: %s\n", mipi->source->name);

	mipi->vdev->ctrl_handler = mipi->source->ctrl_handler;

	ret = sp_mipi_formats_init(mipi);
	if (ret) {
		dev_err(mipi->dev, "No supported mediabus format found\n");
		return ret;
	}

	ret = sp_mipi_framesizes_init(mipi);
	if (ret) {
		dev_err(mipi->dev, "Could not initialize framesizes\n");
		return ret;
	}

	ret = sp_mipi_get_sensor_bounds(mipi, &mipi->sd_bounds);
	if (ret) {
		dev_err(mipi->dev, "Could not get sensor bounds\n");
		return ret;
	}

	ret = sp_mipi_set_default_fmt(mipi);
	if (ret) {
		dev_err(mipi->dev, "Could not set default format\n");
		return ret;
	}

	return 0;
}

static void sp_mipi_graph_notify_unbind(struct v4l2_async_notifier *notifier,
				     struct v4l2_subdev *sd,
				     struct v4l2_async_subdev *asd)
{
	struct sp_mipi_device *mipi = notifier_to_mipi(notifier);

	dev_dbg(mipi->dev, "Removing %s\n", video_device_node_name(mipi->vdev));

	/* Checks internally if vdev has been init or not */
	video_unregister_device(mipi->vdev);
}

static int sp_mipi_graph_notify_bound(struct v4l2_async_notifier *notifier,
				   struct v4l2_subdev *subdev,
				   struct v4l2_async_subdev *asd)
{
	struct sp_mipi_device *mipi = notifier_to_mipi(notifier);
	unsigned int ret;
	int src_pad;

	dev_dbg(mipi->dev, "Subdev \"%s\" bound\n", subdev->name);

	/*
	 * Link this sub-device to DCMI, it could be
	 * a parallel camera sensor or a bridge
	 */
	src_pad = media_entity_get_fwnode_pad(&subdev->entity,
					      subdev->fwnode,
					      MEDIA_PAD_FL_SOURCE);

	dev_dbg(mipi->dev, "subdev->entity: 0x%px\n", &subdev->entity);
	dev_dbg(mipi->dev, "src_pad: %d\n", src_pad);
	dev_dbg(mipi->dev, "&mipi->vdev->entity: 0x%px\n", &mipi->vdev->entity);

	ret = media_create_pad_link(&subdev->entity, src_pad,
				    &mipi->vdev->entity, 0,
				    MEDIA_LNK_FL_IMMUTABLE |
				    MEDIA_LNK_FL_ENABLED);
	if (ret)
		dev_err(mipi->dev, "Failed to create media pad link with subdev \"%s\"\n",
			subdev->name);
	else
		dev_dbg(mipi->dev, "SP MIPI-CSI RX is now linked to \"%s\"\n",
			subdev->name);

	return ret;
}

static const struct v4l2_async_notifier_operations sp_mipi_graph_notify_ops = {
	.bound = sp_mipi_graph_notify_bound,
	.unbind = sp_mipi_graph_notify_unbind,
	.complete = sp_mipi_graph_notify_complete,
};

static int sp_mipi_graph_init(struct sp_mipi_device *mipi)
{
	struct v4l2_async_subdev *asd;
	struct device_node *ep;
	int ret;

	dev_dbg(mipi->dev, "%s, %d\n", __func__, __LINE__);

#if defined(MIPI_CSI_BIST)
	sp_mipi_formats_init_bist(mipi);
	return 0;
#endif

	ep = of_graph_get_next_endpoint(mipi->dev->of_node, NULL);
	if (!ep) {
		dev_err(mipi->dev, "Failed to get next endpoint\n");
		return -EINVAL;
	}

	v4l2_async_notifier_init(&mipi->notifier);

	asd = v4l2_async_notifier_add_fwnode_remote_subdev(
		&mipi->notifier, of_fwnode_handle(ep),
		sizeof(struct v4l2_async_subdev));

	of_node_put(ep);

	if (IS_ERR(asd)) {
		dev_err(mipi->dev, "Failed to add subdev notifier\n");
		return PTR_ERR(asd);
	}

	mipi->notifier.ops = &sp_mipi_graph_notify_ops;

	ret = v4l2_async_notifier_register(&mipi->v4l2_dev, &mipi->notifier);
	if (ret < 0) {
		dev_err(mipi->dev, "Failed to register notifier\n");
		v4l2_async_notifier_cleanup(&mipi->notifier);
		return ret;
	}

	return 0;
}

static int sp_mipi_parse_dt(struct sp_mipi_device *mipi)
{
	struct device_node *np = mipi->dev->of_node;
	struct device_node source_ep_node, *np_source_ep_node;
	struct device_node source_node, *np_source_node;
	struct v4l2_fwnode_endpoint ep = { .bus_type = 0 };
	char data_lanes[V4L2_FWNODE_CSI2_MAX_DATA_LANES * 2];
	unsigned int i;
	u32 id;
	int ret;

	np_source_ep_node = &source_ep_node;
	np_source_node = &source_node;

	/* Make sure MIPI-CSI id is present and sane */
	ret = of_property_read_u32(np, "sunplus,id", &id);
	if (ret) {
		dev_err(mipi->dev, "%pOF: No sunplus,id property found\n", np);
		return -EINVAL;
	}
	mipi->id = id;

#if defined(MIPI_CSI_BIST)
	return 0;
#endif

	/* Get bus characteristics from devicetree */
	np = of_graph_get_next_endpoint(np, NULL);
	if (!np) {
		dev_err(mipi->dev, "Could not find the endpoint\n");
		return -ENODEV;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(np), &ep);
	if (ret) {
		dev_err(mipi->dev, "Could not parse the endpoint\n");
		return ret;
	}

	dev_dbg(mipi->dev, "ep.bus.mipi_csi2.num_data_lanes: %d\n", ep.bus.mipi_csi2.num_data_lanes);

	for (i = 0; i < ep.bus.mipi_csi2.num_data_lanes; i++) {
		unsigned int lane = ep.bus.mipi_csi2.data_lanes[i];

		if (lane > 4) {
			dev_err(mipi->dev, "Invalid position %u for data lane %u\n",
				lane, i);
			ret = -EINVAL;
			goto done;
		}

		data_lanes[i*2] = '0' + lane;
		data_lanes[i*2+1] = ' ';
	}

	data_lanes[i*2-1] = '\0';

	dev_dbg(mipi->dev,
		"CSI-2 bus: clock lane <%u>, data lanes <%s>, flags 0x%08x\n",
		ep.bus.mipi_csi2.clock_lane, data_lanes,
		ep.bus.mipi_csi2.flags);

	/* Store the MIPI-CSI2 bus info */
	memcpy(&mipi->bus, &ep.bus.mipi_csi2, sizeof(struct v4l2_fwnode_bus_mipi_csi2));
	mipi->bus_type = ep.bus_type;

	/* Retrieve the connected device and store it for later use */
	np_source_ep_node = of_graph_get_remote_endpoint(np);
	np_source_node = of_graph_get_port_parent(np_source_ep_node);
	if (!np_source_node) {
		dev_dbg(mipi->dev, "Can't get remote parent\n");
		of_node_put(np_source_ep_node);
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(mipi->dev, "Found connected device %pOFn\n", np_source_node);

done:
	of_node_put(np);
	return ret;
}

/*
 * SP-MIPI driver probe
 */
static int sp_mipi_probe(struct platform_device *pdev)
{
	struct sp_mipi_device *mipi;
	struct device *dev = &pdev->dev;
	struct vb2_queue *q;
	int ret;

	mipi = kzalloc(sizeof(struct sp_mipi_device), GFP_KERNEL);
	if (!mipi) {
		ret = -ENOMEM;
		goto err_kfree_mipi;
	}
	mipi->dev = &pdev->dev;

	platform_set_drvdata(pdev, mipi);

	ret = sp_mipi_parse_dt(mipi);
	if (ret)
		return ret;

	ret = sp_get_register_base(pdev, (void **)&mipi->mipicsi_regs, MIPICSI_REG_NAME, 0);
	if (ret)
		return ret;

	ret = sp_get_register_base(pdev, (void **)&mipi->csiiw_regs, CSIIW_REG_NAME, 0);
	if (ret)
		return ret;

	ret = sp_get_register_base(pdev, (void **)&mipi->moon3_regs, MOON3_REG_NAME, 1);
	if (ret)
		return ret;

	mipi->mipicsi_clk = devm_clk_get(dev, "clk_mipicsi");
	if (IS_ERR(mipi->mipicsi_clk)) {
		ret = PTR_ERR(mipi->mipicsi_clk);
		dev_err(dev, "Failed to retrieve clock resource \'clk_mipicsi\'!\n");
		goto err_kfree_mipi;
	}

	mipi->csiiw_clk = devm_clk_get(dev, "clk_csiiw");
	if (IS_ERR(mipi->csiiw_clk)) {
		ret = PTR_ERR(mipi->csiiw_clk);
		dev_err(dev, "Failed to retrieve clock resource \'clk_csiiw\'!\n");
		goto err_kfree_mipi;
	}

	mipi->mipicsi_rstc = devm_reset_control_get(&pdev->dev, "rstc_mipicsi");
	if (IS_ERR(mipi->mipicsi_rstc)) {
		ret = PTR_ERR(mipi->mipicsi_rstc);
		dev_err(dev, "Failed to retrieve reset controller 'rstc_mipicsi\'!\n");
		goto err_kfree_mipi;
	}

	mipi->csiiw_rstc = devm_reset_control_get(&pdev->dev, "rstc_csiiw");
	if (IS_ERR(mipi->csiiw_rstc)) {
		ret = PTR_ERR(mipi->csiiw_rstc);
		dev_err(dev, "Failed to retrieve reset controller 'rstc_csiiw\'!\n");
		goto err_kfree_mipi;
	}

	mipi->cam_gpio0 = devm_gpiod_get(&pdev->dev, "cam_gpio0", GPIOD_OUT_HIGH);
	if (!IS_ERR(mipi->cam_gpio0)) {
		dev_info(dev, "cam_gpio0 is at G_MX[%d].\n", desc_to_gpio(mipi->cam_gpio0));
		gpiod_set_value(mipi->cam_gpio0, 1);
	}

	mipi->cam_gpio1 = devm_gpiod_get(&pdev->dev, "cam_gpio1", GPIOD_OUT_HIGH);
	if (!IS_ERR(mipi->cam_gpio1)) {
		dev_info(dev, "cam_gpio1 is at G_MX[%d].\n", desc_to_gpio(mipi->cam_gpio1));
		gpiod_set_value(mipi->cam_gpio1, 1);
	}

	ret = clk_prepare_enable(mipi->mipicsi_clk);
	if (ret) {
		dev_err(dev, "Failed to enable \'mipicsi\' clock!\n");
		goto err_kfree_mipi;
	}

	ret = clk_prepare_enable(mipi->csiiw_clk);
	if (ret) {
		dev_err(dev, "Failed to enable \'csiiw\' clock!\n");
		goto err_clk_dis_mipicsi;
	}

	ret = reset_control_deassert(mipi->mipicsi_rstc);
	if (ret) {
		dev_err(dev, "Failed to deassert 'mipicsi' reset controller!\n");
		goto err_clk_dis_csiiw;
	}

	ret = reset_control_deassert(mipi->csiiw_rstc);
	if (ret) {
		dev_err(dev, "Failed to deassert 'csiiw' reset controller!\n");
		goto err_clk_dis_csiiw;
	}

	mipi->v4l2_dev.mdev = &mipi->mdev;

	/* Initialize media device */
	strscpy(mipi->mdev.model, CSI_DRV_NAME, sizeof(mipi->mdev.model));
	snprintf(mipi->mdev.bus_info, sizeof(mipi->mdev.bus_info),
		 "platform:%s", CSI_DRV_NAME);
	mipi->mdev.dev = &pdev->dev;
	media_device_init(&mipi->mdev);

	ret = v4l2_device_register(&pdev->dev, &mipi->v4l2_dev);
	if (ret) {
		dev_err(dev, "Unable to register V4L2 device!\n");
		goto err_clk_dis_csiiw;
	}

	spin_lock_init(&mipi->dma_queue_lock);
	mutex_init(&mipi->lock);

	if (allocator >= ARRAY_SIZE(sp_mem_ops))
		allocator = 0;

	if (allocator == 0)
		dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));

	mipi->vdev = video_device_alloc();
	if (!mipi->vdev) {
		ret = -ENOMEM;
		goto err_device_unregister;
	}

	/* Video node */
	mipi->vdev->fops = &sp_mipi_fops;
	mipi->vdev->v4l2_dev = &mipi->v4l2_dev;
	mipi->vdev->queue = &mipi->buffer_queue;
	strscpy(mipi->vdev->name, CSI_DRV_NAME, sizeof(mipi->vdev->name));
	mipi->vdev->release = video_device_release;
	mipi->vdev->ioctl_ops = &sp_mipi_ioctl_ops;
	mipi->vdev->lock = &mipi->lock;
	mipi->vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
				  V4L2_CAP_READWRITE;
	video_set_drvdata(mipi->vdev, mipi);

	/* Media entity pads */
	mipi->vid_cap_pad.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_pads_init(&mipi->vdev->entity,
					 1, &mipi->vid_cap_pad);
	if (ret) {
		dev_err(mipi->dev, "Failed to init media entity pad\n");
		goto err_device_release;
	}
	mipi->vdev->entity.flags |= MEDIA_ENT_FL_DEFAULT;

	ret = video_register_device(mipi->vdev, VFL_TYPE_VIDEO, video_nr);
	if (ret) {
		dev_err(mipi->dev, "Failed to register video device\n");
		goto err_media_entity_cleanup;
	}

	dev_info(mipi->dev, "Device MIPI-CSI%d registered as %s\n",
		mipi->id, video_device_node_name(mipi->vdev));

	/* Start creating the vb2 queues. */
	q = &mipi->buffer_queue;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
	q->drv_priv = mipi;
	q->buf_struct_size = sizeof(struct sp_mipi_buffer);
	q->ops = &sp_video_qops;
	q->mem_ops = sp_mem_ops[allocator];
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = MIN_BUFFERS;
	q->lock = &mipi->lock;
	q->dev = mipi->v4l2_dev.dev;

	ret = vb2_queue_init(q);
	if (ret) {
		dev_err(dev, "Failed to initialize vb2 queue!\n");
		goto err_media_entity_cleanup;
	}

	/* Initialize dma queues. */
	INIT_LIST_HEAD(&mipi->dma_queue);

	ret = sp_mipi_graph_init(mipi);
	if (ret < 0)
		goto err_media_entity_cleanup;

	sp_ana_macro_cfg(mipi, 1);
	sp_mipicsi_init(mipi);
	sp_csiiw_init(mipi);

	ret = sp_csiiw_irq_init(mipi);
	if (ret)
		goto err_cleanup;

#ifdef CONFIG_PM_RUNTIME_MIPI
	pm_runtime_set_autosuspend_delay(&pdev->dev, 5000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif

	dev_info(dev, "SP MIPI-CSI RX driver probed!!\n");

	return 0;

err_cleanup:
	sp_ana_macro_cfg(mipi, 0);
	v4l2_async_notifier_cleanup(&mipi->notifier);
err_media_entity_cleanup:
	media_entity_cleanup(&mipi->vdev->entity);
err_device_release:
	video_device_release(mipi->vdev);
err_device_unregister:
	v4l2_device_unregister(&mipi->v4l2_dev);
err_clk_dis_csiiw:
	clk_disable_unprepare(mipi->csiiw_clk);
err_clk_dis_mipicsi:
	clk_disable_unprepare(mipi->mipicsi_clk);
err_kfree_mipi:
	kfree(mipi);

	return ret;
}

static int sp_mipi_remove(struct platform_device *pdev)
{
	struct sp_mipi_device *mipi = platform_get_drvdata(pdev);

	sp_ana_macro_cfg(mipi, 0);

	video_device_release(mipi->vdev);
	v4l2_device_unregister(&mipi->v4l2_dev);

	clk_disable_unprepare(mipi->csiiw_clk);
	clk_disable_unprepare(mipi->mipicsi_clk);

	kfree(mipi);

#ifdef CONFIG_PM_RUNTIME_MIPI
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
#endif

	return 0;
}

static int sp_mipi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sp_mipi_device *mipi = platform_get_drvdata(pdev);

	clk_disable(mipi->mipicsi_clk);
	clk_disable(mipi->csiiw_clk);

	return 0;
}

static int sp_mipi_resume(struct platform_device *pdev)
{
	struct sp_mipi_device *mipi = platform_get_drvdata(pdev);
	int ret;

	ret = clk_prepare_enable(mipi->mipicsi_clk);
	if (ret)
		dev_err(mipi->dev, "Failed to enable \'mipicsi\' clock!\n");

	ret = clk_prepare_enable(mipi->csiiw_clk);
	if (ret)
		dev_err(mipi->dev, "Failed to enable \'csiiw\' clock!\n");

	return 0;
}

#ifdef CONFIG_PM_RUNTIME_MIPI
static int sp_mipi_runtime_suspend(struct device *dev)
{
	struct sp_mipi_device *mipi = dev_get_drvdata(dev);

	clk_disable(mipi->mipicsi_clk);
	clk_disable(mipi->csiiw_clk);

	return 0;
}

static int sp_mipi_runtime_resume(struct device *dev)
{
	struct sp_mipi_device *mipi = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(mipi->mipicsi_clk);
	if (ret)
		dev_err(mipi->dev, "Failed to enable \'mipicsi\' clock!\n");

	ret = clk_prepare_enable(mipi->csiiw_clk);
	if (ret)
		dev_err(mipi->dev, "Failed to enable \'csiiw\' clock!\n");

	return 0;
}

static const struct dev_pm_ops sp7021_mipicsi_rx_pm_ops = {
	.runtime_suspend = sp_mipi_runtime_suspend,
	.runtime_resume = sp_mipi_runtime_resume,
};
#endif

static const struct of_device_id sp_mipicsi_rx_of_match[] = {
	{ .compatible = "sunplus,q645-mipicsi-rx", },
	{}
};

static struct platform_driver sp_mipicsi_rx_driver = {
	.probe = sp_mipi_probe,
	.remove = sp_mipi_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = CSI_DRV_NAME,
		.of_match_table = sp_mipicsi_rx_of_match,
#ifdef CONFIG_PM_RUNTIME_MIPI
		.pm = &sp7021_mipicsi_rx_pm_ops,
#endif
	},
	.suspend = sp_mipi_suspend,
	.resume = sp_mipi_resume,
};

module_platform_driver(sp_mipicsi_rx_driver);

MODULE_AUTHOR("Cheng Chung Ho <cc.ho@sunplus.com>");
MODULE_DESCRIPTION("Sunplus MIPI-CSI RX controller driver");
MODULE_LICENSE("GPL v2");
