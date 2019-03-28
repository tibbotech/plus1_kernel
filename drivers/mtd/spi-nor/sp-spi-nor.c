#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/completion.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/mutex.h>
#include <linux/sizes.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>

#define SP_SPINOR_DMA 1
#define CFG_BUFF_MAX		(18 << 10)
typedef struct 
{
    unsigned int sft_cfg0;
    unsigned int sft_cfg1;
    unsigned int sft_cfg2;
    unsigned int sft_cfg3;
    unsigned int sft_cfg4;
    unsigned int sft_reserve[27];
}MOON1_REG;
void __iomem *moon1_base;

//spi_ctrl
#define SPI_CTRL_BUSY (1<<31)
#define PAGE_ACCESS_MODE (1<<29)
#define PAGE_ACCESSMODE_DISABLE (~(1<<29))
#define AUTO_SPI_WEL_EN (1<<19)
#define CUST_CMD(x) (x<<8)
#define CLEAR_CUST_CMD (~0xffff)
enum SPI_CHIP_SEL
{
	  A_CHIP = 1<<24,
	  B_CHIP = 1<<25
};
enum SPI_CLOCK_SEL
{
	  SPI_CLK_D_2 = (1)<<16,
	  SPI_CLK_D_4 = (2)<<16,
	  SPI_CLK_D_6 = (3)<<16,
	  SPI_CLK_D_8 = (4)<<16,
	  SPI_CLK_D_16 = (5)<<16,
	  SPI_CLK_D_24 = (6)<<16,
	  SPI_CLK_D_32 = (7)<<16
};
enum SPI_PIO_ADDRESS_BYTE
{
	  ADDR_0B = 0,
	  ADDR_1B = 1,
	  ADDR_2B = 2,
	  ADDR_3B = 3
};
enum SPI_PIO_DATA_BYTE
{
	  BYTE_0 = 0<<4,
	  BYTE_1 = 1<<4,
	  BYTE_2 = 2<<4,
	  BYTE_3 = 3<<4,
	  BYTE_4 = 4<<4
};
enum SPI_PIO_CMD
{
	  READ_CMD = 0<<2,
	  WRITE_CMD = 1<<2
};
//spi_auto_cfg
#define USER_DEFINED_READ(x) (x<<24)
#define USER_DEFINED_READ_EN (1<<20)
#define PREFETCH_ENABLE (1<<22)
#define PREFETCH_DISABLE (~(1<<22))
#define PIO_TRIGGER (1<<21)
#define AUTO_RDSR_EN (1<<18)
#define DMA_TRIGGER (1<<17)
//spi_cfg0
#define ENHANCE_DATA(x) (x<<24)
#define CLEAR_ENHANCE_DATA (~(0xff<<24))
#define DATA64_EN (1<<20)
#define DATA64_DIS ~(1<<20)
#define SPI_TRS_MODE (1<<19)
#define SPI_DATA64_MAX_LEN ((1<<16)-1)
#define CLEAR_DATA64_LEN (~0xffff)
//spi_cfg1/2
#define SPI_DUMMY_CYC(x) (x<<24)
#define DUMMY_CYCLE(x) (x)
enum SPI_ENHANCE_BIT
{
	  SPI_ENHANCE_NO = (0)<<22,
	  SPI_ENHANCE_1b = (1)<<22,
	  SPI_ENHANCE_2b = (2)<<22,
	  SPI_ENHANCE_4b = (3)<<22
};
enum SPI_CMD_BIT
{
	  SPI_CMD_NO = (0)<<16,
	  SPI_CMD_1b = (1)<<16,
	  SPI_CMD_2b = (2)<<16,
	  SPI_CMD_4b = (3)<<16
};
enum SPI_ADDR_BIT
{
	  SPI_ADDR_NO = (0)<<18,
	  SPI_ADDR_1b = (1)<<18,
	  SPI_ADDR_2b = (2)<<18,
	  SPI_ADDR_4b = (3)<<18
};
enum SPI_DATA_BIT
{
	  SPI_DATA_NO = (0)<<20,
	  SPI_DATA_1b = (1)<<20,
	  SPI_DATA_2b = (2)<<20,
	  SPI_DATA_4b = (3)<<20
};
enum SPI_CMD_OEN_BIT
{
	  SPI_CMD_OEN_NO = 0,
	  SPI_CMD_OEN_1b = 1,
	  SPI_CMD_OEN_2b = 2,
	  SPI_CMD_OEN_4b = 3
};
enum SPI_ADDR_OEN_BIT
{
	  SPI_ADDR_OEN_NO = (0)<<2,
	  SPI_ADDR_OEN_1b = (1)<<2,
	  SPI_ADDR_OEN_2b = (2)<<2,
	  SPI_ADDR_OEN_4b = (3)<<2
};
enum SPI_DATA_OEN_BIT
{
	  SPI_DATA_OEN_NO = (0)<<4,
	  SPI_DATA_OEN_1b = (1)<<4,
	  SPI_DATA_OEN_2b = (2)<<4,
	  SPI_DATA_OEN_4b = (3)<<4
};
enum SPI_DATA_IEN_BIT
{
	  SPI_DATA_IEN_NO = (0)<<6,
	  SPI_DATA_IEN_DQ0 = (1)<<6,
	  SPI_DATA_IEN_DQ1 = (2)<<6
};
enum SPI_CMD_IO_BIT
{
	  CMD_0 = 0,
	  CMD_1 = 1,
	  CMD_2 = 2,
	  CMD_4 = 4
};
enum SPI_ADDR_IO_BIT
{
	  ADDR_0 = 0,
	  ADDR_1 = 1,
	  ADDR_2 = 2,
	  ADDR_4 = 4
};
enum SPI_DATA_IO_BIT
{
	  DATA_0 = 0,
	  DATA_1 = 1,
	  DATA_2 = 2,
	  DATA_4 = 4
};
//spi_buf_addr
#define DATA64_READ_ADDR(x) (x<<16)
#define DATA64_WRITE_ADDR(x) (x)
//spi_status_2
#define SPI_SRAM_ST (0x7<<13)
enum SPI_SRAM_STATUS
{
	  SRAM_CONFLICT = 0,
	  SRAM_EMPTY = 1,
	  SRAM_FULL = 2
};

