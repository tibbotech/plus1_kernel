// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus Parallel NAND Cotroller driver
 *
 * Copyright Sunplus Technology Co., Ltd.
 *
 * Derived from:
 *	Copyright (C) 2019-2021 Faraday Technology Corp.
 *
 * TODO:
 *	1.Convert the driver to exec_op() to have one less driver
 *	relying on the legacy interface.
 *	2.Get the device parameter from sp_paranand_ids.c rather
 * 	than nand_attr[].
 *
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/ktime.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mtd/bbm.h>

#include "sp_paranand.h"

static int startchn = 0;
module_param(startchn, int, 0644);

extern const struct nand_flash_dev sp_pnand_ids[];

/* Note: The unit of tWPST/tRPST/tWPRE/tRPRE field of sp_pnand_chip_timing is ns.
 *
 * tWH, tCH, tCLH, tALH, tCALH, tWP, tREH, tCR, tRSTO, tREAID,
 * tREA, tRP, tWB, tRB, tWHR, tWHR2, tRHW, tRR, tAR, tRC
 * tADL, tRHZ, tCCS, tCS, tCS2, tCLS, tCLR, tALS, tCALS, tCAL2, tCRES, tCDQSS, tDBS, tCWAW, tWPRE,
 * tRPRE, tWPST, tRPST, tWPSTH, tRPSTH, tDQSHZ, tDQSCK, tCAD, tDSL
 * tDSH, tDQSL, tDQSH, tDQSD, tCKWR, tWRCK, tCK, tCALS2, tDQSRE, tWPRE2, tRPRE2, tCEH
 */
static struct sp_pnand_chip_timing chip_timing[] = {
	{ //SAMSUNG_K9F2G08U0A_ZEBU
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	20, 12, 100, 0, 60, 0, 100, 20, 10, 0,
	70, 100, 0, 20, 0, 12, 10, 12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //GD9FS2G8F2A 256MiB 1.8V 8-bit
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	20, 12, 100, 0, 60, 0, 100, 20, 10, 25,
	300, 100, 0, 15, 0, 12, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //GD9AU4G8F3A 512MiB 3.3V 8-bit
	7, 5, 5, 5, 0, 12, 7, 0, 0, 0,
	18, 10, 100, 0, 80, 0, 100, 20, 10, 20,
	100, 100, 0, 15, 0, 12, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //GD9FU4G8F4B 512MiB 3.3V 8-bit
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	20, 12, 100, 0, 60, 0, 100, 20, 10, 0,
	150, 100, 0, 15, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //W29N08GZSIBA 1GMiB 1.8V 8-bit
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	25, 12, 100, 0, 80, 0, 100, 20, 10, 0,
	150, 100, 0, 15, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //K9GBG08U0B 4GiB 3.3V 8-bit
	11, 5, 5, 5, 0, 11, 11, 0, 0, 0,
	20, 11, 100, 0, 120, 300, 100, 20, 10, 25,
	70, 100, 0, 20, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABAAA 4GiB 3.3V 8-bit
	30, 20, 20, 20, 0, 50, 30, 0, 0, 0,
	40, 50, 200, 0, 120, 0, 200, 40, 25, 100,
	200, 200, 0, 70, 0, 20, 50, 20, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static int sp_pnand_ooblayout_ecc(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = 1;

	return 0;
}

static int sp_pnand_ooblayout_free(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = mtd->oobsize;

	return 0;
}

static const struct mtd_ooblayout_ops sp_pnand_ooblayout_ops = {
	.ecc = sp_pnand_ooblayout_ecc,
	.free = sp_pnand_ooblayout_free,
};

static void sp_pnand_set_warmup_cycle(struct nand_chip *chip,
			u8 wr_cyc, u8 rd_cyc)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int val;

	val = readl(data->io_base + MEM_ATTR_SET2);
	val &= ~(0xFF);
	val |= (((rd_cyc & 0x3) << 4) | (wr_cyc & 0x3));
	writel(val, data->io_base + MEM_ATTR_SET2);
}

static struct sp_pnand_chip_timing *sp_pnand_scan_timing(struct nand_chip *chip)
{
	//struct mtd_info *mtd = nand_to_mtd(chip);

