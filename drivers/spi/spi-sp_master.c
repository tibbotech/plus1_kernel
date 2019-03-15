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
//#define DEBUG
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/reset.h> 
#include <linux/dma-mapping.h>
#include <linux/io.h>

#define SLAVE_INT_IN


/* ---------------------------------------------------------------------------------------------- */

//#define SPI_FUNC_DEBUG
//#define SPI_DBG_INFO
//#define SPI_DBG_ERR

#ifdef SPI_FUNC_DEBUG
	#define FUNC_DEBUG()    printk(KERN_INFO "[SPI] Debug: %s(%d)\n", __FUNCTION__, __LINE__)
#else
	#define FUNC_DEBUG()
#endif

#ifdef SPI_DBG_INFO
#define DBG_INFO(fmt, args ...)	printk(KERN_INFO "[SPI] Info: "  fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif

#ifdef SPI_DBG_ERR
#define DBG_ERR(fmt, args ...)	printk(KERN_ERR "[SPI] Info: "  fmt, ## args)
#else
#define DBG_ERR(fmt, args ...)
#endif
/* ---------------------------------------------------------------------------------------------- */


//#define SPI_PIN_SETTING

#define MAS_REG_NAME "spi_master"
#define SLA_REG_NAME "spi_slave"

#define PIN_MUX_MAS_REG_NAME "grp2_sft"
#define PIN_MUX_SLA_REG_NAME "grp3_sft"

#define DMA_IRQ_NAME "dma_w_intr"
#define MAS_IRQ_NAME "mas_risc_intr"

#define SLA_IRQ_NAME "slave_risc_intr"

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

#define SPI_MASTER_NUM (4)

/* slave*/
#define CLEAR_SLAVE_INT (1<<8)
#define SLAVE_DATA_RDY (1<<0)

enum SPI_MASTER_MODE
{
	SPIM_READ = 0,
	SPIM_WRITE = 1,
	SPIM_IDLE = 2
};

enum SPI_SLAVE_MODE
{
	SPIS_READ = 0,
	SPIS_WRITE = 1,
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


typedef struct{
	// Group 092 : SPI_SLAVE
	unsigned int SLV_TX_DATA_2_1_0                     ; // 00  (ADDR : 0x9C00_2E00) 
	unsigned int SLV_TX_DATA_6_5_4_3                   ; // 01  (ADDR : 0x9C00_2E04)
	unsigned int SLV_TX_DATA_10_9_8_7                  ; // 02  (ADDR : 0x9C00_2E08)
	unsigned int SLV_TX_DATA_14_13_12_11               ; // 03  (ADDR : 0x9C00_2E0C)
	unsigned int SLV_TX_DATA_15                        ; // 04  (ADDR : 0x9C00_2E10)
	unsigned int G092_RESERVED_0[4]                    ; //     (ADDR : 0x9C00_2E14) ~ (ADDR : 0x9C00_2E20)
	unsigned int SLV_RX_DATA_3_2_1_0                   ; // 09  (ADDR : 0x9C00_2E24)
	unsigned int SLV_RX_DATA_7_6_5_4                   ; // 10  (ADDR : 0x9C00_2E28)
	unsigned int SLV_RX_DATA_11_10_9_8                 ; // 11  (ADDR : 0x9C00_2E2C)
	unsigned int SLV_RX_DATA_15_14_13_12               ; // 12  (ADDR : 0x9C00_2E30)
	unsigned int G092_RESERVED_1[4]                    ; //     (ADDR : 0x9C00_2E34) ~ (ADDR : 0x9C00_2E40)
	unsigned int RISC_INT_DATA_RDY                     ; // 17  (ADDR : 0x9C00_2E44)
	unsigned int SLV_DMA_CTRL                          ; // 18  (ADDR : 0x9C00_2E48)
	unsigned int SLV_DMA_LENGTH                        ; // 19  (ADDR : 0x9C00_2E4C)
	unsigned int SLV_DMA_INI_ADDR                      ; // 20  (ADDR : 0x9C00_2E50)
	unsigned int G092_RESERVED_2[2]                    ; //     (ADDR : 0x9C00_2E54) ~ (ADDR : 0x9C00_2E58)
	unsigned int ADDR_SPI_BUSY                         ; // 23  (ADDR : 0x9C00_2E5C)
	unsigned int G092_RESERVED_3[8]                    ; //     (ADDR : 0x9C00_2E60) ~ (ADDR : 0x9C00_2E7C)
	
}SPI_SLA;


typedef struct  {
	volatile unsigned int sft_cfg_22;    /* 00 */
	volatile unsigned int sft_cfg_23;    /* 01 */
	volatile unsigned int sft_cfg_24;    /* 02 */
	volatile unsigned int sft_cfg_25;    /* 03 */
	volatile unsigned int sft_cfg_26;    /* 04 */
	volatile unsigned int sft_cfg_27;    /* 05 */
	volatile unsigned int sft_cfg_28;    /* 06 */
	volatile unsigned int sft_cfg_29;    /* 07 */
	volatile unsigned int sft_cfg_30;    /* 06 */
	volatile unsigned int sft_cfg_31;    /* 07 */	
} SPI_MAS_PIN;


typedef struct  {
	volatile unsigned int sft_cfg_0;    /* 00 */
	volatile unsigned int sft_cfg_1;    /* 01 */
	volatile unsigned int sft_cfg_2;    /* 02 */
	volatile unsigned int sft_cfg_3;    /* 03 */
	volatile unsigned int sft_cfg_4;    /* 04 */
	volatile unsigned int sft_cfg_5;    /* 05 */
	volatile unsigned int sft_cfg_6;    /* 06 */
	volatile unsigned int sft_cfg_7;    /* 07 */
	volatile unsigned int sft_cfg_8;    /* 06 */
	volatile unsigned int sft_cfg_9;    /* 07 */	
} SPI_SLA_PIN;


struct pentagram_spi_master {
	struct spi_master *master;
	void __iomem *mas_base;

