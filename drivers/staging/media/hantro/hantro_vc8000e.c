/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
--                                                                            --
--  Abstract : 6280/7280/8270/8290/H1 Encoder device driver (kernel module)
--
------------------------------------------------------------------------------*/

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

/* our own stuff */
#include "hantro_vc8000e.h"

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Verisilicon Inc.");
MODULE_DESCRIPTION("Hantro H1 Encoder driver");

/* this is ARM Integrator specific stuff */
#define INTEGRATOR_LOGIC_MODULE0_BASE   0xC0000000
/*
#define INTEGRATOR_LOGIC_MODULE1_BASE   0xD0000000
#define INTEGRATOR_LOGIC_MODULE2_BASE   0xE0000000
#define INTEGRATOR_LOGIC_MODULE3_BASE   0xF0000000
*/

#define VP_PB_INT_LT                    30
/*
#define INT_EXPINT1                     10
#define INT_EXPINT2                     11
#define INT_EXPINT3                     12
*/
/* these could be module params in the future */

#define ENC_IO_BASE                 INTEGRATOR_LOGIC_MODULE0_BASE
#define ENC_IO_SIZE                 (500 * 4)    /* bytes */

#define ENC_HW_ID1                  0x62800000
#define ENC_HW_ID2                  0x72800000
#define ENC_HW_ID3                  0x82700000
#define ENC_HW_ID4                  0x82900000
#define ENC_HW_ID5                  0x48310000

#define HX280ENC_BUF_SIZE           0

unsigned long vc8000e_base_port = INTEGRATOR_LOGIC_MODULE0_BASE;
int vc8000e_irq = VP_PB_INT_LT;

/* for critical data access */
DEFINE_SPINLOCK(vc8000e_lock);
/* for irq wait */
DECLARE_WAIT_QUEUE_HEAD(enc_wait_queue);
/* for reserve hw */
DECLARE_WAIT_QUEUE_HEAD(enc_hw_queue);  

/* module_param(name, type, perm) */
module_param(vc8000e_base_port, ulong, 0);
module_param(vc8000e_irq, int, 0);

/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hx280enc_major = 0;

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

static int ReserveIO(void);
static void ReleaseIO(void);
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
  
    spin_lock_irqsave(&vc8000e_lock, flags);

    if(dev->irq_received)
    {
     /* reset the wait condition(s) */
     PDEBUG("check %d irq ready\n",i);
     dev->irq_received = 0;
     rdy = 1;
    }

    spin_unlock_irqrestore(&vc8000e_lock, flags);
    //printk("rdy=%d\n",rdy);

    return rdy;
}

unsigned int WaitEncReady(hx280enc_t *dev)
{
   PDEBUG("WaitEncReady\n");

   if(wait_event_interruptible(enc_wait_queue, CheckEncIrq(dev)))
   {
       PDEBUG("ENC wait_event_interruptible interrupted\n");
       return -ERESTARTSYS;
   }

   return 0;
}

int CheckCoreOccupation(hx280enc_t *dev) 
{
    int ret = 0;
    unsigned long flags;

    spin_lock_irqsave(&vc8000e_lock, flags);
    if(!dev->is_reserved) 
    {
        dev->is_reserved = 1;
        dev->pid = current->pid;
        ret = 1;
        PDEBUG("CheckCoreOccupation pid=%d\n",dev->pid);
    }
    spin_unlock_irqrestore(&vc8000e_lock, flags);

    return ret;
}

int GetWorkableCore(hx280enc_t *dev) 
{
    int ret = 0;

    PDEBUG("GetWorkableCore\n");

    if(dev->is_valid && CheckCoreOccupation(dev))
    {
        ret = 1;
    } 
  
    return ret;
}

long ReserveEncoder(hx280enc_t *dev) 
{
  /* lock a core that has specified core id*/
  if(wait_event_interruptible(enc_hw_queue,GetWorkableCore(dev) != 0 ))
      return -ERESTARTSYS;

  return 0;
}

