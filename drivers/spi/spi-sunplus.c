// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2021 Sunplus Inc.
// Author: Li-hao Kuo <lhjeff911@gmail.com>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>

#define SUNPLUS_SLAVE_CFG_REG		0x0020
#define SUNPLUS_SLAVE_DMA_CFG_REG	0x0028
#define SUNPLUS_SLAVE_DMA2_LEN_REG	0x0030
#define SUNPLUS_SLAVE_TX_ADDR_REG	0x0034
#define SUNPLUS_SLAVE_RX_ADDR_REG	0x0038

#define SUNPLUS_DATA_RDY_REG		0x0044
#define SUNPLUS_SLAVE_DMA_CTRL_REG	0x0048
#define SUNPLUS_SLAVE_DMA_LEN_REG	0x004c
#define SUNPLUS_SLAVE_DMA_ADDR_REG	0x004c

#define SUNPLUS_SLAVE_TX_EN		BIT(0)
#define SUNPLUS_SLAVE_RX_EN		BIT(1)
#define SUNPLUS_SLAVE_MSB		BIT(2)
#define SUNPLUS_SLAVE_CS_POL		BIT(3)
#define SUNPLUS_SLAVE_CPOL		BIT(4)
#define SUNPLUS_SLAVE_CPHA_W		BIT(5)
#define SUNPLUS_SLAVE_CPHA_R		BIT(6)
#define SUNPLUS_SLAVE_INT_MASK		BIT(7)
#define SUNPLUS_SLAVE_SW2_RST		BIT(9)

#define SUNPLUS_SLAVE_DMA_EN_V2		BIT(0)
#define SUNPLUS_SLAVE_DMA_INT_MASK	BIT(1)

#define SUNPLUS_SLAVE_DATA_RDY		BIT(0)
#define SUNPLUS_SLAVE_SW_RST		BIT(1)
#define SUNPLUS_SLAVE_CLR_INT		BIT(8)
#define SUNPLUS_SLAVE_DMA_EN		BIT(0)
#define SUNPLUS_SLAVE_DMA_RW		BIT(6)
#define SUNPLUS_SLAVE_DMA_CMD		GENMASK(3, 2)

#define SUNPLUS_SLAVE_DMA_TX_LEN	GENMASK(31, 16)
#define SUNPLUS_SLAVE_DMA_RX_LEN        GENMASK(15, 0)

#define SUNPLUS_DMA_SIZE		0x0020
#define SUNPLUS_DMA_RPTR		0x0024
#define SUNPLUS_DMA_WPTR		0x0028
#define SUNPLUS_DMA_CFG			0x002C

#define SUNPLUS_FIFO_REG		0x0034
#define SUNPLUS_SPI_STATUS_REG		0x0038
#define SUNPLUS_SPI_CONFIG_REG		0x003c
#define SUNPLUS_CLK_INV_REG		0x0040
#define SUNPLUS_INT_CFG_REG		0x0044
#define SUNPLUS_INT_BUSY_REG		0x004c
#define SUNPLUS_DMA_CTRL_REG		0x0050

#define SUNPLUS_SPI_START_FD		BIT(0)
#define SUNPLUS_FD_SW_RST		BIT(1)
#define SUNPLUS_TX_EMP_FLAG		BIT(2)
#define SUNPLUS_RX_EMP_FLAG		BIT(4)
#define SUNPLUS_RX_FULL_FLAG		BIT(5)
#define SUNPLUS_FINISH_FLAG		BIT(6)

#define SUNPLUS_TX_CNT_MASK		GENMASK(11, 8)
#define SUNPLUS_RX_CNT_MASK		GENMASK(15, 12)
#define SUNPLUS_TX_LEN_MASK		GENMASK(23, 16)
#define SUNPLUS_XFER_LEN_MASK		GENMASK(31, 24)

#define SUNPLUS_TX_CNT_MASK_V2		GENMASK(13, 8)
#define SUNPLUS_RX_CNT_MASK_V2		GENMASK(19, 14)
#define SUNPLUS_TX_LEN_MASK_V2		GENMASK(8, 1)
#define SUNPLUS_XFER_LEN_MASK_V2	GENMASK(16, 9)

/* SPI MST DMA_SIZE */
#define SUNPLUS_DMA_RSIZE		GENMASK(31, 16)
#define SUNPLUS_DMA_WSIZE		GENMASK(15, 0)

/* SPI MST DMA config */
#define SUNPLUS_DMA_EN             (1<<1)
#define SUNPLUS_DMA_START          (1<<0)


#define SUNPLUS_CPOL_FD			BIT(0)
#define SUNPLUS_CPHA_R			BIT(1)
#define SUNPLUS_CPHA_W			BIT(2)
#define SUNPLUS_LSB_SEL			BIT(4)
#define SUNPLUS_CS_POR			BIT(5)
#define SUNPLUS_FD_SEL			BIT(6)

