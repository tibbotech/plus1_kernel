/*
 * Sunplus SPI controller driver (master mode only)
 *
 * Author: Sunplus Technology Co., Ltd.
 *		   henry.liou@sunplus.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>

#define SLAVE_INT_IN

#define MAS_REG_NAME "spi_combo0"
#define PIN_MUX_REG_NAME "grp2_sft"
#define DMA_IRQ_NAME "dma_w_intr"
#define MAS_IRQ_NAME "mas_risc_intr"

#define CLEAR_MASTER_INT (1<<6)
#define MASTER_INT (1<<7)
#define DMA_READ (0xd)
#define DMA_WRITE (0x4d)
#define SPI_START (1<<0)
#define SPI_BUSY (1<<0)
#define CLEAR_DMA_W_INT (1<<7)
#define DMA_W_INT (1<<8)
#define CLEAR_ADDR_BIT (~(0x180))
#define ADDR_BIT(x) (x<<7)
#define DMA_DATA_RDY (1<<0)
#define PENTAGRAM_SPI_SLAVE_SET (0x2c)

enum SPI_MASTER_MODE
{
	SPIM_READ = 0,
	SPIM_WRITE = 1,
	SPIM_IDLE = 2
};

typedef struct{
	// Group 091 : SPI_MASTER
    unsigned int  MST_TX_DATA_ADDR                      ; // 00  (ADDR : 0x9C00_2D80)
    unsigned int  MST_TX_DATA_3_2_1_0                   ; // 01  (ADDR : 0x9C00_2D84)
    unsigned int  MST_TX_DATA_7_6_5_4                   ; // 02  (ADDR : 0x9C00_2D88)
    unsigned int  MST_TX_DATA_11_10_9_8                 ; // 03  (ADDR : 0x9C00_2D8C)
    unsigned int  MST_TX_DATA_15_14_13_12               ; // 04  (ADDR : 0x9C00_2D90)
    unsigned int  G091_RESERVED_0[4]                    ; //     (ADDR : 0x9C00_2D94) ~ (ADDR : 0x9C00_2DA0)
    unsigned int  MST_RX_DATA_3_2_1_0                   ; // 09  (ADDR : 0x9C00_2DA4)
    unsigned int  MST_RX_DATA_7_6_5_4                   ; // 10  (ADDR : 0x9C00_2DA8)
    unsigned int  MST_RX_DATA_11_10_9_8                 ; // 11  (ADDR : 0x9C00_2DAC)
    unsigned int  MST_RX_DATA_15_14_13_12               ; // 12  (ADDR : 0x9C00_2DB0)
	unsigned int  FIFO_DATA                             ; // 13  (ADDR : 0x9C00_2DB4)
	unsigned int  SPI_FD_STATUS                         ; // 14  (ADDR : 0x9C00_2DB8)
	unsigned int  SPI_FD_CONFIG                         ; // 15  (ADDR : 0x9C00_2DBC)
	unsigned int  G091_RESERVED_1                       ; // 16  (ADDR : 0x9C00_2DC0)
    unsigned int  SPI_CTRL_CLKSEL                       ; // 17  (ADDR : 0x9C00_2DC4)
    unsigned int  BYTE_NO                               ; // 18  (ADDR : 0x9C00_2DC8)
    unsigned int  SPI_INT_BUSY                          ; // 19  (ADDR : 0x9C00_2DCC)
    unsigned int  DMA_CTRL                              ; // 20  (ADDR : 0x9C00_2DD0)
    unsigned int  DMA_LENGTH                            ; // 21  (ADDR : 0x9C00_2DD4)
    unsigned int  DMA_ADDR                              ; // 22  (ADDR : 0x9C00_2DD8)
    unsigned int  G091_RESERVED_2[1]                    ; // 23  (ADDR : 0x9C00_2DDC)
    unsigned int  DMA_ADDR_STAT                         ; // 24  (ADDR : 0x9C00_2DE0)
    unsigned int  G091_RESERVED_3[1]                    ; // 25  (ADDR : 0x9C00_2DE4)
    unsigned int  UART_DMA_CTRL                         ; // 26  (ADDR : 0x9C00_2DE8)
    unsigned int  G091_RESERVED_4[1]                    ; // 27  (ADDR : 0x9C00_2DEC)
    unsigned int  G091_RESERVED_5[2]                    ; //     (ADDR : 0x9C00_2DF4) ~ (ADDR : 0x9C00_2DF4)
    unsigned int  SPI_EXTRA_CYCLE                       ; // 30  (ADDR : 0x9C00_2DF8)
    unsigned int  MST_DMA_DATA_RDY                      ; // 31  (ADDR : 0x9C00_2DFC)
}SPI_MAS;

struct pentagram_spi_master {
	struct spi_master *master;
	void __iomem *mas_base;
	void __iomem *sft_base;
	int dma_irq;
	int mas_irq;
	struct clk *spi_clk;
	spinlock_t lock;
	unsigned int spi_max_frequency;
	dma_addr_t tx_dma_phy_base;
	dma_addr_t rx_dma_phy_base;
	void * tx_dma_vir_base;
	void * rx_dma_vir_base;
	struct completion isr_done;
	int isr_flag;
};

static unsigned bufsiz = 4096;

static irqreturn_t pentagram_spi_master_dma_irq(int irq, void *dev)
{
	unsigned long flags;
	struct pentagram_spi_master *pspim = (struct pentagram_spi_master *)dev;
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	unsigned int reg_temp;

	spin_lock_irqsave(&pspim->lock, flags);
	reg_temp = readl(&spim_reg->DMA_CTRL);
	reg_temp |= CLEAR_DMA_W_INT;
	writel(reg_temp, &spim_reg->DMA_CTRL);
	spin_unlock_irqrestore(&pspim->lock, flags);

	complete(&pspim->isr_done);
	return IRQ_HANDLED;
}
static irqreturn_t pentagram_spi_master_mas_irq(int irq, void *dev)
{
	unsigned long flags;
	struct pentagram_spi_master *pspim = (struct pentagram_spi_master *)dev;
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	unsigned int reg_temp;

	spin_lock_irqsave(&pspim->lock, flags);
	reg_temp = readl(&spim_reg->SPI_INT_BUSY);
	reg_temp |= CLEAR_MASTER_INT;
	writel(reg_temp, &spim_reg->SPI_INT_BUSY);
	if(pspim->isr_flag == SPIM_WRITE)
	{
		reg_temp = readl(&spim_reg->SPI_CTRL_CLKSEL);
		reg_temp |= SPI_START;
		writel(reg_temp, &spim_reg->SPI_CTRL_CLKSEL);
	}else
		pspim->isr_flag = SPIM_READ;
	spin_unlock_irqrestore(&pspim->lock, flags);

	complete(&pspim->isr_done);
	return IRQ_HANDLED;
}
static int pentagram_spi_master_dma_write(struct spi_master *master, char *buf, unsigned int len)
{
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	struct device dev = master->dev;
	unsigned int addr;
	unsigned int valid = 0;
	unsigned int data_len = len;
	unsigned int reg_temp = 0;
	unsigned long timeout = msecs_to_jiffies(200);
	unsigned long flags;
	int buf_offset = 0;
	switch(buf[0])
	{
		case 0:
			addr = buf[1];
			valid = 0xff;
			buf_offset = 2;
			break;
		case 1:
			addr = buf[1] | buf[2] <<8;
			valid = 0xfff;
			buf_offset = 3;
			break;
		case 2:
			addr = buf[1] | buf[2] <<8;
			valid = 0xffff;
			buf_offset = 3;
			break;
		case 3:
			addr = buf[1] | buf[2] <<8 | buf[3] <<16;
			valid = 0xfffff;
			buf_offset = 4;
			break;
		default:
			dev_err(&dev,"wrong addr bit num \n");
			return 1;
			break;
	}
	memcpy(pspim->tx_dma_vir_base, buf + buf_offset, len);
	reinit_completion(&pspim->isr_done);

	spin_lock_irqsave(&pspim->lock, flags);
	pspim->isr_flag = SPIM_WRITE;
	spin_unlock_irqrestore(&pspim->lock, flags);

	writel(addr & valid, &spim_reg->MST_TX_DATA_ADDR);
	writel(data_len, &spim_reg->DMA_LENGTH);
	writel(pspim->tx_dma_phy_base , &spim_reg->DMA_ADDR);

	spin_lock_irqsave(&pspim->lock, flags);
	writel(DMA_WRITE, &spim_reg->DMA_CTRL);
	reg_temp = readl(&spim_reg->SPI_CTRL_CLKSEL);
	reg_temp &= CLEAR_ADDR_BIT;
	reg_temp |= ADDR_BIT(buf[0]);
	writel(reg_temp, &spim_reg->SPI_CTRL_CLKSEL);
	spin_unlock_irqrestore(&pspim->lock, flags);

	writel(DMA_DATA_RDY, &spim_reg->MST_DMA_DATA_RDY);
	if(!wait_for_completion_timeout(&pspim->isr_done, timeout))
	{
		dev_err(&dev,"wait_for_completion_timeout\n");
		return 1;
	}
	while((readl(&spim_reg->SPI_INT_BUSY) & SPI_BUSY) == SPI_BUSY)
	{
		dev_dbg(&dev,"spim_reg->SPI_INT_BUSY 0x%x\n",readl(&spim_reg->SPI_INT_BUSY));
	};

	spin_lock_irqsave(&pspim->lock, flags);
	pspim->isr_flag = SPIM_IDLE;
	spin_unlock_irqrestore(&pspim->lock, flags);

	return 0;
}
static int pentagram_spi_master_dma_read(struct spi_master *master, char *cmd, unsigned int len)
{
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	struct device dev = master->dev;
	unsigned int addr;
	unsigned int valid = 0;
	unsigned int data_len = len;
	unsigned int reg_temp = 0;
	unsigned long timeout = msecs_to_jiffies(200);
	unsigned long flags;
	#ifdef SLAVE_INT_IN
	while(pspim->isr_flag != SPIM_READ)
	{
		dev_dbg(&dev,"wait read isr %d\n",pspim->isr_flag);
	};
	//while((readl(&spim_reg->SPI_INT_BUSY) & MASTER_INT) == 0x0)
	//{
	//	dev_dbg(&dev,"wait spim_reg->SPI_INT_BUSY 0x%x\n",readl(&spim_reg->SPI_INT_BUSY));
	//};
	//reg_temp = readl(&spim_reg->SPI_INT_BUSY);
	//reg_temp |= CLEAR_MASTER_INT;
	//writel(reg_temp, &spim_reg->SPI_INT_BUSY);
	#endif
	switch(cmd[0])
	{
		case 0:
			addr = cmd[1];
			valid = 0xff;
			break;
		case 1:
			addr = cmd[1] | cmd[2] <<8;
			valid = 0xfff;
			break;
		case 2:
			addr = cmd[1] | cmd[2] <<8;
			valid = 0xffff;
			break;
		case 3:
			addr = cmd[1] | cmd[2] <<8 | cmd[3] <<16;
			valid = 0xfffff;
			break;
		default:
			dev_err(&dev,"wrong addr bit num \n");
			return 1;
			break;
	}
	reinit_completion(&pspim->isr_done);
	writel(addr & valid, &spim_reg->MST_TX_DATA_ADDR);
	writel(data_len, &spim_reg->DMA_LENGTH);
	writel(pspim->rx_dma_phy_base, &spim_reg->DMA_ADDR);

	spin_lock_irqsave(&pspim->lock, flags);
	writel(DMA_READ, &spim_reg->DMA_CTRL);
	reg_temp = readl(&spim_reg->SPI_CTRL_CLKSEL);
	reg_temp &= CLEAR_ADDR_BIT;
	reg_temp |= ADDR_BIT(cmd[0]);
	reg_temp |= SPI_START;
	writel(reg_temp, &spim_reg->SPI_CTRL_CLKSEL);
	spin_unlock_irqrestore(&pspim->lock, flags);

	if(!wait_for_completion_timeout(&pspim->isr_done,timeout))
	{
		dev_err(&dev,"wait_for_completion_timeout\n");
		return 1;
	}
	while((readl(&spim_reg->DMA_CTRL) & DMA_W_INT) == DMA_W_INT)
	{
		dev_dbg(&dev,"spim_reg->DMA_CTRL 0x%x\n",readl(&spim_reg->DMA_CTRL));
	};

	spin_lock_irqsave(&pspim->lock, flags);
	pspim->isr_flag = SPIM_IDLE;
	spin_unlock_irqrestore(&pspim->lock, flags);

	return 0;
}
static int pentagram_spi_master_setup(struct spi_device *spi)
{
	struct device dev = spi->dev;
	struct pentagram_spi_master *pspim = spi_master_get_devdata(spi->master);
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	unsigned int *grp2_sft_cfg = (unsigned int *)pspim->sft_base;
	unsigned int clk_rate;
	unsigned int div;
	unsigned int clk_sel;
	unsigned int reg_temp;
	unsigned long flags;
	dev_dbg(&dev,"%s\n",__FUNCTION__);

	//set clock
	clk_rate = clk_get_rate(pspim->spi_clk);
	div = clk_rate / pspim->spi_max_frequency;
	clk_sel = (div / 2) - 1;
	reg_temp = PENTAGRAM_SPI_SLAVE_SET | ((clk_sel & 0x3fff) << 16);

	spin_lock_irqsave(&pspim->lock, flags);
	writel(reg_temp, &spim_reg->SPI_CTRL_CLKSEL);
	spin_unlock_irqrestore(&pspim->lock, flags);

	dev_dbg(&dev,"clk_sel 0x%x\n",readl(&spim_reg->SPI_CTRL_CLKSEL));

	//set pinmux
	writel(0x2010201, &grp2_sft_cfg[22]);
	writel(0x4030403, &grp2_sft_cfg[23]);
	writel(0x50005, &grp2_sft_cfg[24]);
	dev_dbg(&dev,"grp2_sft_cfg[22] 0x%x\n",grp2_sft_cfg[22]);
	dev_dbg(&dev,"grp2_sft_cfg[23] 0x%x\n",grp2_sft_cfg[23]);
	dev_dbg(&dev,"grp2_sft_cfg[24] 0x%x\n",grp2_sft_cfg[24]);

	spin_lock_irqsave(&pspim->lock, flags);
	pspim->isr_flag = SPIM_IDLE;
	spin_unlock_irqrestore(&pspim->lock, flags);

	return 0;
}
static int pentagram_spi_master_prepare_message(struct spi_master *master,
				    struct spi_message *msg)
{
	struct device dev = master->dev;
	dev_dbg(&dev,"%s\n",__FUNCTION__);
	return 0;
}
static int pentagram_spi_master_unprepare_message(struct spi_master *master,
				    struct spi_message *msg)
{
	struct device dev = master->dev;
	dev_dbg(&dev,"%s\n",__FUNCTION__);
	return 0;
}
static int pentagram_spi_master_transfer_one(struct spi_master *master, struct spi_device *spi,
					struct spi_transfer *xfer)
{ 
	struct device dev = master->dev;
	dev_dbg(&dev,"%s\n",__FUNCTION__);
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
	unsigned int *grp2_sft_cfg = (unsigned int *)pspim->sft_base;
	unsigned char *data_buf;
	unsigned char *cmd_buf;
	unsigned int len;
	unsigned int temp_reg;
	int mode;
	int ret;
	unsigned char *temp;

	if((xfer->tx_buf)&&(!xfer->rx_buf))
	{
		dev_dbg(&dev,"tx\n");
		data_buf = xfer->tx_buf;
		temp = xfer->tx_buf;
		dev_dbg(&dev,"tx %x %x\n",*temp,*(temp+1));
		len = xfer->len;
		dev_dbg(&dev,"len %d\n",len);
		mode = SPIM_WRITE;
	}
	if(xfer->rx_buf)
	{
		dev_dbg(&dev,"rx\n");
		data_buf = xfer->rx_buf;
		cmd_buf = xfer->tx_buf;
		temp = xfer->rx_buf;
		dev_dbg(&dev,"rx %x %x\n",*temp,*(temp+1));
		temp = xfer->tx_buf;
		dev_dbg(&dev,"tx %x %x\n",*temp,*(temp+1));
		len = xfer->len;
		dev_dbg(&dev,"len %d\n",len);
		mode = SPIM_READ;
	}

	if(mode == SPIM_WRITE)
	{
		ret = pentagram_spi_master_dma_write(master, data_buf, len);
	}else if(mode == SPIM_READ)
	{
		ret = pentagram_spi_master_dma_read(master, cmd_buf, len);
		if(ret == 0)
			memcpy(data_buf, pspim->rx_dma_vir_base, len);
	}

	return ret;
}
static int pentagram_spi_master_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev,"pentagram_spi_master_probe\n");
	struct pentagram_spi_master *pspim;
	struct spi_master *master;
	struct resource *res;
	int ret;
	unsigned int max_freq;

	master = spi_alloc_master(&pdev->dev, sizeof(pspim));
	if (!master) {
		dev_err(&pdev->dev,"spi_alloc_master fail\n");
		return -ENODEV;
	}

	/* setup the master state. */
	master->mode_bits = SPI_MODE_0 ;
	master->bus_num = pdev->id;
	master->setup = pentagram_spi_master_setup;
	master->prepare_message = pentagram_spi_master_prepare_message;
	master->unprepare_message = pentagram_spi_master_unprepare_message;
	master->transfer_one = pentagram_spi_master_transfer_one;
	master->num_chipselect = 1;
	master->dev.of_node = pdev->dev.of_node;

	platform_set_drvdata(pdev, master);
	pspim = spi_master_get_devdata(master);

	pspim->master = master;
	if(!of_property_read_u32(pdev->dev.of_node, "spi-max-frequency", &max_freq)) {
		dev_dbg(&pdev->dev,"max_freq %d\n",max_freq);
		pspim->spi_max_frequency = max_freq;
	}else
		pspim->spi_max_frequency = 25000000;

	spin_lock_init(&pspim->lock);
	init_completion(&pspim->isr_done);

	/* find and map our resources */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, MAS_REG_NAME);
	if (res) {
		pspim->mas_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(pspim->mas_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",MAS_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"mas_base 0x%x\n",pspim->mas_base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, PIN_MUX_REG_NAME);
	if (res) {
		pspim->sft_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(pspim->sft_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",PIN_MUX_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"sft_base 0x%x\n",pspim->sft_base);

	/* irq*/
	pspim->dma_irq = platform_get_irq_byname(pdev, DMA_IRQ_NAME);
	if (pspim->dma_irq < 0) {
		dev_err(&pdev->dev, "failed to get %s\n", DMA_IRQ_NAME);
		goto free_alloc;
	}

	pspim->mas_irq = platform_get_irq_byname(pdev, MAS_IRQ_NAME);
	if (pspim->dma_irq < 0) {
		dev_err(&pdev->dev, "failed to get %s\n", MAS_IRQ_NAME);
		goto free_alloc;
	}

	/* requset irq*/
	ret = devm_request_irq(&pdev->dev, pspim->dma_irq, pentagram_spi_master_dma_irq
						, IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret) {
		dev_err(&pdev->dev, "%s devm_request_irq fail\n", DMA_IRQ_NAME);
		goto free_alloc;
	}

	ret = devm_request_irq(&pdev->dev, pspim->mas_irq, pentagram_spi_master_mas_irq
						, IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret) {
		dev_err(&pdev->dev, "%s devm_request_irq fail\n", MAS_IRQ_NAME);
		goto free_alloc;
	}

	/* clk*/
	pspim->spi_clk = devm_clk_get(&pdev->dev,"sys_pll");
	if(IS_ERR(pspim->spi_clk)) {
		dev_err(&pdev->dev, "devm_clk_get fail\n");
		goto free_alloc;
	}
	ret = clk_prepare_enable(pspim->spi_clk);
	if (ret)
		goto free_alloc;

	/* dma alloc*/
	pspim->tx_dma_vir_base = dma_alloc_coherent(&pdev->dev, bufsiz,
					&pspim->tx_dma_phy_base, GFP_ATOMIC);
	if(!pspim->tx_dma_vir_base) {
		dev_err(&pdev->dev, "dma_alloc_coherent fail\n");
		goto free_clk;
	}
	dev_dbg(&pdev->dev, "tx_dma vir 0x%x\n",pspim->tx_dma_vir_base);
	dev_dbg(&pdev->dev, "tx_dma phy 0x%x\n",pspim->tx_dma_phy_base);

	pspim->rx_dma_vir_base = dma_alloc_coherent(&pdev->dev, bufsiz,
					&pspim->rx_dma_phy_base, GFP_ATOMIC);
	if(!pspim->rx_dma_vir_base) {
		dev_err(&pdev->dev, "dma_alloc_coherent fail\n");
		goto free_tx_dma;
	}
	dev_dbg(&pdev->dev, "rx_dma vir 0x%x\n",pspim->rx_dma_vir_base);
	dev_dbg(&pdev->dev, "rx_dma phy 0x%x\n",pspim->rx_dma_phy_base);
	
	ret = spi_register_master(master);
	if (ret != 0) {
		dev_err(&pdev->dev, "spi_register_master fail\n");
		goto free_rx_dma;
	}
	return 0;

free_rx_dma:
	dma_free_coherent(&pdev->dev, bufsiz, pspim->rx_dma_vir_base, pspim->rx_dma_phy_base);
free_tx_dma:
	dma_free_coherent(&pdev->dev, bufsiz, pspim->tx_dma_vir_base, pspim->tx_dma_phy_base);
free_clk:
	clk_disable_unprepare(pspim->spi_clk);
free_alloc:
	spi_master_put(master);

	dev_dbg(&pdev->dev, "spi_master_probe done\n");
	return ret;
}

static int pentagram_spi_master_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);

	dma_free_coherent(&pdev->dev, bufsiz, pspim->tx_dma_vir_base, pspim->tx_dma_phy_base);
	dma_free_coherent(&pdev->dev, bufsiz, pspim->rx_dma_vir_base, pspim->rx_dma_phy_base);

	clk_disable_unprepare(pspim->spi_clk);
	spi_unregister_master(pspim->master);
}

static const struct of_device_id pentagram_spi_master_ids[] = {
	{.compatible = "sunplus,pentagram-spi-master"},
	{}
};
MODULE_DEVICE_TABLE(of, pentagram_spi_master_ids);

static struct platform_driver pentagram_spi_master_driver = {
	.probe = pentagram_spi_master_probe,
	.remove = pentagram_spi_master_remove,
	.driver = {
		.name = "pentagram-spi-master",
		.of_match_table = pentagram_spi_master_ids,
	},
};
module_platform_driver(pentagram_spi_master_driver);

MODULE_AUTHOR("Sunplus");
MODULE_DESCRIPTION("Sunplus SPI master driver");
MODULE_LICENSE("GPL");

