// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 *
 * Based on the Xilinx DMA driver.
 */
#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include "dmaengine.h"

/* cb-dma platform related configs */
#define CB_DMA_REG_NAME	"cb_dma"

//#define CBDMA_DEBUG_ON
#ifdef CBDMA_DEBUG_ON
#define TAG "[CB-DMA] "
#define CBDMA_LOGE(fmt, ...) pr_err(TAG fmt, ##__VA_ARGS__)
#define CBDMA_LOGW(fmt, ...) pr_warn(TAG fmt, ##__VA_ARGS__)
#define CBDMA_LOGI(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)
#define CBDMA_LOGD(fmt, ...) pr_devel(TAG fmt, ##__VA_ARGS__)
#else
#define CBDMA_LOGE(fmt, ...)  do {} while (0)
#define CBDMA_LOGW(fmt, ...)  do {} while (0)
#define CBDMA_LOGI(fmt, ...)  do {} while (0)
#define CBDMA_LOGD(fmt, ...)  do {} while (0)
#endif

/* Register Offsets */
#define SP_DMA_HW_VER				0x00
#define SP_DMA_CONFIG				0x04
#define SP_DMA_LENGTH				0x08
#define SP_DMA_SRC_ADR				0x0c
#define SP_DMA_DES_ADR				0x10
#define SP_DMA_INT_FLAG				0x14
#define SP_DMA_INT_EN				0x18
#define SP_DMA_MEMSET_VAL			0x1c
#define SP_DMA_SDRAM_SIZE_CONFIG	0x20
#define SP_DMA_ILLEGLE_RECORD		0x24
#define SP_DMA_SG_IDX				0x28
#define SP_DMA_SG_CONFIG			0x2c
#define SP_DMA_SG_LENGTH			0x30
#define SP_DMA_SG_SRC_ADR			0x34
#define SP_DMA_SG_DES_ADR			0x38
#define SP_DMA_SG_MEMSET_VAL		0x3c
#define SP_DMA_SG_EN_GO				0x40
#define SP_DMA_SG_LP_MODE			0x44
#define SP_DMA_SG_LP_SRAM_START		0x48
#define SP_DMA_SG_LP_SRAM_SIZE		0x4c
#define SP_DMA_SG_CHK_MODE			0x50
#define SP_DMA_SG_CHK_SUM			0x54
#define SP_DMA_SG_CHK_XOR			0x58
#define SP_DMA_RSV_23				0x5c
#define SP_DMA_RSV_24				0x60

#define CBDMA_CONFIG_CMD_QUE_DEPTH_1	(0x0 << 16)
#define CBDMA_CONFIG_CMD_QUE_DEPTH_2	(0x1 << 16)
#define CBDMA_CONFIG_CMD_QUE_DEPTH_4	(0x2 << 16)
#define CBDMA_CONFIG_CMD_QUE_DEPTH_8	(0x3 << 16)

#define CBDMA_CONFIG_DEFAULT	CBDMA_CONFIG_CMD_QUE_DEPTH_8
#define CBDMA_CONFIG_GO		(0x01 << 8)
#define CBDMA_CONFIG_MEMSET	(0x00 << 0)
#define CBDMA_CONFIG_WR		(0x01 << 0)
#define CBDMA_CONFIG_RD		(0x02 << 0)
#define CBDMA_CONFIG_CP		(0x03 << 0)

#define CBDMA_INT_FLAG_COPY_ADDR_OVLAP	(1 << 6)
#define CBDMA_INT_FLAG_DAM_SDRAMB_OF	(1 << 5)
#define CBDMA_INT_FLAG_DMA_SDRAMA_OF	(1 << 4)
#define CBDMA_INT_FLAG_DMA_SRAM_OF		(1 << 3)
#define CBDMA_INT_FLAG_CBSRAM_OF		(1 << 2)
#define CBDMA_INT_FLAG_ADDR_CONFLICT	(1 << 1)
#define CBDMA_INT_FLAG_DONE				(1 << 0)

#define CBDMA_SG_CFG_NOT_LAST	(0x00 << 2)
#define CBDMA_SG_CFG_LAST	(0x01 << 2)
#define CBDMA_SG_CFG_MEMSET	(0x00 << 0)
#define CBDMA_SG_CFG_WR		(0x01 << 0)
#define CBDMA_SG_CFG_RD		(0x02 << 0)
#define CBDMA_SG_CFG_CP		(0x03 << 0)
#define CBDMA_SG_EN_GO_EN	(0x01 << 31)
#define CBDMA_SG_EN_GO_GO	(0x01 << 0)
#define CBDMA_SG_LP_MODE_LP	(0x01 << 0)
#define CBDMA_SG_LP_SZ_1KB	(0 << 0)
#define CBDMA_SG_LP_SZ_2KB	(1 << 0)
#define CBDMA_SG_LP_SZ_4KB	(2 << 0)
#define CBDMA_SG_LP_SZ_8KB	(3 << 0)
#define CBDMA_SG_LP_SZ_16KB	(4 << 0)
#define CBDMA_SG_LP_SZ_32KB	(5 << 0)
#define CBDMA_SG_LP_SZ_64KB	(6 << 0)

/* Interrupt flag registers bit field definitions */
#define CBDMA_DMA_DONE				BIT(0)
#define CBDMA_ADDR_CONFLICT			BIT(1)
#define CDDMA_CBSRAM_OVFLOW			BIT(2)
#define CDDMA_DMA_SRAM_OVFLOW		BIT(3)
#define CDDMA_DMA_SDRAMA_OVFLOW		BIT(4)
#define CDDMA_DMA_SDRAMB_OVFLOW		BIT(5)
#define CDDMA_COPY_ADDR_OVLAP		BIT(6)

#define SP_DMA_ALL_INT_MASK	\
		(CBDMA_DMA_DONE | \
		 CBDMA_ADDR_CONFLICT | \
		 CDDMA_CBSRAM_OVFLOW | \
		 CDDMA_DMA_SRAM_OVFLOW | \
		 CDDMA_DMA_SDRAMA_OVFLOW | \
		 CDDMA_DMA_SDRAMB_OVFLOW | \
		 CDDMA_COPY_ADDR_OVLAP)

/* Interrupt flag registers bit field definitions */
#define CBDMA_DMA_DONE_EN			BIT(0)
#define RSV0						BIT(1)
#define CDDMA_CBSRAM_OVFLOW_EN		BIT(2)
#define CDDMA_DMA_SRAM_OVFLOW_EN	BIT(3)
#define CDDMA_DMA_SDRAMA_OVFLOW_EN	BIT(4)
#define CDDMA_DMA_SDRAMB_OVFLOW_EN	BIT(5)
#define CDDMA_COPY_ADDR_OVLAP_EN	BIT(6)

/* Register/Descriptor Offsets */
#define SP_CBDMA_MM2S_CTRL_OFFSET		0x0000
#define SP_CBDMA_S2MM_CTRL_OFFSET		0x0000

/* SP CBDMA Specific Masks/Bit fields */
#define SP_CBDMA_MAX_TRANS_LEN	GENMASK(22, 0)

/* HW specific definitions */
#define SP_DMA_MAX_CHANS_PER_DEVICE	0x20


/* Delay loop counter to prevent hardware failure */
#define SP_DMA_LOOP_COUNT		1000000


/* X DMA Specific Masks/Bit fields */
#define X_DMA_NUM_APP_WORDS		5

/**
 * struct sp_dma_desc_hw - Hardware Descriptor for AXI DMA
 * @next_desc: Next Descriptor Pointer @0x00
 * @next_desc_msb: MSB of Next Descriptor Pointer @0x04
 * @buf_addr: Buffer address @0x08
 * @buf_addr_msb: MSB of Buffer address @0x0C
 * @pad1: Reserved @0x10
 * @pad2: Reserved @0x14
 * @control: Control field @0x18
 * @status: Status field @0x1C
 * @app: APP Fields @0x20 - 0x30
 */
struct sp_dma_desc_hw {
	u32 next_desc;
	u32 next_desc_msb;
	u32 buf_addr;
	u32 buf_addr_msb;
	u32 mcdma_control;
	u32 vsize_stride;
	u32 control;
	u32 status;
	u32 app[X_DMA_NUM_APP_WORDS];
} __aligned(64);

/**
 * struct sp_cdma_desc_hw - Hardware Descriptor
 * @next_desc: Next Descriptor Pointer @0x00
 * @next_descmsb: Next Descriptor Pointer MSB @0x04
 * @src_addr: Source address @0x08
 * @src_addrmsb: Source address MSB @0x0C
 * @dest_addr: Destination address @0x10
 * @dest_addrmsb: Destination address MSB @0x14
 * @control: Control field @0x18
 * @status: Status field @0x1C
 */
struct sp_cdma_desc_hw {
	u32 next_desc;
	u32 next_desc_msb;
	u32 src_addr;
	u32 src_addr_msb;
	u32 dest_addr;
	u32 dest_addr_msb;
	u32 control;
	u32 status;
} __aligned(64);