#define SUNPLUS_RX_UNIT			GENMASK(8, 7)
#define SUNPLUS_TX_UNIT			GENMASK(10, 9)
#define SUNPLUS_TX_EMP_FLAG_MASK	BIT(11)
#define SUNPLUS_RX_FULL_FLAG_MASK	BIT(14)
#define SUNPLUS_FINISH_FLAG_MASK	BIT(15)
#define SUNPLUS_CLEAN_RW_BYTE		GENMASK(10, 7)
#define SUNPLUS_CLEAN_FLUG_MASK		GENMASK(15, 11)
#define SUNPLUS_CLK_MASK		GENMASK(31, 16)

#define SUNPLUS_INT_BYPASS		BIT(3)
#define SUNPLUS_CLR_MASTER_INT		BIT(6)

#define SUNPLUS_SPI_DATA_SIZE		(255)
#define SUNPLUS_FIFO_DATA_LEN		(16)
#define SUNPLUS_DMA_DATA_LEN		(65536)

enum {
	SUNPLUS_MASTER_MODE = 0,
	SUNPLUS_SLAVE_MODE = 1,
};

struct sunplus_spi_compatible {
	int ver;
	int fifo_len;
};

struct sunplus_spi_status {
	unsigned int tx_cnt;
	unsigned int rx_cnt;
	unsigned int tx_len;
	unsigned int total_len;
	unsigned int status;
};

struct sunplus_spi_ctlr {
	struct device *dev;
	struct spi_controller *ctlr;
	void __iomem *m_base;
	void __iomem *s_base;
	u32 xfer_conf;
	int mode;
	int m_irq;
	int s_irq;
	int dma_irq;
	struct clk *spi_clk;
	struct reset_control *rstc;
	// irq spin lock
	spinlock_t lock;
	struct completion isr_done;
	struct completion slave_isr;
	unsigned int  rx_cur_len;
	unsigned int  tx_cur_len;
	unsigned int  data_unit;
	const u8 *tx_buf;
	u8 *rx_buf;
	struct spi_transfer *cur_xfet;
	u32 xfer_len;
	u32 num_xfered;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct sunplus_spi_compatible *dev_comp;
};

static irqreturn_t sunplus_spi_slave_irq(int irq, void *dev)
{
	struct sunplus_spi_ctlr *pspim = dev;
	unsigned int data_status;

	if (!(pspim->dev_comp->ver > 1)) {
		data_status = readl(pspim->s_base + SUNPLUS_DATA_RDY_REG);
		data_status |= SUNPLUS_SLAVE_CLR_INT;
		writel(data_status , pspim->s_base + SUNPLUS_DATA_RDY_REG);
	}
	complete(&pspim->slave_isr);
	return IRQ_HANDLED;
}

static int sunplus_spi_slave_abort(struct spi_controller *ctlr)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	complete(&pspim->slave_isr);
	complete(&pspim->isr_done);
	return 0;
}

static int sunplus_spi_slave_tx(struct spi_device *spi, struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_controller_get_devdata(spi->controller);
	u32 value;

	reinit_completion(&pspim->slave_isr);
	value = SUNPLUS_SLAVE_DMA_EN | SUNPLUS_SLAVE_DMA_RW | FIELD_PREP(SUNPLUS_SLAVE_DMA_CMD, 3);
	writel(value, pspim->s_base + SUNPLUS_SLAVE_DMA_CTRL_REG);
	writel(xfer->len, pspim->s_base + SUNPLUS_SLAVE_DMA_LEN_REG);
	writel(xfer->tx_dma, pspim->s_base + SUNPLUS_SLAVE_DMA_ADDR_REG);
	value = readl(pspim->s_base + SUNPLUS_DATA_RDY_REG);
	value |= SUNPLUS_SLAVE_DATA_RDY;
	writel(value, pspim->s_base + SUNPLUS_DATA_RDY_REG);
	if (wait_for_completion_interruptible(&pspim->isr_done)) {
		dev_err(&spi->dev, "%s() wait_for_completion err\n", __func__);
		return -EINTR;
	}
	return 0;
}

static int sunplus_spi_slave_rx(struct spi_device *spi, struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_controller_get_devdata(spi->controller);
	u32 value;

	reinit_completion(&pspim->isr_done);
	value = SUNPLUS_SLAVE_DMA_EN | FIELD_PREP(SUNPLUS_SLAVE_DMA_CMD, 3);
	writel(value, pspim->s_base + SUNPLUS_SLAVE_DMA_CTRL_REG);
	writel(xfer->len, pspim->s_base + SUNPLUS_SLAVE_DMA_LEN_REG);
	writel(xfer->rx_dma, pspim->s_base + SUNPLUS_SLAVE_DMA_ADDR_REG);
	if (wait_for_completion_interruptible(&pspim->isr_done)) {
		dev_err(&spi->dev, "%s() wait_for_completion err\n", __func__);
		return -EINTR;
	}
	writel(SUNPLUS_SLAVE_SW_RST, pspim->s_base + SUNPLUS_SLAVE_DMA_CTRL_REG);
	return 0;
}

