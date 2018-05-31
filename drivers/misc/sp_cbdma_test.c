/*
 * Sunplus CBDMA test driver
 *
 * Copyright (C) 2018 Sunplus Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>

struct cbdma_reg {
	volatile unsigned int hw_ver;
	volatile unsigned int config;
	volatile unsigned int length;
	volatile unsigned int src_adr;
	volatile unsigned int des_adr;
	volatile unsigned int int_flag;
	volatile unsigned int int_en;
	volatile unsigned int memset_val;
	volatile unsigned int sdram_size_config;
	volatile unsigned int rsv_9_31[23];
};

#define NUM_CBDMA		2
#define BUF_SIZE_DRAM		(PAGE_SIZE * 2)

#define PATTERN4TEST(X)		((X << 24) | (X << 16) | (X << 8) | (X << 0))

#define CBDMA_CONFIG_GO		(1 << 8)
#define CBDMA_INT_FLAG_DONE	(1 << 0)
#define MIN(X, Y)		((X) < (Y) ? (X): (Y))


struct cbdma_info_s {
	char name[32];
	struct platform_device *pdev;
	char irq_name[32];
	int irq;
	volatile struct cbdma_reg *cbdma_ptr;
	u32 sram_addr;
	u32 sram_size;
	void *buf_va;
	dma_addr_t dma_handle;
};
static struct cbdma_info_s cbdma_info[NUM_CBDMA];

static struct task_struct *thread_ptr = NULL;
atomic_t isr_cnt = ATOMIC_INIT(0);

void dump_data(u8 *ptr, u32 size)
{
	u32 i, addr_begin;
	int length;
	char buffer[256];

	addr_begin = (u32)(ptr);
	for(i=0; i<size; i++) {
		if((i & 0xf) == 0x0) {
			length = sprintf(buffer, "%08x: ", i + addr_begin);
		}
		length += sprintf(&buffer[length], "%02x ", *ptr);
		ptr++;

		if((i & 0xf) == 0xf) {
			printk(KERN_INFO "%s\n", buffer);
		}
	}
	printk(KERN_INFO "\n");
}


int sp_cbdma_tst_thread(void *data)
{
	int i, j, val;
	u32 *u32_ptr, expected_u32, val_u32, test_size;

	printk(KERN_INFO "%s, %d\n", __func__, __LINE__);
	msleep(100);	/* let console be available */
	printk(KERN_INFO "%s(), start\n", __func__);

	for (i = 0; i < NUM_CBDMA; i++) {
		if (cbdma_info[i].sram_size) {
			printk(KERN_INFO "Test for %s ------------------------\n", cbdma_info[i].name);

			printk(KERN_INFO "MEMSET test\n");
			val = atomic_read(&isr_cnt);
			cbdma_info[i].cbdma_ptr->int_en = 0;
			cbdma_info[i].cbdma_ptr->length = BUF_SIZE_DRAM;
			cbdma_info[i].cbdma_ptr->des_adr = (u32)(cbdma_info[i].dma_handle);
			cbdma_info[i].cbdma_ptr->memset_val = PATTERN4TEST(i);
			cbdma_info[i].cbdma_ptr->int_en = ~0;	/* Enable all interrupts */
			wmb();
			cbdma_info[i].cbdma_ptr->config = 0x00030000 | CBDMA_CONFIG_GO;
			wmb();
			while (1) {
				if (cbdma_info[i].cbdma_ptr->config & CBDMA_CONFIG_GO) {
					/* Still running */
					continue;
				}
				if (atomic_read(&isr_cnt) == val) {
					/* ISR not served */
					continue;
				}
				printk(KERN_INFO "isr_cnt: %d\n", atomic_read(&isr_cnt));
				break;
			}
			dump_data(cbdma_info[i].buf_va, 0x40);
			u32_ptr = (u32 *)(cbdma_info[i].buf_va);
			expected_u32 = PATTERN4TEST(i);
			for (j = 0 ; j < (BUF_SIZE_DRAM >> 2); j++) {
				BUG_ON(*u32_ptr != expected_u32);
				u32_ptr++;
			}
			printk(KERN_INFO "MEMSET test: OK\n\n");

			printk(KERN_INFO "R/W test\n");
			u32_ptr = (u32 *)(cbdma_info[i].buf_va);
			val_u32 = (u32)(u32_ptr);
			test_size = MIN(cbdma_info[i].sram_size, BUF_SIZE_DRAM) >> 1;
			for (j = 0 ; j < (test_size >> 2); j++) {
				/* Fill (test_size) bytes of data to DRAM */
				*u32_ptr = val_u32;
				u32_ptr++;
				val_u32 += 4;
			}
			dump_data(cbdma_info[i].buf_va, 0x40);

			val = atomic_read(&isr_cnt);
			cbdma_info[i].cbdma_ptr->int_en = 0;
			cbdma_info[i].cbdma_ptr->length = test_size;
			cbdma_info[i].cbdma_ptr->des_adr = 0;
			cbdma_info[i].cbdma_ptr->src_adr = (u32)(cbdma_info[i].dma_handle);
			cbdma_info[i].cbdma_ptr->int_en = ~0;	/* Enable all interrupts */
			wmb();
			cbdma_info[i].cbdma_ptr->config = 0x00030002 | CBDMA_CONFIG_GO;		/* Read to SRAM */
			wmb();
			while (1) {
				if (cbdma_info[i].cbdma_ptr->config & CBDMA_CONFIG_GO) {
					/* Still running */
					continue;
				}
				if (atomic_read(&isr_cnt) == val) {
					/* ISR not served */
					continue;
				}
				printk(KERN_INFO "isr_cnt: %d\n", atomic_read(&isr_cnt));
				break;
			}

			val = atomic_read(&isr_cnt);
			cbdma_info[i].cbdma_ptr->int_en = 0;
			cbdma_info[i].cbdma_ptr->length = test_size;
			cbdma_info[i].cbdma_ptr->des_adr = ((u32)(cbdma_info[i].dma_handle)) + test_size;
			cbdma_info[i].cbdma_ptr->src_adr = 0;
			cbdma_info[i].cbdma_ptr->int_en = ~0;	/* Enable all interrupts */
			wmb();
			cbdma_info[i].cbdma_ptr->config = 0x00030001 | CBDMA_CONFIG_GO;		/* Write to DRAM */
			wmb();
			while (1) {
				if (cbdma_info[i].cbdma_ptr->config & CBDMA_CONFIG_GO) {
					/* Still running */
					continue;
				}
				if (atomic_read(&isr_cnt) == val) {
					/* ISR not served */
					continue;
				}
				printk(KERN_INFO "isr_cnt: %d\n", atomic_read(&isr_cnt));
				break;
			}
			dump_data(cbdma_info[i].buf_va + test_size, 0x40);

			u32_ptr = (u32 *)(cbdma_info[i].buf_va + test_size);
			val_u32 = (u32)(cbdma_info[i].buf_va);
			for (j = 0 ; j < (test_size >> 2); j++) {
				/* Compare (test_size) bytes of data in DRAM */
				BUG_ON(*u32_ptr != val_u32);
				u32_ptr++;
				val_u32 += 4;
			}
			printk(KERN_INFO "R/W test: OK\n\n");
		}
	}

	for (i = 0; i < NUM_CBDMA; i++) {
		if (cbdma_info[i].buf_va) {
			dma_free_coherent(&(cbdma_info[i].pdev->dev), BUF_SIZE_DRAM, cbdma_info[i].buf_va, cbdma_info[i].dma_handle);
		}
	}

	printk(KERN_INFO "%s(), end\n", __func__);
	return 0;
}