/**
 * struct sp_dma_tx_segment - Descriptor segment
 * @hw: Hardware descriptor
 * @node: Node in the descriptor segments list
 * @phys: Physical address of segment
 */
struct sp_dma_tx_segment {
	struct sp_dma_desc_hw hw;
	struct list_head node;
	dma_addr_t phys;
} __aligned(64);

/**
 * struct sp_cdma_tx_segment - Descriptor segment
 * @hw: Hardware descriptor
 * @node: Node in the descriptor segments list
 * @phys: Physical address of segment
 */
struct sp_cdma_tx_segment {
	struct sp_cdma_desc_hw hw;
	struct list_head node;
	dma_addr_t phys;
} __aligned(64);

/**
 * struct sp_dma_tx_descriptor - Per Transaction structure
 * @async_tx: Async transaction descriptor
 * @segments: TX segments list
 * @node: Node in the channel descriptors list
 * @cyclic: Check for cyclic transfers.
 */
struct sp_dma_tx_descriptor {
	struct dma_async_tx_descriptor async_tx;
	struct list_head segments;
	struct list_head node;
	bool cyclic;
};

/**
 * struct sp_dma_chan - Driver specific DMA channel structure
 * @xdev: Driver specific device structure
 * @ctrl_offset: Control registers offset
 * @desc_offset: TX descriptor registers offset
 * @lock: Descriptor operation lock
 * @pending_list: Descriptors waiting
 * @active_list: Descriptors ready to submit
 * @done_list: Complete descriptors
 * @common: DMA common channel
 * @desc_pool: Descriptors pool
 * @dev: The dma device
 * @irq: Channel IRQ
 * @id: Channel ID
 * @direction: Transfer direction
 * @num_frms: Number of frames
 * @has_sg: Support scatter transfers
 * @cyclic: Check for cyclic transfers.
 * @genlock: Support genlock mode
 * @err: Channel has errors
 * @tasklet: Cleanup work after irq
 * @config: Device configuration info
 * @flush_on_fsync: Flush on Frame sync
 * @desc_pendingcount: Descriptor pending count
 * @ext_addr: Indicates 64 bit addressing is supported by dma channel
 * @desc_submitcount: Descriptor h/w submitted count
 * @residue: Residue for AXI DMA
 * @seg_v: Statically allocated segments base
 * @cyclic_seg_v: Statically allocated segment base for cyclic transfers
 * @start_transfer: Differentiate b/w DMA IP's transfer
 * @stop_transfer: Differentiate b/w DMA IP's quiesce
 */
struct sp_dma_chan {
	struct sp_dma_device *xdev;
	u32 ctrl_offset;
	u32 desc_offset;
	spinlock_t lock;
	struct list_head pending_list;
	struct list_head active_list;
	struct list_head done_list;
	struct dma_chan common;
	struct dma_pool *desc_pool;
	struct device *dev;
	int irq;
	int id;
	enum dma_transfer_direction direction;
	int num_frms;
	bool has_sg;
	bool cyclic;
	bool genlock;
	bool err;
	struct tasklet_struct tasklet;
	bool flush_on_fsync;
	u32 desc_pendingcount;
	bool ext_addr;
	u32 desc_submitcount;
	u32 residue;
	struct sp_dma_tx_segment *seg_v;
	struct sp_dma_tx_segment *cyclic_seg_v;
	void (*start_transfer)(struct sp_dma_chan *chan);
	int (*stop_transfer)(struct sp_dma_chan *chan);
	u16 tdest;
};

/**
 * enum dma_ip_type - DMA IP type.
 *
 * @DMA_TYPE_DMA: Axi dma ip.
 * @DMA_TYPE_CDMA: Axi cdma ip.
 * @DMA_TYPE_VDMA: Axi vdma ip.
 *
 */
enum dma_ip_type {
	DMA_TYPE_DMA = 0,
	DMA_TYPE_CDMA,
};

struct sp_cbdma_config {
	enum dma_ip_type dmatype;
};

/**
 * struct sp_dma_device - DMA device structure
 * @regs: I/O mapped base address
 * @dev: Device Structure
 * @common: DMA device structure
 * @chan: Driver specific DMA channel
 * @has_sg: Specifies whether Scatter-Gather is present or not
 * @mcdma: Specifies whether Multi-Channel is present or not
 * @flush_on_fsync: Flush on frame sync
 * @ext_addr: Indicates 64 bit addressing is supported by dma device
 * @pdev: Platform device structure pointer
 * @dma_config: DMA config structure
 * @axi_clk: DMA Axi4-lite interace clock
 * @tx_clk: DMA mm2s clock
 * @txs_clk: DMA mm2s stream clock
 * @rx_clk: DMA s2mm clock
 * @rxs_clk: DMA s2mm stream clock
 * @nr_channels: Number of channels DMA device supports
 * @chan_id: DMA channel identifier
 */
struct sp_dma_device {
	void __iomem *regs;
	struct device *dev;
	struct dma_device common;
	struct sp_dma_chan *chan[SP_DMA_MAX_CHANS_PER_DEVICE];
	bool has_sg;
	bool mcdma;
	u32 flush_on_fsync;
	bool ext_addr;
	struct platform_device  *pdev;
	const struct sp_cbdma_config *dma_config;
	struct clk *axi_clk;
	struct clk *tx_clk;
	struct clk *txs_clk;
	struct clk *rx_clk;
	struct clk *rxs_clk;
	u32 nr_channels;
	u32 chan_id;
	u32 sram_addr;
	u32 sram_size;
};

/* Macros */
#define to_sp_dma_chan(chan) \
	container_of(chan, struct sp_dma_chan, common)
#define to_dma_tx_descriptor(tx) \
	container_of(tx, struct sp_dma_tx_descriptor, async_tx)
#define sp_dma_poll_timeout(chan, reg, val, cond, delay_us, timeout_us) \
	readl_poll_timeout(chan->xdev->regs + chan->ctrl_offset + reg, val, \
			   cond, delay_us, timeout_us)

/* IO accessors */
static inline u32 dma_read(struct sp_dma_chan *chan, u32 reg)
{
	return ioread32(chan->xdev->regs + reg);
}

static inline void dma_write(struct sp_dma_chan *chan, u32 reg, u32 value)
{
	iowrite32(value, chan->xdev->regs + reg);
}

static inline void vdma_desc_write(struct sp_dma_chan *chan, u32 reg,
				   u32 value)
{
	dma_write(chan, chan->desc_offset + reg, value);
}

static inline u32 dma_ctrl_read(struct sp_dma_chan *chan, u32 reg)
{
	return dma_read(chan, chan->ctrl_offset + reg);
}

static inline void dma_ctrl_write(struct sp_dma_chan *chan, u32 reg,
				   u32 value)
{
	dma_write(chan, chan->ctrl_offset + reg, value);
}

static inline void dma_ctrl_clr(struct sp_dma_chan *chan, u32 reg,
				 u32 clr)
{
	dma_ctrl_write(chan, reg, dma_ctrl_read(chan, reg) & ~clr);
}

static inline void dma_ctrl_set(struct sp_dma_chan *chan, u32 reg,
				 u32 set)
{
	dma_ctrl_write(chan, reg, dma_ctrl_read(chan, reg) | set);
}

/**
 * vdma_desc_write_64 - 64-bit descriptor write
 * @chan: Driver specific VDMA channel
 * @reg: Register to write
 * @value_lsb: lower address of the descriptor.
 * @value_msb: upper address of the descriptor.
 *
 * Since vdma driver is trying to write to a register offset which is not a
 * multiple of 64 bits(ex : 0x5c), we are writing as two separate 32 bits
 * instead of a single 64 bit register write.
 */
static inline void vdma_desc_write_64(struct sp_dma_chan *chan, u32 reg,
				      u32 value_lsb, u32 value_msb)
{
	CBDMA_LOGI("%s", __func__);

	/* Write the lsb 32 bits*/
	writel(value_lsb, chan->xdev->regs + chan->desc_offset + reg);

	/* Write the msb 32 bits */
	writel(value_msb, chan->xdev->regs + chan->desc_offset + reg + 4);
}

static inline void dma_writeq(struct sp_dma_chan *chan, u32 reg, u64 value)
{
	lo_hi_writeq(value, chan->xdev->regs + chan->ctrl_offset + reg);
}

static inline void sp_dma_buf(struct sp_dma_chan *chan,
				     struct sp_dma_desc_hw *hw,
				     dma_addr_t buf_addr, size_t sg_used,
				     size_t period_len)
{
	CBDMA_LOGI("%s", __func__);

	if (chan->ext_addr) {
		hw->buf_addr = lower_32_bits(buf_addr + sg_used + period_len);
		hw->buf_addr_msb = upper_32_bits(buf_addr + sg_used +
						 period_len);
	} else {
		hw->buf_addr = buf_addr + sg_used + period_len;
	}
}

/* -----------------------------------------------------------------------------
 * Descriptors and segments alloc and free
 */

/**
 * sp_cdma_alloc_tx_segment - Allocate transaction segment
 * @chan: Driver specific DMA channel
 *
 * Return: The allocated segment on success and NULL on failure.
 */