static void sunplus_spi_master_rb(struct sunplus_spi_ctlr *pspim, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++) {
		pspim->rx_buf[pspim->rx_cur_len] =
			readl(pspim->m_base + SUNPLUS_FIFO_REG);
		pspim->rx_cur_len++;
	}
}

static void sunplus_spi_master_wb(struct sunplus_spi_ctlr *pspim, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++) {
		writel(pspim->tx_buf[pspim->tx_cur_len],
		       pspim->m_base + SUNPLUS_FIFO_REG);
		pspim->tx_cur_len++;
	}
}

static void sunplus_spi_set_xfer_lan_and_start(struct sunplus_spi_ctlr *pspim,
					  unsigned int xfer_len)
{
	unsigned int reg_temp;

	if(pspim->dev_comp->ver ==1){
		reg_temp = FIELD_PREP(SUNPLUS_TX_LEN_MASK, xfer_len) |
				   FIELD_PREP(SUNPLUS_XFER_LEN_MASK, xfer_len) |
				   SUNPLUS_SPI_START_FD;
		writel(reg_temp, pspim->m_base + SUNPLUS_SPI_STATUS_REG);
	} else {
		reg_temp = FIELD_PREP(SUNPLUS_TX_LEN_MASK_V2, xfer_len) |
				   FIELD_PREP(SUNPLUS_XFER_LEN_MASK_V2, xfer_len);
		writel(reg_temp, pspim->m_base + SUNPLUS_CLK_INV_REG);
		reg_temp = SUNPLUS_SPI_START_FD;
		writel(reg_temp, pspim->m_base + SUNPLUS_SPI_STATUS_REG);
	}
	}

static void sunplus_spi_get_status(struct sunplus_spi_ctlr *pspim,
				  struct sunplus_spi_status *status)
{
	unsigned int temp_status;

	temp_status = readl(pspim->m_base + SUNPLUS_SPI_STATUS_REG);
	status->status = temp_status;
	if(pspim->dev_comp->ver == 1){
		status->tx_cnt = FIELD_GET(SUNPLUS_TX_CNT_MASK, temp_status);
		status->rx_cnt = FIELD_GET(SUNPLUS_RX_CNT_MASK, temp_status);
		status->tx_len = FIELD_GET(SUNPLUS_TX_LEN_MASK, temp_status);
		status->total_len = FIELD_GET(SUNPLUS_XFER_LEN_MASK, temp_status);
	} else {
		status->tx_cnt = FIELD_GET(SUNPLUS_TX_CNT_MASK_V2, temp_status);
		status->rx_cnt = FIELD_GET(SUNPLUS_RX_CNT_MASK_V2, temp_status);
		temp_status = readl(pspim->m_base + SUNPLUS_CLK_INV_REG);
		status->tx_len = FIELD_GET(SUNPLUS_TX_LEN_MASK_V2, temp_status);
		status->total_len = FIELD_GET(SUNPLUS_XFER_LEN_MASK_V2, temp_status);
	}
}

static irqreturn_t sunplus_spi_master_irq(int irq, void *dev)
{
	struct sunplus_spi_ctlr *pspim = dev;
	struct sunplus_spi_status status;
	bool isrdone = false;
	u32 value;

	sunplus_spi_get_status(pspim, &status);
	if ((status.status & SUNPLUS_TX_EMP_FLAG) && (status.status & SUNPLUS_RX_EMP_FLAG)
		&& status.total_len == 0)
		return IRQ_NONE;

	if (status.tx_len == 0 && status.total_len == 0)
		return IRQ_NONE;

	if (status.status & SUNPLUS_RX_FULL_FLAG)
		status.rx_cnt = pspim->data_unit;

	status.tx_cnt = min(status.tx_len - pspim->tx_cur_len, pspim->data_unit - status.tx_cnt);
	dev_dbg(pspim->dev, "fd_st=0x%x rx_c:%d tx_c:%d tx_l:%d",
		status.status, status.rx_cnt, status.tx_cnt, status.tx_len);

	if (status.rx_cnt > 0)
		sunplus_spi_master_rb(pspim, status.rx_cnt);
	if (status.tx_cnt > 0)
		sunplus_spi_master_wb(pspim, status.tx_cnt);

	sunplus_spi_get_status(pspim, &status);
	if (status.status & SUNPLUS_FINISH_FLAG || status.tx_len == pspim->tx_cur_len) {
		while (status.total_len != pspim->rx_cur_len) {

			sunplus_spi_get_status(pspim, &status);
			if (status.status & SUNPLUS_RX_FULL_FLAG)
				status.rx_cnt = pspim->data_unit;
			if (status.rx_cnt > 0)
				sunplus_spi_master_rb(pspim, status.rx_cnt);
		}
		if (status.status & SUNPLUS_RX_FULL_FLAG){
			value = readl(pspim->m_base + SUNPLUS_INT_BUSY_REG);
			value |= SUNPLUS_CLR_MASTER_INT;
			writel(value, pspim->m_base + SUNPLUS_INT_BUSY_REG);
		}
		writel(SUNPLUS_FINISH_FLAG, pspim->m_base + SUNPLUS_SPI_STATUS_REG);
		isrdone = true;
	}

	if (isrdone)
		complete(&pspim->isr_done);

	return IRQ_HANDLED;
}

