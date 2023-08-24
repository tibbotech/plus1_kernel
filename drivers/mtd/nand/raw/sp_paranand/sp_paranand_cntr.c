// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus Parallel NAND Cotroller driver
 *
 * Copyright Sunplus Technology Co., Ltd.
 *
 * Derived from:
 *	Copyright (C) 2019-2021 Faraday Technology Corp.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
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
#include <linux/mtd/bbm.h>
#include <linux/iopoll.h>

#include "sp_paranand.h"

#define NFC_DEFAULT_TIMEOUT_MS 1000

void sp_pnand_regdump(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	volatile u32 val;
	u32 i;
	printk("===================================================\n");
	printk("0x0000: ");
	for(i = 0; i< 0x50; i=i+4){
		if(i != 0 && (i%0x10)==0){
			printk("\n");
			printk("0x%04x: ", i);
		}
		val = readl(data->io_base + i);
		printk("0x%08x ", val);
	}
	for(i = 0x100; i< 0x1B0; i=i+4){
		if(i != 0 && (i%0x10)==0){
			printk("\n");
			printk("0x%04x: ", i);
		}
		val = readl(data->io_base + i);
		printk("0x%08x ", val);
	}
	for(i = 0x200; i< 0x530; i=i+4){
		if(i != 0 && (i%0x10)==0){
			printk("\n");
			printk("0x%04x: ", i);
		}
		val = readl(data->io_base + i);
		printk("0x%08x ", val);
	}
	printk("\n===================================================\n");
}

static inline void sp_pnand_set_row_col_addr(
		struct sp_pnand_data *data, int row, int col)
{
	int val;

	val = readl(data->io_base + MEM_ATTR_SET);
	val &= ~(0x7 << 12);
	val |= (ATTR_ROW_CYCLE(row) | ATTR_COL_CYCLE(col));

	writel(val, data->io_base + MEM_ATTR_SET);
}

static int sp_pnand_check_cmdq(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	unsigned long timeo = jiffies;
	u32 status;
	int ret;

	ret = -EIO;
	timeo += HZ;
	while (time_before(jiffies, timeo)) {
		status = readl(data->io_base + CMDQUEUE_STATUS);
		if ((status & CMDQUEUE_STATUS_FULL(data->cur_chan)) == 0) {
			ret = 0;
			break;
		}
		cond_resched();
	}
	if(ret != 0)
		printk(KERN_ERR "check cmdq timeout");
	return ret;
}

static void sp_pnand_soft_reset(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	writel((1 << data->cur_chan), data->io_base + NANDC_SW_RESET);
	// Wait for the NANDC024 software reset is complete
	while(readl(data->io_base + NANDC_SW_RESET) & (1 << data->cur_chan)) ;
}

void sp_pnand_abort(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;

	// Abort the operation

	// Step1. Flush Command queue & Poll whether Command queue is ready
	writel((1 << data->cur_chan), data->io_base + CMDQUEUE_FLUSH);
	// Wait until flash is ready!!
	while(!(readl(data->io_base + DEV_BUSY) & (1 << data->cur_chan))) ;

	// Step2. Reset Nandc & Poll whether the "Reset of NANDC" returns to 0
	sp_pnand_soft_reset(chip);

	// Step3. Reset the BMC region
	writel(1 << (data->cur_chan), data->io_base + BMC_REGION_SW_RESET);

	// Step4. Reset the AHB data slave port 0
	if (readl(data->io_base + FEATURE_1) & AHB_SLAVE_MODE_ASYNC(0)) {
		writel(1 << 0, data->io_base + AHB_SLAVE_RESET);
		while(readl(data->io_base + AHB_SLAVE_RESET) & (1 << 0)) ;
	}

	// Step5. Issue the Reset cmd to flash
	cmd_f.cq1 = 0;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
	            CMD_START_CE(data->sel_chip);
	if (data->flash_type == ONFI2 || data->flash_type == ONFI3)
		cmd_f.cq4 |= CMD_INDEX(ONFI_FIXFLOW_SYNCRESET);
	else
		cmd_f.cq4 |= CMD_INDEX(FIXFLOW_RESET);

	sp_pnand_issue_cmd(chip, &cmd_f);

	sp_pnand_wait(chip);
}