static struct sp_cdma_tx_segment *
sp_cdma_alloc_tx_segment(struct sp_dma_chan *chan)
{
	struct sp_cdma_tx_segment *segment;
	dma_addr_t phys;

	CBDMA_LOGI("%s", __func__);

	segment = dma_pool_zalloc(chan->desc_pool, GFP_ATOMIC, &phys);
	if (!segment)
		return NULL;

	segment->phys = phys;

	return segment;
}

/**
 * sp_dma_alloc_tx_segment - Allocate transaction segment
 * @chan: Driver specific DMA channel
 *
 * Return: The allocated segment on success and NULL on failure.
 */
static struct sp_dma_tx_segment *
sp_dma_alloc_tx_segment(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_segment *segment;
	dma_addr_t phys;

	CBDMA_LOGI("%s", __func__);

	segment = dma_pool_zalloc(chan->desc_pool, GFP_ATOMIC, &phys);
	if (!segment)
		return NULL;

	segment->phys = phys;

	return segment;
}

/**
 * sp_dma_free_tx_segment - Free transaction segment
 * @chan: Driver specific DMA channel
 * @segment: DMA transaction segment
 */
static void sp_dma_free_tx_segment(struct sp_dma_chan *chan,
				struct sp_dma_tx_segment *segment)
{
	CBDMA_LOGI("%s", __func__);

	dma_pool_free(chan->desc_pool, segment, segment->phys);
}

/**
 * sp_cdma_free_tx_segment - Free transaction segment
 * @chan: Driver specific DMA channel
 * @segment: DMA transaction segment
 */
static void sp_cdma_free_tx_segment(struct sp_dma_chan *chan,
				struct sp_cdma_tx_segment *segment)
{
	CBDMA_LOGI("%s", __func__);

	dma_pool_free(chan->desc_pool, segment, segment->phys);
}

/**
 * sp_dma_tx_descriptor - Allocate transaction descriptor
 * @chan: Driver specific DMA channel
 *
 * Return: The allocated descriptor on success and NULL on failure.
 */
static struct sp_dma_tx_descriptor *
sp_dma_alloc_tx_descriptor(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_descriptor *desc;

	CBDMA_LOGI("%s", __func__);

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	INIT_LIST_HEAD(&desc->segments);

	return desc;
}

/**
 * sp_dma_free_tx_descriptor - Free transaction descriptor
 * @chan: Driver specific DMA channel
 * @desc: DMA transaction descriptor
 */
static void
sp_dma_free_tx_descriptor(struct sp_dma_chan *chan,
			       struct sp_dma_tx_descriptor *desc)
{
	struct sp_cdma_tx_segment *cdma_segment, *cdma_next;
	struct sp_dma_tx_segment *axidma_segment, *axidma_next;

	CBDMA_LOGI("%s", __func__);

	if (!desc)
		return;

	if (chan->xdev->dma_config->dmatype == DMA_TYPE_CDMA) {
		list_for_each_entry_safe(cdma_segment, cdma_next,
					 &desc->segments, node) {
			list_del(&cdma_segment->node);
			sp_cdma_free_tx_segment(chan, cdma_segment);
		}
	} else {
		list_for_each_entry_safe(axidma_segment, axidma_next,
					 &desc->segments, node) {
			list_del(&axidma_segment->node);
			sp_dma_free_tx_segment(chan, axidma_segment);
		}
	}

	kfree(desc);
}

/* Required functions */

/**
 * sp_dma_free_desc_list - Free descriptors list
 * @chan: Driver specific DMA channel
 * @list: List to parse and delete the descriptor
 */
static void sp_dma_free_desc_list(struct sp_dma_chan *chan,
					struct list_head *list)
{
	struct sp_dma_tx_descriptor *desc, *next;

	CBDMA_LOGI("%s", __func__);

	list_for_each_entry_safe(desc, next, list, node) {
		list_del(&desc->node);
		sp_dma_free_tx_descriptor(chan, desc);
	}
}

/**
 * sp_dma_free_descriptors - Free channel descriptors
 * @chan: Driver specific DMA channel
 */
static void sp_dma_free_descriptors(struct sp_dma_chan *chan)
{
	unsigned long flags;

	CBDMA_LOGI("%s", __func__);

	spin_lock_irqsave(&chan->lock, flags);

	sp_dma_free_desc_list(chan, &chan->pending_list);
	sp_dma_free_desc_list(chan, &chan->done_list);
	sp_dma_free_desc_list(chan, &chan->active_list);

	spin_unlock_irqrestore(&chan->lock, flags);
}

/**
 * sp_dma_free_chan_resources - Free channel resources
 * @dchan: DMA channel
 */
static void sp_dma_free_chan_resources(struct dma_chan *dchan)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);

	CBDMA_LOGI("%s", __func__);

	dev_dbg(chan->dev, "Free all channel resources.\n");

	sp_dma_free_descriptors(chan);
	if (chan->xdev->dma_config->dmatype == DMA_TYPE_DMA)
		sp_dma_free_tx_segment(chan, chan->cyclic_seg_v);

	dma_pool_destroy(chan->desc_pool);
	chan->desc_pool = NULL;
}

/**
 * sp_dma_chan_handle_cyclic - Cyclic dma callback
 * @chan: Driver specific dma channel
 * @desc: dma transaction descriptor
 * @flags: flags for spin lock
 */
static void sp_dma_chan_handle_cyclic(struct sp_dma_chan *chan,
					  struct sp_dma_tx_descriptor *desc,
					  unsigned long *flags)
{
	dma_async_tx_callback callback;
	void *callback_param;

	CBDMA_LOGI("%s", __func__);

	callback = desc->async_tx.callback;
	callback_param = desc->async_tx.callback_param;
	if (callback) {
		spin_unlock_irqrestore(&chan->lock, *flags);
		callback(callback_param);
		spin_lock_irqsave(&chan->lock, *flags);
	}
}

/**
 * sp_dma_chan_desc_cleanup - Clean channel descriptors
 * @chan: Driver specific DMA channel
 */
static void sp_dma_chan_desc_cleanup(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_descriptor *desc, *next;
	unsigned long flags;

	CBDMA_LOGI("%s", __func__);

	spin_lock_irqsave(&chan->lock, flags);

	list_for_each_entry_safe(desc, next, &chan->done_list, node) {
		struct dmaengine_desc_callback cb;

		if (desc->cyclic) {
			sp_dma_chan_handle_cyclic(chan, desc, &flags);
			break;
		}

		/* Remove from the list of running transactions */
		list_del(&desc->node);

		/* Run the link descriptor callback function */
		dmaengine_desc_get_callback(&desc->async_tx, &cb);
		if (dmaengine_desc_callback_valid(&cb)) {
			spin_unlock_irqrestore(&chan->lock, flags);
			dmaengine_desc_callback_invoke(&cb, NULL);
			spin_lock_irqsave(&chan->lock, flags);
		}

		/* Run any dependencies, then free the descriptor */
		dma_run_dependencies(&desc->async_tx);
		sp_dma_free_tx_descriptor(chan, desc);
	}

	spin_unlock_irqrestore(&chan->lock, flags);
}

/**
 * sp_dma_do_tasklet - Schedule completion tasklet
 * @data: Pointer to the SP DMA channel structure
 */
static void sp_dma_do_tasklet(unsigned long data)
{
	struct sp_dma_chan *chan = (struct sp_dma_chan *)data;

	CBDMA_LOGI("%s", __func__);

	sp_dma_chan_desc_cleanup(chan);
}

/**
 * sp_dma_alloc_chan_resources - Allocate channel resources
 * @dchan: DMA channel
 *
 * Return: '0' on success and failure value on error
 */
static int sp_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);

	CBDMA_LOGI("%s", __func__);

	/* Has this channel already been allocated? */
	if (chan->desc_pool)
		return 0;

	/*
	 * We need the descriptor to be aligned to 64bytes
	 * for meeting X VDMA specification requirement.
	 */
	if (chan->xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		chan->desc_pool = dma_pool_create("sp_dma_desc_pool",
				   chan->dev,
				   sizeof(struct sp_dma_tx_segment),
				   __alignof__(struct sp_dma_tx_segment),
				   0);
	} else if (chan->xdev->dma_config->dmatype == DMA_TYPE_CDMA) {
		chan->desc_pool = dma_pool_create("sp_cdma_desc_pool",
				   chan->dev,
				   sizeof(struct sp_cdma_tx_segment),
				   __alignof__(struct sp_cdma_tx_segment),
				   0);
	} else {
		dev_err(chan->dev, "No support!");
	}

	if (!chan->desc_pool) {
		dev_err(chan->dev,
			"unable to allocate channel %d descriptor pool\n",
			chan->id);
		return -ENOMEM;
	}

	if (chan->xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		/*
		 * For AXI DMA case after submitting a pending_list, keep
		 * an extra segment allocated so that the "next descriptor"
		 * pointer on the tail descriptor always points to a
		 * valid descriptor, even when paused after reaching taildesc.
		 * This way, it is possible to issue additional
		 * transfers without halting and restarting the channel.
		 */
		chan->seg_v = sp_dma_alloc_tx_segment(chan);

		/*
		 * For cyclic DMA mode we need to program the tail Descriptor
		 * register with a value which is not a part of the BD chain
		 * so allocating a desc segment during channel allocation for
		 * programming tail descriptor.
		 */
		chan->cyclic_seg_v = sp_dma_alloc_tx_segment(chan);
	}

	dma_cookie_init(dchan);

	if (chan->xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		/* For AXI DMA resetting once channel will reset the
		 * other channel as well so enable the interrupts here.
		 */
		// Don't execute this X code.
		//dma_ctrl_set(chan, X_DMA_REG_DMACR, X_DMA_DMAXR_ALL_IRQ_MASK);
	}

	// Don't execute this X code.
	//if ((chan->xdev->dma_config->dmatype == DMA_TYPE_CDMA) && chan->has_sg)
		//dma_ctrl_set(chan, X_DMA_REG_DMACR, X_CDMA_CR_SGMODE);

	return 0;
}