static void sunplus_prep_transfer(struct spi_controller *ctlr)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	pspim->tx_sgl = NULL;
	pspim->rx_sgl = NULL;
	pspim->tx_sgl_len = 0;
	pspim->rx_sgl_len = 0;
	pspim->num_xfered = 0;
	pspim->tx_cur_len = 0;
	pspim->rx_cur_len = 0;
	pspim->data_unit = pspim->dev_comp->fifo_len;
}

// preliminary set CS, CPOL, CPHA and LSB
static int sunplus_spi_controller_prepare_message(struct spi_controller *ctlr,
						 struct spi_message *msg)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	struct spi_device *s = msg->spi;
	u32 valus, rs = 0;

	if (pspim->mode == SUNPLUS_MASTER_MODE) {
		valus = readl(pspim->m_base + SUNPLUS_SPI_STATUS_REG);
		valus |= SUNPLUS_FD_SW_RST;
		writel(valus, pspim->m_base + SUNPLUS_SPI_STATUS_REG);
		rs |= SUNPLUS_FD_SEL;
		if (s->mode & SPI_CPOL)
			rs |= SUNPLUS_CPOL_FD;
		
		if (s->mode & SPI_LSB_FIRST)
			rs |= SUNPLUS_LSB_SEL;
		
		if (s->mode & SPI_CS_HIGH)
			rs |= SUNPLUS_CS_POR;
		
		if (s->mode & SPI_CPHA)
			rs |=  SUNPLUS_CPHA_R;
		else
			rs |=  SUNPLUS_CPHA_W;
		
		rs |=  FIELD_PREP(SUNPLUS_TX_UNIT, 0) | FIELD_PREP(SUNPLUS_RX_UNIT, 0);
		pspim->xfer_conf = rs;
		if (pspim->xfer_conf & SUNPLUS_CPOL_FD)
			writel(pspim->xfer_conf, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
	}
	else {

		if(pspim->dev_comp->ver == 1){
			valus = readl(pspim->m_base + SUNPLUS_SLAVE_DMA_CTRL_REG);
			valus |= SUNPLUS_SLAVE_SW_RST;
			writel(valus, pspim->s_base + SUNPLUS_SLAVE_DMA_CTRL_REG);
			return 0;
		} else {
			valus = readl(pspim->m_base + SUNPLUS_SLAVE_CFG_REG);
			valus |= SUNPLUS_SLAVE_SW2_RST;
			writel(valus, pspim->m_base + SUNPLUS_SLAVE_CFG_REG);
			
			if (s->mode & SPI_CPOL)
				rs |= SUNPLUS_SLAVE_CPOL;
			
			if (s->mode & SPI_CPHA)
				rs |= (SUNPLUS_SLAVE_CPHA_R | SUNPLUS_SLAVE_CPHA_W);
			
			if ((s->mode & SPI_CS_HIGH) == 0)
				rs |= SUNPLUS_SLAVE_CS_POL;
			rs |= SUNPLUS_SLAVE_MSB;
			pspim->xfer_conf = rs;
			writel(rs, pspim->m_base + SUNPLUS_SLAVE_CFG_REG);
		}
	}

	return 0;
}