#define CMD_FAST_READ (0xb)
#define CMD_READ_STATUS (0x5)
#define SPI_TIMEOUT 450

typedef struct{
	  unsigned char enhance_en;
	  unsigned char enhance_bit;
	  unsigned char enhance_bit_mode;
	  unsigned char enhance_data;
}SPI_ENHANCE;

enum ERASE_TYPE
{
	  SECTOR = 0,
	  BLOCK = 1,
	  CHIP = 2
};

enum SPI_USEABLE_DQ
{
	  DQ0 = 1<<20,
	  DQ1 = 1<<21,
	  DQ2 = 1<<22,
	  DQ3 = 1<<23
};

enum SPI_DMA_MODE
{
	  DMA_OFF = 0,
	  DMA_ON = 1
};

enum SPI_INTR_STATUS
{
	  BUFFER_ENOUGH = 1,
	  DMA_DONE = 2,
	  PIO_DONE = 4
};

typedef struct{
	// Group 022 : SPI_FLASH
	  unsigned int  spi_ctrl							; 
	  unsigned int  spi_timing						; 
	  unsigned int  spi_page_addr						; 
	  unsigned int  spi_data							; 
	  unsigned int  spi_status						; 
	  unsigned int  spi_auto_cfg						; 
	  unsigned int  spi_cfg0							; 
	  unsigned int  spi_cfg1							; 
	  unsigned int  spi_cfg2							; 
	  unsigned int  spi_data64						; 
	  unsigned int  spi_buf_addr						; 
	  unsigned int  spi_status_2						; 
	  unsigned int  spi_err_status					; 
	  unsigned int  spi_mem_data_addr					; 
	  unsigned int  spi_mem_parity_addr				; 
	  unsigned int  spi_col_addr						; 
	  unsigned int  spi_bch							; 
	  unsigned int  spi_intr_msk						; 
	  unsigned int  spi_intr_sts						; 
	  unsigned int  spi_page_size						; 
	  unsigned int  G22_RESERVED[12]					; 
}SPI_NOR_REG;

struct sp_spi_nor 
{
    struct spi_nor nor;
    struct device *dev;
    void __iomem *io_base;
    struct clk *ctrl_clk;
    struct clk *nor_clk;
    u32 clk_rate;
    u32 clk_src;
    u32 read_mode;
    u32 nor_size;
#if (SP_SPINOR_DMA)
    struct{
    	  uint32_t idx;
        uint32_t size;
		    void *virt;
		    dma_addr_t phys;
	  }buff;
#endif
	  struct mutex lock;
	  int irq;
	  wait_queue_head_t wq;
	  int busy;
};

static irqreturn_t sp_nor_int(int irq, void *dev)
{
	struct sp_spi_nor *pspi = dev;
	SPI_NOR_REG *spi_reg = pspi->io_base;
	uint32_t value;
	
	/* clear intrrupt flag */	
	value = readl(&spi_reg->spi_intr_sts);
	//dev_dbg(pspi->dev,"int 0x%x\n",value);
	writel(value, &spi_reg->spi_intr_sts);

	pspi->busy = 0;
	wake_up(&pspi->wq);

	return IRQ_HANDLED;
}

static int sp_spi_nor_init(SPI_NOR_REG *spi_reg)
{
	unsigned int reg_temp;
	
	writel(A_CHIP | SPI_CLK_D_4,&spi_reg->spi_ctrl);

	writel(SPI_CMD_OEN_1b | SPI_ADDR_OEN_1b | SPI_DATA_OEN_1b | SPI_CMD_1b | SPI_ADDR_1b
		| SPI_DATA_1b | SPI_ENHANCE_NO | SPI_DUMMY_CYC(0) | SPI_DATA_IEN_DQ1,&spi_reg->spi_cfg1);
	reg_temp = readl(&spi_reg->spi_auto_cfg);
	reg_temp &= ~(AUTO_RDSR_EN);
	writel(reg_temp,&spi_reg->spi_auto_cfg);
	return 0;
}
#if (SP_SPINOR_DMA)
static void spi_nor_io_CUST_config(SPI_NOR_REG *reg_base, u8 cmd_b, u8 addr_b, u8 data_b, SPI_ENHANCE enhance,u8 dummy)
{
    SPI_NOR_REG *spi_reg = (SPI_NOR_REG *)reg_base;
    unsigned int config;

    if(enhance.enhance_en == 1)
    {
        config = readl(&spi_reg->spi_cfg0)  & CLEAR_ENHANCE_DATA;
        if(enhance.enhance_bit == 4)
        {
            config &= ~(1<<18);
        }else if(enhance.enhance_bit == 8)
        {
            config |= (1<<18);
        }
        config |= ENHANCE_DATA(enhance.enhance_data);
        writel(config, &spi_reg->spi_cfg0);
    }
    config = 0;
    switch (cmd_b)
    {
        case 4:
            config |= SPI_CMD_4b | SPI_CMD_OEN_4b;
            break;
        case 2:
            config |= SPI_CMD_2b | SPI_CMD_OEN_2b;
            break;
        case 1:
            config |= SPI_CMD_1b | SPI_CMD_OEN_1b;
            break;
        case 0:
        default:
            config |= SPI_CMD_NO | SPI_CMD_OEN_NO;
            break;
    }
    switch (addr_b)
    {
        case 4:
            config |= SPI_ADDR_4b | SPI_ADDR_OEN_4b;
            break;
        case 2:
            config |= SPI_ADDR_2b | SPI_ADDR_OEN_2b;
            break;
        case 1:
            config |= SPI_ADDR_1b | SPI_ADDR_OEN_1b;
            break;
        case 0:
        default:
            config |= SPI_ADDR_NO | SPI_ADDR_OEN_NO;
            break;
    }
    switch (data_b)
    {
        case 4:
            config |= SPI_DATA_4b | SPI_DATA_OEN_4b;
            break;
        case 2:
            config |= SPI_DATA_2b | SPI_DATA_OEN_2b;
            break;
        case 1:
            config |= SPI_DATA_1b | SPI_DATA_OEN_1b | SPI_DATA_IEN_DQ1;
            break;
        case 0:
        default:
            config |= SPI_DATA_NO | SPI_DATA_OEN_NO;
            break;
    }
    switch (enhance.enhance_bit_mode)
    {
        case 4:
            config |= SPI_ENHANCE_4b;
            break;                  
        case 2:                     
            config |= SPI_ENHANCE_2b;
            break;                  
        case 1:                     
            config |= SPI_ENHANCE_1b;
            break;                  
        case 0:                     
        default:                    
            config |= SPI_ENHANCE_NO;
            break;
    }
    config |= SPI_DUMMY_CYC(dummy);
    writel(config, &spi_reg->spi_cfg1); 
    writel(config, &spi_reg->spi_cfg2); 
}