	DBGLEVEL2(sp_pnand_dbg("chip->parameters.model %s\n", chip->parameters.model));
	if(strcmp(chip->parameters.model, "K9F2G08XXX 256MiB ZEBU 8-bit") == 0)
		return &chip_timing[0];
	else if(strcmp(chip->parameters.model, "9FS2G8F2A 256MiB 1.8V 8-bit") == 0)
		return &chip_timing[1];
	else if(strcmp(chip->parameters.model, "9AU4G8F3A 512MiB 3.3V 8-bit") == 0)
		return &chip_timing[2];
	else if(strcmp(chip->parameters.model, "9FU4G8F4B 512MiB 3.3V 8-bit") == 0)
		return &chip_timing[3];
	else if(strcmp(chip->parameters.model, "W29N08GZSIBA 1GiB 1.8V 8-bit") == 0)
		return &chip_timing[4];
	else if(strcmp(chip->parameters.model, "K9GBG08U0B 4GiB 3.3V 8-bit") == 0)
		return &chip_timing[5];
	else if(strcmp(chip->parameters.model, "MT29F32G08ABAAA 4GiB 3.3V 8-bit") == 0)
		return &chip_timing[6];
	else
		return NULL;
}

/* The unit of Hclk is MHz, and the unit of Time is ns.
 * We desire to calculate N to satisfy N*(1/Hclk) > Time given Hclk and Time
 * ==> N > Time * Hclk
 * ==> N > Time * 10e(-9) * Hclk *10e(6)        --> take the order out
 * ==> N > Time * Hclk * 10e(-3)
 * ==> N > Time * Hclk / 1000
 * ==> N = (Time * Hclk + 999) / 1000
 */
static void sp_pnand_calc_timing(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int tWH, tWP, tREH, tRES, tBSY, tBUF1;
	int tBUF2, tBUF3, tBUF4, tPRE, tRLAT, t1;
	int tPST, tPSTH, tWRCK;
	int i, toggle_offset = 0;
	struct sp_pnand_chip_timing *p;
	u32 CLK, FtCK, timing[4];

	CLK = data->clkfreq / 1000000;

	tWH = tWP = tREH = tRES =  0;
	tRLAT = tBSY = t1 = 0;
	tBUF4 = tBUF3 = tBUF2 = tBUF1 = 0;
	tPRE = tPST = tPSTH = tWRCK = 0;

	p = sp_pnand_scan_timing(chip);
	if(!p)
		DBGLEVEL1(sp_pnand_dbg("Failed to get AC timing!\n"));

	if(data->flash_type == LEGACY_FLASH) {
		// tWH = max(tWH, tCH, tCLH, tALH)
		tWH = max_4(p->tWH, p->tCH, (int)p->tCLH, (int)p->tALH);
		tWH = (tWH * CLK) / 1000;
		// tWP = tWP
		tWP = (p->tWP * CLK) / 1000;
		// tREH = tREH
		tREH = (p->tREH * CLK) / 1000;
		// tRES = max(tREA, tRSTO, tREAID)
		tRES = max_3(p->tREA, p->tRSTO, (int)p->tREAID);
		tRES = (tRES * CLK) / 1000;
		// tRLAT < (tRES + tREH) + 2
		tRLAT = tRES + tREH;
		// t1 = max(tCS, tCLS, tALS) - tWP
		t1 = max_3(p->tCS, p->tCLS, (int)p->tALS) - p->tWP;
		if (t1 < 0)
			t1 = 0;
		else
			t1 = (t1 * CLK) / 1000;
		// tPSTH(EBI setup time) = max(tCS, tCLS, tALS)
		tPSTH = max_3(p->tCS, p->tCLS, (int)p->tALS);
		tPSTH = (tPSTH * CLK) / 1000;
		// tWRCK(EBI hold time) = max(tRHZ, tREH)
		tWRCK = max_2(p->tRHZ, p->tREH);
		tWRCK = (tWRCK * CLK) / 1000;
	}

	else if(data->flash_type == ONFI2) {
		// tWP = tCAD
		tWP = (p->tCAD * CLK) / 1000;

		// Fill this field with value N, FTck = mem_clk/2(N + 1)
		// Note:mem_clk is same as core_clk. Here, we'd like to
		// assign 30MHz to FTck.
		tRES = 0;
		FtCK = CLK / ( 2 * (tRES + 1));

		// Increase p->tCK by one, is for the fraction which
		// cannot store in the variable, Integer type.
		p->tCK = 1000 / FtCK + 1;

		p->tWPRE = 2 * p->tCK;
		p->tWPST = 2 * p->tCK;
		p->tDQSL = 1 * p->tCK;
		p->tDQSH = 1 * p->tCK;

		p->tCKWR = (p->tDQSCK + p->tCK) / p->tCK;
		if(p->tDQSCK % p->tCK !=0)
			p->tCKWR += 1;

		t1 = (p->tCS * CLK) / 1000;

		tPRE = 2;	// Assign 2 due to p->tWPRE is 1.5*p->tCK
		tPST = 2;	// Assign 2 due to p->tWPST is 1.5*p->tCK
		tPSTH = ((p->tDQSHZ * FtCK) / 1000) + 1;
		tWRCK = (p->tWRCK * CLK) /1000;
	}

	else if(data->flash_type == ONFI3) {
		p->tRC = 1000 / CLK;
		p->tRPST = p->tDQSRE + (p->tRC >> 1);
		p->tRP = p->tREH = p->tDQSH = p->tDQSL = (p->tRC >> 1);

		tWH = max_2(p->tCH, p->tCALH);
		tWH = (tWH * CLK) / 1000;

		tWP = (p->tWP * CLK) / 1000;

		tRES = max_2(p->tRP, p->tREH);
		tRES = (tRES * CLK) / 1000;

		t1 = max_2((p->tCALS2 - p->tWP), p->tCWAW);
		t1 = (t1 * CLK) / 1000;

		tPRE = max_2(max_3(p->tWPRE, p->tWPRE2, p->tRPRE),
						max_3(p->tRPRE2, p->tCS, p->tCS2));
		tPRE = (tPRE * CLK) / 1000;
		tPRE+= 1;

		tPST = max_4(p->tWPST, p->tRPST, p->tCH, p->tCALH);
		tPST = (tPST * CLK) / 1000;
		tPST+= 1;

		tPSTH = max_3(p->tWPSTH, p->tRPSTH, p->tCEH);
		tPSTH = (tPSTH * CLK) / 1000;

		tWRCK = max_4(p->tDSL, p->tDSH, p->tDQSL, p->tDQSH);
		tWRCK = (tWRCK * CLK) / 1000;
	}

	else if(data->flash_type == TOGGLE1) {
		// tWH = max(tWH, tCH, tCALH)
		tWH = max_3(p->tWH, p->tCH, (int)p->tCALH);
		tWH = (tWH * CLK) / 1000;
		// tWP = tWP
		tWP = (p->tWP * CLK) / 1000;
		// tREH = tCR
		tREH = (p->tCR * CLK) / 1000;
		// tRES = max(tRP, tREH)
		tRES = max_2(p->tRP, p->tREH);
		tRES = (tRES * CLK) / 1000;
		// t1 = max(tCALS2-tWP, tCWAW)
		t1 = max_2((p->tCALS2 - p->tWP), p->tCWAW);
		t1 = (t1 * CLK) / 1000;
		// tPRE = max(tWPRE, 2*tRPRE, tRPRE+tDQSRE) + 1
		tPRE = max_3((int)p->tWPRE, (int)(p->tRPRE << 1),\
					 (p->tRPRE + p->tDQSRE));
		tPRE = (tPRE * CLK) / 1000;
		tPRE +=1;
		// tPST = max(tWPST, tRPST) + 1
		tPST = max_2(p->tWPST, p->tRPST);
		tPST = (tPST * CLK) / 1000;
		tPST +=1;
		// tPSTH = max(tWPSTH, tRPSTH)
		tPSTH = max_2(p->tWPSTH, p->tRPSTH);
		tPSTH = (tPSTH * CLK) / 1000;
		// tWRCK = max(tDSL, tDSH, tDQSL, tDQSH)
		tWRCK = max_4(p->tDSL, p->tDSH, (int)p->tDQSL, (int)p->tDQSH);
		tWRCK = (tWRCK * CLK) / 1000;
	}

	else if(data->flash_type == TOGGLE2) {
		p->tRC = 1000 / CLK;
		p->tRPST = p->tDQSRE + (p->tRC >> 1);
		p->tRP = p->tREH = p->tDQSH = p->tDQSL = (p->tRC >> 1);

		tWH = max_2(p->tCH, p->tCALH);
		tWH = (tWH * CLK) / 1000;

		tWP = (p->tWP * CLK) / 1000;

		tRES = max_2(p->tRP, p->tREH);
		tRES = (tRES * CLK) / 1000;

		t1 = max_2((p->tCALS2 - p->tWP), p->tCWAW);
		t1 = (t1 * CLK) / 1000;

		tPRE = max_2(max_3(p->tWPRE, p->tWPRE2, p->tRPRE),
						max_3(p->tRPRE2, p->tCS, p->tCS2));
		tPRE = (tPRE * CLK) / 1000;
		tPRE+= 1;

		tPST = max_2(p->tWPST, p->tRPST);
		tPST = (tPST * CLK) / 1000;
		tPST+= 1;

		tPSTH = max_2(p->tWPSTH, p->tRPSTH);
		tPSTH = (tPSTH * CLK) / 1000;

		tWRCK = max_4(p->tDSL, p->tDSH, p->tDQSL, p->tDQSH);
		tWRCK = (tWRCK * CLK) / 1000;
	}

	// tBSY = max(tWB, tRB), min value = 1
	tBSY = max_2(p->tWB, p->tRB);
	tBSY = (tBSY * CLK) / 1000;
	if(tBSY < 1)
		tBSY = 1;
	// tBUF1 = max(tADL, tCCS)
	tBUF1 = max_2(p->tADL, p->tCCS);
	tBUF1 = (tBUF1 * CLK) / 1000;
	// tBUF2 = max(tAR, tRR, tCLR, tCDQSS, tCRES, tCALS, tCALS2, tDBS)
	tBUF2 = max_2(max_4(p->tAR, p->tRR, (int)p->tCLR, (int)p->tCDQSS),
			max_4((int)p->tCRES, (int)p->tCALS, (int)p->tCALS2, (int)p->tDBS));
	tBUF2 = (tBUF2 * CLK) / 1000;
	// tBUF3 = max(tRHW, tRHZ, tDQSHZ)
	tBUF3 = max_3(p->tRHW, p->tRHZ, (int)p->tDQSHZ);
	tBUF3 = (tBUF3 * CLK) / 1000;
	// tBUF4 = max(tWHR, tWHR2)
	tBUF4 = max_2((int)p->tWHR, p->tWHR2);
	if(data->flash_type == ONFI3)
		tBUF4 = max_2(tBUF4, p->tCCS);
	tBUF4 = (tBUF4 * CLK) / 1000;

	// For FPGA, we use the looser AC timing
	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2) {

		toggle_offset = 3;
		tREH += toggle_offset;
		tRES += toggle_offset;
		tWH +=toggle_offset;
		tWP +=toggle_offset;
		t1  +=toggle_offset;
		tBSY+=toggle_offset;
		tBUF1+=toggle_offset;
		tBUF2+=toggle_offset;
		tBUF3+=toggle_offset;
		tBUF4+=toggle_offset;
		tWRCK+=toggle_offset;
		tPSTH+=toggle_offset;
		tPST+=toggle_offset;
		tPRE+=toggle_offset;
	}