/**
 * sp_dma_tx_status - Get DMA transaction status
 * @dchan: DMA channel
 * @cookie: Transaction identifier
 * @txstate: Transaction state
 *
 * Return: DMA transaction status
 */
static enum dma_status sp_dma_tx_status(struct dma_chan *dchan,
					dma_cookie_t cookie,
					struct dma_tx_state *txstate)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	struct sp_dma_tx_descriptor *desc;
	struct sp_dma_tx_segment *segment;
	struct sp_dma_desc_hw *hw;
	enum dma_status ret;
	unsigned long flags;
	u32 residue = 0;

	CBDMA_LOGI("%s", __func__);

	ret = dma_cookie_status(dchan, cookie, txstate);
	if (ret == DMA_COMPLETE || !txstate)
		return ret;

	if (chan->xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		spin_lock_irqsave(&chan->lock, flags);

		desc = list_last_entry(&chan->active_list,
				       struct sp_dma_tx_descriptor, node);
		if (chan->has_sg) {
			list_for_each_entry(segment, &desc->segments, node) {
				hw = &segment->hw;
				residue += (hw->control - hw->status) &
					   SP_CBDMA_MAX_TRANS_LEN;
			}
		}
		spin_unlock_irqrestore(&chan->lock, flags);

		chan->residue = residue;
		dma_set_residue(txstate, chan->residue);
	}

	return ret;
}

/**
 * sp_cbdma_is_running - Check if DMA channel is running
 * @chan: Driver specific DMA channel
 *
 * Return: '1' if running, '0' if not.
 */
static bool sp_cbdma_is_running(struct sp_dma_chan *chan)
{
	return dma_ctrl_read(chan, SP_DMA_CONFIG) & CBDMA_CONFIG_GO;
}

/**
 * sp_cbdma_is_idle - Check if DMA channel is idle
 * @chan: Driver specific DMA channel
 *
 * Return: '1' if idle, '0' if not.
 */
static bool sp_cbdma_is_idle(struct sp_dma_chan *chan)
{
	return !(dma_ctrl_read(chan, SP_DMA_CONFIG) & CBDMA_CONFIG_GO);
}

/**
 * sp_dma_stop_transfer - Halt DMA channel
 * @chan: Driver specific DMA channel
 */
static int sp_dma_stop_transfer(struct sp_dma_chan *chan)
{
	u32 val;

	return sp_dma_poll_timeout(chan, SP_DMA_CONFIG, val, val & ~CBDMA_CONFIG_GO,
						0, SP_DMA_LOOP_COUNT);
}

/**
 * sp_cdma_stop_transfer - Wait for the current transfer to complete
 * @chan: Driver specific DMA channel
 */
static int sp_cdma_stop_transfer(struct sp_dma_chan *chan)
{
	u32 val;

	return sp_dma_poll_timeout(chan, SP_DMA_CONFIG, val, val & ~CBDMA_CONFIG_GO,
						0, SP_DMA_LOOP_COUNT);
}

/**
 * sp_cbdma_start - Start DMA channel
 * @chan: Driver specific DMA channel
 */
static void sp_cbdma_start(struct sp_dma_chan *chan)
{
	u32 config_reg = CBDMA_CONFIG_DEFAULT | CBDMA_CONFIG_GO;

	if (chan->direction == DMA_MEM_TO_MEM)
		config_reg |= CBDMA_CONFIG_CP;
	else if (chan->direction == DMA_MEM_TO_DEV)
		config_reg |= CBDMA_CONFIG_RD;
	else if (chan->direction == DMA_DEV_TO_MEM)
		config_reg |= CBDMA_CONFIG_WR;

	dma_ctrl_set(chan, SP_DMA_CONFIG, config_reg);
}

/**
 * sp_cdma_start_transfer - Starts cdma transfer
 * @chan: Driver specific channel struct pointer
 */
static void sp_cdma_start_transfer(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_descriptor *head_desc, *tail_desc;
	struct sp_cdma_tx_segment *tail_segment;
	//u32 ctrl_reg = dma_read(chan, XILINX_DMA_REG_DMACR);  // Don't execute this X code.

	CBDMA_LOGI("%s", __func__);

	if (chan->err)
		return;

	if (list_empty(&chan->pending_list))
		return;

	head_desc = list_first_entry(&chan->pending_list,
				     struct sp_dma_tx_descriptor, node);
	tail_desc = list_last_entry(&chan->pending_list,
				    struct sp_dma_tx_descriptor, node);
	tail_segment = list_last_entry(&tail_desc->segments,
				       struct sp_cdma_tx_segment, node);

	// Don't execute this X code.
	//if (chan->desc_pendingcount <= X_DMA_COALESCE_MAX) {
		//ctrl_reg &= ~X_DMA_CR_COALESCE_MAX;
		//ctrl_reg |= chan->desc_pendingcount << X_DMA_CR_COALESCE_SHIFT;
		//dma_ctrl_write(chan, X_DMA_REG_DMACR, ctrl_reg);
	//}

	if (chan->has_sg) {
		//x_write(chan, X_DMA_REG_CURDESC, head_desc->async_tx.phys);

		/* Update tail ptr register which will start the transfer */
		//x_write(chan, X_DMA_REG_TAILDESC, tail_segment->phys);
	} else {
		/* In simple mode */
		struct sp_cdma_tx_segment *segment;
		struct sp_cdma_desc_hw *hw;

		segment = list_first_entry(&head_desc->segments,
					   struct sp_cdma_tx_segment,
					   node);

		hw = &segment->hw;

		// Don't execute this X code.
		//x_write(chan, X_CDMA_REG_SRCADDR, hw->src_addr);
		//x_write(chan, X_CDMA_REG_DSTADDR, hw->dest_addr);

		/* Start the transfer */
		//dma_ctrl_write(chan, X_DMA_REG_BTT, hw->control & X_DMA_MAX_TRANS_LEN);

		dma_write(chan, SP_DMA_SRC_ADR, hw->src_addr);
		dma_write(chan, SP_DMA_DES_ADR, hw->dest_addr);
		dma_write(chan, SP_DMA_LENGTH,	hw->control & SP_CBDMA_MAX_TRANS_LEN);
	}

	list_splice_tail_init(&chan->pending_list, &chan->active_list);
	chan->desc_pendingcount = 0;

	sp_cbdma_start(chan);
}

/**
 * sp_dma_start_transfer - Starts DMA transfer
 * @chan: Driver specific channel struct pointer
 */