void ReleaseEncoder(hx280enc_t * dev)
{
    unsigned long flags;

    PDEBUG("ReleaseEncoder\n");
    
    spin_lock_irqsave(&vc8000e_lock, flags);
    PDEBUG(" relase reseve by pid=%d with current->pid=%d\n",dev->pid,current->pid);
    if(dev->is_reserved && dev->pid == current->pid)
    {
        dev->pid = -1;
        dev->is_reserved = 0;
    }

    dev->irq_received = 0;
    dev->irq_status = 0;
    spin_unlock_irqrestore(&vc8000e_lock, flags);

    wake_up_interruptible_all(&enc_hw_queue);
    
    return;
}

static long hx280enc_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int err = 0;

    PDEBUG("ioctl cmd 0x%08ux\n", cmd);
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
            int ret;
            PDEBUG("Reserve ENC Cores\n");
            ret = ReserveEncoder(&hx280enc_data);
            return ret;
        }
    case HX280ENC_IOCH_ENC_RELEASE: 
        {
            PDEBUG("Release ENC Core\n");
            ReleaseEncoder(&hx280enc_data);
            break;
        }
        
    case HX280ENC_IOCG_CORE_WAIT:
        {
            int ret;
            ret = WaitEncReady(&hx280enc_data);
            return ret;
        }
    }
    return 0;
}

static int hx280enc_open(struct inode *inode, struct file *filp)
{
    int result = 0;
    hx280enc_t *dev = &hx280enc_data;

    filp->private_data = (void *) dev;

    PDEBUG("dev opened\n");
    return result;
}

static int hx280enc_release(struct inode *inode, struct file *filp)
{
    hx280enc_t *dev = (hx280enc_t *) filp->private_data;
    unsigned long flags;
#ifdef HX280ENC_DEBUG
    dump_regs((unsigned long) dev); /* dump the regs */
#endif

    PDEBUG("dev closed\n");
    spin_lock_irqsave(&vc8000e_lock, flags);
    if (dev->is_reserved == 1 && dev->pid == current->pid)
    {
        dev->pid = -1;
        dev->is_reserved = 0;
        dev->irq_received = 0;
        dev->irq_status = 0;
        PDEBUG("release reserved core\n");
    }
    spin_unlock_irqrestore(&vc8000e_lock, flags);
    wake_up_interruptible_all(&enc_hw_queue);
    return 0;
}

/* VFS methods */
static struct file_operations hx280enc_fops = {
    .owner= THIS_MODULE,
    .open = hx280enc_open,
    .release = hx280enc_release,
    .unlocked_ioctl = hx280enc_ioctl,
    .fasync = NULL,
};

int __init hx280enc_init(void)
{
    int result;

    printk(KERN_INFO "hx280enc: module init - base_port=0x%08lx irq=%i\n",
           vc8000e_base_port, vc8000e_irq);

    memset(&hx280enc_data,0,sizeof(hx280enc_t));
    hx280enc_data.iobaseaddr = vc8000e_base_port;
    hx280enc_data.iosize = ENC_IO_SIZE;
    hx280enc_data.irq = vc8000e_irq;
    hx280enc_data.async_queue = NULL;
    hx280enc_data.hwregs = NULL;

    result = register_chrdev(hx280enc_major, "hx280enc", &hx280enc_fops);
    if(result < 0)
    {
        printk(KERN_INFO "hx280enc: unable to get major <%d>\n",
               hx280enc_major);
        return result;
    }
    else if(result != 0)    /* this is for dynamic major */
    {
        hx280enc_major = result;
    }

    result = ReserveIO();
    if(result < 0)
    {
        goto err;
    }

    ResetAsic(&hx280enc_data);  /* reset hardware */

    /* get the IRQ line */
    if(vc8000e_irq != -1)
    {
        result = request_irq(vc8000e_irq, hx280enc_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
                             SA_INTERRUPT | SA_SHIRQ,
#else
                             IRQF_SHARED,
#endif
                             "hx280enc", (void *) &hx280enc_data);
        if(result == -EINVAL)
        {
            printk(KERN_ERR "hx280enc: Bad irq number or handler\n");
            ReleaseIO();
            goto err;
        }
        else if(result == -EBUSY)
        {
            printk(KERN_ERR "hx280enc: IRQ <%d> busy, change your config\n",
                   hx280enc_data.irq);
            ReleaseIO();
            goto err;
        }
    }
    else
    {
        printk(KERN_INFO "hx280enc: IRQ not in use!\n");
    }

    printk(KERN_INFO "hx280enc: module inserted. Major <%d>\n", hx280enc_major);

    return 0;

  err:
    unregister_chrdev(hx280enc_major, "hx280enc");
    printk(KERN_INFO "hx280enc: module not inserted\n");
    return result;
}

