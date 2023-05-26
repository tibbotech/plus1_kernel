// SPDX-License-Identifier: GPL-2.0
/*****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved.
 *    Copyright (c) 2011-2014, Google Inc. All rights reserved.
 *    Copyright (c) 2007-2010, Hantro OY. All rights reserved.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *  Abstract : 6280/7280/8270/8290/H1 Encoder device driver (kernel module)
 *
 *****************************************************************************
 */

#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range 
  SetPageReserved
  ClearPageReserved
*/
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>

#include <linux/moduleparam.h>
/* request_irq(), free_irq() */
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/semaphore.h>
#include <linux/spinlock.h>
/* needed for virt_to_phys() */
#include <asm/io.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <asm/irq.h>

#include <linux/version.h>

#include <linux/cdev.h>  
#include <linux/platform_device.h>  
#include <linux/clk.h>
#include <linux/reset.h>

/* our own stuff */
#include "hx280enc.h"

// #define ZEBU_TEST

#ifdef ZEBU_TEST
static int first_irq = 1;
static u8 *dec_dumpwaveform_regs = 0;
#endif

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Verisilicon Inc.");
MODULE_DESCRIPTION("Hantro H1 Encoder driver");

/* this is ARM Integrator specific stuff */
/*
#define INTEGRATOR_LOGIC_MODULE1_BASE   0xD0000000
#define INTEGRATOR_LOGIC_MODULE2_BASE   0xE0000000
#define INTEGRATOR_LOGIC_MODULE3_BASE   0xF0000000
*/

/*
#define INT_EXPINT1                     10
#define INT_EXPINT2                     11
#define INT_EXPINT3                     12
*/
/* these could be module params in the future */

#define ENC_HW_ID1                  0x6E655000
#define ENC_HW_ID2                  0x72800000
#define ENC_HW_ID3                  0x82700000
#define ENC_HW_ID4                  0x82900000
#define ENC_HW_ID5                  0x48310000

#define HX280ENC_BUF_SIZE           0

/* for critical data access */
DEFINE_SPINLOCK(enc_lock);
/* for irq wait */
DECLARE_WAIT_QUEUE_HEAD(enc_wait_queue);
/* for reserve hw */
DECLARE_WAIT_QUEUE_HEAD(enc_hw_queue);  

/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hx280enc_major = 0;

static struct clk *enc_clk;
static struct reset_control *enc_rstc;

/* here's all the must remember stuff */
typedef struct
{
    u32 hw_id; //hw id to indicate project
    u32 is_valid; //indicate this core is hantro's core or not
    u32 is_reserved; //indicate this core is occupied by user or not
    int pid; //indicate which process is occupying the core
    u32 irq_received; //indicate this core receives irq
    u32 irq_status;
    int irq;
    unsigned long iobaseaddr;
    unsigned int iosize;
    volatile u8 *hwregs;
    struct fasync_struct *async_queue;
} hx280enc_t;

/* dynamic allocation? */
static hx280enc_t hx280enc_data;

static int ReserveIO(struct platform_device *dev);
static void ReleaseIO(struct platform_device *dev);
static void ResetAsic(hx280enc_t * dev);
int CheckCoreOccupation(hx280enc_t *dev);

#ifdef HX280ENC_DEBUG
static void dump_regs(unsigned long data);
#endif

/* IRQ handler */
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
static irqreturn_t hx280enc_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hx280enc_isr(int irq, void *dev_id);
#endif

/******************************************************************************/
static int CheckEncIrq(hx280enc_t *dev)
{
    unsigned long flags;
    int rdy = 0;

    spin_lock_irqsave(&enc_lock, flags);

    if(dev->irq_received)
    {
     /* reset the wait condition(s) */
     dev->irq_received = 0;
     rdy = 1;
    }

    spin_unlock_irqrestore(&enc_lock, flags);

    return rdy;
}

unsigned int WaitEncReady(hx280enc_t *dev)
{
    if(wait_event_interruptible(enc_wait_queue, CheckEncIrq(dev)))
    {
        pr_err("ENC wait_event_interruptible interrupted\n");
        return -ERESTARTSYS;
    }
    return 0;
}