	timing[0] = (tWH << 24) | (tWP << 16) | (tREH << 8) | tRES;
	timing[1] = (tRLAT << 16) | (tBSY << 8) | t1;
	timing[2] = (tBUF4 << 24) | (tBUF3 << 16) | (tBUF2 << 8) | tBUF1;
	timing[3] = (tPRE << 28) | (tPST << 24) | (tPSTH << 16) | tWRCK;

	for (i = 0;i < MAX_CHANNEL;i++) {
		writel(timing[0], data->io_base + FL_AC_TIMING0(i));
		writel(timing[1], data->io_base + FL_AC_TIMING1(i));
		writel(timing[2], data->io_base + FL_AC_TIMING2(i));
		writel(timing[3], data->io_base + FL_AC_TIMING3(i));

		/* A380: Illegal data latch occur at setting "rlat" field
		 * of ac timing register from 0 to 1.
		 * read command failed on A380 Linux
		 * Workaround: Set Software Reset(0x184) after
		 * "Trlat" field of AC Timing Register changing.
		 * Fixed in IP version 2.2.0
		 */
		if (tRLAT) {
			if (readl(data->io_base + REVISION_NUM) < 0x020200) {
				writel((1 << i), data->io_base + NANDC_SW_RESET);
				// Wait for the NANDC024 reset is complete
				while(readl(data->io_base + NANDC_SW_RESET) & (1 << i)) ;
			}
		}
	}

	DBGLEVEL2(sp_pnand_dbg("AC Timing 0:0x%08x\n", timing[0]));
	DBGLEVEL2(sp_pnand_dbg("AC Timing 1:0x%08x\n", timing[1]));
	DBGLEVEL2(sp_pnand_dbg("AC Timing 2:0x%08x\n", timing[2]));
	DBGLEVEL2(sp_pnand_dbg("AC Timing 3:0x%08x\n", timing[3]));
}

static void sp_pnand_onfi_set_feature(struct nand_chip *chip, int val)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;

	/* val is sub-feature Parameter P1 (P2~P4 = 0)
	 * b[5:4] means Data interface: 0x0(SDR); 0x1(NV-DDR); 0x2(NV-DDR2)
	 * b[3:0] means Timing mode number
	 */
	writel(val, data->io_base + SPARE_SRAM + (data->cur_chan << data->spare_ch_offset));

	/* 0x1 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0x1;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(LEGACY_FLASH) |\
			CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(4) |\
			CMD_INDEX(ONFI_FIXFLOW_SETFEATURE);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

}

static u32 sp_pnand_onfi_get_feature(struct nand_chip *chip, int type)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u32 val;

	/* 0x1 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0x1;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(type) |\
			CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(4) |\
			CMD_INDEX(ONFI_FIXFLOW_GETFEATURE);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

	val = readl(data->io_base + SPARE_SRAM + (data->cur_chan << data->spare_ch_offset));

	return val;
}

static void sp_pnand_onfi_config_DDR2(struct nand_chip *chip, u8 wr_cyc, u8 rd_cyc)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u8 i;
	u8 param[4];

	//sub-feature Parameter P1/P2 (P3~P4 = 0)
	//P1:
	param[0] = 0;
	//P2: Warmup DQS cycles for Data Input and Data Output
	param[1] = ((wr_cyc & 0x3) << 4) | (rd_cyc & 0x3);
	param[2] = param[3] = 0;

	for (i = 0; i< 4; i ++) {
		writel(param[i], data->io_base + SPARE_SRAM +
			(data->cur_chan << data->spare_ch_offset) + i);
	}

	/* 0x2 is NV-DDR2 Configuration feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0x2;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(LEGACY_FLASH) |\
			CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(4) |\
			CMD_INDEX(ONFI_FIXFLOW_SETFEATURE);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

	// Set the controller
	sp_pnand_set_warmup_cycle(chip, wr_cyc, rd_cyc);
}

static int sp_pnand_onfi_sync(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	u32 val;
	int ret = -1;

	sp_pnand_select_chip(chip, 0);
	val = sp_pnand_onfi_get_feature(chip, LEGACY_FLASH);
	printk("SDR feature for Ch %d, CE %d: 0x%x\n", data->cur_chan, data->sel_chip, val);

	//Check if the SP support DDR interface
	val = readl(data->io_base + FEATURE_1);
	if ((val & DDR_IF_EN) == 0)
		goto out;

	if (data->flash_type == ONFI2) {
		sp_pnand_onfi_set_feature(chip, 0x11);

		val = sp_pnand_onfi_get_feature(chip, ONFI2);
		printk("NV-DDR feature for Ch %d, CE %d: 0x%x\n", data->cur_chan, data->sel_chip, val);
		if (val != 0x1111) {
			goto out;
		}
	}
	else if (data->flash_type == ONFI3) {
		/* set NV-DDR2 Configuration feature address before Timing mode feature */
		sp_pnand_onfi_config_DDR2(chip, 0, 0);

		sp_pnand_onfi_set_feature(chip, 0x21);

		val = sp_pnand_onfi_get_feature(chip, ONFI3);
		printk("NV-DDR2 feature for Ch %d, CE %d: 0x%x\n", data->cur_chan, data->sel_chip, val);
		if (val != 0x2121) {
			// Reset the setting
			sp_pnand_set_warmup_cycle(chip, 0, 0);
			goto out;
		}
	}
	ret = 0;

out:
	return ret;
}

static void sp_pnand_read_raw_id(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u8 id_size = 5;

	data->cur_chan = 0;
	data->sel_chip = 0;

	// Set the flash to Legacy mode, in advance.
	if(data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		sp_pnand_onfi_set_feature(chip, 0x00);
	}

	// Issue the RESET cmd
	cmd_f.cq1 = 0;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(LEGACY_FLASH) |\
			CMD_START_CE(data->sel_chip) | CMD_INDEX(FIXFLOW_RESET);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

	// Issue the READID cmd
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = CMD_COUNT(1);
	cmd_f.cq4 = CMD_FLASH_TYPE(LEGACY_FLASH) | CMD_COMPLETE_EN |\
			CMD_INDEX(FIXFLOW_READID) | CMD_START_CE(data->sel_chip) |\
			CMD_BYTE_MODE | CMD_SPARE_NUM(id_size);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

	memcpy(data->flash_raw_id, data->io_base + SPARE_SRAM + (data->cur_chan << data->spare_ch_offset) , id_size);

	DBGLEVEL2(sp_pnand_dbg("ID@(ch:%d, ce:%d):0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
					data->cur_chan, data->sel_chip, data->flash_raw_id[0],
					data->flash_raw_id[1], data->flash_raw_id[2],
					data->flash_raw_id[3], data->flash_raw_id[4]));
}

