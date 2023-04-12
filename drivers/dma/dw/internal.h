/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Driver for the Synopsys DesignWare DMA Controller
 *
 * Copyright (C) 2013 Intel Corporation
 */

#ifndef _DMA_DW_INTERNAL_H
#define _DMA_DW_INTERNAL_H

#if defined(CONFIG_SOC_SP7350)
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/platform_data/dma-dw.h>
#else
#include <linux/dma/dw.h>
#endif

#include "regs.h"

#if defined(CONFIG_SOC_SP7350)
/**
 * struct dw_dma_chip - representation of DesignWare DMA controller hardware
 * @dev:		struct device of the DMA controller
 * @id:			instance ID
 * @irq:		irq line
 * @regs:		memory mapped I/O space
 * @clk:		hclk clock
 * @dw:			struct dw_dma that is filed by dw_dma_probe()
 * @pdata:		pointer to platform data
 */
struct dw_dma_chip {
	struct device	*dev;
	int		id;
	int		irq[8];
	unsigned char	chan_mode;
	void __iomem	*regs;
	struct clk	*clk;
	struct dw_dma	*dw;

	const struct dw_dma_platform_data	*pdata;
};

/* Export to the platform drivers */
int dw_dma_probe(struct dw_dma_chip *chip);
int dw_dma_remove(struct dw_dma_chip *chip);
int idma32_dma_probe(struct dw_dma_chip *chip);
int idma32_dma_remove(struct dw_dma_chip *chip);
#endif

int do_dma_probe(struct dw_dma_chip *chip);
int do_dma_remove(struct dw_dma_chip *chip);

void do_dw_dma_on(struct dw_dma *dw);
void do_dw_dma_off(struct dw_dma *dw);

int do_dw_dma_disable(struct dw_dma_chip *chip);
int do_dw_dma_enable(struct dw_dma_chip *chip);

extern bool dw_dma_filter(struct dma_chan *chan, void *param);

#ifdef CONFIG_ACPI
void dw_dma_acpi_controller_register(struct dw_dma *dw);
void dw_dma_acpi_controller_free(struct dw_dma *dw);
#else /* !CONFIG_ACPI */
static inline void dw_dma_acpi_controller_register(struct dw_dma *dw) {}
static inline void dw_dma_acpi_controller_free(struct dw_dma *dw) {}
#endif /* !CONFIG_ACPI */

struct platform_device;

#ifdef CONFIG_OF
struct dw_dma_platform_data *dw_dma_parse_dt(struct platform_device *pdev);
void dw_dma_of_controller_register(struct dw_dma *dw);
void dw_dma_of_controller_free(struct dw_dma *dw);
#else
static inline struct dw_dma_platform_data *dw_dma_parse_dt(struct platform_device *pdev)
{
	return NULL;
}
static inline void dw_dma_of_controller_register(struct dw_dma *dw) {}
static inline void dw_dma_of_controller_free(struct dw_dma *dw) {}
#endif

#if defined(CONFIG_SOC_SP7350)
enum {
	DW_NORMAL_MODE = 0,
	DW_SPI_MODE = 1,
};
#endif

struct dw_dma_chip_pdata {
	const struct dw_dma_platform_data *pdata;
	int (*probe)(struct dw_dma_chip *chip);
	int (*remove)(struct dw_dma_chip *chip);
	struct dw_dma_chip *chip;
#if defined(CONFIG_SOC_SP7350)
	unsigned char	chan_mode;
#endif
};

static __maybe_unused const struct dw_dma_chip_pdata dw_dma_chip_pdata = {
	.probe = dw_dma_probe,
	.remove = dw_dma_remove,
};

#if defined(CONFIG_SOC_SP7350)
static __maybe_unused const struct dw_dma_chip_pdata dw_spi_dma_chip_pdata = {
	.probe = dw_dma_probe,
	.remove = dw_dma_remove,
	.chan_mode = DW_SPI_MODE,
};
#endif

static const struct dw_dma_platform_data idma32_pdata = {
	.nr_channels = 8,
	.chan_allocation_order = CHAN_ALLOCATION_ASCENDING,
	.chan_priority = CHAN_PRIORITY_ASCENDING,
	.block_size = 131071,
	.nr_masters = 1,
	.data_width = {4},
	.multi_block = {1, 1, 1, 1, 1, 1, 1, 1},
};

static __maybe_unused const struct dw_dma_chip_pdata idma32_chip_pdata = {
	.pdata = &idma32_pdata,
	.probe = idma32_dma_probe,
	.remove = idma32_dma_remove,
};

#endif /* _DMA_DW_INTERNAL_H */