static void sp_dma_start_transfer(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_descriptor *head_desc, *tail_desc;
	struct sp_dma_tx_segment *tail_segment, *old_head, *new_head;
	//u32 reg;      // Don't execute X code.

	CBDMA_LOGI("%s", __func__);

	if (chan->err)
		return;

	if (list_empty(&chan->pending_list))
		return;

	/* If it is SG mode and hardware is busy, cannot submit */
	if (chan->has_sg && sp_cbdma_is_running(chan) &&
	    !sp_cbdma_is_idle(chan)) {
		dev_dbg(chan->dev, "DMA controller still busy\n");
		return;
	}

	head_desc = list_first_entry(&chan->pending_list,
				     struct sp_dma_tx_descriptor, node);
	tail_desc = list_last_entry(&chan->pending_list,
				    struct sp_dma_tx_descriptor, node);
	tail_segment = list_last_entry(&tail_desc->segments,
				       struct sp_dma_tx_segment, node);

	if (chan->has_sg && !chan->xdev->mcdma) {
		old_head = list_first_entry(&head_desc->segments,
					struct sp_dma_tx_segment, node);
		new_head = chan->seg_v;
		/* Copy Buffer Descriptor fields. */
		new_head->hw = old_head->hw;

		/* Swap and save new reserve */
		list_replace_init(&old_head->node, &new_head->node);
		chan->seg_v = old_head;

		tail_segment->hw.next_desc = chan->seg_v->phys;
		head_desc->async_tx.phys = new_head->phys;
	}

	// Don't execute this X code.
	//reg = dma_ctrl_read(chan, X_DMA_REG_DMACR);

	//if (chan->desc_pendingcount <= X_DMA_COALESCE_MAX) {
		//reg &= ~X_DMA_CR_COALESCE_MAX;
		//reg |= chan->desc_pendingcount << X_DMA_CR_COALESCE_SHIFT;
		//dma_ctrl_write(chan, X_DMA_REG_DMACR, reg);
	//}

	//if (chan->has_sg && !chan->xdev->mcdma)
		//x_write(chan, X_DMA_REG_CURDESC, head_desc->async_tx.phys);

	if (chan->has_sg && chan->xdev->mcdma) {
		if (chan->direction == DMA_MEM_TO_DEV) {
			//dma_ctrl_write(chan, X_DMA_REG_CURDESC, head_desc->async_tx.phys);
		} else {
			if (!chan->tdest) {
				CBDMA_LOGI("%s, %d, set registers\n", __func__, __LINE__);
				//dma_ctrl_write(chan, X_DMA_REG_CURDESC, head_desc->async_tx.phys);
			} else {
				CBDMA_LOGI("%s, %d, set registers\n", __func__, __LINE__);
				//dma_ctrl_write(chan, X_DMA_MCRX_CDESC(chan->tdest), ead_desc->async_tx.phys);
			}
		}
	}

	/* Start the transfer */
	if (chan->has_sg && !chan->xdev->mcdma) {
		//if (chan->cyclic)
			//x_write(chan, X_DMA_REG_TAILDESC, chan->cyclic_seg_v->phys);
		//else
			//x_write(chan, X_DMA_REG_TAILDESC, tail_segment->phys);
	} else if (chan->has_sg && chan->xdev->mcdma) {
		if (chan->direction == DMA_MEM_TO_DEV) {
			//dma_ctrl_write(chan, X_DMA_REG_TAILDESC, tail_segment->phys);
		} else {
			if (!chan->tdest) {
				CBDMA_LOGI("%s, %d, set registers\n", __func__, __LINE__);
				//dma_ctrl_write(chan, X_DMA_REG_TAILDESC, tail_segment->phys);
			} else {
				CBDMA_LOGI("%s, %d, set registers\n", __func__, __LINE__);
				//dma_ctrl_write(chan, X_DMA_MCRX_TDESC(chan->tdest), tail_segment->phys);
			}
		}
	} else {
		struct sp_dma_tx_segment *segment;
		struct sp_dma_desc_hw *hw;

		segment = list_first_entry(&head_desc->segments,
					   struct sp_dma_tx_segment,
					   node);
		hw = &segment->hw;

		if (chan->direction == DMA_MEM_TO_DEV) {
			dma_write(chan, SP_DMA_SRC_ADR, hw->buf_addr);
			dma_write(chan, SP_DMA_DES_ADR, 0);
			dma_write(chan, SP_DMA_LENGTH,  hw->control & SP_CBDMA_MAX_TRANS_LEN);
		} else if (chan->direction == DMA_DEV_TO_MEM) {
			dma_write(chan, SP_DMA_SRC_ADR, 0);
			dma_write(chan, SP_DMA_DES_ADR, hw->buf_addr);
			dma_write(chan, SP_DMA_LENGTH,  hw->control & SP_CBDMA_MAX_TRANS_LEN);
		}
	}

	list_splice_tail_init(&chan->pending_list, &chan->active_list);
	chan->desc_pendingcount = 0;

	sp_cbdma_start(chan);
}

/**
 * sp_dma_issue_pending - Issue pending transactions
 * @dchan: DMA channel
 */
static void sp_dma_issue_pending(struct dma_chan *dchan)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	unsigned long flags;

	CBDMA_LOGI("%s", __func__);

	spin_lock_irqsave(&chan->lock, flags);
	chan->start_transfer(chan);
	spin_unlock_irqrestore(&chan->lock, flags);
}

/**
 * sp_dma_complete_descriptor - Mark the active descriptor as complete
 * @chan : sp DMA channel
 *
 * CONTEXT: hardirq
 */
static void sp_dma_complete_descriptor(struct sp_dma_chan *chan)
{
	struct sp_dma_tx_descriptor *desc, *next;

	CBDMA_LOGI("%s", __func__);

	/* This function was invoked with lock held */
	if (list_empty(&chan->active_list))
		return;

	list_for_each_entry_safe(desc, next, &chan->active_list, node) {
		list_del(&desc->node);
		if (!desc->cyclic)
			dma_cookie_complete(&desc->async_tx);
		list_add_tail(&desc->node, &chan->done_list);
	}
}

/**
 * sp_cbdma_reset - Reset DMA channel
 * @chan: Driver specific DMA channel
 *
 * Return: '0' on success and failure value on error
 */
static int sp_cbdma_reset(struct sp_dma_chan *chan)
{
	int err;
	u32 reg_val, val;

	/* Wait for the hardware to finish */
	err = sp_dma_poll_timeout(chan, SP_DMA_CONFIG, val, val & ~CBDMA_CONFIG_GO,
						0, SP_DMA_LOOP_COUNT);
	if (err)
		dev_err(chan->dev, "wait hardware complettion timeout!\n");

	reg_val = CBDMA_CONFIG_DEFAULT;
	dma_ctrl_set(chan, SP_DMA_CONFIG, reg_val);

	/* Clear the interrupt flags */
	reg_val = 0;
	dma_write(chan, SP_DMA_INT_EN, reg_val);
	reg_val = 0x0000003F;
	dma_write(chan, SP_DMA_INT_FLAG, reg_val);

	chan->err = false;

	return 0;
}

/**
 * sp_cbdma_chan_reset - Reset DMA channel and enable interrupts
 * @chan: Driver specific DMA channel
 *
 * Return: '0' on success and failure value on error
 */
static int sp_cbdma_chan_reset(struct sp_dma_chan *chan)
{
	int err;

	/* Reset VDMA */
	err = sp_cbdma_reset(chan);
	if (err)
		return err;

	/* Enable interrupts */
	dma_ctrl_set(chan, SP_DMA_INT_EN, CBDMA_DMA_DONE_EN);
	return 0;
}

/**
 * sp_cbdma_irq_handler - DMA Interrupt handler
 * @irq: IRQ number
 * @data: Pointer to the SP DMA channel structure
 *
 * Return: IRQ_HANDLED/IRQ_NONE
 */
static irqreturn_t sp_cbdma_irq_handler(int irq, void *data)
{
	struct sp_dma_chan *chan = data;
	u32 int_flag, int_en, status;
	irqreturn_t ret = IRQ_NONE;

	/*
	 * This interrupt service routine will be executed for each channels which have registered the
	 * same interrupt source. So we have to check active_list to know which channel is running.
	 */
	if (list_empty(&chan->active_list)) {
		dev_info(chan->dev, "Channel %p active list is empty", chan);
	} else {
		/* Read the status and ack the interrupts. */
		int_flag = dma_read(chan, SP_DMA_INT_FLAG);
		int_en = dma_read(chan, SP_DMA_INT_EN);
		status = int_flag & int_en;

		if (!status) {
			dev_info(chan->dev, "Channel %p other interrupts are triggerd\n", chan);
		} else {
			/* Clear the interrupt flags */
			dma_ctrl_write(chan, SP_DMA_INT_FLAG, int_flag);

			if (status & CBDMA_INT_FLAG_DONE) {
				dev_info(chan->dev, "Channel %p transfer done\n", chan);
				ret = IRQ_HANDLED;
			}

			if (status & CBDMA_INT_FLAG_DMA_SRAM_OF) {
				dev_info(chan->dev, "Channel %p overflow interrupt\n", chan);
				chan->err = true;
				ret = IRQ_HANDLED;
			}

			if (status & CBDMA_INT_FLAG_DONE) {
				dev_info(chan->dev, "Channel %p complete\n", chan);
				spin_lock(&chan->lock);
				sp_dma_complete_descriptor(chan);
				chan->start_transfer(chan);
				spin_unlock(&chan->lock);
			}

			tasklet_schedule(&chan->tasklet);
		}
	}

	return ret;
}

/**
 * append_desc_queue - Queuing descriptor
 * @chan: Driver specific dma channel
 * @desc: dma transaction descriptor
 */
static void append_desc_queue(struct sp_dma_chan *chan,
			      struct sp_dma_tx_descriptor *desc)
{
	struct sp_dma_tx_descriptor *tail_desc;
	struct sp_dma_tx_segment *axidma_tail_segment;
	struct sp_cdma_tx_segment *cdma_tail_segment;

	CBDMA_LOGI("%s", __func__);

	if (list_empty(&chan->pending_list))
		goto append;