static void sp_spi_cfg12_set(SPI_NOR_REG *reg_base, u8 cmd)
{
    SPI_ENHANCE enhance;
    enhance.enhance_en = 0;
    enhance.enhance_bit_mode = 0;
    //diag_printf("%s\n",__FUNCTION__);
	
    switch (cmd)
    {
        case 0x0B:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_1,DATA_1,enhance,DUMMY_CYCLE(8));
            break;
        case 0x3B:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_1,DATA_2,enhance,DUMMY_CYCLE(8));
            break;
        case 0x6B:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_1,DATA_4,enhance,DUMMY_CYCLE(8));
            break;
        case 0xBB:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_2,DATA_2,enhance,DUMMY_CYCLE(4));
            break;
        case 0xEB:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_4,DATA_4,enhance,DUMMY_CYCLE(6));
            break;
        case 0x32:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_1,DATA_4,enhance,DUMMY_CYCLE(0));
            break;
        default:
            spi_nor_io_CUST_config(reg_base, CMD_1,ADDR_1,DATA_1,enhance,DUMMY_CYCLE(0));
            break;
    }	
	  return;
}

static int sp_spi_nor_xfer_dmawrite(struct spi_nor *nor, u8 opcode, u32 addr, u8 addr_len,
				const u8 *buf, size_t len)
{
    struct sp_spi_nor *pspi = nor->priv;
    SPI_NOR_REG *spi_reg = pspi->io_base;
    unsigned int addr_temp = 0;
    unsigned char cmd = opcode;
    const u8 *data_in = buf;
    unsigned int time = 0;
    unsigned int ctrl = 0;
    unsigned int autocfg = 0;
    unsigned int temp_len = len;
    int value = 0;
    
    dev_dbg(pspi->dev,"%s\n",__FUNCTION__);    
    while ((readl(&spi_reg->spi_auto_cfg) & DMA_TRIGGER) ||  (readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)){
        time++;
        if (time > 0x30000){
             dev_dbg(pspi->dev,"##w init time out\n");
             break;
        }     
    }

    sp_spi_cfg12_set(spi_reg, cmd);
    ctrl = readl(&spi_reg->spi_ctrl) & (CLEAR_CUST_CMD & (~AUTO_SPI_WEL_EN));
    ctrl |= (WRITE_CMD | BYTE_0 | ADDR_0B | AUTO_SPI_WEL_EN);
    if (cmd == 6)//need to check
        ctrl &= ~AUTO_SPI_WEL_EN;

    writel(0, &spi_reg->spi_page_addr);
    writel(0, &spi_reg->spi_data);
    if (addr_len > 0)
    {
        addr_temp = addr;
        ctrl |= ADDR_3B;
    }
    writel(ctrl, &spi_reg->spi_ctrl);
    value = readl(&spi_reg->spi_ctrl);
    dev_dbg(pspi->dev,"w data ctrl 0x%x\n",value);
    writel((0x100<<4), &spi_reg->spi_page_size);
    
    do
    {
        writel(addr_temp, &spi_reg->spi_page_addr);
        if (len > CFG_BUFF_MAX){
            temp_len = CFG_BUFF_MAX;
            len -= CFG_BUFF_MAX;
        }else{
            temp_len = len;
            len = 0;
        }
        dev_dbg(pspi->dev,"w remain len  0x%x\n", len);
        if (temp_len > 0) {
            memcpy(pspi->buff.virt, data_in, temp_len); // copy data to dma
        }        

        value =  (readl(&spi_reg->spi_cfg0) & CLEAR_DATA64_LEN) | temp_len ;
        writel(value, &spi_reg->spi_cfg0);

        writel(pspi->buff.phys, &spi_reg->spi_mem_data_addr);
        value = readl(&spi_reg->spi_mem_data_addr);
        dev_dbg(pspi->dev,"w spi_mem_data_addr 0x%x\n", value);

        autocfg = DMA_TRIGGER|(cmd<<8)|(1); 
        value = (readl(&spi_reg->spi_auto_cfg)&(~(0xff<<8))) | autocfg;
        
        writel(0x5, &spi_reg->spi_intr_msk);
        writel(0x07, &spi_reg->spi_intr_sts);
        pspi->busy = 1;
        writel(value, &spi_reg->spi_auto_cfg);
    #if 0
        value = readl(&spi_reg->spi_auto_cfg);
        dev_dbg(pspi->dev,"r spi_auto_cfg 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg0);
        dev_dbg(pspi->dev,"w spi_cfg0 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg1);
        dev_dbg(pspi->dev,"w spi_cfg1 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg2);
        dev_dbg(pspi->dev,"w spi_cfg2 0x%x\n", value);
    #endif
        dev_dbg(pspi->dev,"wait intr, busy 0x%x\n", pspi->busy);
        wait_event(pspi->wq, !pspi->busy); 
        
        writel(0, &spi_reg->spi_intr_msk);
        time = 0;
        while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
        {
            time++;
            if (time > 0x20000){
                dev_dbg(pspi->dev,"##w busy check time out\n");
                len = 0;
                break;
            }
        }        
        addr_temp += CFG_BUFF_MAX;
        data_in += CFG_BUFF_MAX;
    } while (len != 0);
    value = readl(&spi_reg->spi_cfg0) & DATA64_DIS;
    writel(value, &spi_reg->spi_cfg0);
    value = readl(&spi_reg->spi_auto_cfg) & (~autocfg); 
    writel(value, &spi_reg->spi_auto_cfg);
    sp_spi_cfg12_set(spi_reg, 0);
    return 0;
}

static int sp_spi_nor_xfer_dmaread(struct spi_nor *nor, u8 opcode, u32 addr, u8 addr_len,
				u8 *buf, size_t len)
{
    struct sp_spi_nor *pspi = nor->priv;
    SPI_NOR_REG *spi_reg = pspi->io_base;
    unsigned int addr_temp = 0;
    u8 *data_in = buf;
    unsigned int time = 0;
    unsigned int ctrl = 0;
    unsigned int autocfg = 0;
    unsigned int temp_len = 0;
    int value = 0;
    
    dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
    while ((readl(&spi_reg->spi_auto_cfg) & DMA_TRIGGER) || (readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)){
        time++;
        if (time > 0x30000){
            dev_dbg(pspi->dev,"##r init time out\n"); 
            break; 
        }     
    }
   
    sp_spi_cfg12_set(spi_reg, opcode);
        
    ctrl = readl(&spi_reg->spi_ctrl) & (CLEAR_CUST_CMD & (~AUTO_SPI_WEL_EN));
    ctrl |= (READ_CMD | BYTE_0 | ADDR_0B);
        
    writel(0, &spi_reg->spi_data);
    if (addr_len > 0)
    {
        addr_temp =addr;
        ctrl |= ADDR_3B;
    }
        
    writel(ctrl, &spi_reg->spi_ctrl);
    value = readl(&spi_reg->spi_ctrl);
    dev_dbg(pspi->dev,"r data ctrl 0x%x\n",value);
    writel((0x100<<4), &spi_reg->spi_page_size);
    
    do
    {
        writel(addr_temp, &spi_reg->spi_page_addr);
        if (len > CFG_BUFF_MAX){
            temp_len = CFG_BUFF_MAX;
            len -= CFG_BUFF_MAX;
        }else{
            temp_len = len;
            len = 0;
        }
        dev_dbg(pspi->dev,"r remain len  0x%x\n", len);
        value =  (readl(&spi_reg->spi_cfg0) & CLEAR_DATA64_LEN) |  temp_len | DATA64_EN;
        if (opcode == 5)//need to check
            value |= SPI_TRS_MODE;
        writel(value, &spi_reg->spi_cfg0);
    
        writel( pspi->buff.phys, &spi_reg->spi_mem_data_addr);
        value = readl(&spi_reg->spi_mem_data_addr);
        dev_dbg(pspi->dev,"r spi_mem_data_addr 0x%x\n", value);
        
        autocfg =  (opcode<<24) | (1<<20) | DMA_TRIGGER ;   
        value = (readl(&spi_reg->spi_auto_cfg)&(~(0xff<<24))) | autocfg ; 
    
        writel(0x5, &spi_reg->spi_intr_msk) ;
        writel(0x07, &spi_reg->spi_intr_sts);
	      pspi->busy = 1;
        writel(value, &spi_reg->spi_auto_cfg);
    #if 0  
        value = readl(&spi_reg->spi_auto_cfg);
        dev_dbg(pspi->dev,"r spi_auto_cfg 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg0);
        dev_dbg(pspi->dev,"r spi_cfg0 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg1);
        dev_dbg(pspi->dev,"r spi_cfg1 0x%x\n", value);
        value = readl(&spi_reg->spi_cfg2);
        dev_dbg(pspi->dev,"r spi_cfg2 0x%x\n", value);
    #endif
        dev_dbg(pspi->dev,"wait intr, busy 0x%x\n", pspi->busy);   
        wait_event(pspi->wq, !pspi->busy); 
             
        writel(0, &spi_reg->spi_intr_msk);
        time = 0;
        while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
        {
            time++;
            if (time > 0x10000){
                dev_dbg(pspi->dev,"##r busy check time out\n");
                len = 0; 
                break; 
            }
        };
        
        memcpy(data_in, pspi->buff.virt, temp_len);
        addr_temp += CFG_BUFF_MAX;
        data_in += CFG_BUFF_MAX; 
    } while (len != 0);
 
    value = readl(&spi_reg->spi_cfg0) & (DATA64_DIS & (~SPI_TRS_MODE));
    writel(value, &spi_reg->spi_cfg0);
    value = readl(&spi_reg->spi_auto_cfg) & (~autocfg);
    writel(value, &spi_reg->spi_auto_cfg);  
    sp_spi_cfg12_set(spi_reg, 0);
    return 0;
}
#else
static unsigned char sp_spi_nor_rdsr(SPI_NOR_REG *reg_base)
{
	unsigned char data;
	SPI_NOR_REG* spi_reg = (SPI_NOR_REG*)reg_base;
	unsigned int reg_temp;
	
	reg_temp = readl(&spi_reg->spi_ctrl) & CLEAR_CUST_CMD;
	reg_temp = reg_temp | READ_CMD | BYTE_0 | ADDR_0B | CUST_CMD(0x05);
	while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0);

	writel(0,&spi_reg->spi_data);
	writel(reg_temp,&spi_reg->spi_ctrl);
	reg_temp = readl(&spi_reg->spi_auto_cfg);
	reg_temp |= PIO_TRIGGER;
	writel(reg_temp,&spi_reg->spi_auto_cfg);
	while((readl(&spi_reg->spi_auto_cfg) & PIO_TRIGGER)!=0);
	data = readl(&spi_reg->spi_status) & 0xff;
	return data;
}
static int sp_spi_nor_xfer_write(struct spi_nor *nor, u8 opcode, u32 addr, u8 addr_len,
				u8 *buf, size_t len)
{
	struct sp_spi_nor *pspi = nor->priv;
	SPI_NOR_REG* spi_reg = (SPI_NOR_REG *)pspi->io_base;
	int ret;
	int total_count = len;
	int data_count = 0;
	unsigned int temp_reg = 0;
	unsigned int offset = (unsigned int)addr;
	unsigned int addr_offset = 0;
	unsigned int addr_temp = 0;
	unsigned int reg_temp = 0;
	unsigned int cfg0 = 0;
	unsigned int data_temp = 0;
	unsigned char * data_in = buf;
	unsigned char cmd = opcode;
	struct timeval time;
	struct timeval time_out;
	
  dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
	mutex_lock(&pspi->lock);
	if (total_count == 0) 
	{
		while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
		{
			dev_dbg(pspi->dev,"wait ctrl busy\n");
		};
		reg_temp = readl(&spi_reg->spi_ctrl) & CLEAR_CUST_CMD;
		reg_temp = reg_temp | WRITE_CMD | BYTE_0 | ADDR_0B | CUST_CMD(cmd);
		cfg0 = readl(&spi_reg->spi_cfg0);
		cfg0 = (cfg0 & CLEAR_DATA64_LEN) | data_count | DATA64_EN;
		writel(cfg0,&spi_reg->spi_cfg0);
		writel(DATA64_READ_ADDR(0) | DATA64_WRITE_ADDR(0),&spi_reg->spi_buf_addr);
		if (addr_len > 0)
		{
			writel(addr,&spi_reg->spi_page_addr);
			reg_temp = reg_temp | ADDR_3B ;
			dev_dbg(pspi->dev,"addr 0x%x\n", spi_reg->spi_page_addr);
		}
		writel(0,&spi_reg->spi_data);
		writel(reg_temp,&spi_reg->spi_ctrl);
		reg_temp = readl(&spi_reg->spi_auto_cfg);
		reg_temp |= PIO_TRIGGER;
		writel(reg_temp,&spi_reg->spi_auto_cfg);
	}
	while (total_count > 0) 
	{
		if (total_count > SPI_DATA64_MAX_LEN) {
			total_count = total_count - SPI_DATA64_MAX_LEN;
			data_count = SPI_DATA64_MAX_LEN;
		} else {
			data_count = total_count;
			total_count = 0;
		}
		while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
		{
			dev_dbg(pspi->dev,"wait ctrl busy\n");
		};
		reg_temp = readl(&spi_reg->spi_ctrl) & CLEAR_CUST_CMD;
		reg_temp = reg_temp | WRITE_CMD | BYTE_0 | ADDR_0B | CUST_CMD(cmd);
		cfg0 = readl(&spi_reg->spi_cfg0);
		cfg0 = (cfg0 & CLEAR_DATA64_LEN) | data_count | DATA64_EN;
		writel(cfg0,&spi_reg->spi_cfg0);
		writel(DATA64_READ_ADDR(0) | DATA64_WRITE_ADDR(0),&spi_reg->spi_buf_addr);
		if (addr_len > 0) 
		{
			addr_temp = offset +  addr_offset* SPI_DATA64_MAX_LEN;
			dev_dbg(pspi->dev,"addr_offset 0x%x\n",addr_offset);
			dev_dbg(pspi->dev,"offset 0x%x\n",offset);
			dev_dbg(pspi->dev,"addr 0x%x\n",addr_temp);
			writel(addr_temp,&spi_reg->spi_page_addr);
			reg_temp = reg_temp | ADDR_3B ;
		}
		writel(0,&spi_reg->spi_data);
		writel(reg_temp,&spi_reg->spi_ctrl);
		reg_temp = readl(&spi_reg->spi_auto_cfg);
		reg_temp |= PIO_TRIGGER;
		writel(reg_temp,&spi_reg->spi_auto_cfg);

		while (data_count > 0) 
		{
			if ((data_count / 4) > 0) {
				if ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) == SRAM_FULL) {
					do_gettimeofday(&time);
					while ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) != SRAM_EMPTY)
					{
						do_gettimeofday(&time_out);
						if ((time_out.tv_usec - time.tv_usec) > SPI_TIMEOUT)
						{
							dev_dbg(pspi->dev,"timeout \n");
							break;
						}
					};
				}
				data_temp = (data_in[3] << 24) | (data_in[2] << 16) | (data_in[1] << 8) | data_in[0];
				writel(data_temp,&spi_reg->spi_data64);
				data_in = data_in + 4;
				data_count = data_count - 4;
			} else {
				if ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) == SRAM_FULL) {
					do_gettimeofday(&time);
					while ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) != SRAM_EMPTY)
					{
						do_gettimeofday(&time_out);
						if ((time_out.tv_usec - time.tv_usec) > SPI_TIMEOUT)
						{
							dev_dbg(pspi->dev,"timeout \n");
							break;
						}
					};
				}
				if(data_count%4 == 3)
				{
					data_temp = (data_in[2] << 16) | (data_in[1] << 8) | data_in[0];
					data_count = data_count-3; 
				}else if(data_count%4 == 2)
				{
					data_temp =  (data_in[1] << 8) | data_in[0];
					data_count = data_count-2; 
				}else if (data_count%4 == 1)
				{
					data_temp = data_in[0];
					data_count = data_count-1; 
				}
				writel(data_temp,&spi_reg->spi_data64);
			}
		}
		addr_offset = addr_offset + 1;
		while ((sp_spi_nor_rdsr(spi_reg) & 0x01) != 0)
		{
			dev_dbg(pspi->dev,"wait DEVICE busy \n");
		};
	}
	while((readl(&spi_reg->spi_auto_cfg) & PIO_TRIGGER)!=0)
	{
		dev_dbg(pspi->dev,"wait PIO_TRIGGER\n");
	};
	while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
	{
		dev_dbg(pspi->dev,"wait ctrl busy\n");
	};
	cfg0 = readl(&spi_reg->spi_cfg0);
	cfg0 &= DATA64_DIS;
	writel(cfg0,&spi_reg->spi_cfg0);
	mutex_unlock(&pspi->lock);
	return 0;
}
static int sp_spi_nor_xfer_read(struct spi_nor *nor, u8 opcode, u32 addr, u8 addr_len,
				u8 *buf, size_t len)
{
	struct sp_spi_nor *pspi = nor->priv;
	SPI_NOR_REG* spi_reg = (SPI_NOR_REG *)pspi->io_base;
	int ret;
	int total_count = len;
	int data_count = 0;
	unsigned int temp_reg = 0;
	unsigned int offset = (unsigned int)addr;
	unsigned int addr_offset = 0;
	unsigned int addr_temp = 0;
	unsigned int reg_temp = 0;
	unsigned int cfg0 = 0; 
	unsigned int data_temp = 0;
	unsigned char * data_in = buf;
	unsigned char cmd = opcode;
	struct timeval time;
	struct timeval time_out;

  dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
	mutex_lock(&pspi->lock);

	while (total_count > 0)
	{
		if (total_count > SPI_DATA64_MAX_LEN)
		{
			total_count = total_count - SPI_DATA64_MAX_LEN;
			data_count = SPI_DATA64_MAX_LEN;
		} else
		{
			data_count = total_count;
			total_count = 0;
		}
		while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
		{
			dev_dbg(pspi->dev,"wait ctrl busy\n");
		};
		reg_temp = readl(&spi_reg->spi_ctrl) & CLEAR_CUST_CMD;
		reg_temp = reg_temp | READ_CMD | BYTE_0 | ADDR_0B | CUST_CMD(cmd);
		cfg0 = readl(&spi_reg->spi_cfg0);
		cfg0 = (cfg0 & CLEAR_DATA64_LEN) | data_count | DATA64_EN;
		dev_dbg(pspi->dev,"spi_cfg0 0x%x\n",cfg0);
		writel(cfg0,&spi_reg->spi_cfg0);
		writel(0,&spi_reg->spi_page_addr);
		writel(DATA64_READ_ADDR(0) | DATA64_WRITE_ADDR(0),&spi_reg->spi_buf_addr);
		if(addr_len > 0)
		{
			addr_temp = offset +  addr_offset* SPI_DATA64_MAX_LEN;
			dev_dbg(pspi->dev,"addr_offset 0x%x\n",addr_offset);
			dev_dbg(pspi->dev,"offset 0x%x\n",offset);
			dev_dbg(pspi->dev,"addr 0x%x\n",addr_temp);
			writel(addr_temp,&spi_reg->spi_page_addr);
			reg_temp = reg_temp | ADDR_3B ;
		}
		writel(0,&spi_reg->spi_data);
		dev_dbg(pspi->dev,"spi_ctrl 0x%x\n",reg_temp);
		writel(reg_temp,&spi_reg->spi_ctrl);
		reg_temp = readl(&spi_reg->spi_auto_cfg);
		reg_temp |= PIO_TRIGGER;
		writel(reg_temp,&spi_reg->spi_auto_cfg);
		if (opcode == SPINOR_OP_RDSR)
		{
			data_in[0] = readl(&spi_reg->spi_status)&0xff;
			data_count = 0;
		}
		dev_dbg(pspi->dev,"cfg1 0x%x, len 0x%x\n",readl(&spi_reg->spi_cfg1));
		while (data_count > 0)
		{
			if ((data_count / 4) > 0)
			{
				do_gettimeofday(&time);
				while ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) == SRAM_EMPTY)
				{
					do_gettimeofday(&time_out);
					if ((time_out.tv_usec - time.tv_usec) > SPI_TIMEOUT)
					{
						dev_dbg(pspi->dev,"timeout \n");
						break;
					}
				};
				data_temp = readl(&spi_reg->spi_data64);
				dev_dbg(pspi->dev,"data_temp 0x%x\n",data_temp);
				data_in[0] = data_temp & 0xff;
				data_in[1] = ((data_temp & 0xff00) >> 8);
				data_in[2] = ((data_temp & 0xff0000) >> 16);
				data_in[3] = ((data_temp & 0xff000000) >> 24);
				data_in = data_in + 4;
				data_count = data_count - 4;
			} else {
				do_gettimeofday(&time);
				while ((readl(&spi_reg->spi_status_2) & SPI_SRAM_ST) == SRAM_EMPTY)
				{
					do_gettimeofday(&time_out);
					if ((time_out.tv_usec - time.tv_usec) > SPI_TIMEOUT)
					{
						dev_dbg(pspi->dev,"timeout \n");
						break;
					}
				};
				data_temp = readl(&spi_reg->spi_data64);
				dev_dbg(pspi->dev,"data_temp 0x%x\n",data_temp);
				if(data_count%4 == 3)
				{
					data_in[0] = data_temp & 0xff;
					data_in[1] = ((data_temp & 0xff00) >> 8);
					data_in[2] = ((data_temp & 0xff0000) >> 16);
					data_count = data_count-3; 
				}else if(data_count%4 == 2)
				{
					data_in[0] = data_temp & 0xff;
					data_in[1] = ((data_temp & 0xff00) >> 8);
					data_count = data_count-2; 
				}else if (data_count%4 == 1)
				{
					data_in[0] = data_temp & 0xff;
					data_count = data_count-1; 
				}
			}
		}
		addr_offset = addr_offset + 1;
		while ((sp_spi_nor_rdsr(spi_reg) & 0x01) != 0)
		{
			dev_dbg(pspi->dev,"wait DEVICE busy \n");
		};
	}
	while((readl(&spi_reg->spi_auto_cfg) & PIO_TRIGGER)!=0)
	{
		dev_dbg(pspi->dev,"wait PIO_TRIGGER\n");
	};
	while((readl(&spi_reg->spi_ctrl) & SPI_CTRL_BUSY)!=0)
	{
		dev_dbg(pspi->dev,"wait ctrl busy\n");
	};
	cfg0 = readl(&spi_reg->spi_cfg0);
	cfg0 &= DATA64_DIS;
	writel(cfg0,&spi_reg->spi_cfg0);
	mutex_unlock(&pspi->lock);
	return 0;
}
#endif