static const struct platform_device_id sp_cbdma_tst_devtypes[] = {
	{
		.name = "sp_cbdma_tst",
	}, {
		/* sentinel */
	}
};

static const struct of_device_id sp_cbdma_tst_dt_ids[] = {
	{
		.compatible = "sunplus,sp-cbdma-tst",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, sp_cbdma_tst_dt_ids);

static irqreturn_t sp_cbdma_tst_irq(int irq, void *args)
{
	struct cbdma_info_s *ptr;
	u32 int_flag;
	unsigned long flags;

	local_irq_save(flags);
	atomic_inc(&isr_cnt);

	ptr = (struct cbdma_info_s *)(args);
	int_flag = ptr->cbdma_ptr->int_flag;
	printk(KERN_INFO  "%s, %d, %s, int_flag: 0x%x\n", __func__, __LINE__, ptr->irq_name, int_flag);
	BUG_ON(int_flag != CBDMA_INT_FLAG_DONE);
	ptr->cbdma_ptr->int_flag = int_flag;

	local_irq_restore(flags);

	return IRQ_HANDLED;
}

static int sp_cbdma_tst_probe(struct platform_device *pdev)
{
	static int idx_cbdma = 0;
	struct resource *res_mem;
	const struct of_device_id *match;
	void __iomem *membase;
	int ret, num_irq;

	if (idx_cbdma >= NUM_CBDMA) {
		printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	printk(KERN_INFO "%s, %d\n", __func__, __LINE__);

	if (idx_cbdma == 0) {
		memset(&cbdma_info, 0, sizeof(cbdma_info));
	}

	if (pdev->dev.of_node) {
		match = of_match_node(sp_cbdma_tst_dt_ids, pdev->dev.of_node);
		if (match == NULL) {
			printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
			return -ENODEV;
		}
		num_irq = of_irq_count(pdev->dev.of_node);
		if (num_irq != 1) {
			printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		}
	}

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res_mem)) {
		printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		return PTR_ERR(res_mem);
	}

	membase = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(membase)) {
		printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		return PTR_ERR(membase);
	}

	cbdma_info[idx_cbdma].pdev = pdev;
	cbdma_info[idx_cbdma].cbdma_ptr = (volatile struct cbdma_reg *)(membase);
	cbdma_info[idx_cbdma].cbdma_ptr->int_flag = ~0;	/* clear all interrupt flags */
	cbdma_info[idx_cbdma].irq = platform_get_irq(pdev, 0);
	if (cbdma_info[idx_cbdma].irq < 0) {
		printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		return -ENODEV;
	}
	sprintf(cbdma_info[idx_cbdma].name, "CBDMA_%x", (u32)(res_mem->start));
	sprintf(cbdma_info[idx_cbdma].irq_name, "irq_%x", (u32)(res_mem->start));
	ret = request_irq(cbdma_info[idx_cbdma].irq, sp_cbdma_tst_irq, 0, cbdma_info[idx_cbdma].irq_name, &cbdma_info[idx_cbdma]);
	if (ret) {
		printk(KERN_ERR "Error: %s, %d\n", __func__, __LINE__);
		return ret;
	}
	printk(KERN_INFO "%s, %d, irq: %d, %s\n", __func__, __LINE__, cbdma_info[idx_cbdma].irq, cbdma_info[idx_cbdma].irq_name);
	if (((u32)(res_mem->start)) == 0x9c000d00) {
		/* CBDMA0 */
		cbdma_info[idx_cbdma].sram_addr = 0x9E800000;
		cbdma_info[idx_cbdma].sram_size = 40 << 10;
	} else {
		/* CBDMA1 */
		cbdma_info[idx_cbdma].sram_addr = 0x9E820000;
		cbdma_info[idx_cbdma].sram_size = 4 << 10;
	}
	printk(KERN_INFO "%s, %d, SRAM: 0x%x bytes @ 0x%x\n", __func__, __LINE__, cbdma_info[idx_cbdma].sram_size, cbdma_info[idx_cbdma].sram_addr);

	/* Allocate uncached memory for test */
	cbdma_info[idx_cbdma].buf_va = dma_alloc_coherent(&(pdev->dev), BUF_SIZE_DRAM, &(cbdma_info[idx_cbdma].dma_handle), GFP_KERNEL);
	if (cbdma_info[idx_cbdma].buf_va == NULL) {
		printk(KERN_INFO "%s, %d, Can't allocation buffer for %s\n", __func__, __LINE__, cbdma_info[idx_cbdma].name);
		/* Skip error handling here */
		ret = -ENOMEM;
	}
	printk(KERN_INFO "DMA buffer for %s, VA: 0x%p, PA: 0x%x\n", cbdma_info[idx_cbdma].name, cbdma_info[idx_cbdma].buf_va, (u32)(cbdma_info[idx_cbdma].dma_handle));

	if (thread_ptr == NULL) {
		printk(KERN_INFO "Start a thread for test ...\n");
		thread_ptr = kthread_run(sp_cbdma_tst_thread, cbdma_info, "sp_cbdma_tst_thread");
	}

	idx_cbdma++;
	return 0;

}

static struct platform_driver ssc_driver = {
	.driver		= {
		.name		= "sp_cbdma_tst",
		.of_match_table	= of_match_ptr(sp_cbdma_tst_dt_ids),
	},
	.id_table	= sp_cbdma_tst_devtypes,
	.probe		= sp_cbdma_tst_probe,
};
module_platform_driver(ssc_driver);

MODULE_DESCRIPTION("CBDMA test driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sp_cbdma_tst");