static void sunplus_spi_setup_clk(struct spi_controller *ctlr, struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	u32 clk_rate, clk_sel, div;

	clk_rate = clk_get_rate(pspim->spi_clk);
	div = max(2U, clk_rate / xfer->speed_hz);

	clk_sel = (div / 2) - 1;
	pspim->xfer_conf &= ~SUNPLUS_CLK_MASK;
	pspim->xfer_conf |= FIELD_PREP(SUNPLUS_CLK_MASK, clk_sel);
	writel(pspim->xfer_conf, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
}

static int sunplus_spi_fifo_transfer(struct spi_controller *ctlr, struct spi_device *spi,
				       struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	unsigned long timeout = msecs_to_jiffies(1000);
	unsigned int xfer_cnt, xfer_len, last_len;
	unsigned int i, len_temp;
	u32 reg_temp;

	xfer_cnt = xfer->len / SUNPLUS_SPI_DATA_SIZE;
	last_len = xfer->len % SUNPLUS_SPI_DATA_SIZE;

	for (i = 0; i <= xfer_cnt; i++) {
		sunplus_prep_transfer(ctlr);
		sunplus_spi_setup_clk(ctlr, xfer);
		reinit_completion(&pspim->isr_done);

		if (i == xfer_cnt)
			xfer_len = last_len;
		else
			xfer_len = SUNPLUS_SPI_DATA_SIZE;

		pspim->tx_buf = xfer->tx_buf + i * SUNPLUS_SPI_DATA_SIZE;
		pspim->rx_buf = xfer->rx_buf + i * SUNPLUS_SPI_DATA_SIZE;

		if (pspim->tx_cur_len < xfer_len) {
			len_temp = min(pspim->data_unit, xfer_len);
			sunplus_spi_master_wb(pspim, len_temp);
		}
		reg_temp = readl(pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
		reg_temp &= ~SUNPLUS_CLEAN_RW_BYTE;
		reg_temp &= ~SUNPLUS_CLEAN_FLUG_MASK;
		reg_temp |= SUNPLUS_FD_SEL | SUNPLUS_FINISH_FLAG_MASK |
			    SUNPLUS_TX_EMP_FLAG_MASK | SUNPLUS_RX_FULL_FLAG_MASK |
			    FIELD_PREP(SUNPLUS_TX_UNIT, 0) | FIELD_PREP(SUNPLUS_RX_UNIT, 0);
		writel(reg_temp, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);

		sunplus_spi_set_xfer_lan_and_start(pspim, xfer_len);
		if (!wait_for_completion_interruptible_timeout(&pspim->isr_done, timeout)) {
			dev_err(&spi->dev, "wait_for_completion err\n");
			return -ETIMEDOUT;
		}

		reg_temp = readl(pspim->m_base + SUNPLUS_SPI_STATUS_REG);
		if (reg_temp & SUNPLUS_FINISH_FLAG) {
			writel(SUNPLUS_FINISH_FLAG, pspim->m_base + SUNPLUS_SPI_STATUS_REG);
			writel(readl(pspim->m_base + SUNPLUS_SPI_CONFIG_REG) &
				SUNPLUS_CLEAN_FLUG_MASK, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
		}

		if (pspim->xfer_conf & SUNPLUS_CPOL_FD)
			writel(pspim->xfer_conf, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);

	}
	return 0;
}

int sunplus_spi_slave_dma_rw(struct spi_controller *ctlr, struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	struct device *dev = pspim->dev;
	u32 reg_temp;

	reinit_completion(&pspim->slave_isr);

	reg_temp = pspim->xfer_conf | SUNPLUS_SLAVE_INT_MASK;
	if(xfer->tx_dma) {
		writel(xfer->tx_dma, pspim->m_base + SUNPLUS_SLAVE_RX_ADDR_REG);
		reg_temp |= SUNPLUS_SLAVE_TX_EN;
	}
	if(xfer->rx_dma) {
		writel(xfer->rx_dma, pspim->m_base + SUNPLUS_SLAVE_TX_ADDR_REG);
		reg_temp |= SUNPLUS_SLAVE_RX_EN;
	}
	writel(reg_temp, pspim->m_base + SUNPLUS_SLAVE_CFG_REG);

	reg_temp = 0;
	if(xfer->tx_dma)
		reg_temp |= FIELD_PREP(SUNPLUS_SLAVE_DMA_RX_LEN, xfer->len);
	if(xfer->rx_dma)
		reg_temp |= FIELD_PREP(SUNPLUS_SLAVE_DMA_TX_LEN, xfer->len);
	writel(reg_temp, pspim->m_base + SUNPLUS_SLAVE_DMA2_LEN_REG);

	reg_temp = readl(pspim->m_base + SUNPLUS_SLAVE_DMA_CFG_REG);
	reg_temp |= SUNPLUS_SLAVE_DMA_EN_V2 | SUNPLUS_SLAVE_DMA_INT_MASK;
	writel(reg_temp, pspim->m_base + SUNPLUS_SLAVE_DMA_CFG_REG);

	if (wait_for_completion_interruptible(&pspim->slave_isr)) {
		dev_err(dev, "wait_for_completion\n");
		return -ETIMEDOUT;
	}

	//writel(0, pspim->m_base + SUNPLUS_SLAVE_DMA_CFG_REG);
	//writel(0, pspim->m_base + SUNPLUS_SLAVE_DMA2_LEN_REG);
	//writel(0, pspim->m_base + SUNPLUS_SLAVE_CFG_REG);
	//reg_temp = readl(pspim->m_base + SUNPLUS_SLAVE_CFG_REG);
	//reg_temp |= SUNPLUS_SLAVE_SW2_RST;
	//writel(reg_temp, pspim->m_base + SUNPLUS_SLAVE_CFG_REG);

	return 0;
}

static int sunplus_spi_slave_fullduplex_transfer(struct spi_controller *ctlr, struct spi_device *spi,
				       struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	struct device *dev = pspim->dev;
	int ret;

	ret = 0;
	if((!xfer->tx_buf) && (!xfer->tx_buf)) {
		dev_dbg(&ctlr->dev, "%s() wrong command\n", __func__);
		return -EINVAL;
	}
	if (xfer->tx_buf) {
		xfer->tx_dma = dma_map_single(dev, (void *)xfer->tx_buf,
					      xfer->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, xfer->tx_dma))
			return -ENOMEM;
	}
	if (xfer->rx_buf) {
		xfer->rx_dma = dma_map_single(dev, xfer->rx_buf, xfer->len,
					      DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, xfer->rx_dma))
			return -ENOMEM;		
	}

        if(xfer->tx_dma | xfer->rx_dma)
		ret = sunplus_spi_slave_dma_rw(ctlr, xfer);
        if(xfer->tx_dma)
		dma_unmap_single(dev, xfer->tx_dma, xfer->len, DMA_TO_DEVICE);
	if(xfer->rx_dma)
		dma_unmap_single(dev, xfer->rx_dma, xfer->len, DMA_FROM_DEVICE);

	spi_finalize_current_transfer(ctlr);
	return ret;
}

static int sunplus_spi_slave_helfduplex_transfer(struct spi_controller *ctlr, struct spi_device *spi,
				       struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	struct device *dev = pspim->dev;
	int ret;

	if (xfer->tx_buf && !xfer->rx_buf) {
		xfer->tx_dma = dma_map_single(dev, (void *)xfer->tx_buf,
					      xfer->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, xfer->tx_dma))
			return -ENOMEM;

		ret = sunplus_spi_slave_tx(spi, xfer);
		dma_unmap_single(dev, xfer->tx_dma, xfer->len, DMA_TO_DEVICE);
	} else if (xfer->rx_buf && !xfer->tx_buf) {
		xfer->rx_dma = dma_map_single(dev, xfer->rx_buf, xfer->len,
					      DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, xfer->rx_dma))
			return -ENOMEM;
		
		ret = sunplus_spi_slave_rx(spi, xfer);
		dma_unmap_single(dev, xfer->rx_dma, xfer->len, DMA_FROM_DEVICE);
	} else {
		dev_dbg(&ctlr->dev, "%s() wrong command\n", __func__);
		return -EINVAL;
	}

	spi_finalize_current_transfer(ctlr);
	return ret;
}