	/*
	 * Add the hardware descriptor to the chain of hardware descriptors
	 * that already exists in memory.
	 */
	tail_desc = list_last_entry(&chan->pending_list,
				    struct sp_dma_tx_descriptor, node);
	if (chan->xdev->dma_config->dmatype == DMA_TYPE_CDMA) {
		cdma_tail_segment = list_last_entry(&tail_desc->segments,
						struct sp_cdma_tx_segment,
						node);
		cdma_tail_segment->hw.next_desc = (u32)desc->async_tx.phys;
	} else {
		axidma_tail_segment = list_last_entry(&tail_desc->segments,
					       struct sp_dma_tx_segment,
					       node);
		axidma_tail_segment->hw.next_desc = (u32)desc->async_tx.phys;
	}

	/*
	 * Add the software descriptor and all children to the list
	 * of pending transactions
	 */
append:
	list_add_tail(&desc->node, &chan->pending_list);
	chan->desc_pendingcount++;
}

/**
 * sp_dma_tx_submit - Submit DMA transaction
 * @tx: Async transaction descriptor
 *
 * Return: cookie value on success and failure value on error
 */
static dma_cookie_t sp_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct sp_dma_tx_descriptor *desc = to_dma_tx_descriptor(tx);
	struct sp_dma_chan *chan = to_sp_dma_chan(tx->chan);
	dma_cookie_t cookie;
	unsigned long flags;
	int err;

	CBDMA_LOGI("%s", __func__);

	if (chan->cyclic) {
		sp_dma_free_tx_descriptor(chan, desc);
		return -EBUSY;
	}

	if (chan->err) {
		/*
		 * If reset fails, need to hard reset the system.
		 * Channel is no longer functional
		 */
		err = sp_cbdma_chan_reset(chan);
		if (err < 0)
			return err;
	}

	spin_lock_irqsave(&chan->lock, flags);

	cookie = dma_cookie_assign(tx);

	/* Put this transaction onto the tail of the pending queue */
	append_desc_queue(chan, desc);

	if (desc->cyclic)
		chan->cyclic = true;

	spin_unlock_irqrestore(&chan->lock, flags);

	return cookie;
}

/**
 * sp_cdma_prep_memcpy - prepare descriptors for a memcpy transaction
 * @dchan: DMA channel
 * @dma_dst: destination address
 * @dma_src: source address
 * @len: transfer length
 * @flags: transfer ack flags
 *
 * Return: Async transaction descriptor on success and NULL on failure
 */
static struct dma_async_tx_descriptor *
sp_cdma_prep_memcpy(struct dma_chan *dchan, dma_addr_t dma_dst,
			dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	struct sp_dma_tx_descriptor *desc;
	struct sp_cdma_tx_segment *segment;
	struct sp_cdma_desc_hw *hw;

	CBDMA_LOGI("%s", __func__);

	if (!len || len > SP_CBDMA_MAX_TRANS_LEN)
		return NULL;

	desc = sp_dma_alloc_tx_descriptor(chan);
	if (!desc)
		return NULL;

	dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
	desc->async_tx.tx_submit = sp_dma_tx_submit;

	/* Allocate the link descriptor from DMA pool */
	segment = sp_cdma_alloc_tx_segment(chan);
	if (!segment)
		goto error;

	hw = &segment->hw;
	hw->control = len;
	hw->src_addr = dma_src;
	hw->dest_addr = dma_dst;
	if (chan->ext_addr) {
		hw->src_addr_msb = upper_32_bits(dma_src);
		hw->dest_addr_msb = upper_32_bits(dma_dst);
	}

	/* Insert the segment into the descriptor segments list. */
	list_add_tail(&segment->node, &desc->segments);


	desc->async_tx.phys = segment->phys;
	hw->next_desc = segment->phys;

	return &desc->async_tx;

error:
	sp_dma_free_tx_descriptor(chan, desc);
	return NULL;
}

/**
 * sp_dma_prep_slave_sg - prepare descriptors for a DMA_SLAVE transaction
 * @dchan: DMA channel
 * @sgl: scatterlist to transfer to/from
 * @sg_len: number of entries in @scatterlist
 * @direction: DMA direction
 * @flags: transfer ack flags
 * @context: APP words of the descriptor
 *
 * Return: Async transaction descriptor on success and NULL on failure
 */
static struct dma_async_tx_descriptor *sp_dma_prep_slave_sg(
	struct dma_chan *dchan, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	struct sp_dma_tx_descriptor *desc;
	struct sp_dma_tx_segment *segment = NULL, *prev = NULL;
	u32 *app_w = (u32 *)context;
	struct scatterlist *sg;
	size_t copy;
	size_t sg_used;
	unsigned int i;

	CBDMA_LOGI("%s", __func__);

	if (!is_slave_direction(direction))
		return NULL;

	/* Allocate a transaction descriptor. */
	desc = sp_dma_alloc_tx_descriptor(chan);
	if (!desc)
		return NULL;

	dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
	desc->async_tx.tx_submit = sp_dma_tx_submit;

	/* Build transactions using information in the scatter gather list */
	for_each_sg(sgl, sg, sg_len, i) {
		sg_used = 0;

		/* Loop until the entire scatterlist entry is used */
		while (sg_used < sg_dma_len(sg)) {
			struct sp_dma_desc_hw *hw;

			/* Get a free segment */
			segment = sp_dma_alloc_tx_segment(chan);
			if (!segment)
				goto error;

			/*
			 * Calculate the maximum number of bytes to transfer,
			 * making sure it is less than the hw limit
			 */
			copy = min_t(size_t, sg_dma_len(sg) - sg_used,
				     SP_CBDMA_MAX_TRANS_LEN);
			hw = &segment->hw;

			/* Fill in the descriptor */
			sp_dma_buf(chan, hw, sg_dma_address(sg),
					  sg_used, 0);

			hw->control = copy;

			if (chan->direction == DMA_MEM_TO_DEV) {
				if (app_w)
					memcpy(hw->app, app_w, sizeof(u32) *
					       X_DMA_NUM_APP_WORDS);
			}

			if (prev)
				prev->hw.next_desc = segment->phys;

			prev = segment;
			sg_used += copy;

			/*
			 * Insert the segment into the descriptor segments
			 * list.
			 */
			list_add_tail(&segment->node, &desc->segments);
		}
	}

	segment = list_first_entry(&desc->segments,
				   struct sp_dma_tx_segment, node);
	desc->async_tx.phys = segment->phys;
	prev->hw.next_desc = segment->phys;

	/* For the last DMA_MEM_TO_DEV transfer, set EOP */
	if (chan->direction == DMA_MEM_TO_DEV) {
		//segment->hw.control |= X_DMA_BD_SOP;
		segment = list_last_entry(&desc->segments,
					  struct sp_dma_tx_segment,
					  node);
		//segment->hw.control |= X_DMA_BD_EOP;
	}

	return &desc->async_tx;

error:
	sp_dma_free_tx_descriptor(chan, desc);
	return NULL;
}

/**
 * sp_dma_prep_dma_cyclic - prepare descriptors for a DMA_SLAVE transaction
 * @chan: DMA channel
 * @sgl: scatterlist to transfer to/from
 * @sg_len: number of entries in @scatterlist
 * @direction: DMA direction
 * @flags: transfer ack flags
 */
static struct dma_async_tx_descriptor *sp_dma_prep_dma_cyclic(
	struct dma_chan *dchan, dma_addr_t buf_addr, size_t buf_len,
	size_t period_len, enum dma_transfer_direction direction,
	unsigned long flags)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	struct sp_dma_tx_descriptor *desc;
	struct sp_dma_tx_segment *segment, *head_segment, *prev = NULL;
	size_t copy, sg_used;
	unsigned int num_periods;
	int i;

	CBDMA_LOGI("%s", __func__);

	if (!period_len)
		return NULL;

	num_periods = buf_len / period_len;

	if (!num_periods)
		return NULL;

	if (!is_slave_direction(direction))
		return NULL;

	/* Allocate a transaction descriptor. */
	desc = sp_dma_alloc_tx_descriptor(chan);
	if (!desc)
		return NULL;

	chan->direction = direction;
	dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
	desc->async_tx.tx_submit = sp_dma_tx_submit;

	for (i = 0; i < num_periods; ++i) {
		sg_used = 0;

		while (sg_used < period_len) {
			struct sp_dma_desc_hw *hw;

			/* Get a free segment */
			segment = sp_dma_alloc_tx_segment(chan);
			if (!segment)
				goto error;

			/*
			 * Calculate the maximum number of bytes to transfer,
			 * making sure it is less than the hw limit
			 */
			copy = min_t(size_t, period_len - sg_used,
				     SP_CBDMA_MAX_TRANS_LEN);
			hw = &segment->hw;
			sp_dma_buf(chan, hw, buf_addr, sg_used,
					  period_len * i);
			hw->control = copy;

			if (prev)
				prev->hw.next_desc = segment->phys;

			prev = segment;
			sg_used += copy;

			/*
			 * Insert the segment into the descriptor segments
			 * list.
			 */
			list_add_tail(&segment->node, &desc->segments);
		}
	}

	head_segment = list_first_entry(&desc->segments,
				   struct sp_dma_tx_segment, node);
	desc->async_tx.phys = head_segment->phys;

	desc->cyclic = true;
	//reg = dma_ctrl_read(chan, X_DMA_REG_DMACR);
	//reg |= X_DMA_CR_CYCLIC_BD_EN_MASK;
	//dma_ctrl_write(chan, X_DMA_REG_DMACR, reg);

	segment = list_last_entry(&desc->segments,
				  struct sp_dma_tx_segment,
				  node);
	segment->hw.next_desc = (u32) head_segment->phys;

	/* For the last DMA_MEM_TO_DEV transfer, set EOP */
	if (direction == DMA_MEM_TO_DEV) {
		//head_segment->hw.control |= X_DMA_BD_SOP;
		//segment->hw.control |= X_DMA_BD_EOP;
	}

	return &desc->async_tx;

