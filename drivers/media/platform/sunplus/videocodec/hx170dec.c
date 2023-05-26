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
 */

#include "hx170dec.h"

#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#define HXDEC_MAX_CORES 4

#define HANTRO_DEC_ORG_REGS 60
#define HANTRO_PP_ORG_REGS 41

#define HANTRO_DEC_EXT_REGS 27
#define HANTRO_PP_EXT_REGS 9

#define HANTRO_DEC_TOTAL_REGS (HANTRO_DEC_ORG_REGS + HANTRO_DEC_EXT_REGS)
#define HANTRO_PP_TOTAL_REGS (HANTRO_PP_ORG_REGS + HANTRO_PP_EXT_REGS)
#define HANTRO_TOTAL_REGS 155

#define HANTRO_DEC_ORG_FIRST_REG 0
#define HANTRO_DEC_ORG_LAST_REG 59
#define HANTRO_DEC_EXT_FIRST_REG 119
#define HANTRO_DEC_EXT_LAST_REG 145

#define HANTRO_PP_ORG_FIRST_REG 60
#define HANTRO_PP_ORG_LAST_REG 100
#define HANTRO_PP_EXT_FIRST_REG 146
#define HANTRO_PP_EXT_LAST_REG 154

#define DEC_IRQ_STAT_ADDR       4
#define PP_IRQ_STAT_ADDR        240
#define PP_SYNTH_CFG_ADDR       400
#define DEC_SYNTH_CFG_ADDR      200
#define DEC_SYNTH_CFG_2_ADDR    216

/* Logic module base address */
#define SOCLE_LOGIC_0_BASE 0x20000000
#define SOCLE_LOGIC_1_BASE 0x21000000

#define VEXPRESS_LOGIC_0_BASE 0xFC010000
#define VEXPRESS_LOGIC_1_BASE 0xFC020000

/* Logic module IRQs */
#define HXDEC_NO_IRQ -1
#define VP_PB_INT_LT 30
#define SOCLE_INT 36

/* module defaults */
#define DEC_IO_BASE SOCLE_LOGIC0_BASE

#ifdef USE_64BIT_ENV
#define DEC_IO_SIZE ((HANTRO_TOTAL_REGS)*4) /* bytes */
#else
#define DEC_IO_SIZE ((HANTRO_DEC_ORG_REGS + HANTRO_PP_ORG_REGS) * 4) /* bytes */
#endif

#define DEC_IRQ HXDEC_NO_IRQ

static const int DecHwId[] =
    {
        0x8190,
        0x8170,
        0x9170,
        0x9190,
        0x6731,
        0x6e64
    };

int elements = 0;

static int hx170dec_major = 0; /* dynamic allocation */

/* here's all the must remember stuff */
typedef struct
{
    char *buffer;
    __u32 iobaseaddr[HXDEC_MAX_CORES];
    volatile __u32 iosize;
    volatile u8 *hwregs[HXDEC_MAX_CORES];
    volatile int irq;
    int cores;
    // struct fasync_struct *async_queue_dec;
    // struct fasync_struct *async_queue_pp;
} hx170dec_t;

static hx170dec_t hx170dec_data; /* dynamic allocation? */

static struct clk *dec_clk=NULL;
static struct reset_control *dec_rstc;

static void ResetAsic(hx170dec_t *dec);

#ifdef HX170DEC_DEBUG
static void dump_regs(hx170dec_t *dec);
#endif

/* IRQ handler */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
static irqreturn_t hx170dec_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hx170dec_isr(int irq, void *dev_id);
#endif

static u32 dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE / 4];
struct semaphore dec_core_sem;
struct semaphore pp_core_sem;

static int dec_irq = 0;
static int pp_irq = 0;

atomic_t irq_rx = ATOMIC_INIT(0);
atomic_t irq_tx = ATOMIC_INIT(0);

static struct file *dec_owner[HXDEC_MAX_CORES];
static struct file *pp_owner[HXDEC_MAX_CORES];

// spinlock_t dec_lock = SPIN_LOCK_UNLOCKED;
DEFINE_SPINLOCK(dec_lock);

DECLARE_WAIT_QUEUE_HEAD(dec_wait_queue);
DECLARE_WAIT_QUEUE_HEAD(pp_wait_queue);

DECLARE_WAIT_QUEUE_HEAD(hw_queue);

#define DWL_CLIENT_TYPE_H264_DEC    1U
#define DWL_CLIENT_TYPE_MPEG4_DEC   2U
#define DWL_CLIENT_TYPE_JPEG_DEC    3U
#define DWL_CLIENT_TYPE_PP          4U
#define DWL_CLIENT_TYPE_VC1_DEC     5U
#define DWL_CLIENT_TYPE_MPEG2_DEC   6U
#define DWL_CLIENT_TYPE_VP6_DEC     7U
#define DWL_CLIENT_TYPE_AVS_DEC     8U
#define DWL_CLIENT_TYPE_RV_DEC      9U
#define DWL_CLIENT_TYPE_VP8_DEC     10U

static u32 cfg[HXDEC_MAX_CORES];