static void sp_pnand_calibrate_dqs_delay(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int i, max_dqs_delay = 0;
	int id_size = 5;
	int id_size_ddr = (id_size << 1);
	u8 *p, *golden_p;
	u8 dqs_lower_bound, dqs_upper_bound, state;
	u32 val;

	dqs_lower_bound = dqs_upper_bound = 0;
	p = kmalloc(id_size_ddr, GFP_KERNEL);
	golden_p = kmalloc(id_size_ddr, GFP_KERNEL);

	if(data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		/* Extent the data from SDR to DDR.
		   Ex. If "0xaa, 0xbb, 0xcc, 0xdd, 0xee" is in SDR,
		          "0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee" is in DDR(ONFI).
		*/
		for(i = 0; i< id_size; i++) {
			*(golden_p + (i << 1) + 0) = *(data->flash_raw_id + i);
			*(golden_p + (i << 1) + 1) = *(data->flash_raw_id + i);
		}
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					*golden_p, *(golden_p+1), *(golden_p+2),
					*(golden_p+3), *(golden_p+4), *(golden_p+5)));
		max_dqs_delay = 20;
	}
	else if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2) {
		/* Extent the data from SDR to DDR.
		   Ex. If "0xaa, 0xbb, 0xcc, 0xdd, 0xee" is in SDR,
		          "0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee" is in DDR(TOGGLE).
		*/
		for(i = 0; i< id_size; i++) {
			*(golden_p + (i << 1) + 0) = *(data->flash_raw_id + i);
			*(golden_p + (i << 1) + 1) = *(data->flash_raw_id + i);
		}
		golden_p ++;

		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					*golden_p, *(golden_p+1), *(golden_p+2),
					*(golden_p+3), *(golden_p+4), *(golden_p+5)));
		max_dqs_delay = 18;
	}
	else {
		printk("%s:Type:%d isn't allowed\n", __func__, data->flash_type);
		goto out;
	}


	state = 0;
	for(i = 0; i <= max_dqs_delay; i++) {
		// setting the dqs delay before READID.
		writel(i, data->io_base + DQS_DELAY);
		memset(p, 0, id_size_ddr);

		// Issuing the READID
		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 = CMD_FLASH_TYPE(data->flash_type) | CMD_COMPLETE_EN |\
				CMD_INDEX(FIXFLOW_READID) | CMD_BYTE_MODE |\
				CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(id_size_ddr);

		sp_pnand_issue_cmd(chip, &cmd_f);

		sp_pnand_wait(chip);


		if(data->flash_type == ONFI2 || data->flash_type == ONFI3) {
			memcpy(p, data->io_base + SPARE_SRAM + (data->cur_chan<< data->spare_ch_offset), id_size_ddr);
			if(state == 0 && memcmp(golden_p, p, id_size_ddr) == 0) {
				dqs_lower_bound = i;
				state = 1;
			}
			else if(state == 1 && memcmp(golden_p, p, id_size_ddr) != 0){
				dqs_upper_bound = i - 1;
				break;
			}
		}
		else if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2) {
			memcpy(p, data->io_base + SPARE_SRAM + (data->cur_chan<< data->spare_ch_offset), id_size_ddr-1);

			if(state == 0 && memcmp(golden_p, p, (id_size_ddr - 1)) == 0) {
				dqs_lower_bound = i;
				state = 1;
			}
			else if(state == 1 && memcmp(golden_p, p, (id_size_ddr - 1)) != 0){
				dqs_upper_bound = (i - 1);
				break;
			}

		}
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		DBGLEVEL2(sp_pnand_dbg("ID       :0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*p, *(p+1), *(p+2), *(p+3),
							*(p+4), *(p+5), *(p+6), *(p+7),
							*(p+8) ));
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*golden_p, *(golden_p+1), *(golden_p+2), *(golden_p+3),
							*(golden_p+4), *(golden_p+5),*(golden_p+6), *(golden_p+7),
							*(golden_p+8) ));
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
	}
	// Prevent the dqs_upper_bound is zero when ID still accuracy on the max dqs delay
	if(i == max_dqs_delay + 1)
		dqs_upper_bound = max_dqs_delay;

	printk("Upper:%d & Lower:%d for DQS, then Middle:%d\n",
		dqs_upper_bound, dqs_lower_bound, ((dqs_upper_bound + dqs_lower_bound) >> 1));
	// Setting the middle dqs delay
	val = readl(data->io_base + DQS_DELAY);
	val &= ~0x1F;
	val |= (((dqs_lower_bound + dqs_upper_bound) >> 1) & 0x1F);
	writel(val, data->io_base + DQS_DELAY);
out:
	kfree(p);
	kfree(golden_p);

}

static void sp_pnand_calibrate_rlat(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int i, max_rlat;
	int id_size = 5;
	u8 *p, *golden_p;
	u8 rlat_lower_bound, rlat_upper_bound, state;
	u32 ac_reg0, ac_reg1, val;

	rlat_lower_bound = rlat_upper_bound = 0;
	p = kmalloc(id_size, GFP_KERNEL);
	golden_p = kmalloc(id_size, GFP_KERNEL);

	if(data->flash_type == LEGACY_FLASH) {
		for(i = 0; i< id_size; i++) {
			*(golden_p + i) = *(data->flash_raw_id + i);
		}
	} else {
		printk("%s:Type:%d isn't allowed\n", __func__, data->flash_type);
		goto out;
	}

	ac_reg0 = readl(data->io_base + FL_AC_TIMING0(0));
	max_rlat = (ac_reg0 & 0x1F) + ((ac_reg0 >> 8) & 0xF);
	ac_reg1 = readl(data->io_base + FL_AC_TIMING1(0));
	state = 0;
	for(i = 0; i <= max_rlat; i++) {
		// setting the trlat delay before READID.
		val = (ac_reg1 & ~(0x3F<<16)) | (i<<16);
		writel(val, data->io_base + FL_AC_TIMING1(0));
		memset(p, 0, id_size);

		// Issuing the READID
		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 = CMD_FLASH_TYPE(data->flash_type) | CMD_COMPLETE_EN |\
				CMD_INDEX(FIXFLOW_READID) | CMD_BYTE_MODE |\
				CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(id_size);

		sp_pnand_issue_cmd(chip, &cmd_f);

		sp_pnand_wait(chip);

		memcpy(p, data->io_base + SPARE_SRAM + (data->cur_chan<< data->spare_ch_offset), id_size);
		if(state == 0 && memcmp(golden_p, p, id_size) == 0) {
			rlat_lower_bound = i;
			state = 1;
		}
		else if(state == 1 && memcmp(golden_p, p, id_size) != 0) {
			rlat_upper_bound = i - 1;
			break;
		}

		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		DBGLEVEL2(sp_pnand_dbg("ID       :0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*p, *(p+1), *(p+2), *(p+3), *(p+4)));
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*golden_p, *(golden_p+1), *(golden_p+2), *(golden_p+3), *(golden_p+4)));
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
	}

	// Prevent the dqs_upper_bound is zero when ID still accuracy on the max dqs delay
	if(i == max_rlat + 1)
		rlat_upper_bound = max_rlat;

	DBGLEVEL2(sp_pnand_dbg("Upper:%d & Lower:%d for tRLAT, then Middle:%d\n",
		rlat_upper_bound, rlat_lower_bound, ((rlat_upper_bound + rlat_lower_bound) >> 1)));

	// Setting the middle tRLAT
	val = ac_reg1&~(0x3F<<16);
	val |= ((((rlat_upper_bound + rlat_lower_bound) >> 1) & 0x3F) << 16);
	writel(val, data->io_base + FL_AC_TIMING1(0));
out:
	kfree(p);
	kfree(golden_p);
}