int CheckCoreOccupation(hx280enc_t *dev) 
{
    int ret = 0;
    unsigned long flags;

    spin_lock_irqsave(&enc_lock, flags);

    if(!dev->is_reserved) 
    {
        dev->is_reserved = 1;
        dev->pid = current->pid;
        ret = 1;
    }
    spin_unlock_irqrestore(&enc_lock, flags);
    return ret;
}

int GetWorkableCore(hx280enc_t *dev) 
{
    int ret = 0;

    if(dev->is_valid && CheckCoreOccupation(dev))
    {
        ret = 1;
    } 
  
    return ret;
}

long ReserveEncoder(hx280enc_t *dev) 
{
    /* lock a core that has specified core id*/
    if(wait_event_interruptible(enc_hw_queue,GetWorkableCore(dev) != 0 )) {
        return -ERESTARTSYS;
    }
    return 0;
}

void ReleaseEncoder(hx280enc_t * dev)
{
    unsigned long flags;

    
    spin_lock_irqsave(&enc_lock, flags);
    if(dev->is_reserved && dev->pid == current->pid)
    {
        dev->pid = -1;
        dev->is_reserved = 0;
    }

    dev->irq_received = 0;
    dev->irq_status = 0;
    spin_unlock_irqrestore(&enc_lock, flags);

    wake_up_interruptible_all(&enc_hw_queue);
    
    return;
}

static long hx280enc_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int err = 0;
    int ret;

    /*
     * extract the type and number bitfields, and don't encode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */

    if(_IOC_TYPE(cmd) != HX280ENC_IOC_MAGIC)
        return -ENOTTY;
    if(_IOC_NR(cmd) > HX280ENC_IOC_MAXNR)
        return -ENOTTY;

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok((void *) arg, _IOC_SIZE(cmd));
    else if(_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok((void *) arg, _IOC_SIZE(cmd));
    if(err)
        return -EFAULT;

    switch (cmd)
    {
    case HX280ENC_IOCGHWOFFSET:
        __put_user(hx280enc_data.iobaseaddr, (unsigned long *) arg);
        break;

    case HX280ENC_IOCGHWIOSIZE:
        __put_user(hx280enc_data.iosize, (unsigned int *) arg);
        break;
        
    case HX280ENC_IOCH_ENC_RESERVE: 
        {
            ret = ReserveEncoder(&hx280enc_data);
            return ret;
        }
    case HX280ENC_IOCH_ENC_RELEASE: 
        {
            ReleaseEncoder(&hx280enc_data);
            break;
        }
        
    case HX280ENC_IOCG_CORE_WAIT:
        {
            ret = WaitEncReady(&hx280enc_data);
            return ret;
        }
    }
    return 0;
}

static int hx280enc_open(struct inode *inode, struct file *filp)
{
    int result = 0;
    filp->private_data = (void *) &hx280enc_data;

    return result;
}

static int hx280enc_release(struct inode *inode, struct file *filp)
{
    hx280enc_t *dev = (hx280enc_t *) filp->private_data;
    unsigned long flags;
#ifdef HX280ENC_DEBUG
    dump_regs((unsigned long) dev); /* dump the regs */
#endif

    spin_lock_irqsave(&enc_lock, flags);
    if (dev->is_reserved == 1 && dev->pid == current->pid)
    {
        dev->pid = -1;
        dev->is_reserved = 0;
        dev->irq_received = 0;
        dev->irq_status = 0;
        pr_info("release reserved core\n");
    }
    spin_unlock_irqrestore(&enc_lock, flags);
    wake_up_interruptible_all(&enc_hw_queue);
    return 0;
}

/* VFS methods */
static struct file_operations hx280enc_fops = {
    .owner= THIS_MODULE,
    .open = hx280enc_open,
    .release = hx280enc_release,
    .compat_ioctl = hx280enc_ioctl,
    .unlocked_ioctl = hx280enc_ioctl,
    .fasync = NULL,
};

#define ENC_CHRDEV_NAME  "hantro_enc"