static ssize_t sp_spi_nor_read(struct spi_nor *nor, loff_t from, size_t len, u_char *buf)
{
    struct sp_spi_nor *pspi = nor->priv;
    u8 opcode = nor->read_opcode;
    unsigned int offset = (unsigned int)from;
    
    dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
    dev_dbg(pspi->dev,"read cmd 0x%x addr 0x%x len 0x%x\n", opcode, offset, len);
#if (SP_SPINOR_DMA)
    sp_spi_nor_xfer_dmaread(nor, opcode, offset, 0x3, buf, len);
#else
    sp_spi_nor_xfer_read(nor, opcode, offset, 0x3, buf, len);
#endif    
    return len;
}
static ssize_t sp_spi_nor_write(struct spi_nor *nor, loff_t to,
				size_t len, const u_char *buf)
{
    struct sp_spi_nor *pspi = nor->priv;  
    u8 opcode = nor->program_opcode;
    unsigned int offset = (unsigned int)to;
    
    dev_dbg(pspi->dev,"%s\n",__FUNCTION__);

    dev_dbg(pspi->dev,"write cmd 0x%x addr 0x%x len 0x%x\n", opcode, offset, len);
#if (SP_SPINOR_DMA)
    sp_spi_nor_xfer_dmawrite(nor, opcode, offset, 0x3, buf, len);
#else
    sp_spi_nor_xfer_write(nor, opcode, offset, 0x3, buf, len);
#endif
	return len;
}
static int sp_spi_nor_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
    struct sp_spi_nor *pspi = nor->priv;
    
    dev_dbg(pspi->dev,"%s cmd 0x%x len 0x%x\n",__FUNCTION__, opcode, len);   