static void sp_pnand_t2_get_feature(struct nand_chip *chip, u8 *buf)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u8 i;

	/* 0x2 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.cq1 = 0x2;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(8) |\
			CMD_INDEX(ONFI_FIXFLOW_GETFEATURE);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

	for (i = 0; i< 4; i++)
		*(buf + i) = readb(data->io_base + SPARE_SRAM +
				   (data->cur_chan << data->spare_ch_offset) + (i << 1));

	printk("T2 Get feature:0x%08x\n", *((int *)(buf)));

}

static void sp_pnand_t2_set_feature(struct nand_chip *chip, u8 *buf)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u8 i;

	for (i = 0; i< 4; i ++) {
		writel(*(buf + i), data->io_base + SPARE_SRAM +
			(data->cur_chan << data->spare_ch_offset) + (i << 1));
		writel(*(buf + i), data->io_base + SPARE_SRAM +
			(data->cur_chan << data->spare_ch_offset) + (i << 1) + 1);
	}

	/* 0x2 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.cq1 = 0x2;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(8) |\
			CMD_INDEX(ONFI_FIXFLOW_SETFEATURE);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);

}


static int sp_pnand_t2_sync(struct nand_chip *chip, u8 wr_cyc, u8 rd_cyc)
{
	u8 param[4];

	// Reset the setting to make sure the accuracy of Get/Set feature
	sp_pnand_set_warmup_cycle(chip, 0, 0);

	sp_pnand_select_chip(chip, 0);

	// Set feature
	param[0] = param[1] = param[2] = param[3] = 0;
	sp_pnand_t2_set_feature(chip, param);

	// Get feature
	sp_pnand_t2_get_feature(chip, param);

	// Set feature
	param[1] = ((wr_cyc & 0x3) << 4) | (rd_cyc & 0x3);
	sp_pnand_t2_set_feature(chip, param);

	// Get feature
	sp_pnand_t2_get_feature(chip, param);

	// Set the controller
	sp_pnand_set_warmup_cycle(chip, wr_cyc, rd_cyc);

	return 0;
}


static int sp_pnand_available_oob(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	int ret = 0;
	int consume_byte, eccbyte, eccbyte_spare;
	int available_spare;

	if (data->useecc < 0)
		goto out;
	if (data->protect_spare != 0)
		data->protect_spare = 1;
	else
		data->protect_spare = 0;

	eccbyte = (data->useecc * 14) / 8;
	if (((data->useecc * 14) % 8) != 0)
		eccbyte++;

	consume_byte = (eccbyte * data->sector_per_page);
	if (data->protect_spare == 1) {

		eccbyte_spare = (data->useecc_spare * 14) / 8;
		if (((data->useecc_spare * 14) % 8) != 0)
			eccbyte_spare++;
		consume_byte += eccbyte_spare;
	}
	consume_byte += CONFIG_BI_BYTE;
	available_spare = data->spare - consume_byte;

	DBGLEVEL2(sp_pnand_dbg(
		"mtd->erasesize:%d, mtd->writesize:%d\n", mtd->erasesize, mtd->writesize));
	DBGLEVEL2(sp_pnand_dbg(
		"page num:%d, data->eccbasft:%d, protect_spare:%d, spare:%d Byte\n",
		mtd->erasesize/mtd->writesize,data->eccbasft, data->protect_spare,
		data->spare));
	DBGLEVEL2(sp_pnand_dbg(
		"consume_byte:%d, eccbyte:%d, eccbytes(spare):%d, useecc:%d bit\n",
		consume_byte, eccbyte, eccbyte_spare, data->useecc));

	/*----------------------------------------------------------
	 * YAFFS require 16 bytes OOB without ECC, 28 bytes with
	 * ECC enable.
	 * BBT require 5 bytes for Bad Block Table marker.
	 */
	if (available_spare >= 4) {
		if (available_spare >= data->max_spare) {
			ret = data->max_spare;
		}
		else {
			if (available_spare >= 64) {
				ret = 64;
			}
			else if (available_spare >= 32) {
				ret = 32;
			}
			else if (available_spare >= 16) {
				ret = 16;
			}
			else if (available_spare >= 8) {
				ret = 8;
			}
			else if (available_spare >= 4) {
				ret = 4;
			}
		}
		printk(KERN_INFO "Available OOB is %d byte, but we use %d bytes in page mode.\n", available_spare, ret);
	} else {
		printk(KERN_INFO "Not enough OOB, try to reduce ECC correction bits.\n");
		printk(KERN_INFO "(Currently ECC setting for Data:%d)\n", data->useecc);
		printk(KERN_INFO "(Currently ECC setting for Spare:%d)\n", data->useecc_spare);
	}
out:
	return ret;
}

static u8 sp_pnand_read_byte(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	u32 lv;
	u8 b = 0;

	switch (data->cur_cmd) {
	case NAND_CMD_READID:
		b = readb(data->io_base + SPARE_SRAM + (data->cur_chan << data->spare_ch_offset) + data->byte_ofs);
		data->byte_ofs += 1;
		if (data->byte_ofs == data->max_spare)
			data->byte_ofs = 0;
		break;
	case NAND_CMD_STATUS:
		lv = readl(data->io_base + READ_STATUS0);
		lv = lv >> (data->cur_chan * 8);
		b = (lv & 0xFF);
		break;
	}
	return b;
}