int sp_pnand_issue_cmd(struct nand_chip *chip, struct cmd_feature *cmd_f)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int status;

	status = sp_pnand_check_cmdq(chip);
	if (status == 0) {
		sp_pnand_set_row_col_addr(data, cmd_f->row_cycle, cmd_f->col_cycle);

		writel(cmd_f->cq1, data->io_base + CMDQUEUE1(data->cur_chan));
		writel(cmd_f->cq2, data->io_base + CMDQUEUE2(data->cur_chan));
		writel(cmd_f->cq3, data->io_base + CMDQUEUE3(data->cur_chan));
		writel(cmd_f->cq4, data->io_base + CMDQUEUE4(data->cur_chan)); // Issue cmd
	}
	return status;
}

void sp_pnand_set_default_timing(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int i;
	u32 timing[4];

	timing[0] = 0x0f1f0f1f;
	timing[1] = 0x00007f7f;
	timing[2] = 0x7f7f7f7f;
	timing[3] = 0xff1f001f;

	for (i = 0;i < MAX_CHANNEL;i++) {
		writel(timing[0], data->io_base + FL_AC_TIMING0(i));
		writel(timing[1], data->io_base + FL_AC_TIMING1(i));
		writel(timing[2], data->io_base + FL_AC_TIMING2(i));
		writel(timing[3], data->io_base + FL_AC_TIMING3(i));
	}
}

int sp_pnand_wait(struct nand_chip *chip)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);

	unsigned long timeo;
	int ret;
	volatile u32 intr_sts, ecc_intr_sts;
	volatile u8 cmd_comp_sts, sts_fail_sts;
	volatile u8 ecc_sts_for_data;
	volatile u8 ecc_sts_for_spare;

	data->cmd_status = CMD_SUCCESS;
	ret = NAND_STATUS_FAIL;
	timeo = jiffies;
	timeo += 5 * HZ;

	//No command
	if (readl(data->io_base + CMDQUEUE_STATUS) & CMDQUEUE_STATUS_EMPTY(data->cur_chan)) {
		ret = NAND_STATUS_READY;
		goto out;
	}

	do {
		intr_sts = readl(data->io_base + INTR_STATUS);
		cmd_comp_sts = ((intr_sts & 0xFF0000) >> 16);

		if (likely(cmd_comp_sts & (1 << (data->cur_chan)))) {
			// Clear the intr status when the cmd complete occurs.
			writel(intr_sts, data->io_base + INTR_STATUS);

			ret = NAND_STATUS_READY;
			sts_fail_sts = (intr_sts & 0xFF);

			if (sts_fail_sts & (1 << (data->cur_chan))) {
				printk(KERN_ERR "STATUS FAIL@(pg_addr:0x%x)\n", data->page_addr);
				data->cmd_status |= CMD_STATUS_FAIL;
				ret = CMD_STATUS_FAIL;
				sp_pnand_abort(chip);
			}

			if (data->read_state) {
				ecc_intr_sts = readl(data->io_base + ECC_INTR_STATUS);
				// Clear the ECC intr status
				writel(ecc_intr_sts, data->io_base + ECC_INTR_STATUS);
				// ECC failed on data
				ecc_sts_for_data = (ecc_intr_sts & 0xFF);
				if (ecc_sts_for_data & (1 << data->cur_chan)) {
					data->cmd_status |= CMD_ECC_FAIL_ON_DATA;
					ret = NAND_STATUS_FAIL;
				}

				ecc_sts_for_spare = ((ecc_intr_sts & 0xFF0000) >> 16);
				// ECC failed on spare
				if (ecc_sts_for_spare & (1 << data->cur_chan)) {
					data->cmd_status |= CMD_ECC_FAIL_ON_SPARE;
					ret = NAND_STATUS_FAIL;
				}
			}
			goto out;
		}
		cond_resched();
	} while (time_before(jiffies, timeo));

	DBGLEVEL1(sp_pnand_dbg("nand wait time out\n"));
	sp_pnand_regdump(chip);
out:
	return ret;
}

void sp_pnand_fill_prog_code(struct nand_chip *chip, int location,
		int cmd_index) {
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	writeb((cmd_index & 0xff), data->io_base + PROGRAMMABLE_OPCODE+location);
}