#if (SP_SPINOR_DMA)
    sp_spi_nor_xfer_dmaread(nor, opcode, 0, 0, buf, len);
#else
    sp_spi_nor_xfer_read(nor, opcode, 0, 0, buf, len);
#endif
    return 0;
}

static int sp_spi_nor_write_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
    struct sp_spi_nor *pspi = nor->priv;
    u8 *data_in = buf;
    u32 addr = 0;
    int addr_len = 0;
    int data_len = 0;

    dev_dbg(pspi->dev,"%s cmd 0x%x len 0x%x\n",__FUNCTION__, opcode, len);
    switch(opcode)
    {
        case SPINOR_OP_BE_4K:
            addr_len = len;
            data_len = 0;
            addr = data_in[2]<<16 | data_in[1]<<8 | data_in[0];
            break;
        default:
            addr_len = 0;
            data_len = len;
            break;
    };
#if (SP_SPINOR_DMA)
    sp_spi_nor_xfer_dmawrite(nor, opcode, addr, addr_len, buf, data_len);
#else
    sp_spi_nor_xfer_write(nor, opcode, addr, addr_len, buf, data_len);
#endif
    return 0;
}

static int sp_spi_nor_prep(struct spi_nor *nor, enum spi_nor_ops ops)
{
    struct sp_spi_nor *pspi = nor->priv;
    
	  dev_dbg(pspi->dev,"%s ops 0x%x\n",__FUNCTION__, ops);
	  switch(ops)
	  {
		    case 0:
			    dev_dbg(pspi->dev,"SPI_NOR_OPS_READ \n");
			    break;
		    case 1:
			    dev_dbg(pspi->dev,"SPI_NOR_OPS_WRITE \n");
			    break;
		    case 2:
			    dev_dbg(pspi->dev,"SPI_NOR_OPS_ERASE \n");
			    break;
		    case 3:
			    dev_dbg(pspi->dev,"SPI_NOR_OPS_LOCK \n");
			    break;
		    case 4:
			    dev_dbg(pspi->dev,"SPI_NOR_OPS_UNLOCK \n");
			    break;
		    default:
			    dev_dbg(pspi->dev,"Unknow ops \n");
			    break;
	  };
	  return 0;
}