static void sp_pnand_cmdfunc(struct nand_chip *chip, unsigned command,
				    int column, int page_addr)
{
//	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int real_pg, cmd_sts;
	u8 id_size = 5;

	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type);
	data->cur_cmd = command;
	if (page_addr != -1)
		data->page_addr = page_addr;
	if (column != -1)
		data->column = column;

	switch (command) {
	case NAND_CMD_READID:
		DBGLEVEL2(sp_pnand_dbg( "Read ID@(CH:%d, CE:%d)\n", data->cur_chan, data->sel_chip));
		data->byte_ofs = 0;
		// ID size is doubled when the mode is DDR.
		if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2 ||
		   data->flash_type == ONFI2 || data->flash_type == ONFI3) {
			id_size = (id_size << 1);
		}

		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(data->sel_chip) | CMD_BYTE_MODE |\
				CMD_SPARE_NUM(id_size) | CMD_INDEX(FIXFLOW_READID);

		cmd_sts = sp_pnand_issue_cmd(chip, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(chip);
		else
			printk(KERN_ERR "Read ID err\n");

		break;
	case NAND_CMD_RESET:
		DBGLEVEL2(sp_pnand_dbg("Cmd Reset@(CH:%d, CE:%d)\n", data->cur_chan, data->sel_chip));

		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = 0;
		cmd_f.cq4 |= CMD_START_CE(data->sel_chip);
		if (data->flash_type == ONFI2 || data->flash_type == ONFI3)
			cmd_f.cq4 |= CMD_INDEX(ONFI_FIXFLOW_SYNCRESET);
		else
			cmd_f.cq4 |= CMD_INDEX(FIXFLOW_RESET);

		cmd_sts = sp_pnand_issue_cmd(chip, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(chip);
		else
			printk(KERN_ERR "Reset Flash err\n");

		break;
	case NAND_CMD_STATUS:
		DBGLEVEL2(sp_pnand_dbg( "Read Status\n"));

		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(data->sel_chip) | CMD_INDEX(FIXFLOW_READSTATUS);

		cmd_sts = sp_pnand_issue_cmd(chip, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(chip);
		else
			printk(KERN_ERR "Read Status err\n");

		break;
	case NAND_CMD_ERASE1:
		real_pg = data->page_addr;
		DBGLEVEL2(sp_pnand_dbg(
			"Erase Page: 0x%x, Real:0x%x\n", data->page_addr, real_pg));

		cmd_f.cq1 = real_pg;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(data->sel_chip) | CMD_SCALE(1);

		if (data->large_page) {
			cmd_f.row_cycle = ROW_ADDR_3CYCLE;
			cmd_f.col_cycle = COL_ADDR_2CYCLE;
			cmd_f.cq4 |= CMD_INDEX(LARGE_FIXFLOW_ERASE);
		} else {
			cmd_f.row_cycle = ROW_ADDR_2CYCLE;
			cmd_f.col_cycle = COL_ADDR_1CYCLE;
			cmd_f.cq4 |= CMD_INDEX(SMALL_FIXFLOW_ERASE);
		}

		/* Someone may be curious the following snippet that
		* sp_pnand_issue_cmd doesn't be followed by
		* sp_pnand_wait.
		* Waiting cmd complete will be call on the mtd upper layer via
		* the registered chip->waitfunc.
		*/
		cmd_sts = sp_pnand_issue_cmd(chip, &cmd_f);
		if(cmd_sts)
			printk(KERN_ERR "Erase block err\n");

		break;
	case NAND_CMD_ERASE2:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_SEQIN:
	default:
		DBGLEVEL2(sp_pnand_dbg( "Unimplemented command (cmd=%u)\n", command));
		break;
	}
}

void sp_pnand_select_chip(struct nand_chip *chip, int cs)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	data->cur_chan = 0;
	data->sel_chip = 0;
	DBGLEVEL2(sp_pnand_dbg("==>chan = %d, ce = %d\n", data->cur_chan, data->sel_chip));
}

static void sp_nand_set_ecc(struct nand_chip *chip)
{
	u32 val;
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	if (data->useecc > 0) {
		DBGLEVEL1(sp_pnand_dbg("ECC correction bits: %d\n", data->useecc));
		writel(0x01010101, data->io_base + ECC_THRES_BITREG1);
		writel(0x01010101, data->io_base + ECC_THRES_BITREG2);
		val = (data->useecc - 1) | ((data->useecc - 1) << 8) |
			((data->useecc - 1) << 16) | ((data->useecc - 1) << 24);
		writel(val, data->io_base + ECC_CORRECT_BITREG1);
		writel(val, data->io_base + ECC_CORRECT_BITREG2);

		val = readl(data->io_base + ECC_CONTROL);
		val &= ~ECC_BASE;
		if (data->eccbasft > 9)
			val |= ECC_BASE;
		val |= (ECC_EN(0xFF) | ECC_ERR_MASK(0xFF));
		writel(val, data->io_base + ECC_CONTROL);
		writel(ECC_INTR_THRES_HIT | ECC_INTR_CORRECT_FAIL, data->io_base + ECC_INTR_EN);
	} else {
		DBGLEVEL1(sp_pnand_dbg("ECC disabled\n"));
		writel(0, data->io_base + ECC_THRES_BITREG1);
		writel(0, data->io_base + ECC_THRES_BITREG2);
		writel(0, data->io_base + ECC_CORRECT_BITREG1);
		writel(0, data->io_base + ECC_CORRECT_BITREG2);

		val = readl(data->io_base + ECC_CONTROL);
		val &= ~ECC_BASE;
		val &= ~(ECC_EN(0xFF) | ECC_ERR_MASK(0xFF));
		val |= ECC_NO_PARITY;
		writel(val, data->io_base + ECC_CONTROL);
	}

	// Enable the Status Check Intr
	val = readl(data->io_base + INTR_ENABLE);
	val &= ~INTR_ENABLE_STS_CHECK_EN(0xff);
	val |= INTR_ENABLE_STS_CHECK_EN(0xff);
	writel(val, data->io_base + INTR_ENABLE);

	// Setting the ecc capability & threshold for spare
	writel(0x01010101, data->io_base + ECC_THRES_BIT_FOR_SPARE_REG1);
	writel(0x01010101, data->io_base + ECC_THRES_BIT_FOR_SPARE_REG2);
	val = (data->useecc_spare-1) | ((data->useecc_spare-1) << 8) |
		((data->useecc_spare-1) << 16) | ((data->useecc_spare-1) << 24);
	writel(val, data->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG1);
	writel(val, data->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG2);
}

void sp_pnand_set_actiming(struct nand_chip *chip)
{
	/* TODO: calibrate the DQS delay for Sync */
	flashtype type = LEGACY_FLASH;
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	/*----------------------------------------------------------
	 * ONFI synch mode means High Speed. If fails to change to
	 * Synch mode, then use flash as Async mode(Normal speed) and
	 * use LEGACY_LARGE fix flow.
	 */
	if (type == ONFI2 || type == ONFI3) {
		data->flash_type = type;
		if (sp_pnand_onfi_sync(chip) == 0) {
			sp_pnand_calc_timing(chip);
			sp_pnand_calibrate_dqs_delay(chip);
		}
		else{
			data->flash_type = LEGACY_FLASH;
		}
	}

	// Toggle & ONFI flash has set the proper timing before READ ID.
	// We don't do that twice.
	if(data->flash_type == LEGACY_FLASH) {
		sp_pnand_calc_timing(chip);
		sp_pnand_calibrate_rlat(chip);
	}
	else if(data->flash_type == TOGGLE2) {
#if defined(CONFIG_PNANDC_SAMSUNG_K9GCGY8S0A)
		sp_pnand_t2_sync(chip, 0, 2);
#elif defined(CONFIG_PNANDC_TOSHIBA_TH58TEG7DCJBA4C) ||\
	defined(CONFIG_PNANDC_TOSHIBA_TH58TFT0DDLBA8H)
		sp_pnand_t2_sync(chip, 3, 3);
#endif
	}
}