void __exit hx280enc_cleanup(void)
{
    writel(0, hx280enc_data.hwregs + 0x38); /* disable HW */
    writel(0, hx280enc_data.hwregs + 0x04); /* clear enc IRQ */

    /* free the encoder IRQ */
    if(hx280enc_data.irq != -1)
    {
        free_irq(hx280enc_data.irq, (void *) &hx280enc_data);
    }

    ReleaseIO();

    unregister_chrdev(hx280enc_major, "hx280enc");

    printk(KERN_INFO "hx280enc: module removed\n");
    return;
}

static int ReserveIO(void)
{
    long int hwid;

    if(!request_mem_region
       (hx280enc_data.iobaseaddr, hx280enc_data.iosize, "hx280enc"))
    {
        printk(KERN_INFO "hx280enc: failed to reserve HW regs\n");
        return -EBUSY;
    }

    hx280enc_data.hwregs =
        (volatile u8 *) ioremap_nocache(hx280enc_data.iobaseaddr,
                                        hx280enc_data.iosize);

    if(hx280enc_data.hwregs == NULL)
    {
        printk(KERN_INFO "hx280enc: failed to ioremap HW regs\n");
        ReleaseIO();
        return -EBUSY;
    }

    hwid = readl(hx280enc_data.hwregs);
    printk("the hwid=0x%lx\n", hwid);

    /* check for encoder HW ID */
    if((((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID1 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID2 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID3 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID4 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID5 >> 16) & 0xFFFF)))
    {
        printk(KERN_INFO "hx280enc: HW not found at 0x%08lx\n",
               hx280enc_data.iobaseaddr);
#ifdef HX280ENC_DEBUG
        dump_regs((unsigned long) &hx280enc_data);
#endif
        ReleaseIO();
        return -EBUSY;
    }
    hx280enc_data.hw_id = hwid;
    hx280enc_data.is_valid = 1;
    printk(KERN_INFO
           "hx280enc: HW at base <0x%08lx> with ID <0x%08lx>\n",
           hx280enc_data.iobaseaddr, hwid);

    return 0;
}

static void ReleaseIO(void)
{
    if (hx280enc_data.is_valid == 0)
        return;
    if(hx280enc_data.hwregs)
        iounmap((void *) hx280enc_data.hwregs);
    release_mem_region(hx280enc_data.iobaseaddr, hx280enc_data.iosize);
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
    spin_lock_irqsave(&vc8000e_lock, flags);
    if (!dev->is_reserved)
    {
      spin_unlock_irqrestore(&vc8000e_lock, flags);
      return IRQ_HANDLED;
    }
    spin_unlock_irqrestore(&vc8000e_lock, flags);
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
            PDEBUG("Slice ready IRQ handled!\n");
            return IRQ_HANDLED;
        }

        spin_lock_irqsave(&vc8000e_lock, flags);
        dev->irq_received = 1;
        dev->irq_status = irq_status & (~0x01);
        spin_unlock_irqrestore(&vc8000e_lock, flags);
        
        wake_up_interruptible_all(&enc_wait_queue);

        PDEBUG("IRQ handled!\n");
        return IRQ_HANDLED;
    }
    else
    {
        PDEBUG("IRQ received, but NOT handled!\n");
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

    PDEBUG("Reg Dump Start\n");
    for(i = 0; i < dev->iosize; i += 4)
    {
        PDEBUG("\toffset %02X = %08X\n", i, readl(dev->hwregs + i));
    }
    PDEBUG("Reg Dump End\n");
}
#endif

module_init(hx280enc_init);
module_exit(hx280enc_cleanup);