error:
	sp_dma_free_tx_descriptor(chan, desc);
	return NULL;
}

/**
 * sp_dma_prep_interleaved - prepare a descriptor for a
 *	DMA_SLAVE transaction
 * @dchan: DMA channel
 * @xt: Interleaved template pointer
 * @flags: transfer ack flags
 *
 * Return: Async transaction descriptor on success and NULL on failure
 */
static struct dma_async_tx_descriptor *
sp_dma_prep_interleaved(struct dma_chan *dchan,
				 struct dma_interleaved_template *xt,
				 unsigned long flags)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	struct sp_dma_tx_descriptor *desc;
	struct sp_dma_tx_segment *segment;
	struct sp_dma_desc_hw *hw;

	CBDMA_LOGI("%s", __func__);

	if (!is_slave_direction(xt->dir))
		return NULL;

	if (!xt->numf || !xt->sgl[0].size)
		return NULL;

	if (xt->frame_size != 1)
		return NULL;

	/* Allocate a transaction descriptor. */
	desc = sp_dma_alloc_tx_descriptor(chan);
	if (!desc)
		return NULL;

	chan->direction = xt->dir;
	dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
	desc->async_tx.tx_submit = sp_dma_tx_submit;

	/* Get a free segment */
	segment = sp_dma_alloc_tx_segment(chan);
	if (!segment)
		goto error;

	hw = &segment->hw;

	/* Fill in the descriptor */
	if (xt->dir != DMA_MEM_TO_DEV)
		hw->buf_addr = xt->dst_start;
	else
		hw->buf_addr = xt->src_start;

	//hw->mcdma_control = chan->tdest & X_DMA_BD_TDEST_MASK;
	//hw->vsize_stride = (xt->numf << X_DMA_BD_VSIZE_SHIFT) & X_DMA_BD_VSIZE_MASK;
	//hw->vsize_stride |= (xt->sgl[0].icg + xt->sgl[0].size) & X_DMA_BD_STRIDE_MASK;
	//hw->control = xt->sgl[0].size & X_DMA_BD_HSIZE_MASK;

	/*
	 * Insert the segment into the descriptor segments
	 * list.
	 */
	list_add_tail(&segment->node, &desc->segments);


	segment = list_first_entry(&desc->segments,
				   struct sp_dma_tx_segment, node);
	desc->async_tx.phys = segment->phys;

	/* For the last DMA_MEM_TO_DEV transfer, set EOP */
	if (xt->dir == DMA_MEM_TO_DEV) {
		//segment->hw.control |= X_DMA_BD_SOP;
		segment = list_last_entry(&desc->segments,
					  struct sp_dma_tx_segment,
					  node);
		//segment->hw.control |= X_DMA_BD_EOP;
	}

	return &desc->async_tx;

error:
	sp_dma_free_tx_descriptor(chan, desc);
	return NULL;
}

/**
 * sp_dma_terminate_all - Halt the channel and free descriptors
 * @chan: Driver specific DMA Channel pointer
 */