void sp_pnand_fill_prog_flow(struct nand_chip *chip, int *program_flow_buf,
		int buf_len) {
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	u8 *p = (u8 *)program_flow_buf;

	int i;
	for(i = 0; i < buf_len; i++) {
		writeb( *(p+i), data->io_base + PROGRAMMABLE_FLOW_CONTROL + i);
	}
}

int byte_rd(struct nand_chip *chip, int real_pg, int col, int len,
				u_char *spare_buf)
{
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f = {0};
	int status, i, tmp_col, tmp_len, cmd_len, ret;
	u_char *buf;
	int j;

	ret = 0;
	tmp_col = col;
	tmp_len = len;

	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2 ||
		data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		if(col & 0x1) {
			tmp_col --;

			if (tmp_len & 0x1)
				tmp_len ++;
			else
				tmp_len += 2;
		}
		else if(tmp_len & 0x1) {
			tmp_len ++;
		}
	}

	buf = (u_char *)vmalloc(tmp_len);
	if (!buf)
                return -ENOMEM;

	for(i = 0; i < tmp_len; i += data->max_spare) {
		if(tmp_len - i >= data->max_spare)
			cmd_len = data->max_spare;
		else
			cmd_len = tmp_len - i;

		cmd_f.row_cycle = ROW_ADDR_3CYCLE;
		cmd_f.col_cycle = COL_ADDR_2CYCLE;
		cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
		cmd_f.cq2 = CMD_EX_SPARE_NUM(cmd_len) | SCR_SEED_VAL2(data->seed_val);
		cmd_f.cq3 = CMD_COUNT(1) | tmp_col;
		cmd_f.cq4 = CMD_COMPLETE_EN | CMD_BYTE_MODE |\
				CMD_FLASH_TYPE(data->flash_type) |\
				CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(cmd_len) |\
				CMD_INDEX(LARGE_FIXFLOW_BYTEREAD);

		status = sp_pnand_issue_cmd(chip, &cmd_f);
		if(status < 0) {
			ret = 1;
			break;
		}
		sp_pnand_wait(chip);
#if 0
		timeo = jiffies;
		timeo += HZ/1000;

		do {
		} while (time_before(jiffies, timeo));
#endif

		for(j = 0; j < cmd_len + 1; j += 4) {
			if (j / 4 == cmd_len / 4) {
				memcpy(buf + i + j, data->io_base + SPARE_SRAM + j, cmd_len % 4);
				break;
			}
			memcpy(buf + i + j, data->io_base + SPARE_SRAM + j, 4);
		}

		tmp_col += cmd_len;
	}

	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2 ||
		data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		if(col & 0x1)
			memcpy(spare_buf, buf + 1, len);
		else
			memcpy(spare_buf, buf, len);
	}
	else
		memcpy(spare_buf, buf, len);

	vfree(buf);
	return ret;
}

int rd_pg_w_oob(struct nand_chip *chip, int real_pg,
			uint8_t *data_buf, u8 *spare_buf)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int status;
	int i;
	u32 *lbuf;

	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(data->spare) | SCR_SEED_VAL2(data->seed_val);
	cmd_f.cq3 = CMD_COUNT(mtd->writesize >> data->eccbasft) |
			(data->column & 0xFF);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(data->spare) |\
			CMD_INDEX(LARGE_FIXFLOW_PAGEREAD_W_SPARE);

	status = sp_pnand_issue_cmd(chip, &cmd_f);
	if(status < 0)
		return 1;

	sp_pnand_wait(chip);

	lbuf = (u32 *) data_buf;
	for (i = 0; i < mtd->writesize; i += 4)
		*lbuf++ = *(volatile unsigned *)(chip->legacy.IO_ADDR_R);

	for(i = 0; i < mtd->oobsize; i += 4) {
		memcpy(spare_buf + i, data->io_base + SPARE_SRAM + i, 4);
	}

	return 0;
}

int rd_oob(struct nand_chip *chip, int real_pg, u8 *spare_buf)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int status;
	int i;

	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(data->spare) | SCR_SEED_VAL2(data->seed_val);
	cmd_f.cq3 = CMD_COUNT(1);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(data->spare) |\
			CMD_INDEX(LARGE_FIXFLOW_READOOB);

	status = sp_pnand_issue_cmd(chip, &cmd_f);
	if (status < 0)
		return 1;

	sp_pnand_wait(chip);

	for(i = 0; i < mtd->oobsize; i += 4) {
		memcpy(spare_buf + i, data->io_base + SPARE_SRAM + i, 4);
	}

	return 0;
}