static int sunplus_spi_slave_transfer_one(struct spi_controller *ctlr, struct spi_device *spi,
				       struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	if (pspim->dev_comp->ver > 1)
		return sunplus_spi_slave_fullduplex_transfer(ctlr, spi, xfer);
	else
		return sunplus_spi_slave_helfduplex_transfer(ctlr, spi, xfer);

}

static void sunplus_spi_master_updata_dma_len(struct sunplus_spi_ctlr *pspim)
{
	if (pspim->tx_sgl_len && pspim->rx_sgl_len) {
		if (pspim->tx_sgl_len > pspim->rx_sgl_len)
			pspim->xfer_len = pspim->rx_sgl_len;
		else
			pspim->xfer_len = pspim->tx_sgl_len;
	} else if(pspim->rx_sgl_len)
		pspim->xfer_len = pspim->rx_sgl_len;
	else if(pspim->tx_sgl_len)
		pspim->xfer_len = pspim->tx_sgl_len;

	if(pspim->xfer_len > SUNPLUS_DMA_DATA_LEN)
		pspim->xfer_len = SUNPLUS_DMA_DATA_LEN;
}

static int sunplus_spi_master_fullduplex_dma(struct spi_controller *ctlr, struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	u32 reg_temp;
				       
	sunplus_spi_setup_clk(ctlr, xfer);
			       
	// initial SPI master config and change to Full-Duplex mode (SPI_CONFIG)  91.15
	reg_temp = readl(pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
	reg_temp &= ~SUNPLUS_CLEAN_RW_BYTE;
	reg_temp &= ~SUNPLUS_CLEAN_FLUG_MASK;
				       
	// set SPI master config for full duplex (SPI_CONFIG)  91.15
	reg_temp |= SUNPLUS_FINISH_FLAG_MASK|
		FIELD_PREP(SUNPLUS_TX_UNIT, 0) | FIELD_PREP(SUNPLUS_RX_UNIT, 0);
	writel(reg_temp, pspim->m_base + SUNPLUS_SPI_CONFIG_REG);
				       
	reg_temp = 0;
	if (pspim->tx_sgl && pspim->tx_sgl_len) {
		reg_temp |= FIELD_PREP(SUNPLUS_DMA_RSIZE, pspim->xfer_len);
		writel(xfer->tx_dma, pspim->m_base + SUNPLUS_DMA_RPTR);
		pspim->tx_sgl_len -= pspim->xfer_len;	
	} else
		reg_temp |= FIELD_PREP(SUNPLUS_DMA_RSIZE, 0);

	if (pspim->rx_sgl && pspim->rx_sgl_len) {
		reg_temp |= FIELD_PREP(SUNPLUS_DMA_WSIZE, pspim->xfer_len);
		writel(xfer->rx_dma, pspim->m_base + SUNPLUS_DMA_WPTR);
		pspim->rx_sgl_len -= pspim->xfer_len;
	} else
		reg_temp |= FIELD_PREP(SUNPLUS_DMA_WSIZE, 0);

	writel(reg_temp, pspim->m_base + SUNPLUS_DMA_SIZE);
				       
	// enable DMA mode and start DMA
	reg_temp = readl(pspim->m_base + SUNPLUS_DMA_CFG);
	reg_temp |= SUNPLUS_DMA_EN | SUNPLUS_DMA_START;
	writel(reg_temp, pspim->m_base + SUNPLUS_DMA_CFG);
				       		       
	return 1;			       
}

static int sunplus_spi_dma_transfer(struct spi_controller *ctlr,
				    struct spi_device *spi,
				    struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);
	
	sunplus_prep_transfer(ctlr);
	pspim->cur_xfet = xfer;

	if (xfer->tx_buf){
		pspim->tx_sgl = xfer->tx_sg.sgl;
		xfer->tx_dma = sg_dma_address(pspim->tx_sgl);
		pspim->tx_sgl_len = sg_dma_len(pspim->tx_sgl);
	}
	if (xfer->rx_buf){
		pspim->rx_sgl = xfer->rx_sg.sgl;
		xfer->rx_dma = sg_dma_address(pspim->rx_sgl);
		pspim->rx_sgl_len = sg_dma_len(pspim->rx_sgl);
	}

	sunplus_spi_master_updata_dma_len(pspim);
	sunplus_spi_master_fullduplex_dma(ctlr, xfer);
	//spi_finalize_current_transfer(ctlr);
	return 1;
}
				    