static int sp_dma_terminate_all(struct dma_chan *dchan)
{
	struct sp_dma_chan *chan = to_sp_dma_chan(dchan);
	int err;

	CBDMA_LOGI("%s", __func__);

	if (chan->cyclic)
		sp_cbdma_chan_reset(chan);

	err = chan->stop_transfer(chan);
	if (err) {
		dev_err(chan->dev, "Cannot stop channel %p\n", chan);
		chan->err = true;
	}

	/* Remove and free all of the descriptors in the lists */
	sp_dma_free_descriptors(chan);

	if (chan->cyclic) {
		//reg = dma_ctrl_read(chan, X_DMA_REG_DMACR);
		//reg &= ~X_DMA_CR_CYCLIC_BD_EN_MASK;
		//dma_ctrl_write(chan, X_DMA_REG_DMACR, reg);
		chan->cyclic = false;
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * Probe and remove
 */

/**
 * sp_dma_chan_remove - Per Channel remove function
 * @chan: Driver specific DMA channel
 */
static void sp_dma_chan_remove(struct sp_dma_chan *chan)
{
	CBDMA_LOGI("%s", __func__);

	/* Disable all interrupts */
	dma_ctrl_clr(chan, SP_DMA_INT_EN, SP_DMA_ALL_INT_MASK);

	if (chan->irq > 0)
		free_irq(chan->irq, chan);

	tasklet_kill(&chan->tasklet);

	list_del(&chan->common.device_node);
}

static void sp_disable_allclks(struct sp_dma_device *xdev)
{
	CBDMA_LOGI("%s", __func__);

	clk_disable_unprepare(xdev->rxs_clk);
	clk_disable_unprepare(xdev->rx_clk);
	clk_disable_unprepare(xdev->txs_clk);
	clk_disable_unprepare(xdev->tx_clk);
	clk_disable_unprepare(xdev->axi_clk);
}

/**
 * sp_dma_chan_probe - Per Channel Probing
 * It get channel features from the device tree entry and
 * initialize special channel handling routines
 *
 * @xdev: Driver specific device structure
 * @node: Device node
 *
 * Return: '0' on success and failure value on error
 */
static int sp_dma_chan_probe(struct sp_dma_device *xdev,
				  struct device_node *node, int chan_id)
{
	struct sp_dma_chan *chan;
	bool has_dre = false;
	u32 value, width;
	int err;

	CBDMA_LOGI("%s", __func__);
	CBDMA_LOGI("DMA channel %d probed", chan_id);

	/* Allocate and initialize the channel structure */
	chan = devm_kzalloc(xdev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->dev = xdev->dev;
	chan->xdev = xdev;
	chan->has_sg = xdev->has_sg;
	chan->desc_pendingcount = 0x0;
	chan->ext_addr = xdev->ext_addr;

	spin_lock_init(&chan->lock);
	INIT_LIST_HEAD(&chan->pending_list);
	INIT_LIST_HEAD(&chan->done_list);
	INIT_LIST_HEAD(&chan->active_list);

	/* Retrieve the channel properties from the device tree */
	has_dre = of_property_read_bool(node, "include-dre");

	chan->genlock = of_property_read_bool(node, "genlock-mode");

	err = of_property_read_u32(node, "datawidth", &value);
	if (err) {
		dev_err(xdev->dev, "missing datawidth property\n");
		return err;
	}
	width = value >> 3; /* Convert bits to bytes */

	/* If data width is greater than 8 bytes, DRE is not in hw */
	if (width > 8)
		has_dre = false;

	if (!has_dre)
		xdev->common.copy_align = fls(width - 1);

	if (of_device_is_compatible(node, "sunplus,cb-vdma-mm2s-channel") ||
	    of_device_is_compatible(node, "sunplus,cb-dma-mm2s-channel") ||
	    of_device_is_compatible(node, "sunplus,cb-cdma-channel")) {
	    // Change direction from DMA_MEM_TO_DEV to DMA_MEM_TO_MEM for cdma
		if (of_device_is_compatible(node, "sunplus,cb-cdma-channel"))
			chan->direction = DMA_MEM_TO_MEM;
		else
			chan->direction = DMA_MEM_TO_DEV;
		chan->id = chan_id;
		chan->tdest = chan_id;

		chan->ctrl_offset = SP_CBDMA_MM2S_CTRL_OFFSET;
	} else if (of_device_is_compatible(node,
					   "sunplus,cb-vdma-s2mm-channel") ||
		   of_device_is_compatible(node,
					   "sunplus,cb-dma-s2mm-channel")) {
		chan->direction = DMA_DEV_TO_MEM;
		chan->id = chan_id;
		chan->tdest = chan_id - xdev->nr_channels;

		chan->ctrl_offset = SP_CBDMA_S2MM_CTRL_OFFSET;
	} else {
		dev_err(xdev->dev, "Invalid channel compatible node\n");
		return -EINVAL;
	}

	/* Request the interrupt */
	chan->irq = irq_of_parse_and_map(node, 0);
	err = request_irq(chan->irq, sp_cbdma_irq_handler, IRQF_SHARED,
			  "sp-dma-controller", chan);
	if (err) {
		dev_err(xdev->dev, "unable to request IRQ %d\n", chan->irq);
		return err;
	}

	if (xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		chan->start_transfer = sp_dma_start_transfer;
		chan->stop_transfer = sp_dma_stop_transfer;
	} else if (xdev->dma_config->dmatype == DMA_TYPE_CDMA) {
		chan->start_transfer = sp_cdma_start_transfer;
		chan->stop_transfer = sp_cdma_stop_transfer;
	} else {
		dev_err(xdev->dev, "No support!\n");
	}

	/* Initialize the tasklet */
	tasklet_init(&chan->tasklet, sp_dma_do_tasklet,
			(unsigned long)chan);

	/*
	 * Initialize the DMA channel and add it to the DMA engine channels
	 * list.
	 */
	chan->common.device = &xdev->common;

	list_add_tail(&chan->common.device_node, &xdev->common.channels);
	xdev->chan[chan->id] = chan;

	/* Reset the channel */
	err = sp_cbdma_chan_reset(chan);
	if (err < 0) {
		dev_err(xdev->dev, "Reset channel failed\n");
		return err;
	}

	return 0;
}

/**
 * sp_dma_child_probe - Per child node probe
 * It get number of dma-channels per child node from
 * device-tree and initializes all the channels.
 *
 * @xdev: Driver specific device structure
 * @node: Device node
 *
 * Return: 0 always.
 */
static int sp_dma_child_probe(struct sp_dma_device *xdev,
						struct device_node *node)
{
	int ret, i, nr_channels = 1;

	CBDMA_LOGI("%s", __func__);

	/* Get number of dma channels in child node for MCDMA */
	ret = of_property_read_u32(node, "dma-channels", &nr_channels);
	if ((ret < 0) && xdev->mcdma)
		dev_warn(xdev->dev, "missing dma-channels property\n");

	for (i = 0; i < nr_channels; i++)
		sp_dma_chan_probe(xdev, node, xdev->chan_id++);

	xdev->nr_channels += nr_channels;

	return 0;
}

/**
 * of_dma_sp_xlate - Translation function
 * @dma_spec: Pointer to DMA specifier as found in the device tree
 * @ofdma: Pointer to DMA controller data
 *
 * Return: DMA channel pointer on success and NULL on error
 */
static struct dma_chan *of_dma_sp_xlate(struct of_phandle_args *dma_spec,
						struct of_dma *ofdma)
{
	struct sp_dma_device *xdev = ofdma->of_dma_data;
	int chan_id = dma_spec->args[0];

	CBDMA_LOGI("%s", __func__);

	if (chan_id >= xdev->nr_channels || !xdev->chan[chan_id])
		return NULL;

	return dma_get_slave_channel(&xdev->chan[chan_id]->common);
}

static const struct sp_cbdma_config dma_config = {
	.dmatype = DMA_TYPE_DMA,
};

static const struct sp_cbdma_config cdma_config = {
	.dmatype = DMA_TYPE_CDMA,
};

static const struct of_device_id sp_cbdma_of_ids[] = {
	{ .compatible = "sunplus,cb-dma", .data = &dma_config },
	{ .compatible = "sunplus,cb-cdma", .data = &cdma_config },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_cbdma_of_ids);

/**
 * sp_cbdma_probe - Driver probe function
 * @pdev: Pointer to the platform_device structure
 *
 * Return: '0' on success and failure value on error
 */
static int sp_cbdma_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct sp_dma_device *xdev;
	struct device_node *child, *np = pdev->dev.of_node;
	struct resource *io;
	u32 addr_width;
	int i, err;

	CBDMA_LOGI("%s", __func__);

	/* Allocate and initialize the DMA engine structure */
	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if (!xdev)
		return -ENOMEM;

	xdev->dev = &pdev->dev;
	if (np) {
		const struct of_device_id *match;

		match = of_match_node(sp_cbdma_of_ids, np);
		if (match && match->data)
			xdev->dma_config = match->data;
	}

	/* Request and map I/O memory */
	io = platform_get_resource_byname(pdev, IORESOURCE_MEM, CB_DMA_REG_NAME);
	xdev->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(xdev->regs))
		return PTR_ERR(xdev->regs);

	/* The SRAM buffer of CB-DMA */
#ifndef CONFIG_SOC_Q645
	if (((u32)(io->start)) == 0x9C000D00) {
		/* CBDMA0 */
		xdev->sram_addr = 0x9E800000;
		xdev->sram_size = 40 << 10;
	} else {
		/* CBDMA1 */
		xdev->sram_addr = 0x9E820000;
		xdev->sram_size = 4 << 10;
	}
#else
	xdev->sram_addr = 0xFA200000;
	xdev->sram_size = 256 << 10;
#endif
	dev_info(xdev->dev, "SRAM:%d bytes@0x%x\n", xdev->sram_size, xdev->sram_addr);

	/* Retrieve the DMA engine properties from the device tree */
	xdev->has_sg = of_property_read_bool(node, "include-sg");
	if (xdev->dma_config->dmatype == DMA_TYPE_DMA)
		xdev->mcdma = of_property_read_bool(node, "mcdma");

	err = of_property_read_u32(node, "addrwidth", &addr_width);
	if (err < 0)
		dev_warn(xdev->dev, "missing addrwidth property\n");

	if (addr_width > 32)
		xdev->ext_addr = true;
	else
		xdev->ext_addr = false;

	/* Set the dma mask bits */
	dma_set_mask(xdev->dev, DMA_BIT_MASK(addr_width));

	/* Initialize the DMA engine */
	xdev->common.dev = &pdev->dev;

	INIT_LIST_HEAD(&xdev->common.channels);
	if (!(xdev->dma_config->dmatype == DMA_TYPE_CDMA)) {
		dma_cap_set(DMA_SLAVE, xdev->common.cap_mask);
		dma_cap_set(DMA_PRIVATE, xdev->common.cap_mask);
	}

	xdev->common.device_alloc_chan_resources =
				sp_dma_alloc_chan_resources;
	xdev->common.device_free_chan_resources =
				sp_dma_free_chan_resources;
	xdev->common.device_terminate_all = sp_dma_terminate_all;
	xdev->common.device_tx_status = sp_dma_tx_status;
	xdev->common.device_issue_pending = sp_dma_issue_pending;
	if (xdev->dma_config->dmatype == DMA_TYPE_DMA) {
		dma_cap_set(DMA_CYCLIC, xdev->common.cap_mask);
		xdev->common.device_prep_slave_sg = sp_dma_prep_slave_sg;
		xdev->common.device_prep_dma_cyclic =
					  sp_dma_prep_dma_cyclic;
		xdev->common.device_prep_interleaved_dma =
					sp_dma_prep_interleaved;
		/* Residue calculation is supported by only AXI DMA */
		xdev->common.residue_granularity =
					  DMA_RESIDUE_GRANULARITY_SEGMENT;
	} else if (xdev->dma_config->dmatype == DMA_TYPE_CDMA) {
		dma_cap_set(DMA_MEMCPY, xdev->common.cap_mask);
		xdev->common.device_prep_dma_memcpy = sp_cdma_prep_memcpy;
	} else {
		dev_err(&pdev->dev, "No support!\n");
		goto disable_clks;
	}

	platform_set_drvdata(pdev, xdev);

	/* Initialize the channels */
	for_each_child_of_node(node, child) {
		err = sp_dma_child_probe(xdev, child);
		if (err < 0)
			goto disable_clks;
	}

	/* Register the DMA engine with the core */
	dma_async_device_register(&xdev->common);

	err = of_dma_controller_register(node, of_dma_sp_xlate,
					 xdev);
	if (err < 0) {
		dev_err(&pdev->dev, "Unable to register DMA to DT\n");
		dma_async_device_unregister(&xdev->common);
		goto error;
	}

	dev_info(&pdev->dev, "Sunplus CBDMA Engine Driver Probed!!\n");

	return 0;

disable_clks:
	sp_disable_allclks(xdev);
error:
	for (i = 0; i < xdev->nr_channels; i++)
		if (xdev->chan[i])
			sp_dma_chan_remove(xdev->chan[i]);

	return err;
}

/**
 * sp_cbdma_remove - Driver remove function
 * @pdev: Pointer to the platform_device structure
 *
 * Return: Always '0'
 */
static int sp_cbdma_remove(struct platform_device *pdev)
{
	struct sp_dma_device *xdev = platform_get_drvdata(pdev);
	int i;

	CBDMA_LOGI("%s", __func__);

	of_dma_controller_free(pdev->dev.of_node);

	dma_async_device_unregister(&xdev->common);

	for (i = 0; i < xdev->nr_channels; i++)
		if (xdev->chan[i])
			sp_dma_chan_remove(xdev->chan[i]);

	sp_disable_allclks(xdev);

	return 0;
}

static struct platform_driver sp_cbdma_driver = {
	.driver = {
		.name	= "sunplus,cbdma",
		.of_match_table = sp_cbdma_of_ids,
	},
		.probe = sp_cbdma_probe,
		.remove = sp_cbdma_remove,
};

module_platform_driver(sp_cbdma_driver);


/**************************************************************************
 *                  M O D U L E    D E C L A R A T I O N                  *
 **************************************************************************/
MODULE_AUTHOR("Cheng Chung Ho <cc.ho@sunplus.com>");
MODULE_DESCRIPTION("Sunplus CB-DMA controller driver");
MODULE_LICENSE("GPL v2");