static struct class *enc_chrdev_class = NULL;  
static struct device *enc_chrdev_device = NULL;  
static struct cdev enc_chrdev_cdev;  

static int enc_reset_release(struct platform_device *dev, const char *id) 
{  
    int ret = 0;

	enc_rstc = devm_reset_control_get(&dev->dev, id);

	if (IS_ERR(enc_rstc)) {
		return PTR_ERR(enc_rstc);
	} else {
		ret = reset_control_deassert(enc_rstc);
		if (ret) {
			dev_err(&dev->dev, "failed to deassert reset line\n");
			return ret;
		}
        dev_info(&dev->dev, "reset okay\n");
	}

    return ret;
}

static int enc_clock_enable(struct platform_device *dev, const char *id) 
{  
    int ret = 0;

    enc_clk = devm_clk_get(&dev->dev, id);

    if (IS_ERR(enc_clk)) {
        return PTR_ERR(enc_clk);
    } else {
        ret = clk_prepare_enable(enc_clk);
        if (ret) {
            dev_err(&dev->dev, "enabled clock failed\n");
            return ret;
        }
        dev_info(&dev->dev, "clock enabled\n");
    }
    return ret;
}

static int enc_chrdev_probe(struct platform_device *dev) 
{  
    struct resource *res_mem;
    int ret = 0, err = 0, irq;  
    int result;

    memset(&hx280enc_data,0,sizeof(hx280enc_t));
     
    // alloc character device number  
    ret = alloc_chrdev_region(&hx280enc_major, 0, 1, ENC_CHRDEV_NAME);  
    if (ret) {  
        dev_err(&dev->dev, "alloc_chrdev_region failed!\n");  
        goto PROBE_ERR;  
    }  
    dev_info(&dev->dev, "major:%d minor:%d\n", MAJOR(hx280enc_major), MINOR(hx280enc_major));  
    
    // add a character device  
    cdev_init(&enc_chrdev_cdev, &hx280enc_fops);  
    enc_chrdev_cdev.owner = THIS_MODULE;  
    err = cdev_add(&enc_chrdev_cdev, hx280enc_major, 1);  
    if (err) {  
        dev_err(&dev->dev, " cdev_add failed!\n");  
        goto PROBE_ERR;  
    }  
      
    // create the device class  
    enc_chrdev_class = class_create(THIS_MODULE, ENC_CHRDEV_NAME);  
    if (IS_ERR(enc_chrdev_class)) {  
        dev_err(&dev->dev, " class_create failed!\n");  
        goto PROBE_ERR;  
    }  
      
    // create the device node in /dev  
    enc_chrdev_device = device_create(enc_chrdev_class, NULL, hx280enc_major,  
        NULL, ENC_CHRDEV_NAME);  
    if (NULL == enc_chrdev_device) {  
        dev_err(&dev->dev, " device_create failed!\n");  
        goto PROBE_ERR;  
    }  

    res_mem = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (!res_mem) {
        return -ENODEV;
    }

	irq = platform_get_irq(dev, 0);
	if (irq < 0)
		return -ENODEV;

    hx280enc_data.iobaseaddr = (unsigned long)(res_mem->start);
    hx280enc_data.iosize = res_mem->end - res_mem->start + 1;
    hx280enc_data.irq = irq;
    hx280enc_data.async_queue = NULL;
    hx280enc_data.hwregs = devm_ioremap_resource(&dev->dev, res_mem);

    #ifdef ZEBU_TEST
    dec_dumpwaveform_regs = ioremap(0xf8000000, 32);
    #endif

    dev_info(&dev->dev, "mem iobaseaddr = %lxh \n", (unsigned long)(hx280enc_data.iobaseaddr));
    dev_info(&dev->dev, "mem mapping = %lxh \n", (unsigned long)(hx280enc_data.hwregs));
    dev_info(&dev->dev, "mem iosize = %lxh \n", (unsigned long)(hx280enc_data.iosize));
    
    if (IS_ERR((u8 *)hx280enc_data.hwregs)) {
        return PTR_ERR((u8 *)hx280enc_data.hwregs);
    }

    if (enc_clock_enable(dev, "clk_vc8000e")) goto PROBE_ERR;

    if (enc_reset_release(dev, "rstc_vc8000e")) goto PROBE_ERR;

    if (ReserveIO(dev)) goto PROBE_ERR;
    

    ResetAsic(&hx280enc_data);  /* reset hardware */

    /* get the IRQ line */
    if(irq != -1)
    {
        result = request_irq(irq, hx280enc_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
                             SA_INTERRUPT | SA_SHIRQ,
#else
                             IRQF_SHARED,
#endif
                             "hx280enc", (void *) &hx280enc_data);
        if(result == -EINVAL)
        {
            dev_err(&dev->dev, "Bad irq number or handler\n");
            goto PROBE_ERR;
        }
        else if(result == -EBUSY)
        {
            dev_err(&dev->dev, "IRQ <%d> busy, change your config\n",
                   hx280enc_data.irq);
            goto PROBE_ERR;
        }
    }
    else
    {
        dev_err(&dev->dev, "IRQ not in use!\n");
    }
    return 0;  

PROBE_ERR:
    
    if (err)  
        cdev_del(&enc_chrdev_cdev);  
    if (ret)   
        unregister_chrdev_region(hx280enc_major, 1);  
    return -1;  
}  
  