// spi master irq handler
static irqreturn_t sunplus_spi_master_irq_dma(int _irq, void *dev)
{
	struct sunplus_spi_ctlr *pspim = dev;
	struct spi_transfer *xfer = pspim->cur_xfet;
	struct spi_controller *ctlr = pspim->ctlr;

	if (pspim->tx_sgl)
		xfer->tx_dma += pspim->xfer_len;
	if (pspim->rx_sgl)
		xfer->rx_dma += pspim->xfer_len;

	if (pspim->tx_sgl && (pspim->tx_sgl_len == 0)) {
	pspim->tx_sgl = sg_next(pspim->tx_sgl);
	if (pspim->tx_sgl) {
		xfer->tx_dma = sg_dma_address(pspim->tx_sgl);
		pspim->tx_sgl_len = sg_dma_len(pspim->tx_sgl);
	}
	}
	if (pspim->rx_sgl && (pspim->rx_sgl_len == 0)) {
	pspim->rx_sgl = sg_next(pspim->rx_sgl);
	if (pspim->rx_sgl) {		
		xfer->rx_dma = sg_dma_address(pspim->rx_sgl);
		pspim->rx_sgl_len = sg_dma_len(pspim->rx_sgl);
	}
	}
				    
	if (!pspim->tx_sgl && !pspim->rx_sgl) {
		/* spi disable dma */
		spi_finalize_current_transfer(ctlr);
		return IRQ_HANDLED;
	}
	
	sunplus_spi_master_updata_dma_len(pspim);
	sunplus_spi_master_fullduplex_dma(ctlr, xfer);
	return IRQ_HANDLED;
		    
}

static int sunplus_spi_master_transfer_one(struct spi_controller *ctlr,
					struct spi_device *spi,
					struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	if (ctlr->can_dma(ctlr, spi, xfer) && (pspim->dev_comp->ver > 1))
		return sunplus_spi_dma_transfer(ctlr, spi, xfer);
	else
		return sunplus_spi_fifo_transfer(ctlr, spi, xfer);
}
				       
static bool sunplus_spi_can_dma(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	return (xfer->len >= pspim->dev_comp->fifo_len);
}

static void sunplus_spi_disable_unprepare(void *data)
{
	clk_disable_unprepare(data);
}

static void sunplus_spi_reset_control_assert(void *data)
{
	reset_control_assert(data);
}