int rd_pg(struct nand_chip *chip, int real_pg, uint8_t *data_buf)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int status;
	u32 *lbuf;
	int i;

	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(data->spare) | SCR_SEED_VAL2(data->seed_val);
	cmd_f.cq3 = CMD_COUNT(mtd->writesize >> data->eccbasft) | (data->column & 0xFF);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(data->spare) |\
			CMD_INDEX(LARGE_FIXFLOW_PAGEREAD);

	status = sp_pnand_issue_cmd(chip, &cmd_f);
	if(status < 0)
		return 1;

	sp_pnand_wait(chip);

	lbuf = (u32 *) data_buf;
	for (i = 0; i < mtd->writesize; i += 4)
		*lbuf++ = *(volatile unsigned *)(chip->legacy.IO_ADDR_R);

	return 0;
}

int sp_pnand_check_bad_spare(struct nand_chip *chip, int pg)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int spare_phy_start, spare_phy_len, eccbyte;
	int errbit_num, i, j, ret;
	int sec_num = (mtd->writesize >> data->eccbasft);
	u_char *spare_buf;
	int ecc_corr_bit_spare, chan;

	eccbyte = (data->useecc * 14) / 8;
	if (((data->useecc * 14) % 8) != 0)
		eccbyte++;

	// The amount of data-payload in each (sector+sector_parity) or
	// (spare + spare_parity) on Toggle/ONFI mode must be even.
	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2 ||
		data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		if (eccbyte & 0x1)
			eccbyte ++;
	}
	spare_phy_start = mtd->writesize + (eccbyte * sec_num) + CONFIG_BI_BYTE;

	eccbyte = (data->useecc_spare * 14) / 8;
	if (((data->useecc_spare * 14) % 8) != 0)
		eccbyte++;

	if(data->flash_type == TOGGLE1 || data->flash_type == TOGGLE2 ||
		data->flash_type == ONFI2 || data->flash_type == ONFI3) {
		if (eccbyte & 0x1)
			eccbyte ++;
	}
	spare_phy_len = data->spare + eccbyte;
	spare_buf = vmalloc(spare_phy_len);

	ret = 0;
	errbit_num = 0;

	if(!byte_rd(chip, pg, spare_phy_start, spare_phy_len, spare_buf)) {

		for(i = 0; i < spare_phy_len; i++) {
			if(*(spare_buf + i) != 0xFF) {
				DBGLEVEL1(sp_pnand_dbg("D[%d]:0x%x\n", i, *(spare_buf + i)));
				for(j = 0; j < 8; j ++) {
					if((*(spare_buf + i) & (0x1 << j)) == 0)
						errbit_num ++;
				}
			}
		}
		if (errbit_num != 0) {
			if (data->cur_chan < 4) {
				chan = (data->cur_chan << 3);
				ecc_corr_bit_spare = (readl(data->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG1) >> chan) & 0x7F;
			}
			else {
				chan = (data->cur_chan - 4) << 3;
				ecc_corr_bit_spare = (readl(data->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG2) >> chan) & 0x7F;
			}
			DBGLEVEL1(sp_pnand_dbg("spare_phy_len = %d, errbit_num = %d\n", spare_phy_len, errbit_num));

			if(errbit_num > ecc_corr_bit_spare + 1)
				ret = 1;
		}
	}
	else
		ret = 1;

	vfree(spare_buf);
	return ret;

}

static int sp_nfc_wait_cmd_fifo_empty(struct sp_pnand_data *nfc)
{
	u32 status;
	int ret;

	ret = readl_poll_timeout(nfc->io_base + CMDQUEUE_STATUS, status,
				 status & CMDQUEUE_STATUS_EMPTY(nfc->cur_chan), 1,
				 NFC_DEFAULT_TIMEOUT_MS * 1000);
	if (ret)
		dev_err(nfc->dev, "wait for empty cmd FIFO timedout\n");

	return ret;
}

static void sp_nand_slave_dma_transfer_finished(void *data)
{
	struct completion *finished = data;

	complete(finished);
}