	void __iomem *sla_base;	
	void __iomem *sft_base;
	void __iomem *sft3_base;
	
	int dma_irq;
	int mas_irq;

	int sla_irq;
	
	struct clk *spi_clk;
	struct reset_control *rstc;	
	
	spinlock_t lock;
	struct mutex		buf_lock;
	unsigned int spi_max_frequency;
	dma_addr_t tx_dma_phy_base;
	dma_addr_t rx_dma_phy_base;
	void * tx_dma_vir_base;
	void * rx_dma_vir_base;
	struct completion isr_done;
	
	struct completion sla_isr;
	unsigned int bufsiz;	
	
	int isr_flag;
};

static unsigned bufsiz = 4096;

static irqreturn_t pentagram_spi_slave_sla_irq(int irq, void *dev)
{
	unsigned long flags;
	struct pentagram_spi_master *pspim = (struct pentagram_spi_master *)dev;
	SPI_SLA* spis_reg = (SPI_SLA *)pspim->sla_base;
	unsigned int reg_temp;

    FUNC_DEBUG();

	spin_lock_irqsave(&pspim->lock, flags);
	reg_temp = readl(&spis_reg->RISC_INT_DATA_RDY);
	reg_temp |= CLEAR_SLAVE_INT;
	writel(reg_temp, &spis_reg->RISC_INT_DATA_RDY);
	spin_unlock_irqrestore(&pspim->lock, flags);

	complete(&pspim->sla_isr);

    DBG_INFO("pentagram_spi_slave_sla_irq done\n");	
	  
	return IRQ_HANDLED;
}
int pentagram_spi_slave_dma_rw(struct spi_device *spi,char *buf, unsigned int len, int RW_phase)
{

	struct pentagram_spi_master *pspim = spi_master_get_devdata(spi->master);

	SPI_SLA* spis_reg = (SPI_SLA *)(pspim->sla_base);
	SPI_MAS* spim_reg = (SPI_MAS *)(pspim->mas_base);
	struct device dev = spi->dev;
	unsigned int reg_temp;
	unsigned long timeout = msecs_to_jiffies(2000);


    FUNC_DEBUG();


	if(RW_phase == SPIS_WRITE) {

			  
		memcpy(pspim->tx_dma_vir_base, buf, len);
		writel_relaxed(DMA_WRITE, &spis_reg->SLV_DMA_CTRL);
		writel_relaxed(len, &spis_reg->SLV_DMA_LENGTH);
		writel_relaxed(pspim->tx_dma_phy_base, &spis_reg->SLV_DMA_INI_ADDR);
		reg_temp = readl(&spis_reg->RISC_INT_DATA_RDY);
		reg_temp |= SLAVE_DATA_RDY;
		writel(reg_temp, &spis_reg->RISC_INT_DATA_RDY);
		//regs1->SLV_DMA_CTRL = 0x4d;
		//regs1->SLV_DMA_LENGTH = 0x50;//0x50
		//regs1->SLV_DMA_INI_ADDR = 0x300;
		//regs1->RISC_INT_DATA_RDY |= 0x1;
	}else if (RW_phase == SPIS_READ) {

		//reinit_completion(&pspim->dma_isr);
		reinit_completion(&pspim->isr_done);
		writel(DMA_READ, &spis_reg->SLV_DMA_CTRL);
		writel(len, &spis_reg->SLV_DMA_LENGTH);
		writel(pspim->rx_dma_phy_base, &spis_reg->SLV_DMA_INI_ADDR);
		if(!wait_for_completion_timeout(&pspim->sla_isr,timeout)){
			dev_err(&dev,"wait_for_completion_timeout\n");
			return 1;
		}

	      if(!wait_for_completion_timeout(&pspim->isr_done,timeout)){
			dev_err(&dev,"wait_for_completion_timeout\n");
			return 1;
		}

		while((readl(&spim_reg->DMA_CTRL) & DMA_W_INT) == DMA_W_INT)
		{
			dev_dbg(&dev,"spim_reg->DMA_CTRL 0x%x\n",readl(&spim_reg->DMA_CTRL));
		};

		printk(KERN_INFO "[SPI_slave] spi_slave_read 005\n");
		memcpy(buf, pspim->rx_dma_vir_base, len);
		printk(KERN_INFO "[SPI_slave] spi_slave_read 006\n");
		/* read*/
		//regs1->SLV_DMA_CTRL = 0xd;
		//regs1->SLV_DMA_LENGTH = 0x50;//0x50
		//regs1->SLV_DMA_INI_ADDR = 0x400;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(pentagram_spi_slave_dma_rw);





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

    FUNC_DEBUG();

    mutex_lock(&pspim->buf_lock);


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

	mutex_unlock(&pspim->buf_lock);
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

    FUNC_DEBUG();
   
    mutex_lock(&pspim->buf_lock);
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

	if(!wait_for_completion_timeout(&pspim->isr_done,timeout)){
		dev_err(&dev,"wait_for_completion_timeout\n");
		return 1;
	}
	while((readl(&spim_reg->DMA_CTRL) & DMA_W_INT) == DMA_W_INT){
		dev_dbg(&dev,"spim_reg->DMA_CTRL 0x%x\n",readl(&spim_reg->DMA_CTRL));
	};

	spin_lock_irqsave(&pspim->lock, flags);
	pspim->isr_flag = SPIM_IDLE;
	spin_unlock_irqrestore(&pspim->lock, flags);

	mutex_unlock(&pspim->buf_lock);


	return 0;
}
static int pentagram_spi_master_setup(struct spi_device *spi)
{
	struct device dev = spi->dev;
	struct pentagram_spi_master *pspim = spi_master_get_devdata(spi->master);
	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;

#ifdef SPI_PIN_SETTING		
	SPI_MAS_PIN *grp2_sft_cfg = (SPI_MAS_PIN *) pspim->sft_base;
	SPI_SLA_PIN *grp3_sft_cfg = (SPI_SLA_PIN *) pspim->sft3_base;
#endif

	unsigned int spi_id;
	unsigned int clk_rate;
	unsigned int div;
	unsigned int clk_sel;
	unsigned int reg_temp;
	unsigned long flags;
	dev_dbg(&dev,"%s\n",__FUNCTION__);


       spi_id = pspim->master->bus_num;


     FUNC_DEBUG();
     DBG_INFO(" spi_id  = %d\n",spi_id);


	//set pinmux

#ifdef SPI_PIN_SETTING	
	
if( spi_id < SPI_MASTER_NUM){
	switch(spi_id)
	{
		case 0:			
	writel(0x02010201, &(grp2_sft_cfg->sft_cfg_22));
	writel(0x04030403, &(grp2_sft_cfg->sft_cfg_23));
	writel(0x00050005, &(grp2_sft_cfg->sft_cfg_24));	
	
	writel(0x18171817, &(grp3_sft_cfg->sft_cfg_0));
	writel(0x1A191A19, &(grp3_sft_cfg->sft_cfg_1));
	writel(0x001B001B, &(grp3_sft_cfg->sft_cfg_2));		
			break;
		case 1:
	writel(0x12001200, &(grp2_sft_cfg->sft_cfg_24));
	writel(0x14131413, &(grp2_sft_cfg->sft_cfg_25));
	writel(0x16151615, &(grp2_sft_cfg->sft_cfg_26));	

	writel(0x26002600, &(grp3_sft_cfg->sft_cfg_2));
	writel(0x28272827, &(grp3_sft_cfg->sft_cfg_3));
	writel(0x2A292A29, &(grp3_sft_cfg->sft_cfg_4));

//	writel(0x06000600, &(grp2_sft_cfg->sft_cfg_24));
//	writel(0x08070807, &(grp2_sft_cfg->sft_cfg_25));
//	writel(0x0A090A09, &(grp2_sft_cfg->sft_cfg_26));	

//	writel(0x1C001C00, &(grp3_sft_cfg->sft_cfg_2));
//	writel(0x1E1D1E1D, &(grp3_sft_cfg->sft_cfg_3));
//	writel(0x201F201F, &(grp3_sft_cfg->sft_cfg_4));	
			break;
		case 2:						
	writel(0x0E0B0E0B, &(grp2_sft_cfg->sft_cfg_27));   // GPIO_P2_3[0x0C] : TCLK  ;  GPIO_P2_4[0x0D] : RCLK  ; 
	writel(0x100F100F, &(grp2_sft_cfg->sft_cfg_28));
	writel(0x00110011, &(grp2_sft_cfg->sft_cfg_29));	

	writel(0x22212221, &(grp3_sft_cfg->sft_cfg_5));
	writel(0x24232423, &(grp3_sft_cfg->sft_cfg_6));
	writel(0x00250025, &(grp3_sft_cfg->sft_cfg_7));		
			break;
		case 3:	
	writel(0x06000600, &(grp2_sft_cfg->sft_cfg_29));
	writel(0x08070807, &(grp2_sft_cfg->sft_cfg_30));
	writel(0x0A090A09, &(grp2_sft_cfg->sft_cfg_31));	

	writel(0x1C001C00, &(grp3_sft_cfg->sft_cfg_7));
	writel(0x1E1D1E1D, &(grp3_sft_cfg->sft_cfg_8));
	writel(0x201F201F, &(grp3_sft_cfg->sft_cfg_9));
			
//	writel(0x12001200, &(grp2_sft_cfg->sft_cfg_29));
//	writel(0x14131413, &(grp2_sft_cfg->sft_cfg_30));
//	writel(0x16151615, &(grp2_sft_cfg->sft_cfg_31));	

//	writel(0x26002600, &(grp3_sft_cfg->sft_cfg_7));
//	writel(0x28272827, &(grp3_sft_cfg->sft_cfg_8));
//	writel(0x2A292A29, &(grp3_sft_cfg->sft_cfg_9));		
			break;
		default:
			dev_err(&dev,"wrong pin mux\n");
			return 1;
			break;
        	}

	dev_dbg(&dev,"grp2_sft_cfg[22] 0x%x\n",grp2_sft_cfg->sft_cfg_22);
	dev_dbg(&dev,"grp2_sft_cfg[23] 0x%x\n",grp2_sft_cfg->sft_cfg_22);
	dev_dbg(&dev,"grp2_sft_cfg[24] 0x%x\n",grp2_sft_cfg->sft_cfg_22);

	dev_dbg(&dev,"grp3_sft_cfg[7] 0x%x\n",grp3_sft_cfg->sft_cfg_7);
	dev_dbg(&dev,"grp3_sft_cfg[8] 0x%x\n",grp3_sft_cfg->sft_cfg_8);
	dev_dbg(&dev,"grp3_sft_cfg[9] 0x%x\n",grp3_sft_cfg->sft_cfg_9);
	
     }
#endif


	//set clock
	clk_rate = clk_get_rate(pspim->spi_clk);
	div = clk_rate / pspim->spi_max_frequency;
	clk_sel = (div / 2) - 1;
	reg_temp = PENTAGRAM_SPI_SLAVE_SET | ((clk_sel & 0x3fff) << 16);

	spin_lock_irqsave(&pspim->lock, flags);
	writel(reg_temp, &spim_reg->SPI_CTRL_CLKSEL);
	spin_unlock_irqrestore(&pspim->lock, flags);

	dev_dbg(&dev,"clk_sel 0x%x\n",readl(&spim_reg->SPI_CTRL_CLKSEL));
	
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

	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);
	struct device dev = master->dev;

//	SPI_MAS* spim_reg = (SPI_MAS *)pspim->mas_base;
//	unsigned int *grp2_sft_cfg = (unsigned int *)sft_base;
	unsigned char *data_buf;
	unsigned char *cmd_buf;
	//const u8 *data_buf;
	//u8 *cmd_buf;
	unsigned int len;
	//unsigned int temp_reg;
	int mode;
	int ret;
	unsigned char *temp;


    FUNC_DEBUG();


	dev_dbg(&dev,"%s\n",__FUNCTION__);	


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
	struct resource *res;
	int ret;
	unsigned int max_freq;
	struct spi_master *master;
	struct pentagram_spi_master *pspim;	


    FUNC_DEBUG();


	master = spi_alloc_master(&pdev->dev, sizeof(pspim));
	if (!master) {
		dev_err(&pdev->dev,"spi_alloc_master fail\n");
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		pdev->id = of_alias_get_id(pdev->dev.of_node, "spi");
	}


    DBG_INFO(" pdev->id  = %d\n",pdev->id);
    DBG_INFO(" pdev->dev.of_node  = %d\n",pdev->dev.of_node);


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
	mutex_init(&pspim->buf_lock);
	init_completion(&pspim->isr_done);
	init_completion(&pspim->sla_isr);



	/* find and map our resources */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, MAS_REG_NAME);
	if (res) {
		pspim->mas_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(pspim->mas_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",MAS_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"mas_base 0x%x\n",(unsigned int)pspim->mas_base);



	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, SLA_REG_NAME);
	if (res) {
		pspim->sla_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(pspim->sla_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",SLA_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"sla_base 0x%x\n",(unsigned int)pspim->sla_base);



	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, PIN_MUX_MAS_REG_NAME);
	if (res) {
		pspim->sft_base = devm_ioremap(&pdev->dev, res->start, resource_size(res) );
		if (IS_ERR(pspim->sft_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",PIN_MUX_MAS_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"sft_base 0x%x\n",(unsigned int)pspim->sft_base);


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, PIN_MUX_SLA_REG_NAME);
	if (res) {
		pspim->sft3_base = devm_ioremap(&pdev->dev, res->start, resource_size(res) );
		if (IS_ERR(pspim->sft_base)) {
			dev_err(&pdev->dev,"%s devm_ioremap_resource fail\n",PIN_MUX_SLA_REG_NAME);
			goto free_alloc;
		}
	}
	dev_dbg(&pdev->dev,"sft3_base 0x%x\n",(unsigned int)pspim->sft3_base);


	/* irq*/
	pspim->dma_irq = platform_get_irq_byname(pdev, DMA_IRQ_NAME);
	if (pspim->dma_irq < 0) {
		dev_err(&pdev->dev, "failed to get %s\n", DMA_IRQ_NAME);
		goto free_alloc;
	}

	pspim->mas_irq = platform_get_irq_byname(pdev, MAS_IRQ_NAME);
	if (pspim->mas_irq < 0) {
		dev_err(&pdev->dev, "failed to get %s\n", MAS_IRQ_NAME);
		goto free_alloc;
	}


	pspim->sla_irq = platform_get_irq_byname(pdev, SLA_IRQ_NAME);
	if (pspim->sla_irq < 0) {
		dev_err(&pdev->dev, "failed to get %s\n", SLA_IRQ_NAME);
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


	ret = devm_request_irq(&pdev->dev, pspim->sla_irq, pentagram_spi_slave_sla_irq
						, IRQF_TRIGGER_RISING, pdev->name, pspim);
	if (ret) {
		dev_err(&pdev->dev, "%s devm_request_irq fail\n", SLA_IRQ_NAME);
		goto free_alloc;
	}

	/* clk*/
	pspim->spi_clk = devm_clk_get(&pdev->dev,NULL);
	if(IS_ERR(pspim->spi_clk)) {
		dev_err(&pdev->dev, "devm_clk_get fail\n");
		goto free_alloc;
	}
	ret = clk_prepare_enable(pspim->spi_clk);
	if (ret)
		goto free_alloc;

	/* reset*/
	pspim->rstc = devm_reset_control_get(&pdev->dev, NULL);
	DBG_INFO( "pspim->rstc : 0x%x \n",pspim->rstc);
	if (IS_ERR(pspim->rstc)) {
		ret = PTR_ERR(pspim->rstc);
		dev_err(&pdev->dev, "SPI failed to retrieve reset controller: %d\n", ret);
		goto free_clk;
	}
	ret = reset_control_deassert(pspim->rstc);
	if (ret)
		goto free_clk;


	/* dma alloc*/
	pspim->tx_dma_vir_base = dma_alloc_coherent(&pdev->dev, bufsiz,
					&pspim->tx_dma_phy_base, GFP_ATOMIC);
	if(!pspim->tx_dma_vir_base) {
		dev_err(&pdev->dev, "dma_alloc_coherent fail\n");
		goto free_reset_assert;
	}
	dev_dbg(&pdev->dev, "tx_dma vir 0x%x\n",(unsigned int)pspim->tx_dma_vir_base);
	dev_dbg(&pdev->dev, "tx_dma phy 0x%x\n",(unsigned int)pspim->tx_dma_phy_base);

	pspim->rx_dma_vir_base = dma_alloc_coherent(&pdev->dev, bufsiz,
					&pspim->rx_dma_phy_base, GFP_ATOMIC);
	if(!pspim->rx_dma_vir_base) {
		dev_err(&pdev->dev, "dma_alloc_coherent fail\n");
		goto free_tx_dma;
	}
	dev_dbg(&pdev->dev, "rx_dma vir 0x%x\n",(unsigned int)pspim->rx_dma_vir_base);
	dev_dbg(&pdev->dev, "rx_dma phy 0x%x\n",(unsigned int)pspim->rx_dma_phy_base);

	
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
free_reset_assert:
	reset_control_assert(pspim->rstc);
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

    FUNC_DEBUG();

	dma_free_coherent(&pdev->dev, bufsiz, pspim->tx_dma_vir_base, pspim->tx_dma_phy_base);
	dma_free_coherent(&pdev->dev, bufsiz, pspim->rx_dma_vir_base, pspim->rx_dma_phy_base);


	spi_unregister_master(pspim->master);
	clk_disable_unprepare(pspim->spi_clk);
	reset_control_assert(pspim->rstc);

	return 0;
	
}

static int pentagram_spi_master_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);

    FUNC_DEBUG();

	reset_control_assert(pspim->rstc);


	return 0;
	
}

static int pentagram_spi_master_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pentagram_spi_master *pspim = spi_master_get_devdata(master);

    FUNC_DEBUG();
	
	reset_control_deassert(pspim->rstc);
	clk_prepare_enable(pspim->spi_clk);


	return 0;
	
}

static const struct of_device_id pentagram_spi_master_ids[] = {
	{.compatible = "sunplus,sp7021-spi-master"},
	{}
};
MODULE_DEVICE_TABLE(of, pentagram_spi_master_ids);

static struct platform_driver pentagram_spi_master_driver = {
	.probe = pentagram_spi_master_probe,
	.remove = pentagram_spi_master_remove,
	.suspend	= pentagram_spi_master_suspend,
	.resume		= pentagram_spi_master_resume,	
	.driver = {
		.name = "sunplus,sp7021-spi-master",
		.of_match_table = pentagram_spi_master_ids,
	},
};
module_platform_driver(pentagram_spi_master_driver);

MODULE_AUTHOR("Sunplus");
MODULE_DESCRIPTION("Sunplus SPI master driver");
MODULE_LICENSE("GPL");