static int sunplus_spi_controller_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sunplus_spi_ctlr *pspim;
	struct spi_controller *ctlr;
	int mode, ret;

	pdev->id = of_alias_get_id(pdev->dev.of_node, "sp_spi");

	if (device_property_read_bool(dev, "spi-slave"))
		mode = SUNPLUS_SLAVE_MODE;
	else
		mode = SUNPLUS_MASTER_MODE;

	if (mode == SUNPLUS_SLAVE_MODE)
		ctlr = devm_spi_alloc_slave(dev, sizeof(*pspim));
	else
		ctlr = devm_spi_alloc_master(dev, sizeof(*pspim));
	if (!ctlr)
		return -ENOMEM;
	//device_set_node(&ctlr->dev, pdev->dev.fwnode);
	//dev_fwnode(dev);
	ctlr->dev.of_node = pdev->dev.of_node;
	ctlr->bus_num = pdev->id;
	ctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST;
	ctlr->auto_runtime_pm = true;
	ctlr->prepare_message = sunplus_spi_controller_prepare_message;
	if (mode == SUNPLUS_SLAVE_MODE) {
		ctlr->transfer_one = sunplus_spi_slave_transfer_one;
		ctlr->slave_abort = sunplus_spi_slave_abort;
		ctlr->flags = SPI_CONTROLLER_HALF_DUPLEX;
	} else {
		ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
		ctlr->min_speed_hz = 40000;
		ctlr->max_speed_hz = 25000000;
		ctlr->use_gpio_descriptors = true;
		ctlr->flags = SPI_CONTROLLER_MUST_RX | SPI_CONTROLLER_MUST_TX;
		ctlr->transfer_one = sunplus_spi_master_transfer_one;
		ctlr->can_dma = sunplus_spi_can_dma;
	}
	platform_set_drvdata(pdev, ctlr);
	pspim = spi_controller_get_devdata(ctlr);
	pspim->dev_comp = of_device_get_match_data(&pdev->dev);
	pspim->mode = mode;
	pspim->ctlr = ctlr;
	pspim->dev = dev;
	spin_lock_init(&pspim->lock);
	init_completion(&pspim->isr_done);
	init_completion(&pspim->slave_isr);

	pspim->m_base = devm_platform_ioremap_resource_byname(pdev, "master");
	if (IS_ERR(pspim->m_base))
		return dev_err_probe(dev, PTR_ERR(pspim->m_base), "m_base get fail\n");

	pspim->s_base = devm_platform_ioremap_resource_byname(pdev, "slave");
	if (IS_ERR(pspim->s_base))
		return dev_err_probe(dev, PTR_ERR(pspim->s_base), "s_base get fail\n");

	pspim->m_irq = platform_get_irq_byname(pdev, "master_risc");
	if (pspim->m_irq < 0)
		return pspim->m_irq;

	pspim->s_irq = platform_get_irq_byname(pdev, "slave_risc");
	if (pspim->s_irq < 0)
		return pspim->s_irq;

	pspim->dma_irq = platform_get_irq_byname(pdev, "dma_w");
	if (pspim->dma_irq < 0)
		return pspim->dma_irq;

	pspim->spi_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(pspim->spi_clk))
		return dev_err_probe(dev, PTR_ERR(pspim->spi_clk), "clk get fail\n");

	pspim->rstc = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(pspim->rstc))
		return dev_err_probe(dev, PTR_ERR(pspim->rstc), "rst get fail\n");

	ret = clk_prepare_enable(pspim->spi_clk);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable clk\n");

	ret = devm_add_action_or_reset(dev, sunplus_spi_disable_unprepare, pspim->spi_clk);
	if (ret)
		return ret;

	ret = reset_control_deassert(pspim->rstc);
	if (ret)
		return dev_err_probe(dev, ret, "failed to deassert reset\n");

	ret = devm_add_action_or_reset(dev, sunplus_spi_reset_control_assert, pspim->rstc);
	if (ret)
		return ret;

	ret = devm_request_irq(dev, pspim->m_irq, sunplus_spi_master_irq,
			       IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret)
		return ret;

	ret = devm_request_irq(dev, pspim->s_irq, sunplus_spi_slave_irq,
			       IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret)
		return ret;

	ret = devm_request_irq(dev, pspim->dma_irq, sunplus_spi_master_irq_dma,
			       IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret)
		return ret;

	pm_runtime_enable(dev);
	ret = spi_register_controller(ctlr);
	if (ret) {
		pm_runtime_disable(dev);
		return dev_err_probe(dev, ret, "spi_register_master fail\n");
	}
	return 0;
}

static int sunplus_spi_controller_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = dev_get_drvdata(&pdev->dev);

	spi_unregister_controller(ctlr);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	return 0;
}

static int __maybe_unused sunplus_spi_controller_suspend(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	return reset_control_assert(pspim->rstc);
}

static int __maybe_unused sunplus_spi_controller_resume(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	reset_control_deassert(pspim->rstc);
	return clk_prepare_enable(pspim->spi_clk);
}

#ifdef CONFIG_PM
static int sunplus_spi_runtime_suspend(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	return reset_control_assert(pspim->rstc);
}

static int sunplus_spi_runtime_resume(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunplus_spi_ctlr *pspim = spi_master_get_devdata(ctlr);

	return reset_control_deassert(pspim->rstc);
}
#endif

static const struct dev_pm_ops sunplus_spi_pm_ops = {
	SET_RUNTIME_PM_OPS(sunplus_spi_runtime_suspend,
			   sunplus_spi_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(sunplus_spi_controller_suspend,
				sunplus_spi_controller_resume)
};

static const struct sunplus_spi_compatible q645_compat = {
	.ver = 2,
	.fifo_len = 64,
};

static const struct sunplus_spi_compatible sp7021_compat = {
	.ver = 1,
	.fifo_len = 16,
};

static const struct of_device_id sunplus_spi_controller_ids[] = {
	{ .compatible = "sunplus,q645-spi-controller", (void *)&q645_compat },
	{ .compatible = "sunplus,sp7021-spi", (void *)&sp7021_compat },
	{}
};
MODULE_DEVICE_TABLE(of, sunplus_spi_controller_ids);

static struct platform_driver sunplus_spi_controller_driver = {
	.probe = sunplus_spi_controller_probe,
	.remove = sunplus_spi_controller_remove,
	.driver = {
		.name = "sunplus,sunplus-spi-controller",
		.of_match_table = sunplus_spi_controller_ids,
		.pm     = &sunplus_spi_pm_ops,
	},
};
module_platform_driver(sunplus_spi_controller_driver);

MODULE_AUTHOR("Li-hao Kuo <lhjeff911@gmail.com>");
MODULE_DESCRIPTION("Sunplus SPI controller driver");
MODULE_LICENSE("GPL");
