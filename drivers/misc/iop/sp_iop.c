#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <asm/irq.h>

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include "hal_iop.h"
#include "sp_iop.h"
#include <dt-bindings/memory/sp-q628-mem.h> 
#include <linux/delay.h>

#include <linux/of_irq.h>
#include <linux/kthread.h>
/* ---------------------------------------------------------------------------------------------- */
#define IOP_FUNC_DEBUG
#define IOP_KDBG_INFO
#define IOP_KDBG_ERR


#ifdef IOP_FUNC_DEBUG
	#define FUNC_DEBUG()    printk(KERN_INFO "[IOP] Debug: %s(%d)\n", __FUNCTION__, __LINE__)
#else
	#define FUNC_DEBUG()
#endif

#ifdef IOP_KDBG_INFO
#define DBG_INFO(fmt, args ...)	printk(KERN_INFO "K_IOP: " fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif

#ifdef IOP_KDBG_ERR
#define DBG_ERR(fmt, args ...)	printk(KERN_ERR "K_IOP: " fmt, ## args)
#else
#define DBG_ERR(fmt, args ...)
#endif
/* ---------------------------------------------------------------------------------------------- */
#define IOP_REG_NAME        "iop"
#define IOP_MOON0_REG_NAME "iop_moon0"
#define IOP_QCTL_REG_NAME       "iop_qctl"
#define IOP_PMC_REG_NAME       "iop_pmc"
#define IOP_RTC_REG_NAME       "iop_rtc"

#define DEVICE_NAME			"sunplus,sp7021-iop"


typedef struct {
	struct miscdevice dev;			// iop device
	struct mutex write_lock;
	void __iomem *iop_regs;
	void __iomem *moon0_regs;
	void __iomem *qctl_regs;
	void __iomem *pmc_regs;
	void __iomem *rtc_regs;
	int irq;
} sp_iop_t;

//typedef struct IOP_SYS_RegBase_t_ {
//	void __iomem *moon0_regs;
//	void __iomem *qctl_regs;
//	void __iomem *pmc_regs;
//	void __iomem *rtc_regs;
	
//} IOP_SYS_RegBase_t;

//static IOP_SYS_RegBase_t stIopSysRegBase;

struct iop_cbdma_reg {
	volatile unsigned int hw_ver;
	volatile unsigned int config;
	volatile unsigned int length;
	volatile unsigned int src_adr;
	volatile unsigned int des_adr;
	volatile unsigned int int_flag;
	volatile unsigned int int_en;
	volatile unsigned int memset_val;
	volatile unsigned int sdram_size_config;
	volatile unsigned int illegle_record;
	volatile unsigned int sg_idx;
	volatile unsigned int sg_cfg;
	volatile unsigned int sg_length;
	volatile unsigned int sg_src_adr;
	volatile unsigned int sg_des_adr;
	volatile unsigned int sg_memset_val;
	volatile unsigned int sg_en_go;
	volatile unsigned int sg_lp_mode;
	volatile unsigned int sg_lp_sram_start;
	volatile unsigned int sg_lp_sram_size;
	volatile unsigned int sg_chk_mode;
	volatile unsigned int sg_chk_sum;
	volatile unsigned int sg_chk_xor;
	volatile unsigned int rsv_23_31[9];
};


/**************************************************************************
*                         G L O B A L    D A T A                         *
**************************************************************************/

static sp_iop_t *iop;



static int sp_iop_open(struct inode *inode, struct file *pfile)
{
	printk("Sunplus IOP module open\n");

	return 0;
}

static int sp_iop_release(struct inode *inode, struct file *pfile)
{
	printk("Sunplus IOP module release\n");
	return 0;
}

static long sp_iop_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd)
	{
		default:
			printk("Unknow command\n");
			break;
	}

	return ret;
}

static struct file_operations sp_iop_fops = {
	.owner          	= THIS_MODULE,
	.open           = sp_iop_open,
	.release        = sp_iop_release,
	.unlocked_ioctl = sp_iop_ioctl,


};



static int _sp_iop_get_irq(struct platform_device *pdev, sp_iop_t *pstSpIOPInfo)
{
	int irq;

	FUNC_DEBUG();

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		DBG_ERR("[IOP] get irq number fail, irq = %d\n", irq);
		return -ENODEV;
	}

	pstSpIOPInfo->irq = irq;
	return IOP_SUCCESS;
}

static int _sp_iop_get_register_base(struct platform_device *pdev, unsigned int *membase, const char *res_name)
{
	struct resource *r;
	void __iomem *p;

	FUNC_DEBUG();
	DBG_INFO("[IOP] register name  : %s!!\n", res_name);

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if(r == NULL) {
		DBG_INFO("[IOP] platform_get_resource_byname fail\n");
		return -ENODEV;
	}

	p = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(p)) {
		DBG_ERR("[IOP] ioremap fail\n");
		return PTR_ERR(p);
	}

	DBG_INFO("[IOP ioremap addr : 0x%x!!\n", (unsigned int)p);
	*membase = (unsigned int)p;

	return IOP_SUCCESS;
}