static void sp_spi_nor_unprep(struct spi_nor *nor, enum spi_nor_ops ops)
{
	  struct sp_spi_nor *pspi = nor->priv;
	  
	  dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
	  return ;
}

static int sp_spi_nor_erase(struct spi_nor *nor, loff_t offs)
{
	  struct sp_spi_nor *pspi = nor->priv;
	  
	  dev_dbg(pspi->dev,"%s 0x%x 0x%x\n",__FUNCTION__,nor->erase_opcode, (u32)offs);
	  sp_spi_nor_xfer_dmawrite(nor, nor->erase_opcode, offs, 3, 0, 0);
	  return ;
}
#if 0
static int sp_spi_nor_flashlock(struct spi_nor *nor, loff_t offs, uint64_t len)
{
	  struct sp_spi_nor *pspi = nor->priv;
	  
	  dev_dbg(pspi->dev,"%s 0x%x\n",__FUNCTION__, nor->program_opcode);
	  return ;
}

static int sp_spi_nor_flashunlock(struct spi_nor *nor, loff_t offs, uint64_t len)
{
	  struct sp_spi_nor *pspi = nor->priv;
	  
	  printk("%s 0x%x\n",__FUNCTION__, nor->program_opcode);
	  return ;
}

static int sp_spi_nor_flashlocked(struct spi_nor *nor, loff_t offs, uint64_t len)
{
	  struct sp_spi_nor *pspi = nor->priv;
	  
	  printk("%s 0x%x\n",__FUNCTION__, nor->program_opcode);
	  return ;
}
#endif
static int sp_spi_nor_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    struct device *dev = &pdev->dev;
    struct sp_spi_nor *pspi ;
    struct resource *res;
    struct spi_nor *nor;
    struct mtd_info *mtd;
    SPI_NOR_REG *spi_reg;
    int ret;
    volatile MOON1_REG *moon1;

    dev_dbg(pspi->dev,"%s\n",__FUNCTION__);
    pspi = devm_kzalloc(dev, sizeof(*pspi), GFP_KERNEL);
    if (!pspi)
        return -ENOMEM;

    pspi->dev = dev;

    platform_set_drvdata(pdev, pspi);
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    pspi->io_base = devm_ioremap_resource(dev, res);
    if (IS_ERR(pspi->io_base))
		    return PTR_ERR(pspi->io_base);
		
		pspi->irq = platform_get_irq(pdev, 0);
	  init_waitqueue_head(&pspi->wq);
	  if (pspi->irq <= 0) {
		    devm_kfree(dev, (void *)pspi);
		    dev_dbg(pspi->dev,"get spi nor irq resource fail\n");
		    return -EINVAL;
	  }else {
		    ret = request_irq(pspi->irq, sp_nor_int, IRQF_SHARED, "sp_nor", pspi);
		    if (ret) {
			      dev_dbg(pspi->dev,"sp_spinor: unable to register IRQ(%d) \n", pspi->irq);
		    }
	  }