int rd_pg_by_dma(struct nand_chip *nand, int real_pg, uint8_t *data_buf)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct sp_pnand_data *nfc = nand_get_controller_data(nand);
	struct nand_ecc_ctrl *ecc = &nand->ecc;
	struct cmd_feature cmd_f;
	struct scatterlist sg;
	struct dma_async_tx_descriptor *tx;
	enum dma_data_direction ddir = DMA_FROM_DEVICE;
	enum dma_transfer_direction tdir = DMA_DEV_TO_MEM;
	dma_cookie_t cookie;
	DECLARE_COMPLETION_ONSTACK(finished);
	int ret = 0;

	sg_init_one(&sg, data_buf, ecc->steps * ecc->size);

	/* streaming DMA, flush cache in the beginning of transfer */
	ret = dma_map_sg(nfc->dev, &sg, 1, ddir);
	if (!ret) {
		dev_err(nfc->dev, "Failed to map DMA buffer\n");
		return -ENOMEM;
	}

	tx = dmaengine_prep_slave_sg(nfc->dmac, &sg, 1, tdir, DMA_PREP_INTERRUPT);
	if (!tx) {
		dev_err(nfc->dev, "Failed to prepare DMA S/G list\n");
		ret = -EINVAL;
		goto err_unmap_buf;
	}

	tx->callback = sp_nand_slave_dma_transfer_finished;
	tx->callback_param = &finished;

	cookie = dmaengine_submit(tx);

	if (dma_submit_error(cookie)) {
		dev_err(nfc->dev, "Failed to do DMA tx submit\n");
		goto err_unmap_buf;
	}

	dma_async_issue_pending(nfc->dmac);

	/* config cmd register and fire the transfer*/
	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(nfc->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(nfc->spare) | SCR_SEED_VAL2(nfc->seed_val);
	cmd_f.cq3 = CMD_COUNT(mtd->writesize >> nfc->eccbasft) | (nfc->column & 0xFF);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_DMA_HANDSHAKE_EN |\
			CMD_FLASH_TYPE(nfc->flash_type) |\
			CMD_START_CE(nfc->sel_chip) | CMD_SPARE_NUM(nfc->spare) |\
			CMD_INDEX(LARGE_FIXFLOW_PAGEREAD);

	ret = sp_pnand_issue_cmd(nand, &cmd_f);
	if(ret < 0) {
		printk("sp_pnand_issue_cmd failed!\n");
		goto err_unmap_buf;
	}

	sp_pnand_wait(nand);

	wait_for_completion(&finished);

err_unmap_buf:
	/* streaming DMA, invalid cached in the end of transfer */
	dma_unmap_sg(nfc->dev, &sg, 1, ddir);

	return ret;
}

int wr_pg_by_dma(struct nand_chip *nand, int real_pg, const uint8_t *data_buf)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct sp_pnand_data *nfc = nand_get_controller_data(nand);
	struct nand_ecc_ctrl *ecc = &nand->ecc;
	struct cmd_feature cmd_f;
	struct scatterlist sg;
	struct dma_async_tx_descriptor *tx;
	enum dma_data_direction ddir = DMA_TO_DEVICE;
	enum dma_transfer_direction tdir = DMA_MEM_TO_DEV;
	dma_cookie_t cookie;
	DECLARE_COMPLETION_ONSTACK(finished);
	int ret = 0;

	sg_init_one(&sg, data_buf, ecc->steps * ecc->size);
	/* streaming DMA, flush cache in the beginning of transfer */
	ret = dma_map_sg(nfc->dev, &sg, 1, ddir);
	if (!ret) {
		dev_err(nfc->dev, "Failed to map DMA buffer\n");
		return -ENOMEM;
	}

	tx = dmaengine_prep_slave_sg(nfc->dmac, &sg, 1, tdir, DMA_PREP_INTERRUPT);
	if (!tx) {
		dev_err(nfc->dev, "Failed to prepare DMA S/G list\n");
		ret = -EINVAL;
		goto err_unmap_buf;
	}

	tx->callback = sp_nand_slave_dma_transfer_finished;
	tx->callback_param = &finished;

	cookie = dmaengine_submit(tx);
	if (dma_submit_error(cookie)) {
		dev_err(nfc->dev, "Failed to do DMA tx submit\n");
		goto err_unmap_buf;
	}

	dma_async_issue_pending(nfc->dmac);

	/* config cmd register and fire the transfer*/
	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(nfc->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(nfc->spare) | SCR_SEED_VAL2(nfc->seed_val);
	cmd_f.cq3 = CMD_COUNT(mtd->writesize >> nfc->eccbasft) | (nfc->column & 0xFF);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_DMA_HANDSHAKE_EN |\
			CMD_FLASH_TYPE(nfc->flash_type) |\
			CMD_START_CE(nfc->sel_chip) | CMD_SPARE_NUM(nfc->spare) |\
			CMD_INDEX(LARGE_PAGEWRITE);

	ret = sp_pnand_issue_cmd(nand, &cmd_f);
	if(ret < 0)
		goto err_unmap_buf;

	sp_pnand_wait(nand);

	wait_for_completion(&finished);

err_unmap_buf:
	/* streaming DMA, invalid cached in the end of transfer */
	dma_unmap_sg(nfc->dev, &sg, 1, ddir);

	return ret;
}