static int sp_pnand_attach_chip(struct nand_chip *chip)
{
	struct nand_memory_organization *memorg;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	struct nand_device *base = &chip->base;
	const struct nand_ecc_props *requirements = nanddev_get_ecc_requirements(base);
	u32 val;
	int i;

	data->eccbasft = fls(requirements->step_size) - 1;
	data->useecc = requirements->strength;
	data->protect_spare = 1;
	data->useecc_spare = 4;
	data->sector_per_page = mtd->writesize >> data->eccbasft;
	data->spare = mtd->oobsize;

	memorg = nanddev_get_memorg(&chip->base);

	//usually, spare size is 1/32 page size
	if (data->spare < (mtd->writesize >> 5))
		data->spare = (mtd->writesize >> 5);

	val = readl(data->io_base + MEM_ATTR_SET);
	val &= ~(0x7 << 16);

	if(mtd->writesize > 512) {
		data->large_page = 1;
		val |= ((fls(mtd->writesize) - 11) << 16); //bit[18:16] 1/2/3/4 -> PageSize=2k/4k/8k/16k
	} else {
		data->large_page = 0;
		val |= PG_SZ_512;
	}

	val &= ~(0x3FF << 2);
	val |= ((mtd->erasesize / mtd->writesize - 1) << 2);
//lichun@add, For BI_byte test
	val &= ~BI_BYTE_MASK;
	val |= (CONFIG_BI_BYTE << 19);
//~lichun
	writel(val, data->io_base + MEM_ATTR_SET);

	val = readl(data->io_base + MEM_ATTR_SET2);
	val &= ~(0x3FF << 16);
	val |=  VALID_PAGE((mtd->erasesize / mtd->writesize - 1));
	writel(val, data->io_base + MEM_ATTR_SET2);

	i = sp_pnand_available_oob(mtd);
	if (likely(i >= 4)) {
		if (i > data->max_spare)
			memorg->oobsize = data->max_spare;
		else
			memorg->oobsize = i;
		data->spare = memorg->oobsize;
	} else
		return -ENXIO;

	DBGLEVEL1(sp_pnand_dbg("total oobsize: %d\n", memorg->oobsize));

	sp_nand_set_ecc(chip);

	ecc->engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;
	ecc->size = requirements->step_size;
	ecc->bytes = (data->useecc * 14) / 8;//why is 0
	ecc->strength = requirements->strength;

	ecc->read_oob = sp_pnand_read_oob;
	ecc->write_oob = sp_pnand_write_oob;
	ecc->read_page_raw = sp_pnand_read_page;

	if (data->dmac) {
		ecc->read_page = sp_pnand_read_page_by_dma;
		ecc->write_page = sp_pnand_write_page_by_dma;
		DBGLEVEL1(sp_pnand_dbg("Transfer: DMA\n"));
	} else {
		ecc->read_page = sp_pnand_read_page;
		ecc->write_page = sp_pnand_write_page;
		DBGLEVEL1(sp_pnand_dbg("Transfer: PIO\n"));
	}

	mtd_set_ooblayout(mtd, &sp_pnand_ooblayout_ops);

	DBGLEVEL2(sp_pnand_dbg("Use nand flash %s\n", mtd->name));
	DBGLEVEL2(sp_pnand_dbg("data->eccbasft: %d\n", data->eccbasft));
	DBGLEVEL2(sp_pnand_dbg("data->useecc: %d\n", data->useecc));
	DBGLEVEL2(sp_pnand_dbg("data->protect_spare: %d\n", data->protect_spare));
	DBGLEVEL2(sp_pnand_dbg("data->useecc_spare: %d\n", data->useecc_spare));
	DBGLEVEL2(sp_pnand_dbg("data->spare: %d\n", data->spare));
	DBGLEVEL2(sp_pnand_dbg("data->flash_type: %d\n", data->flash_type));
	DBGLEVEL2(sp_pnand_dbg("data->sector_per_page: %d\n", data->sector_per_page));

	return 0;
}

static const struct nand_controller_ops sp_pnand_controller_ops = {
	.attach_chip = sp_pnand_attach_chip,
};

static void sp_clk_disable_unprepare(void *data)
{
	clk_disable_unprepare(data);
}

static void sp_reset_control_assert(void *data)
{
	reset_control_assert(data);
}

static int sp_pnand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sp_pnand_data *data;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct resource *r;
	int ret, chipnum;
	uint64_t size, max_sz = -1;
	int i, type;
	u32 val;

	ret = chipnum = size = type = 0;
	/* Allocate memory for the device structure (and zero it) */

	/*
	 * Initialize the parameter
	 */
	data = devm_kzalloc(dev, sizeof(struct sp_pnand_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "failed to allocate device structure.\n");
		ret = -ENOMEM;
	}

	chip = &data->chip;
	chip->controller = &data->controller;

	/*reset*/
	data->rstc = devm_reset_control_get(dev, NULL);
	if (IS_ERR(data->rstc))
		return dev_err_probe(dev, PTR_ERR(data->rstc), "Failed to get reset\n");

	reset_control_deassert(data->rstc);

	ret = devm_add_action_or_reset(dev, sp_reset_control_assert, data->rstc);
	if (ret)
		return ret;

	/* clk */
	data->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(data->clk))
		return dev_err_probe(dev, PTR_ERR(data->clk), "Failed to get clock\n");
	printk("core_clk 0x%x 0x%x\n", (u32)data->clk, *(u32 *)data->clk);

	ret = clk_prepare_enable(data->clk);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to enable clock\n");

	ret = devm_add_action_or_reset(dev, sp_clk_disable_unprepare, data->clk);
	if (ret)
		return ret;


	data->io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->io_base)) {
		dev_err(dev, "Failed to ioremap for register.\n");
		return PTR_ERR(data->io_base);
	}

	DBGLEVEL2(sp_pnand_dbg("data->io_base:0x%08lx", (unsigned long)data->io_base));

	r = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	chip->legacy.IO_ADDR_R = devm_ioremap_resource(dev, r);
	if (IS_ERR(chip->legacy.IO_ADDR_R)) {
		dev_err(dev, "Failed to ioremap for data port.\n");
		return PTR_ERR(chip->legacy.IO_ADDR_R);
	}

	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency", &data->clkfreq))
		data->clkfreq = CONFIG_SP_CLK_100M;

	ret = clk_set_rate(data->clk, data->clkfreq);
	if (ret) {
		dev_err(dev, "Failed to set clk rate\n");
		return ret;
	}

	data->clkfreq = clk_get_rate(data->clk);
	DBGLEVEL1(sp_pnand_dbg("data->clkfreq %d\n", data->clkfreq));

	data->dmac = NULL;
#if 1//turn off the DMA mode
	/* request dma channel */
	data->dmac = dma_request_chan(dev, "rxtx");
	if (IS_ERR(data->dmac)) {
		ret = PTR_ERR(data->dmac);
		data->dmac = NULL;
		return dev_err_probe(dev, ret, "Failed to request DMA channel\n");
	} else {
		struct dma_slave_config dmac_cfg = {};

		dmac_cfg.src_addr = (phys_addr_t)r->start;
		dmac_cfg.dst_addr = dmac_cfg.src_addr;
		dmac_cfg.src_addr_width = DMA_SLAVE_BUSWIDTH_16_BYTES;
		dmac_cfg.dst_addr_width = dmac_cfg.src_addr_width;
		dmac_cfg.src_maxburst = 16;
		dmac_cfg.dst_maxburst = 16;
		ret = dmaengine_slave_config(data->dmac, &dmac_cfg);
		if (ret < 0) {
			dev_err(dev, "Failed to configure DMA channel\n");
			dma_release_channel(data->dmac);
			data->dmac = NULL;
		}
	}