static void ReadCoreConfig(struct platform_device *dev, hx170dec_t *dec)
{
    int c;
    u32 reg, mask, tmp;

    memset(cfg, 0, sizeof(cfg));

    for (c = 0; c < dec->cores; c++)
    {
        /* Decoder configuration */
        reg = ioread32(dec->hwregs[c] + DEC_SYNTH_CFG_ADDR);

        tmp = (reg >> 24) & 0x3U; /* H264 enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has H264\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

        tmp = (reg >> 28) & 0x01U; /* JPEG enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has JPEG\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

        tmp = (reg >> 26) & 0x3U; /* MPEG4 enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has MPEG4\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

        tmp = (reg >> 29) & 0x3U; /* VC1 enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has VC1\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC : 0;

        tmp = (reg >> 31) & 0x01U; /* MPEG2 enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has MPEG2\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

        tmp = (reg >> 23) & 0x01U; /* VP6 enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has VP6\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

        reg = ioread32(dec->hwregs[c] + DEC_SYNTH_CFG_2_ADDR);

        /* VP7 and WEBP is part of VP8 */
        mask = (1 << 23) | (1 << 24) | (1 << 19);
        tmp = (reg & mask);
        if (tmp & (1 << 23)) /* VP8 enable */
            dev_info(&dev->dev, "core[%d] has VP8\n", c);
        if (tmp & (1 << 24)) /* VP7 enable */
            dev_info(&dev->dev, "core[%d] has VP7\n", c);
        if (tmp & (1 << 19)) /* WEBP enable */
            dev_info(&dev->dev, "core[%d] has WebP\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC : 0;

        tmp = (reg >> 22) & 0x01U; /* AVS enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has AVS\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC : 0;

        tmp = (reg >> 26) & 0x03U; /* RV enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has RV\n", c);

        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

        /* Post-processor configuration */
        reg = ioread32(dec->hwregs[c] + PP_SYNTH_CFG_ADDR);

        tmp = (reg >> 16) & 0x01U; /* PP enable */
        if (tmp)
            dev_info(&dev->dev, "core[%d] has PP\n", c);
        cfg[c] |= tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;
    }
}

static int CoreHasFormat(const u32 *cfg, int core, u32 format)
{
    return (cfg[core] & (1 << format)) ? 1 : 0;
}

int GetDecCore(long core, hx170dec_t *dec, struct file *filp)
{
    int success = 0;
    unsigned long flags;

    spin_lock_irqsave(&dec_lock, flags);
    if (dec_owner[core] == NULL)
    {
        dec_owner[core] = filp;
        success = 1;
    }

    spin_unlock_irqrestore(&dec_lock, flags);

    return success;
}

int GetDecCoreAny(long *core, hx170dec_t *dec, struct file *filp,
                  unsigned long format)
{
    int success = 0;
    long c;

    *core = -1;

    for (c = 0; c < dec->cores; c++)
    {
        /* a free core that has format */
        if (CoreHasFormat(cfg, c, format) && GetDecCore(c, dec, filp))
        {
            success = 1;
            *core = c;
            break;
        }
    }

    return success;
}

long ReserveDecoder(hx170dec_t *dec, struct file *filp, unsigned long format)
{
    long core = -1;

    /* reserve a core */
    if (down_interruptible(&dec_core_sem))
        return -ERESTARTSYS;

    /* lock a core that has specific format*/
    if (wait_event_interruptible(hw_queue,
                                 GetDecCoreAny(&core, dec, filp, format) != 0))
        return -ERESTARTSYS;

    return core;
}

void ReleaseDecoder(hx170dec_t *dec, long core)
{
    u32 status;
    unsigned long flags;

    status = ioread32(dec->hwregs[core] + DEC_IRQ_STAT_ADDR);

    /* make sure HW is disabled */
    if (status & 0x01)
    {
        pr_info("hx170dec: dec[%ld] still enabled -> reset\n", core);

        /* abort decoder */
        status |= 0x30;
        iowrite32(status, dec->hwregs[core] + DEC_IRQ_STAT_ADDR);
    }

    spin_lock_irqsave(&dec_lock, flags);

    dec_owner[core] = NULL;

    spin_unlock_irqrestore(&dec_lock, flags);

    up(&dec_core_sem);

    wake_up_interruptible_all(&hw_queue);
}

long ReservePostProcessor(hx170dec_t *dec, struct file *filp)
{
    unsigned long flags;

    long core = 0;

    /* single core PP only */
    if (down_interruptible(&pp_core_sem))
        return -ERESTARTSYS;

    spin_lock_irqsave(&dec_lock, flags);

    pp_owner[core] = filp;

    spin_unlock_irqrestore(&dec_lock, flags);

    return core;
}

void ReleasePostProcessor(hx170dec_t *dec, long core)
{
    unsigned long flags;

    u32 status = ioread32(dec->hwregs[core] + PP_IRQ_STAT_ADDR);

    /* make sure HW is disabled */
    if (status & 0x01)
    {
        pr_info("hx170dec: PP[%ld] still enabled -> reset\n", core);

        /* disable IRQ */
        status |= 0x10;

        /* disable postprocessor */
        status &= (~0x01);
        iowrite32(0x10, dec->hwregs[core] + PP_IRQ_STAT_ADDR);
    }

    spin_lock_irqsave(&dec_lock, flags);

    pp_owner[core] = NULL;

    spin_unlock_irqrestore(&dec_lock, flags);

    up(&pp_core_sem);
}

long ReserveDecPp(hx170dec_t *dec, struct file *filp, unsigned long format)
{
    /* reserve core 0, dec+PP for pipeline */
    unsigned long flags;

    long core = 0;

    /* check that core has the requested dec format */
    if (!CoreHasFormat(cfg, core, format))
        return -EFAULT;

    /* check that core has PP */
    if (!CoreHasFormat(cfg, core, DWL_CLIENT_TYPE_PP))
        return -EFAULT;

    /* reserve a core */
    if (down_interruptible(&dec_core_sem))
        return -ERESTARTSYS;

    /* wait until the core is available */
    if (wait_event_interruptible(hw_queue,
                                 GetDecCore(core, dec, filp) != 0))
    {
        up(&dec_core_sem);
        return -ERESTARTSYS;
    }

    if (down_interruptible(&pp_core_sem))
    {
        ReleaseDecoder(dec, core);
        return -ERESTARTSYS;
    }

    spin_lock_irqsave(&dec_lock, flags);
    pp_owner[core] = filp;
    spin_unlock_irqrestore(&dec_lock, flags);

    return core;
}

long DecFlushRegs(hx170dec_t *dec, struct core_desc *core)
{
    long ret = 0, i;

    u32 id = core->id;
    unsigned long pregs = core->regs;

    /* copy original dec regs to kernal space*/
    ret = copy_from_user((__u32 *)dec_regs[id], (__u32 *)pregs, HANTRO_DEC_ORG_REGS * 4);

#ifdef USE_64BIT_ENV
    /* copy extended dec regs to kernal space*/
    ret = copy_from_user(dec_regs[id] + HANTRO_DEC_EXT_FIRST_REG,
                         pregs + HANTRO_DEC_EXT_FIRST_REG,
                         HANTRO_DEC_EXT_REGS * 4);
#endif
    if (ret)
    {
        pr_err("sizeof(core) = %ld\n", sizeof(core));
        pr_err("copy_from_user failed, returned %ld\n", ret);
        return -EFAULT;
    }

    /* write dec regs but the status reg[1] to hardware */
    /* both original and extended regs need to be written */
    for (i = 2; i <= HANTRO_DEC_ORG_LAST_REG; i++)
        iowrite32(dec_regs[id][i], dec->hwregs[id] + i * 4);
#ifdef USE_64BIT_ENV
    for (i = HANTRO_DEC_EXT_FIRST_REG; i <= HANTRO_DEC_EXT_LAST_REG; i++)
        iowrite32(dec_regs[id][i], dec->hwregs[id] + i * 4);
#endif
    /* write the status register, which may start the decoder */
    iowrite32(dec_regs[id][1], dec->hwregs[id] + 4);

    return 0;
}

long DecRefreshRegs(hx170dec_t *dec, struct core_desc *core)
{
    long ret, i;
    u32 id = core->id;
    unsigned long pregs = core->regs;

#ifdef USE_64BIT_ENV
    /* user has to know exactly what they are asking for */
    if (core->size != (HANTRO_DEC_TOTAL_REGS * 4))
        return -EFAULT;
#else
    /* user has to know exactly what they are asking for */
    if (core->size != (HANTRO_DEC_ORG_REGS * 4))
        return -EFAULT;
#endif
    /* read all registers from hardware */
    /* both original and extended regs need to be read */
    for (i = 0; i <= HANTRO_DEC_ORG_LAST_REG; i++)
        dec_regs[id][i] = ioread32(dec->hwregs[id] + i * 4);
#ifdef USE_64BIT_ENV
    for (i = HANTRO_DEC_EXT_FIRST_REG; i <= HANTRO_DEC_EXT_LAST_REG; i++)
        dec_regs[id][i] = ioread32(dec->hwregs[id] + i * 4);
#endif
    /* put registers to user space*/
    /* put original registers to user space*/

    ret = copy_to_user((__u32 *)pregs, (__u32 *)dec_regs[id], HANTRO_DEC_ORG_REGS * 4);
#ifdef USE_64BIT_ENV
    /*put extended registers to user space*/
    ret = copy_to_user(pregs + HANTRO_DEC_EXT_FIRST_REG,
                       dec_regs[id] + HANTRO_DEC_EXT_FIRST_REG,
                       HANTRO_DEC_EXT_REGS * 4);
#endif
    if (ret)
    {
        pr_err("id = %d\n", id);
        pr_err("copy_to_user failed, returned %ld\n", ret);
        return -EFAULT;
    }

    return 0;
}

static int CheckDecIrq(hx170dec_t *dec, int id)
{
    unsigned long flags;
    int rdy = 0;

    const u32 irq_mask = (1 << id);

    spin_lock_irqsave(&dec_lock, flags);

    if (dec_irq & irq_mask)
    {
        /* reset the wait condition(s) */
        dec_irq &= ~irq_mask;
        rdy = 1;
    }

    spin_unlock_irqrestore(&dec_lock, flags);

    return rdy;
}

long WaitDecReadyAndRefreshRegs(hx170dec_t *dec, struct core_desc *core)
{
    u32 id = core->id;

    if (wait_event_interruptible(dec_wait_queue, CheckDecIrq(dec, id)))
    {
        pr_info("dec[%d]  wait_event_interruptible interrupted\n", id);
        return -ERESTARTSYS;
    }

    atomic_inc(&irq_tx);

    /* refresh registers */
    return DecRefreshRegs(dec, core);
}

long PPFlushRegs(hx170dec_t *dec, struct core_desc *core)
{
    long ret = 0;
    u32 id = core->id;
    u32 i;
    u32 *pregs = (u32 *)core->regs;

    /* copy original dec regs to kernal space*/
    ret = copy_from_user(dec_regs[id] + HANTRO_PP_ORG_FIRST_REG,
                         pregs + HANTRO_PP_ORG_FIRST_REG,
                         HANTRO_PP_ORG_REGS * 4);

#ifdef USE_64BIT_ENV
    /* copy extended dec regs to kernal space*/
    ret = copy_from_user(dec_regs[id] + HANTRO_PP_EXT_FIRST_REG,
                         pregs + HANTRO_PP_EXT_FIRST_REG,
                         HANTRO_PP_EXT_REGS * 4);
#endif
    if (ret)
    {
        pr_err("copy_from_user failed, returned %ld\n", ret);
        return -EFAULT;
    }

    /* write all regs but the status reg[1] to hardware */
    /* both original and extended regs need to be written */
    for (i = HANTRO_PP_ORG_FIRST_REG + 1; i <= HANTRO_PP_ORG_LAST_REG; i++)
        iowrite32(dec_regs[id][i], dec->hwregs[id] + i * 4);
#ifdef USE_64BIT_ENV
    for (i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
        iowrite32(dec_regs[id][i], dec->hwregs[id] + i * 4);
#endif
    /* write the stat reg, which may start the PP */

    iowrite32(dec_regs[id][HANTRO_PP_ORG_FIRST_REG],
              dec->hwregs[id] + HANTRO_PP_ORG_FIRST_REG * 4);

    return 0;
}

long PPRefreshRegs(hx170dec_t *dec, struct core_desc *core)
{
    long i, ret;
    u32 id = core->id;
    u32 *pregs = (u32 *)(long)core->regs;

#ifdef USE_64BIT_ENV
    /* user has to know exactly what they are asking for */
    if (core->size != (HANTRO_PP_TOTAL_REGS * 4))
        return -EFAULT;
#else
    /* user has to know exactly what they are asking for */
    if (core->size != (HANTRO_PP_ORG_REGS * 4))
        return -EFAULT;
#endif

    /* read all registers from hardware */
    /* both original and extended regs need to be read */
    for (i = HANTRO_PP_ORG_FIRST_REG; i <= HANTRO_PP_ORG_LAST_REG; i++)
        dec_regs[id][i] = ioread32(dec->hwregs[id] + i * 4);
#ifdef USE_64BIT_ENV
    for (i = HANTRO_PP_EXT_FIRST_REG; i <= HANTRO_PP_EXT_LAST_REG; i++)
        dec_regs[id][i] = ioread32(dec->hwregs[id] + i * 4);
#endif
    /* put registers to user space*/
    /* put original registers to user space*/
    ret = copy_to_user(pregs + HANTRO_PP_ORG_FIRST_REG,
                       dec_regs[id] + HANTRO_PP_ORG_FIRST_REG,
                       HANTRO_PP_ORG_REGS * 4);
#ifdef USE_64BIT_ENV
    /* put extended registers to user space*/
    ret = copy_to_user(pregs + HANTRO_PP_EXT_FIRST_REG,
                       dec_regs[id] + HANTRO_PP_EXT_FIRST_REG,
                       HANTRO_PP_EXT_REGS * 4);
#endif
    if (ret)
    {
        pr_err("copy_to_user failed, returned %ld\n", ret);
        return -EFAULT;
    }

    return 0;
}

static int CheckPPIrq(hx170dec_t *dec, int id)
{
    unsigned long flags;
    int rdy = 0;

    const u32 irq_mask = (1 << id);

    spin_lock_irqsave(&dec_lock, flags);

    if (pp_irq & irq_mask)
    {
        /* reset the wait condition(s) */
        pp_irq &= ~irq_mask;
        rdy = 1;
    }

    spin_unlock_irqrestore(&dec_lock, flags);

    return rdy;
}

long WaitPPReadyAndRefreshRegs(hx170dec_t *dec, struct core_desc *core)
{
    u32 id = core->id;

    if (wait_event_interruptible(pp_wait_queue, CheckPPIrq(dec, id)))
    {
        pr_info("PP[%d]  wait_event_interruptible interrupted\n", id);
        return -ERESTARTSYS;
    }

    atomic_inc(&irq_tx);

    /* refresh registers */
    return PPRefreshRegs(dec, core);
}

static int CheckCoreIrq(hx170dec_t *dec, const struct file *filp, int *id)
{
    unsigned long flags;
    int rdy = 0, n = 0;

    do
    {
        u32 irq_mask = (1 << n);

        spin_lock_irqsave(&dec_lock, flags);

        if (dec_irq & irq_mask)
        {
            if (dec_owner[n] == filp)
            {
                /* we have an IRQ for our client */

                /* reset the wait condition(s) */
                dec_irq &= ~irq_mask;

                /* signal ready core no. for our client */
                *id = n;

                rdy = 1;

                break;
            }
            else if (dec_owner[n] == NULL)
            {
                /* zombie IRQ */
                pr_info("IRQ on core[%d], but no owner!!!\n", n);

                /* reset the wait condition(s) */
                dec_irq &= ~irq_mask;
            }
        }

        spin_unlock_irqrestore(&dec_lock, flags);

        n++; /* next core */
    } while (n < dec->cores);

    return rdy;
}

long WaitCoreReady(hx170dec_t *dec, const struct file *filp, int *id)
{
    if (wait_event_interruptible(dec_wait_queue, CheckCoreIrq(dec, filp, id)))
    {
        pr_info("CORE  wait_event_interruptible interrupted\n");
        return -ERESTARTSYS;
    }

    atomic_inc(&irq_tx);

    return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hx170dec_ioctl
 Description     : communication method to/from the user space

 Return type     : long
------------------------------------------------------------------------------*/

static long hx170dec_ioctl(struct file *filp, unsigned int cmd,
                           unsigned long arg)
{
    int err = 0;
    long tmp;

#ifdef HW_PERFORMANCE
    struct timeval *end_time_arg;
#endif

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != HX170DEC_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > HX170DEC_IOC_MAXNR)
        return -ENOTTY;

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok((void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok((void *)arg, _IOC_SIZE(cmd));

    if (err)
        return -EFAULT;

    switch (cmd)
    {
    case HX170DEC_IOC_CLI:
        disable_irq(hx170dec_data.irq);
        break;
    case HX170DEC_IOC_STI:
        enable_irq(hx170dec_data.irq);
        break;
    case HX170DEC_IOCGHWOFFSET:
        __put_user(hx170dec_data.iobaseaddr[0], (__u32 *)arg);
        break;
    case HX170DEC_IOCGHWIOSIZE:
        __put_user(hx170dec_data.iosize, (__u32 *)arg);
        break;
    case HX170DEC_IOC_MC_OFFSETS:
    {
        tmp = copy_to_user((__u32 *)arg, hx170dec_data.iobaseaddr, sizeof(hx170dec_data.iobaseaddr));
        if (err)
        {
            pr_err("copy_to_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }
        break;
    }
    case HX170DEC_IOC_MC_CORES:
        __put_user(hx170dec_data.cores, (__u32 *)arg);
        break;
    case HX170DEC_IOCS_DEC_PUSH_REG:
    {
        struct core_desc core;

        /* get registers from user space*/
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(core));
        if (tmp)
        {
            pr_err("sizeof(core) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }
        DecFlushRegs(&hx170dec_data, &core);
        break;
    }
    case HX170DEC_IOCS_PP_PUSH_REG:
    {
        struct core_desc core;

        /* get registers from user space*/
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(core));
        if (tmp)
        {
            pr_err("sizeof(core) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }

        PPFlushRegs(&hx170dec_data, &core);
        break;
    }
    case HX170DEC_IOCS_DEC_PULL_REG:
    {
        struct core_desc core;
        /* get registers from user space*/
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(core));
        if (tmp)
        {
            pr_err("sizeof(core_desc) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }

        return DecRefreshRegs(&hx170dec_data, &core);
    }
    case HX170DEC_IOCS_PP_PULL_REG:
    {
        struct core_desc core;

        /* get registers from user space*/
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(struct core_desc));
        if (tmp)
        {
            pr_err("sizeof(core_desc) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }

        return PPRefreshRegs(&hx170dec_data, &core);
    }
    case HX170DEC_IOCH_DEC_RESERVE:
    {
        return ReserveDecoder(&hx170dec_data, filp, arg);
    }
    case HX170DEC_IOCT_DEC_RELEASE:
    {
        if (arg >= hx170dec_data.cores || dec_owner[arg] != filp)
        {
            pr_info("dec release, core = %ld\n", arg);
            return -EFAULT;
        }

        ReleaseDecoder(&hx170dec_data, arg);

        break;
    }
    case HX170DEC_IOCQ_PP_RESERVE:
        return ReservePostProcessor(&hx170dec_data, filp);
    case HX170DEC_IOCT_PP_RELEASE:
    {
        if (arg != 0 || pp_owner[arg] != filp)
        {
            pr_info("PP release %ld\n", arg);
            return -EFAULT;
        }

        ReleasePostProcessor(&hx170dec_data, arg);

        break;
    }
    case HX170DEC_IOCX_DEC_WAIT:
    {
        struct core_desc core;

        /* get registers from user space */
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(struct core_desc));
        if (tmp)
        {
            pr_err("sizeof(core) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }

        return WaitDecReadyAndRefreshRegs(&hx170dec_data, &core);
    }
    case HX170DEC_IOCX_PP_WAIT:
    {
        struct core_desc core;

        /* get registers from user space */
        tmp = copy_from_user(&core, (__u32 *)arg, sizeof(struct core_desc));
        if (tmp)
        {
            pr_err("sizeof(core) = %ld\n", sizeof(core));
            pr_err("copy_from_user failed, returned %ld\n", tmp);
            return -EFAULT;
        }

        return WaitPPReadyAndRefreshRegs(&hx170dec_data, &core);
    }
    case HX170DEC_IOCG_CORE_WAIT:
    {
        int id;
        tmp = WaitCoreReady(&hx170dec_data, filp, &id);
        __put_user(id, (u32 *)arg);
        return tmp;
    }
    case HX170DEC_IOX_ASIC_ID:
    {
        u32 id;
        __get_user(id, (u32 *)arg);

        if (id >= hx170dec_data.cores)
        {
            return -EFAULT;
        }
        id = ioread32(hx170dec_data.hwregs[id]);
        __put_user(id, (u32 *)arg);
        break;
    }
    case HX170DEC_DEBUG_STATUS:
    {
        pr_info("hx170dec: dec_irq     = 0x%08x \n", dec_irq);
        pr_info("hx170dec: pp_irq      = 0x%08x \n", pp_irq);

        pr_info("hx170dec: IRQs received/sent2user = %d / %d \n",
               atomic_read(&irq_rx), atomic_read(&irq_tx));

        for (tmp = 0; tmp < hx170dec_data.cores; tmp++)
        {
            pr_info("hx170dec: dec_core[%ld] %s\n",
                   tmp, dec_owner[tmp] == NULL ? "FREE" : "RESERVED");
            pr_info("hx170dec: pp_core[%ld]  %s\n",
                   tmp, pp_owner[tmp] == NULL ? "FREE" : "RESERVED");
        }
        break;
    }
    default:
        return -ENOTTY;
    }

    return 0;
}

#define DEC_CHRDEV_NAME "hantro_dec"

static struct class *dec_chrdev_class = NULL;
static struct device *dec_chrdev_device = NULL;
static struct cdev dec_chrdev_cdev;

/*------------------------------------------------------------------------------
 Function name   : hx170dec_open
 Description     : open method

 Return type     : int
------------------------------------------------------------------------------*/

static int hx170dec_open(struct inode *inode, struct file *filp)
{
    return 0;
}

/*------------------------------------------------------------------------------
 Function name   : hx170dec_release
 Description     : Release driver

 Return type     : int
------------------------------------------------------------------------------*/

static int hx170dec_release(struct inode *inode, struct file *filp)
{
    int n;
    hx170dec_t *dec = &hx170dec_data;

    for (n = 0; n < dec->cores; n++)
    {
        if (dec_owner[n] == filp)
        {
            pr_info("releasing dec core %i lock\n", n);
            ReleaseDecoder(dec, n);
        }
    }

    for (n = 0; n < 1; n++)
    {
        if (pp_owner[n] == filp)
        {
            pr_info("releasing pp core %i lock\n", n);
            ReleasePostProcessor(dec, n);
        }
    }

    return 0;
}

/* VFS methods */
static struct file_operations hx170dec_fops = {
    .owner = THIS_MODULE,
    .open = hx170dec_open,
    .release = hx170dec_release,
    .compat_ioctl = hx170dec_ioctl,
    .unlocked_ioctl = hx170dec_ioctl,
    .fasync = NULL};

/*------------------------------------------------------------------------------
 Function name   : CheckHwId
 Return type     : int
------------------------------------------------------------------------------*/
static int CheckHwId(struct platform_device *dev, hx170dec_t *dec)
{
    long int hwid;
    int i;
    size_t numHw = sizeof(DecHwId) / sizeof(*DecHwId);

    int found = 0;

    for (i = 0; i < dec->cores; i++)
    {
        if (dec->hwregs[i] != NULL)
        {
            hwid = readl(dec->hwregs[i]);
            dev_info(&dev->dev, "Core %d HW ID=0x%08lx\n", i, hwid);
            hwid = (hwid >> 16) & 0xFFFF; /* product version only */

            while (numHw--)
            {
                if (hwid == DecHwId[numHw])
                {
                    dev_info(&dev->dev, "Supported HW found at 0x%08x\n",
                           hx170dec_data.iobaseaddr[i]);
                    found++;
                    break;
                }
            }
            if (!found)
            {
                dev_err(&dev->dev, "Unknown HW found at 0x%08x\n",
                       hx170dec_data.iobaseaddr[i]);
                return 0;
            }
            found = 0;
            numHw = sizeof(DecHwId) / sizeof(*DecHwId);
        }
    }

    return 1;
}

/*------------------------------------------------------------------------------
 Function name   : hx170dec_isr
 Description     : interrupt handler

 Return type     : irqreturn_t
------------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
irqreturn_t hx170dec_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t hx170dec_isr(int irq, void *dev_id)
#endif
{
    unsigned long flags;
    unsigned int handled = 0;
    int i;
    volatile u8 *hwregs;

    hx170dec_t *dev = (hx170dec_t *)dev_id;
    u32 irq_status_dec;
    u32 irq_status_pp;

    spin_lock_irqsave(&dec_lock, flags);

    for (i = 0; i < dev->cores; i++)
    {
        volatile u8 *hwregs = dev->hwregs[i];

        /* interrupt status register read */
        irq_status_dec = ioread32(hwregs + DEC_IRQ_STAT_ADDR);
        if (irq_status_dec & 0x100) // DEC IRQ 
        {
            /* clear dec IRQ */
            irq_status_dec &= (~0x100);
            iowrite32(irq_status_dec, hwregs + DEC_IRQ_STAT_ADDR);

            atomic_inc(&irq_rx);

            dec_irq |= (1 << i);

            wake_up_interruptible_all(&dec_wait_queue);
            handled++;
        }
    }

    /* check PP also */
    hwregs = dev->hwregs[0];
    irq_status_pp = ioread32(hwregs + PP_IRQ_STAT_ADDR);
    if (irq_status_pp & 0x100) /* PP IRQ*/
    {
        /* clear pp IRQ */
        irq_status_pp &= (~0x100);
        iowrite32(irq_status_pp, hwregs + PP_IRQ_STAT_ADDR);

        atomic_inc(&irq_rx);

        pp_irq |= 1;

        wake_up_interruptible_all(&pp_wait_queue);
        handled++;
    }

    spin_unlock_irqrestore(&dec_lock, flags);

    if (!handled)
    {
        pr_err("IRQ received, but not x170's!\n");
    }

    return IRQ_RETVAL(handled);
}

static int dec_reset_release(struct platform_device *dev, const char *id)
{
    struct reset_control *dec_rstc;
    int ret = 0;

    dec_rstc = devm_reset_control_get(&dev->dev, id);

    if (IS_ERR(dec_rstc))
    {
        dev_err(&dev->dev, "failed to retrieve reset controller\n");
        return PTR_ERR(dec_rstc);
    }
    else
    {
        ret = reset_control_deassert(dec_rstc);
        if (ret)
        {
            dev_err(&dev->dev, "failed to deassert reset line\n");
            return ret;
        }
        dev_info(&dev->dev, "reset okay\n");
    }

    return ret;
}

static int dec_clock_enable(struct platform_device *dev, const char *id)
{
    int ret = 0;

    dec_clk = devm_clk_get(&dev->dev, id);

    if (IS_ERR(dec_clk))
    {
        return PTR_ERR(dec_clk);
    }
    else
    {
        ret = clk_prepare_enable(dec_clk);
        if (ret)
        {
            dev_err(&dev->dev, "enabled clock failed\n");
            return ret;
        }
        dev_info(&dev->dev, "clock enabled\n");
    }
    return ret;
}

static int dec_chrdev_probe(struct platform_device *dev)
{
    int ret = 0, err = 0;
    int result = -ENODEV, irq;
    long int hwid;

    struct resource *res_mem;

    dev_info(&dev->dev, "dec/pp kernel module. \n");

    memset(&hx170dec_data, 0, sizeof(hx170dec_t));

    // alloc character device number
    ret = alloc_chrdev_region(&hx170dec_major, 0, 1, DEC_CHRDEV_NAME);
    if (ret)
    {
        dev_err(&dev->dev, "alloc_chrdev_region failed!\n");
        goto PROBE_ERR;
    }
    dev_info(&dev->dev, "major:%d minor:%d\n", MAJOR(hx170dec_major), MINOR(hx170dec_major));

    // add a character device
    cdev_init(&dec_chrdev_cdev, &hx170dec_fops);
    dec_chrdev_cdev.owner = THIS_MODULE;

    err = cdev_add(&dec_chrdev_cdev, hx170dec_major, 1);
    if (err)
    {
        dev_err(&dev->dev, "cdev_add failed!\n");
        goto PROBE_ERR;
    }

    // create the device class
    dec_chrdev_class = class_create(THIS_MODULE, DEC_CHRDEV_NAME);
    if (IS_ERR(dec_chrdev_class))
    {
        dev_err(&dev->dev, "class_create failed!\n");
        goto PROBE_ERR;
    }

    // create the device node in /dev
    dec_chrdev_device = device_create(dec_chrdev_class, NULL, hx170dec_major,
                                      NULL, DEC_CHRDEV_NAME);
    if (NULL == dec_chrdev_device)
    {
        dev_err(&dev->dev, "device_create failed!\n");
        goto PROBE_ERR;
    }

    hx170dec_data.cores = 1;

    res_mem = platform_get_resource(dev, IORESOURCE_MEM, 0);

    if (!res_mem)
    {
        return -ENODEV;
    }

    irq = platform_get_irq(dev, 0);
    if (irq < 0)
        return -ENODEV;

    hx170dec_data.iobaseaddr[0] = (unsigned long)(res_mem->start);
    hx170dec_data.iosize = res_mem->end - res_mem->start + 1;
    hx170dec_data.irq = irq;

    hx170dec_data.hwregs[0] = (volatile u8 *)devm_ioremap_resource(&dev->dev, res_mem);

    if (hx170dec_data.hwregs[0] == NULL)
    {
        dev_err(&dev->dev, "failed to ioremap HW regs\n");
        return -EBUSY;
    }

    hwid = ((readl(hx170dec_data.hwregs[0])) >> 16) & 0xFFFF; /* product version only */

    /* check for correct HW */
    if (CheckHwId(dev, &hx170dec_data) == 0)
    {
        return -EBUSY;
    }

    if (dec_clock_enable(dev, "clk_vc8000d"))
        goto PROBE_ERR;

    if (dec_reset_release(dev, "rstc_vc8000d"))
        goto PROBE_ERR;

    /* register irq for each core */
    result = request_irq(hx170dec_data.irq, hx170dec_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
                         SA_INTERRUPT | SA_SHIRQ,
#else
                         IRQF_SHARED,
#endif
                         DEC_CHRDEV_NAME, (void *)&hx170dec_data);

    if (result != 0)
    {
        if (result == -EINVAL)
        {
            dev_err(&dev->dev, "Bad irq number or handler\n");
        }
        else if (result == -EBUSY)
        {
            dev_err(&dev->dev, "IRQ <%d> busy, change your config\n",
                    hx170dec_data.irq);
        }
        goto PROBE_ERR;
    }

    memset(dec_owner, 0, sizeof(dec_owner));
    memset(pp_owner, 0, sizeof(pp_owner));

    sema_init(&dec_core_sem, hx170dec_data.cores);
    sema_init(&pp_core_sem, 1);

    /* read configuration fo all cores */
    ReadCoreConfig(dev, &hx170dec_data);

    /* reset hardware */
    dev_info(&dev->dev, "reset hardware\n");
    ResetAsic(&hx170dec_data);

    return 0;

PROBE_ERR:

    dev_err(&dev->dev, "module not inserted\n");
    unregister_chrdev(hx170dec_major, DEC_CHRDEV_NAME);
    return result;
}

static int dec_chrdev_remove(struct platform_device *dev)
{
    hx170dec_t *hdev = &hx170dec_data;
    int n = 0;

    /* reset hardware */
    ResetAsic(hdev);

    /* free the IRQ */
    for (n = 0; n < hdev->cores; n++)
    {
        if (hdev->irq != -1)
        {
            free_irq(hdev->irq, (void *)hdev);
        }
    }

    cdev_del(&dec_chrdev_cdev);
    unregister_chrdev_region(hx170dec_major, 1);

    device_destroy(dec_chrdev_class, hx170dec_major);
    class_destroy(dec_chrdev_class);

    return 0;
}

/*------------------------------------------------------------------------------
 Function name   : ResetAsic
 Description     : reset asic

 Return type     :
------------------------------------------------------------------------------*/
void ResetAsic(hx170dec_t *dev)
{
    int i, j;
    u32 status;

    for (j = 0; j < dev->cores; j++)
    {
        status = ioread32(dev->hwregs[j] + DEC_IRQ_STAT_ADDR);

        if (status & 0x01)
        {
            /* abort with IRQ disabled */
            status = 0x30;
            iowrite32(status, dev->hwregs[j] + DEC_IRQ_STAT_ADDR);
        }

        /* reset PP */
        iowrite32(0, dev->hwregs[j] + PP_IRQ_STAT_ADDR);

        for (i = 4; i < dev->iosize; i += 4)
        {
            iowrite32(0, dev->hwregs[j] + i);
        }
    }
}

#ifdef CONFIG_PM
static int dec_chrdev_suspend(struct platform_device *pdev, pm_message_t state)
{
    clk_disable(dec_clk);
    pr_info ("%s: clk_disable\n", __func__);
	return 0;
}

static int dec_chrdev_resume(struct platform_device *pdev)
{
    int ret;
    ret = clk_prepare_enable(dec_clk);
    if (ret) {
        dev_err(&pdev->dev, "enabled clock failed\n");
        return ret;
    }
    ret = reset_control_deassert(dec_rstc);
    if (ret)
    {
        dev_err(&pdev->dev, "failed to deassert reset\n");
        return ret;
    }
    pr_info ("%s: clk_prepare_enable\n", __func__);
	return 0;
}
#else
#define dec_chrdev_suspend	NULL
#define dec_chrdev_resume	NULL
#endif

static const struct of_device_id dec_chrdev_of_match[] = {
    {
        .compatible = "sunplus,q645-hantro-vc8000d",
    },
    {
        .compatible = "sunplus,sp7350-hantro-vc8000d",
    },
    {},
};

static struct platform_driver dec_chrdev_platform_driver = {
    .probe = dec_chrdev_probe,
    .remove = dec_chrdev_remove,
    .driver = {
        .name = DEC_CHRDEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = dec_chrdev_of_match,
    },
	.suspend	= dec_chrdev_suspend,
	.resume		= dec_chrdev_resume,
};

/*------------------------------------------------------------------------------
 Function name   : hx170dec_init
 Description     : Initialize the driver

 Return type     : int
------------------------------------------------------------------------------*/

int __init hx170dec_init(void)
{
    int ret = 0;

    ret = platform_driver_register(&dec_chrdev_platform_driver);
    if (ret)
        pr_err("hx170dec init failed!\n");

    return ret;
}

void __exit hx170dec_exit(void)
{
    platform_driver_unregister(&dec_chrdev_platform_driver);
    pr_info("hx170dec exit\n");
    return;
}

/*------------------------------------------------------------------------------
 Function name   : dump_regs
 Description     : Dump registers

 Return type     :
------------------------------------------------------------------------------*/
#ifdef HX170DEC_DEBUG
void dump_regs(hx170dec_t *dev)
{
    int i, c;

    pr_info("Reg Dump Start\n");
    for (c = 0; c < dev->cores; c++)
    {
        for (i = 0; i < dev->iosize; i += 4 * 4)
        {
            pr_info("\toffset %04X: %08X  %08X  %08X  %08X\n", i,
                   ioread32(dev->hwregs[c] + i),
                   ioread32(dev->hwregs[c] + i + 4),
                   ioread32(dev->hwregs[c] + i + 16),
                   ioread32(dev->hwregs[c] + i + 24));
        }
    }
    pr_info("Reg Dump End\n");
}
#endif

module_init(hx170dec_init);
module_exit(hx170dec_exit);

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Google Finland Oy");
MODULE_DESCRIPTION("Driver module for Hantro Decoder/Post-Processor");