int sp_pnand_read_page_by_dma(struct nand_chip *nand,
			      uint8_t *buf, int oob_required, int page)
{
	struct sp_pnand_data *nfc = nand_get_controller_data(nand);
	int ret;
	//sp_nfc_select_chip(nand, nand->cur_cs);

	//TODO
	ret = sp_nfc_wait_cmd_fifo_empty(nfc);
	if (ret)
		return ret;

	if (oob_required)
		rd_oob(nand, page, nand->oob_poi);

	ret = rd_pg_by_dma(nand, page, buf);
	if (ret == 0)
		return 0;

	DBGLEVEL1(sp_pnand_dbg("Failed to read page by dma! Use PIO mode\n"));
	return sp_pnand_read_page(nand, buf, oob_required, page);
}

int sp_pnand_write_page_by_dma(struct nand_chip *nand,
			       const uint8_t *buf, int oob_required, int page)
{
	struct sp_pnand_data *nfc = nand_get_controller_data(nand);
	int ret;

	//sp_nfc_select_chip(nand, nand->cur_cs);

	ret = sp_nfc_wait_cmd_fifo_empty(nfc);
	if (ret)
		return ret;

	if (oob_required)
		sp_pnand_write_oob(nand, page);

	ret = wr_pg_by_dma(nand, page, buf);
	if (ret == 0)
		return 0;

	DBGLEVEL1(sp_pnand_dbg("Failed to write page by dma! Use PIO mode\n"));
	return sp_pnand_write_page(nand, buf, oob_required, page);
}