#if (SP_SPINOR_DMA)
    pspi->buff.size =CFG_BUFF_MAX;
    pspi->buff.virt = (void *)dma_alloc_coherent(NULL, PAGE_ALIGN(pspi->buff.size), \
							   &pspi->buff.phys , GFP_DMA | GFP_KERNEL);
    dev_dbg(pspi->dev,"phy 0x%x virt 0x%x size 0x%x\n",pspi->buff.phys, (int)pspi->buff.virt,pspi->buff.size);
    if (!pspi->buff.virt) {
		    ret = -ENOMEM;
		    dev_err(&pdev->dev,"spi nor:Failed to allocate dma\n");
		    return (ret);
	  }	
#endif
    /* pinmux settings*/ 
    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    moon1_base = devm_ioremap_resource(dev, res);
    if (IS_ERR(moon1_base))
        return PTR_ERR(moon1_base);
    moon1 =  (volatile MOON1_REG *) moon1_base;
    dev_dbg(pspi->dev,"sft_cfg1 0x%x\n",moon1->sft_cfg1);
    moon1->sft_cfg1= 0xf000a;
#if 0
	  /* find the clocks */
	  pspi->ctrl_clk = devm_clk_get(dev, "sysslow");
	  if (IS_ERR(pspi->ctrl_clk))
		    return PTR_ERR(pspi->ctrl_clk);

	  pspi->nor_clk = devm_clk_get(dev, "sysslow");
	  if (IS_ERR(pspi->nor_clk))
		  return PTR_ERR(pspi->nor_clk);

	  ret = clk_prepare_enable(pspi->ctrl_clk);
	  if (ret)
	  {
		    dev_err(dev, "can not enable the clock\n");
		    goto clk_failed;
	  }

	  ret = clk_prepare_enable(pspi->nor_clk);
	  if (ret)
	  {
		    goto clk_failed_nor;
	  }