static int _sp_iop_get_resources(struct platform_device *pdev, sp_iop_t *pstSpIOPInfo)
{
	int ret;
	unsigned int membase = 0;

	FUNC_DEBUG();

	ret = _sp_iop_get_register_base(pdev, &membase, IOP_REG_NAME);
	if (ret) {
		DBG_ERR("[IOP] %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	}
	else {
		pstSpIOPInfo->iop_regs = (void __iomem *)membase;
	}


	//ret = _sp_iop_get_register_base(pdev, &membase, IOP_MOON0_REG_NAME);
	//if (ret) {
	//	DBG_ERR("[IOP] %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
	//	return ret;
	//} else {
	//	pstSpIOPInfo->moon0_regs = (void __iomem *)membase;
	//}
	
	ret = _sp_iop_get_register_base(pdev, &membase, IOP_QCTL_REG_NAME);
	if (ret) {
		DBG_ERR("[IOP] %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	} else {
		pstSpIOPInfo->qctl_regs = (void __iomem *)membase;
	}

	ret = _sp_iop_get_register_base(pdev, &membase, IOP_PMC_REG_NAME);
	if (ret) {
		DBG_ERR("[IOP] %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	} else {
		pstSpIOPInfo->pmc_regs = (void __iomem *)membase;
	}

	//ret = _sp_iop_get_register_base(pdev, &membase, IOP_RTC_REG_NAME);
	//if (ret) {
	//	DBG_ERR("[IOP] %s (%d) ret = %d\n", __FUNCTION__, __LINE__, ret);
	//	return ret;
	//} else {
	//	pstSpIOPInfo->rtc_regs = (void __iomem *)membase;
	//}	


	return IOP_SUCCESS;
}







static int sp_iop_start(sp_iop_t *iopbase)
{

	FUNC_DEBUG();

	hal_iop_init(iopbase->iop_regs);


	return IOP_SUCCESS;
}

static int sp_iop_suspend(sp_iop_t *iopbase)
{
	DBG_ERR("Leon sp_iop_suspend\n");
	//early_printk("[MBOX_%d] %08x (%u)\n", i, d, d);
	early_printk("Leon  sp_iop_suspend\n");

	FUNC_DEBUG();
	

	hal_iop_suspend(iopbase->iop_regs, iopbase->pmc_regs);


	return IOP_SUCCESS;
}


static int sp_iop_platform_driver_probe(struct platform_device *pdev)
{
	int ret = -ENXIO;
	
	DBG_ERR("Leon sp_iop_platform_driver_probe\n");
	FUNC_DEBUG();



	iop = (sp_iop_t *)devm_kzalloc(&pdev->dev, sizeof(sp_iop_t), GFP_KERNEL);
	if (iop == NULL) {
		printk("sp_iop_t malloc fail\n");
		ret	= -ENOMEM;
		goto fail_kmalloc;
	}
	/* init */
		mutex_init(&iop->write_lock);
	/* register device */
	iop->dev.name  = "sp_iop";
	iop->dev.minor = MISC_DYNAMIC_MINOR;
	iop->dev.fops  = &sp_iop_fops;
	ret = misc_register(&iop->dev);
	if (ret != 0) {
		printk("sp_iop device register fail\n");
		goto fail_regdev;
	}

	#if 1
	ret = _sp_iop_get_resources(pdev, iop);

	
	ret = sp_iop_start(iop);
	if (ret != 0) {
		DBG_ERR("[IOP] sp iop init err=%d\n", ret);
		return ret;
	}

	#else
	ret = _sp_iop_get_resources(pdev, iop);

	ret = sp_iop_suspend(iop);
	if (ret != 0) {
		DBG_ERR("[IOP] sp suspend init err=%d\n", ret);
		return ret;
	}
	#endif

	
	return 0;


fail_regdev:
		mutex_destroy(&iop->write_lock);
fail_kmalloc:
	return ret;


}

static int sp_iop_platform_driver_remove(struct platform_device *pdev)
{

	FUNC_DEBUG();

	return 0;
}

static int sp_iop_platform_driver_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	unsigned int*   IOP_base;	
	unsigned int checksum=0;	
	int i;	
	
	IOP_base=ioremap((unsigned long)SP_IOP_RESERVE_BASE, SP_IOP_RESERVE_SIZE);
	for(i=0;i<0x400;i++)
	{	
		checksum+=*(IOP_base+i);			
	}
	early_printk("\n Leon IOP standby checksum=%x IOP_base=%x\n",checksum,IOP_base);	

	FUNC_DEBUG();
	ret = _sp_iop_get_resources(pdev, iop);

	ret = sp_iop_suspend(iop);
	if (ret != 0) {
		DBG_ERR("[IOP] sp suspend init err=%d\n", ret);
		return ret;
	}


	return 0;
}

static int sp_iop_platform_driver_resume(struct platform_device *pdev)
{

	FUNC_DEBUG();

	return 0;
}




static const struct of_device_id sp_iop_of_match[] = {
	{ .compatible = "sunplus,sp7021-iop" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, sp_iop_of_match);

static struct platform_driver sp_iop_platform_driver = {
	.probe		= sp_iop_platform_driver_probe,
	.remove		= sp_iop_platform_driver_remove,
	.suspend		= sp_iop_platform_driver_suspend,
	.resume		= sp_iop_platform_driver_resume,
	.driver = {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_iop_of_match),
	}
};



module_platform_driver(sp_iop_platform_driver);

/**************************************************************************
 *                  M O D U L E    D E C L A R A T I O N                  *
 **************************************************************************/

MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus IOP Driver");
MODULE_LICENSE("GPL");