static int enc_chrdev_remove (struct platform_device *dev) 
{  
    dev_info(&dev->dev, "chrdev remove!\n");  
      
    writel(0, hx280enc_data.hwregs + 0x38); /* disable HW */
    writel(0, hx280enc_data.hwregs + 0x04); /* clear enc IRQ */

    #ifdef ZEBU_TEST
    if (dec_dumpwaveform_regs!=0)
        iounmap(dec_dumpwaveform_regs);
    #endif

    ReleaseIO(dev);

    /* free the encoder IRQ */
    if(hx280enc_data.irq != -1)
    {
        free_irq(hx280enc_data.irq, (void *) &hx280enc_data);
    }

    cdev_del(&enc_chrdev_cdev);  
    unregister_chrdev_region(hx280enc_major, 1);  
      
    device_destroy(enc_chrdev_class, hx280enc_major);  
    class_destroy(enc_chrdev_class);  
    
    return 0;  
}

#ifdef CONFIG_PM
static int enc_chrdev_suspend(struct platform_device *pdev, pm_message_t state)
{
    clk_disable(enc_clk);
    pr_info ("%s: clk_disable\n", __func__);
	return 0;
}

static int enc_chrdev_resume(struct platform_device *pdev)
{
    int ret;
    ret = clk_prepare_enable(enc_clk);
    if (ret) {
        dev_err(&pdev->dev, "enabled clock failed\n");
        return ret;
    }
    ret = reset_control_deassert(enc_rstc);
    if (ret)
    {
        dev_err(&pdev->dev, "failed to deassert reset\n");
        return ret;
    }
    pr_info ("%s: clk_prepare_enable\n", __func__);
	return 0;
}
#else
#define enc_chrdev_suspend	NULL
#define enc_chrdev_resume	NULL
#endif

// platform_device and platform_driver must has a same name!  
// or it will not work normally  

static const struct of_device_id enc_chrdev_of_match[] = {
	{ .compatible = "sunplus,q645-hantro-vc8000e", },
    { .compatible = "sunplus,sp7350-hantro-vc8000e", },
	{},
};

static struct platform_driver enc_chrdev_platform_driver = {
    .probe  =   enc_chrdev_probe,  
    .remove =   enc_chrdev_remove,  
    .driver =   {  
        .name   =   ENC_CHRDEV_NAME,  
        .owner  =   THIS_MODULE,
        .of_match_table = enc_chrdev_of_match,
    },  
	.suspend	= enc_chrdev_suspend,
	.resume		= enc_chrdev_resume,
};  

int __init hx280enc_init(void)
{     
    int ret = 0;
    
    ret = platform_driver_register(&enc_chrdev_platform_driver);  
    if (ret)
        pr_err("hx280enc: init failed!\n");  

    return ret;  
}

void __exit hx280enc_exit(void)
{
    platform_driver_unregister(&enc_chrdev_platform_driver); 
    pr_info("hx280enc: module removed\n");
    return;
}