#endif
	
	/* find the irq */
#if 0
	  ret = platform_get_irq(pdev, 0);
	  if (ret < 0) 
	  {
		    dev_err(dev, "failed to get the irq: %d\n", ret);
		    goto irq_failed;
	  }

	  ret = devm_request_irq(dev, ret,fsl_qspi_irq_handler, 0, pdev->name, pspi);
	  if (ret) 
	  {
		    dev_err(dev, "failed to request irq: %d\n", ret);
		    goto irq_failed;
	  }
#endif

    nor = &pspi->nor;
    mtd = &nor->mtd;

    nor->dev = dev;
    spi_nor_set_flash_node(nor, np);
    nor->priv = pspi;

    mutex_init(&pspi->lock);
    /* fill the hooks */
    nor->read_reg = sp_spi_nor_read_reg;
    nor->write_reg = sp_spi_nor_write_reg;
    nor->read = sp_spi_nor_read;
    nor->write = sp_spi_nor_write;
    nor->erase = sp_spi_nor_erase;
#if 0
    nor->flash_lock = sp_spi_nor_flashlock;
    nor->flash_unlock = sp_spi_nor_flashunlock;
    nor->flash_is_locked = sp_spi_nor_flashlocked;
#endif
    nor->prepare = sp_spi_nor_prep;
    nor->unprepare = sp_spi_nor_unprep;

    ret = of_property_read_u32(np, "spi-max-frequency",&pspi->clk_rate);
    if (ret < 0)
        goto mutex_failed;

    spi_reg = (SPI_NOR_REG *)pspi->io_base;
    sp_spi_nor_init(spi_reg);

    ret = spi_nor_scan(nor, NULL, SPI_NOR_NORMAL);
    if (ret)
        goto mutex_failed;

    ret = mtd_device_register(mtd, NULL, 0);
    if (ret)
        goto mutex_failed;

    if (pspi->nor_size == 0)
    {
		    pspi->nor_size = mtd->size;
    }
    goto exit;
mutex_failed:
	mutex_destroy(&pspi->lock);
  goto exit;
#if 0
clk_failed:
clk_failed_nor:
	clk_disable_unprepare(pspi->nor_clk);
	clk_disable_unprepare(pspi->ctrl_clk);
irq_failed:
#endif
exit:
	return 0;
}
static int sp_spi_nor_remove(struct platform_device *pdev)
{
	  struct sp_spi_nor *pspi = platform_get_drvdata(pdev);

	  mtd_device_unregister(&pspi->nor.mtd);

	  mutex_destroy(&pspi->lock);

	  clk_disable_unprepare(pspi->ctrl_clk);
	  clk_disable_unprepare(pspi->nor_clk);
	
	  return 0;
}

static const struct of_device_id sp_spi_nor_ids[] = {
	{.compatible = "sunplus,sp-spi-nor"},
	{}
};
MODULE_DEVICE_TABLE(of, sp_spi_nor_ids);

static struct platform_driver sp_spi_nor_driver = {
	.probe	= sp_spi_nor_probe,
	.remove	= sp_spi_nor_remove,
	.driver	= {
		.name = "sp-spi-nor",
		.owner = THIS_MODULE,
		.of_match_table = sp_spi_nor_ids,
	},
};
module_platform_driver(sp_spi_nor_driver);

MODULE_AUTHOR("Sunplus");
MODULE_DESCRIPTION("Sunplus spi nor driver");
MODULE_LICENSE("GPL");