int sp_pnand_read_page(struct nand_chip *chip,
		uint8_t *buf, int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int status = 0, chk_data_0xFF, chk_spare_0xFF;
	int i, ecc_original_setting, generic_original_setting, val, ret;
	int real_pg;
	u8  data_empty, spare_empty;
	u32 *lbuf;

	data->page_addr = page;
	real_pg = data->page_addr;

	DBGLEVEL2(sp_pnand_dbg
		("r: ch = %d, ce = %d, page = 0x%x, real = 0x%x, size = %d, data->column = %d\n",
		data->cur_chan, data->sel_chip, data->page_addr, real_pg, mtd->writesize, data->column));

	data->read_state = 1;

	chk_data_0xFF = chk_spare_0xFF = 0;
	data_empty = spare_empty = 0;

	if(oob_required)
		ret = rd_pg_w_oob(chip, real_pg, buf, chip->oob_poi);
	else
		ret = rd_pg(chip, real_pg, buf);
	if(ret)
		goto out;

	if (data->cmd_status &
		(CMD_ECC_FAIL_ON_DATA | CMD_ECC_FAIL_ON_SPARE)) {
		// Store the original setting
		ecc_original_setting = readl(data->io_base + ECC_CONTROL);
		generic_original_setting = readl(data->io_base + GENERAL_SETTING);
		// Disable the ECC engine & HW-Scramble, temporarily.
		val = readl(data->io_base + ECC_CONTROL);
		val = val & ~(ECC_EN(0xFF));
		writel(val, data->io_base + ECC_CONTROL);
		val = readl(data->io_base + GENERAL_SETTING);
		val &= ~DATA_SCRAMBLER;
		writel(val, data->io_base + GENERAL_SETTING);

		if(data->cmd_status==(CMD_ECC_FAIL_ON_DATA|CMD_ECC_FAIL_ON_SPARE))
		{
			if(!rd_pg_w_oob(chip, real_pg, buf, chip->oob_poi)) {
				chk_data_0xFF = chk_spare_0xFF = 1;
				data_empty = spare_empty = 1;
			}
		}
		else if(data->cmd_status == CMD_ECC_FAIL_ON_DATA) {
			if(!rd_pg(chip, real_pg, buf)) {
				chk_data_0xFF = 1;
				data_empty = 1;
			}
		}
		else if(data->cmd_status == CMD_ECC_FAIL_ON_SPARE) {
			if(!rd_oob(chip, real_pg, chip->oob_poi)) {
				chk_spare_0xFF = 1;
				spare_empty = 1;
			}
		}

		// Restore the ecc original setting & generic original setting.
		writel(ecc_original_setting, data->io_base + ECC_CONTROL);
		writel(generic_original_setting, data->io_base + GENERAL_SETTING);

		if(chk_data_0xFF == 1) {

			lbuf = (int *)buf;
			for (i = 0; i < (mtd->writesize >> 2); i++) {
				if (*(lbuf + i) != 0xFFFFFFFF) {
					printk(KERN_ERR "ECC err @ page0x%x real:0x%x\n",
						data->page_addr, real_pg);
					data_empty = 0;
					break;
				}
			}
			if(data_empty == 1)
				DBGLEVEL2(sp_pnand_dbg("Data Real 0xFF\n"));
		}

		if(chk_spare_0xFF == 1) {
			//lichun@add, If BI_byte test
			if (readl(data->io_base + MEM_ATTR_SET) & BI_BYTE_MASK) {
				for (i = 0; i < mtd->oobsize; i++) {
					if (*(chip->oob_poi + i) != 0xFF) {
						printk(KERN_ERR "ECC err for spare(Read page) @");
						printk(KERN_ERR	"ch:%d ce:%d page0x%x real:0x%x\n",
							data->cur_chan, data->sel_chip, data->page_addr, real_pg);
						spare_empty = 0;
						break;
					}
				}
			}
			else {
			//~lichun
				if(sp_pnand_check_bad_spare(chip, data->page_addr)) {
					printk(KERN_ERR "ECC err for spare(Read page) @");
					printk(KERN_ERR	"ch:%d ce:%d page0x%x real:0x%x\n",
						data->cur_chan, data->sel_chip, data->page_addr, real_pg);
					spare_empty = 0;
				}
			}

			if(spare_empty == 1)
				DBGLEVEL2(sp_pnand_dbg("Spare Real 0xFF\n"));
		}

		if( (chk_data_0xFF == 1 && data_empty == 0) ||
			(chk_spare_0xFF == 1 && spare_empty == 0) ) {
			mtd->ecc_stats.failed++;
			status = -1;
		}
	}
out:
	data->read_state = 0;
	// Returning the any value isn't allowed, except 0, -EBADMSG, or -EUCLEAN
	return 0;
}

int sp_pnand_write_page(struct nand_chip *chip, const uint8_t *buf,
			int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	u8 *p;
	u32 *lbuf;
	int real_pg;
	int i, status = 0;

	data->page_addr = page;
	real_pg = data->page_addr;

	DBGLEVEL2(sp_pnand_dbg (
		"w: ch = %d, ce = %d, page = 0x%x, real page:0x%x size = %d, data->column = %d\n",
		data->cur_chan, data->sel_chip,  data->page_addr, real_pg, mtd->writesize, data->column));
	p = chip->oob_poi;
	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(data->spare) | SCR_SEED_VAL2(data->seed_val);
	cmd_f.cq3 = CMD_COUNT(mtd->writesize >> data->eccbasft) |
			(data->column & 0xFF);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) | \
			CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(data->spare);

	if(oob_required) {
		for(i = 0; i < mtd->oobsize; i += 4) {
			memcpy(data->io_base + SPARE_SRAM + i, p + i, 4);
		}
		cmd_f.cq4 |= CMD_INDEX(LARGE_PAGEWRITE_W_SPARE);
	}
	else {
		cmd_f.cq4 |= CMD_INDEX(LARGE_PAGEWRITE);
	}

	status = sp_pnand_issue_cmd(chip, &cmd_f);
	if (status < 0)
		goto out;

	lbuf = (u32 *) buf;
	for (i = 0; i < mtd->writesize; i += 4)
		*(volatile unsigned *)(chip->legacy.IO_ADDR_R) = *lbuf++;

	if (sp_pnand_wait(chip) == NAND_STATUS_FAIL) {
		status = -EIO;
		printk(KERN_ERR "FAILED\n");
	}