static int ReserveIO(struct platform_device *dev)
{
    long int hwid;

    hwid = readl(hx280enc_data.hwregs);

    /* check for encoder HW ID */
    if((((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID1 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID2 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID3 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID4 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID5 >> 16) & 0xFFFF)))
    {
        dev_err(&dev->dev, "HW not found at 0x%08lx\n", hx280enc_data.iobaseaddr);
#ifdef HX280ENC_DEBUG
        dump_regs((unsigned long) &hx280enc_data);
#endif
        return -EBUSY;
    }
    hx280enc_data.hw_id = hwid;
    hx280enc_data.is_valid = 1;
    dev_info(&dev->dev,
           "HW at base <0x%08lx> with ID <0x%08lx>\n", hx280enc_data.iobaseaddr, hwid);

    return 0;
}

static void ReleaseIO(struct platform_device *dev)
{
    if(hx280enc_data.hwregs)
        devm_iounmap(&dev->dev, (u8 *)hx280enc_data.hwregs);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
irqreturn_t hx280enc_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t hx280enc_isr(int irq, void *dev_id)
#endif
{
    hx280enc_t *dev = (hx280enc_t *) dev_id;
    u32 irq_status;
    unsigned long flags;
    u32 is_write1_clr;
    
    /*If core is not reserved by any user, but irq is received, just ignore it*/
    spin_lock_irqsave(&enc_lock, flags);

    #ifdef ZEBU_TEST
    if (first_irq)
    {
        u32 dump_start = 0xaaaaaaa2;
        pr_info("write %x to 0xf8000000\n", dump_start);
        iowrite32(dump_start, dec_dumpwaveform_regs);
        first_irq = 0;
    }
    #endif

    if (!dev->is_reserved)
    {
      spin_unlock_irqrestore(&enc_lock, flags);
      return IRQ_HANDLED;
    }
    spin_unlock_irqrestore(&enc_lock, flags);
    irq_status = readl(dev->hwregs + 0x04);
    
    /* BASE_HWFuse2 = 0x4a0; HWCFGIrqClearSupport = 0x00800000 */
    is_write1_clr = (readl(dev->hwregs + 0x4a0) & 0x00800000);
    if(irq_status & 0x01)
    {
        /* clear enc IRQ and slice ready interrupt bit */
        if (is_write1_clr)
            writel(irq_status & (0x101), dev->hwregs + 0x04);
        else
            writel(irq_status & (~0x101), dev->hwregs + 0x04);

        /* Handle slice ready interrupts. The reference implementation
         * doesn't signal slice ready interrupts to EWL.
         * The EWL will poll the slices ready register value. */

        if ((irq_status & 0x1FE) == 0x100)
        {
            pr_info("Slice ready IRQ handled!\n");
            return IRQ_HANDLED;
        }

        spin_lock_irqsave(&enc_lock, flags);
        dev->irq_received = 1;
        dev->irq_status = irq_status & (~0x01);
        spin_unlock_irqrestore(&enc_lock, flags);
        
        wake_up_interruptible_all(&enc_wait_queue);

        return IRQ_HANDLED;
    }
    else
    {
        pr_err("IRQ received, but NOT handled!\n");
        return IRQ_NONE;
    }
}

void ResetAsic(hx280enc_t * dev)
{
    int i;

    if (dev->is_valid==0)
        return;
    writel(0, dev->hwregs + 0x38);

    for(i = 4; i < dev->iosize; i += 4)
    {
        writel(0, dev->hwregs + i);
    }
}

#ifdef HX280ENC_DEBUG
void dump_regs(unsigned long data)
{
    hx280enc_t *dev = (hx280enc_t *) data;
    int i;

    pr_info("Reg Dump Start\n");
    for(i = 0; i < dev->iosize; i += 4)
    {
        pr_info("\toffset %02X = %08X\n", i, readl(dev->hwregs + i));
    }
    pr_info("Reg Dump End\n");
}
#endif

module_init(hx280enc_init);
module_exit(hx280enc_exit);