#endif
	nand_controller_init(&data->controller);
	data->controller.ops = &sp_pnand_controller_ops;

	data->inverse = 0;		/* disable */
	data->scramble = 0;		/* disable */
	data->seed_val = 0;
	data->max_spare = 128;
	data->spare_ch_offset = 7;	/* shift 7 means 0x80*/
	data->flash_type = LEGACY_FLASH;
	// Reset the HW
	// Note: We can't use the function of sp_pnand_soft_reset to reset the hw
	//       because the private data field of sp_pnand_data is null.
	writel(1, data->io_base + GLOBAL_RESET);
	while (readl(data->io_base + GLOBAL_RESET)) ;

	val = BUSY_RDY_LOC(6) | CMD_STS_LOC(0) | CE_NUM(2);
	if (data->inverse)
		val |= DATA_INVERSE;
	if (data->scramble)
		val |= DATA_SCRAMBLER;

	writel(val, data->io_base + GENERAL_SETTING);

	if (data->scramble) {
		/* Support FW to program scramble seed */
		val = readl(data->io_base + NANDC_EXT_CTRL);
		for(i = 0; i < MAX_CHANNEL; i++)
			val |= SEED_SEL(i);
		writel(val, data->io_base + NANDC_EXT_CTRL);
		/* random set, b[13:0] */
		data->seed_val = 0x2fa5;
	}

	val = readl(data->io_base + AHB_SLAVEPORT_SIZE);
	val &= ~0xFFF0FF;
	val |= AHB_SLAVE_SPACE_32KB;//64K?
	for(i = 0; i < MAX_CHANNEL; i++)
		val |= AHB_PREFETCH(i);
	val |= AHB_PRERETCH_LEN(128);
	writel(val, data->io_base + AHB_SLAVEPORT_SIZE);

	nand_set_controller_data(&data->chip, data);
	mtd = nand_to_mtd(&data->chip);
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;
	/* Set the name same as uboot cmdline */
	mtd->name = NAME_DEFINE_IN_UBOOT;
	data->dev = &pdev->dev;
	chip->legacy.IO_ADDR_W = chip->legacy.IO_ADDR_R;
	chip->legacy.select_chip = sp_pnand_select_chip;
	chip->legacy.cmdfunc = sp_pnand_cmdfunc;
	chip->legacy.read_byte = sp_pnand_read_byte;
	chip->legacy.waitfunc = sp_pnand_wait;
	chip->legacy.chip_delay = 0;
	chip->options = NAND_NO_SUBPAGE_WRITE;// | NAND_OWN_BUFFERS;
#if 0
	// FIXME: Expect bad block management properly work after removing the option.
	chip->options |= NAND_SKIP_BBTSCAN | NAND_NO_BBM_QUIRK;
#endif
	chip->options |= NAND_USES_DMA;
	chip->bbt_options = NAND_BBT_USE_FLASH | NAND_BBT_NO_OOB;
	platform_set_drvdata(pdev, data);

	// Set the default AC timing/Warmup cyc for sp_pnand.
	// The register of AC timing/Warmup  keeps the value
	// set before although the Global Reset is set.
	sp_pnand_set_default_timing(chip);
	sp_pnand_set_warmup_cycle(chip, 0, 0);
#if 0
	// Disable the scan state for sp_pnand_select_chip
	data->scan_state = 0;

	// Read the raw id to calibrate the DQS delay for Sync. latching(DDR)
	sp_pnand_read_raw_id(chip);
	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2) {
		sp_pnand_calc_timing(chip);
		sp_pnand_calibrate_dqs_delay(chip);
	}

	/*--------------------------------------------------------
	 * ONFI flash must work in Asynch mode for READ ID command.
	 * Switch it back to Legacy.
	 */
	if (data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		type = data->flash_type;
		data->flash_type = LEGACY_FLASH;
	}

	// Enable the scan state for sp_pnand_select_chip
	data->scan_state = 1;
	// Reset the flash delay set before.
	if(data->flash_type == TOGGLE2)
		sp_pnand_t2_sync(chip, 0, 0);
#endif
	/* Scan to find existance of the device */
	for (i = startchn; i < MAX_CHANNEL; i++) {
		printk(KERN_INFO "Scan Channel %d...\n", i);
		data->cur_chan = i;
		if (!nand_scan_with_ids(chip, MAX_CE, (struct nand_flash_dev *)sp_pnand_ids)) {
			if (((max_sz - size) > mtd->size)
			    && ((chipnum + nanddev_ntargets(&chip->base)) <= NAND_MAX_CHIPS)) {
				data->valid_chip[i] = nanddev_ntargets(&chip->base);
				chipnum += nanddev_ntargets(&chip->base);
				size += (chipnum * nanddev_target_size(&chip->base));
			} else {
				printk(KERN_INFO "Can not accept more flash chips.\n");
				break;
			}
		}
	}
	// Disable the scan-state for sp_pnand_select_chip
	//data->scan_state = 0;

	if (chipnum == 0) {
		return -ENXIO;
	}

	mtd->size = size;

	sp_pnand_set_actiming(chip);

	ret = mtd_device_register(mtd, NULL, 0);
	if (ret) {
		dev_err(dev, "Failed to register mtd device\n");
		nand_cleanup(chip);
		return ret;
	}

	return 0;
}

/*
 * Remove a NAND device.
 */
static int sp_pnand_remove(struct platform_device *pdev)
{
	struct sp_pnand_data *data = platform_get_drvdata(pdev);
	struct nand_chip *chip = &data->chip;
	int ret;

	iounmap(chip->legacy.IO_ADDR_R);

	ret = mtd_device_unregister(nand_to_mtd(chip));
	WARN_ON(ret);
	nand_cleanup(chip);

	iounmap(data->io_base);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static u32 regs[124];// 7 + 55 + 60 + 1 + 1
static int sp_pnand_suspend(struct device *dev)
{
	struct sp_pnand_data *data = dev_get_drvdata(dev);
	struct nand_chip *chip = &data->chip;
	void __iomem *base = data->io_base;
	u32 i, index;

	//printk(">>>>>> [DEBUG] PNAND suspend <<<<<<\n");

	/* Save the reg val */
	for (i = 0x8; i < 0x24; i += 4) {
		index = (i - 0x8) / 4;
		regs[index] = readl(data->io_base + i);
	}
	for (i = 0x104; i < 0x1E4; i += 4) {
		index = (i - 0x104) / 4;
		regs[7 + index] = readl(data->io_base + i);
	}
	for (i = 0x300; i < 0x3F0; i += 4) {
		index = (i - 0x300) / 4;
		regs[62 + index] = readl(data->io_base + i);
	}
	regs[122] = readl(data->io_base + 0x42C);
	regs[123] = readl(data->io_base + 0x508);

	return 0;
}

static int sp_pnand_resume(struct device *dev)
{
	struct sp_pnand_data *data = dev_get_drvdata(dev);
	struct nand_chip *chip = &data->chip;
	void __iomem *base = data->io_base;
	u32 i, index;

	//printk(">>>>>> [DEBUG] PNAND resume <<<<<<\n");

	/* Restore the reg val */
	for (i = 0x8; i < 0x24; i += 4) {
		index = (i - 0x8) / 4;
		writel(regs[index], data->io_base + i);
	}
	for (i = 0x104; i < 0x1E4; i += 4) {
		index = (i - 0x104) / 4;
		writel(regs[7 + index], data->io_base + i);
	}
	for (i = 0x300; i < 0x3F0; i += 4) {
		index = (i - 0x300) / 4;
		writel(regs[62 + index], data->io_base + i);
	}
	writel_relaxed(regs[122], data->io_base + 0x42C);
	writel_relaxed(regs[123], data->io_base + 0x508);

	sp_pnand_abort(chip);

	return 0;
}
#endif

static const struct of_device_id sp_pnand_dt_ids[] = {
	{ .compatible = "sunplus,sp7350-para-nand" },
	{ }
};

MODULE_DEVICE_TABLE(of, sp_pnand_dt_ids);

static const struct dev_pm_ops sp_pnand_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sp_pnand_suspend,
				sp_pnand_resume)
};

static struct platform_driver sp_pnand_driver __refdata = {
	.probe	= sp_pnand_probe,
	.remove	= sp_pnand_remove,
	.driver	= {
		.name = "sp7350-para-nand",
		.owner = THIS_MODULE,
		.pm		= &sp_pnand_pm_ops,
		.of_match_table = of_match_ptr(sp_pnand_dt_ids),
	},
};

module_platform_driver(sp_pnand_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sunplus Technology Corporation");
MODULE_DESCRIPTION("Sunplus Parallel NAND Flash Controller Driver");