out:
	return status;
}

int sp_pnand_read_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	int status = 0, i, ecc_original_setting, generic_original_setting, val;
	int real_pg, empty;
	u8 *buf = chip->oob_poi;

	data->page_addr = page;
	real_pg = data->page_addr;

	DBGLEVEL2(sp_pnand_dbg(
		"read_oob: ch = %d, ce = %d, page = 0x%x, real: 0x%x, size = %d\n",
		data->cur_chan, data->sel_chip, data->page_addr, real_pg, mtd->writesize));

	data->read_state = 1;
	if(!rd_oob(chip, real_pg, buf)) {
		if(data->cmd_status & CMD_ECC_FAIL_ON_SPARE) {
			// Store the original setting
			ecc_original_setting = readl(data->io_base + ECC_CONTROL);
			generic_original_setting = readl(data->io_base + GENERAL_SETTING);
			// Disable the ECC engine & HW-Scramble, temporarily.
			val = readl(data->io_base + ECC_CONTROL);
			val = val & ~(ECC_EN(0xFF));
			writel(val, data->io_base + ECC_CONTROL);
			val = readl(data->io_base + GENERAL_SETTING);
			val &= ~DATA_SCRAMBLER;
			writel(val, data->io_base + GENERAL_SETTING);

			if(!rd_oob(chip, real_pg, buf)) {
				empty = 1;
				for (i = 0; i < mtd->oobsize; i++) {
					if (*(buf + i) != 0xFF) {
						printk(KERN_ERR "ECC err for spare(Read oob) @");
						printk(KERN_ERR	"ch:%d ce:%d page0x%x real:0x%x\n",
							data->cur_chan, data->sel_chip, data->page_addr, real_pg);
						mtd->ecc_stats.failed++;
						status = -1;
						empty = 0;
						break;
					}
				}
				if (empty == 1)
					DBGLEVEL2(sp_pnand_dbg("Spare real 0xFF"));
			}
			// Restore the ecc original setting & generic original setting.
			writel(ecc_original_setting, data->io_base + ECC_CONTROL);
			writel(generic_original_setting, data->io_base + GENERAL_SETTING);
		}
	}
	data->read_state = 0;

	// Returning the any value isn't allowed, except 0, -EBADMSG, or -EUCLEAN
	return 0;
}

int sp_pnand_write_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct sp_pnand_data *data = nand_get_controller_data(chip);
	struct cmd_feature cmd_f;
	int status = 0, real_pg, i;
	u8 *buf = chip->oob_poi;

	data->page_addr = page;
	real_pg = data->page_addr;

	DBGLEVEL2(sp_pnand_dbg(
		"write_oob: ch = %d, ce = %d, page = 0x%x, real page:0x%x, sz = %d, oobsz = %d\n",
		data->cur_chan, data->sel_chip, data->page_addr, real_pg, mtd->writesize, mtd->oobsize));

	for(i = 0; i < mtd->oobsize; i += 4) {
		memcpy(data->io_base + SPARE_SRAM + i, buf + i, 4);
	}

	cmd_f.row_cycle = ROW_ADDR_3CYCLE;
	cmd_f.col_cycle = COL_ADDR_2CYCLE;
	cmd_f.cq1 = real_pg | SCR_SEED_VAL1(data->seed_val);
	cmd_f.cq2 = CMD_EX_SPARE_NUM(data->spare) | SCR_SEED_VAL2(data->seed_val);
	cmd_f.cq3 = CMD_COUNT(1);
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(data->flash_type) |\
			CMD_START_CE(data->sel_chip) | CMD_SPARE_NUM(data->spare) |\
			CMD_INDEX(LARGE_FIXFLOW_WRITEOOB);

	status = sp_pnand_issue_cmd(chip, &cmd_f);
	if (status < 0)
		goto out;

	if (sp_pnand_wait(chip) == NAND_STATUS_FAIL) {
		status = -EIO;
	}
out:
	// Returning the any value isn't allowed, except 0, -EBADMSG, or -EUCLEAN
	return status;
}
