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
 ****************************************************************************
 *
 *  Abstract : Allocate memory blocks
 *
 ****************************************************************************
 */

#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range */
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>
/* this header files wraps some common module-space operations ...
   here we use mem_map_reserve() macro */

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/list.h>
/* for current pid */
#include <linux/sched.h>
#include <linux/uaccess.h>

#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
/* Our header */
#include "hx280enc_mem.h"

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Google Finland Oy");
MODULE_DESCRIPTION("RAM allocation");

/* dmabuf */
#include <linux/dma-buf.h>

#define MEMALLOC_SW_MINOR 0
#define MEMALLOC_SW_MAJOR 1
#define MEMALLOC_SW_BUILD ((MEMALLOC_SW_MAJOR * 1000) + MEMALLOC_SW_MINOR)

#define MAX_OPEN 32
#define ID_UNUSED 0xFF
#define MEMALLOC_BASIC 0
#define MEMALLOC_MAX_OUTPUT 1
#define MEMALLOC_X280 2

#define MEMALLOC_DYNAMIC 9

#define MAX_CHUNKS 256

/* memory size in MBs for MEMALLOC_DYNAMIC */
unsigned long ENC_HLINA_START_ADDRESS_ENC;
unsigned long ENC_HLINA_END_ADDRESS_ENC;

/* selects the memory allocation method, i.e. which allocation scheme table is used */
unsigned int alloc_method = MEMALLOC_DYNAMIC; /*MEMALLOC_BASIC;*/

static int memalloc_major = 0; /* dynamic */
static struct dma_buf *dmabuf_exported;

int eid[MAX_OPEN] = {ID_UNUSED};

// module_param(enc_reserved_saddress, uint, 0);
// module_param(alloc_method, uint, 0);

/* here's all the must remember stuff */
struct allocation
{
    struct list_head list;
    void *buffer;
    unsigned int order;
    int fid;
};

struct list_head heap_list;

DEFINE_SPINLOCK(mem_lock);

typedef struct hlinc
{
    unsigned long bus_address;
    unsigned int used;
    unsigned int size;
    int file_id;
} hlina_chunk;

unsigned int *size_table;

/* MEMALLOC_BASIC, 24500 chunks, roughly 96MB */
unsigned int size_table_0[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10,
    22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75,
    86, 86, 86, 86, 86,
    113, 113,
    152, 152,
    162, 162, 162,
    270, 270, 270,
    400, 400, 400,
    450, 450,
    769, 769,
    2000, 2000, 2000,
    3072,
    8192
};

/* MEMALLOC_MAX_OUTPUT, 65532 chunks, 256MB */
unsigned int size_table_1[] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    16, 16, 16, 16, 16, 16, 16, 16,
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64,
    512, 512, 512, 512, 512, 512, 512, 512,
    2675,
    2675,
    21575,
    32960
};

/* MEMALLOC_X280, 48092 chunks, 188MB */
unsigned int size_table_2[] = {
    /* Table calculated for 155x255 max frame size */
    1, 1, 1, 1,
    4, 4, 4, 4,
    16, 16, 16, 16,
    64, 64, 64, 64,
    512,
    2048,
    /* Reference frame buffers, 3x luma + 4x chroma */
    1250, 1250, 1250, 1250,
    2500, 2500, 2500,
    /* Input frame buffer + stab */
    4096, 4096,
    /* 96MB used so far, if more is available we can allocate big chunk for
     * maximum input resolution */
    24500};

/* PC PCI test */
unsigned int size_table_3[] = {
    1, 1,
    57, 57, 57, 57, 57,
    225, 225, 225, 225, 225,
    384, 384,
    1013, 1013, 1013, 1013, 1013, 1013,
    16000};

static int max_chunks;
static int dynamic = 0; /* Allocate chunks dynamically */

static hlina_chunk hlina_chunks[MAX_CHUNKS];

static int AllocMemory(unsigned int *busaddr, unsigned int size, struct file *filp);
static int FreeMemory(unsigned long busaddr);
static void ResetMems(struct platform_device *dev, int);


static struct sg_table *exporter_map_dma_buf(struct dma_buf_attachment *attachment,
					 enum dma_data_direction dir)
{
	return NULL;
}

static void exporter_unmap_dma_buf(struct dma_buf_attachment *attachment,
			       struct sg_table *table,
			       enum dma_data_direction dir)
{
}

static void exporter_release(struct dma_buf *dmabuf)
{
	kfree(dmabuf->priv);
	dmabuf_exported = NULL;
}

static void *exporter_vmap(struct dma_buf *dmabuf)
{
	return dmabuf->priv;
}

static int exporter_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return -ENODEV;
}

static const struct dma_buf_ops exp_dmabuf_ops = {
	.map_dma_buf = exporter_map_dma_buf,
	.unmap_dma_buf = exporter_unmap_dma_buf,
	.release = exporter_release,
	.vmap = exporter_vmap,
	.mmap = exporter_mmap,
};

static long enc_memalloc_ioctl(struct file *filp,
                               unsigned int cmd, unsigned long arg)
{
    int err = 0;
    int ret;

    if (filp == NULL)
    {
        return -EFAULT;
    }
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok((void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok((void *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    switch (cmd)
    {
    case MEMALLOC_IOCHARDRESET:

        ResetMems(NULL, 0);
        break;

    case MEMALLOC_IOCXGETBUFFER:
    {
        MemallocParams memparams;

        spin_lock(&mem_lock);

        if (__copy_from_user(&memparams, (const void *)arg, sizeof(memparams)))
        {
            ret = -EFAULT;
        }
        else
        {
            ret =
                AllocMemory(&memparams.busAddress, memparams.size, filp);

            if (__copy_to_user((void *)arg, &memparams, sizeof(memparams)))
            {
                ret = -EFAULT;
            }
        }

        spin_unlock(&mem_lock);

        return ret;
    }
    case MEMALLOC_IOCSFREEBUFFER:
    {
        unsigned int busaddr;
        spin_lock(&mem_lock);
        __get_user(busaddr, (unsigned int *)arg);
        ret = FreeMemory(busaddr);

        spin_unlock(&mem_lock);
        return ret;
    }
    case MEMALLOC_IOCSIMPORTDMABUF:
    {
        int err;
        struct dma_buf *dmabuf;
        struct dma_buf_attachment *attachment;
        struct sg_table *table;
        struct device *dev;
        unsigned long long dma_mask = 0xffffffff;

        MemDmabufParam dmaparams;

        spin_lock(&mem_lock);
        if (__copy_from_user(&dmaparams, (const void *)arg, sizeof(dmaparams)))
        {
            pr_err("EFAULT\n");
            ret = -EFAULT;
        }
        else
        {
            dmaparams.busAddress = 0;
            
            dmabuf = dma_buf_get(dmaparams.fd);
            if (IS_ERR(dmabuf)) {
                err = PTR_ERR(dmabuf);
                pr_err("got dmabuf fail %d\n", err);
                goto dmabuf_end;
            }

            dev = kzalloc(sizeof(*dev), GFP_ATOMIC | GFP_DMA);
            dev->dma_mask = &dma_mask;
            dev->bus_dma_limit = 0;

            dev_set_name(dev, "importer");

            attachment = dma_buf_attach(dmabuf, dev);
            if (IS_ERR(attachment)) {
                err = PTR_ERR(attachment);
                pr_err("Failed to attach %d\n", err);
                goto dmabuf_end;
            }

            table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
            if (IS_ERR(table)) {
                err = PTR_ERR(table);
                pr_err("Failed to attachement %d\n", err);
                goto dmabuf_end;
            }
            dmaparams.busAddress = sg_dma_address(table->sgl);
            dmaparams.size = sg_dma_len(table->sgl);
            dma_buf_unmap_attachment(attachment, table, DMA_BIDIRECTIONAL);
            dma_buf_detach(dmabuf, attachment);
            dma_buf_put(dmabuf);

            if (__copy_to_user((void *)arg, &dmaparams, sizeof(dmaparams)))
            {
                ret = -EFAULT;
            }
        }
        dmabuf_end:
        spin_unlock(&mem_lock);

        return ret;
    }
    }
    return 0;
}

static int enc_memalloc_open(struct inode *inode, struct file *filp)
{
    int i = 0;

    for (i = 0; i < MAX_OPEN + 1; i++)
    {
        if (i == MAX_OPEN)
            return -1;
        if (eid[i] == ID_UNUSED)
        {
            eid[i] = i;
            filp->private_data = eid + i;
            break;
        }
    }
    return 0;
}

static int enc_memalloc_release(struct inode *inode, struct file *filp)
{
    int i = 0;

    for (i = 0; i < max_chunks; i++)
    {
        if (hlina_chunks[i].file_id == *((int *)(filp->private_data)))
        {
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
        }
    }
    *((int *)filp->private_data) = ID_UNUSED;

    for (i = 0; i < MAX_OPEN; i++)
    {
        eid[i] = ID_UNUSED;
    }

    return 0;
}

/* VFS methods */

static struct file_operations enc_memalloc_fops = {
    .owner = THIS_MODULE,
    .open = enc_memalloc_open,
    .release = enc_memalloc_release,
    .compat_ioctl = enc_memalloc_ioctl,
    .unlocked_ioctl = enc_memalloc_ioctl,
    .fasync = NULL,
};

#define ENC_MEM_CHRDEV_NAME "memalloc_e"

static struct class *enc_mem_chrdev_class = NULL;
static struct device *enc_mem_chrdev_device = NULL;
static struct cdev enc_mem_chrdev_cdev;

static int emem_chrdev_probe(struct platform_device *dev)
{
    int result;
    int i = 0, err = 0, ret = 0;
    int rc;
    struct resource res_mem;
    struct device_node *memnp;

    switch (alloc_method)
    {

    case MEMALLOC_MAX_OUTPUT:
        size_table = size_table_1;
        max_chunks = (sizeof(size_table_1) / sizeof(*size_table_1));
        dev_info(&dev->dev, "allocation method: MEMALLOC_MAX_OUTPUT\n");
        break;
    case MEMALLOC_X280:
        size_table = size_table_2;
        max_chunks = (sizeof(size_table_2) / sizeof(*size_table_2));
        dev_info(&dev->dev, "allocation method: MEMALLOC_X280\n");
        break;
    case 3:
        size_table = size_table_3;
        max_chunks = (sizeof(size_table_3) / sizeof(*size_table_3));
        dev_info(&dev->dev, "allocation method: PC PCI test\n");
        break;
    case 9:
        size_table = size_table_0;
        dynamic = 1;
        max_chunks = 1;
        dev_info(&dev->dev, "allocation method: DYNAMIC\n");
        break;
    default:
        size_table = size_table_0;
        max_chunks = (sizeof(size_table_0) / sizeof(*size_table_0));
        dev_info(&dev->dev, "allocation method: MEMALLOC_BASIC\n");
        break;
    }

    // alloc character device number
    ret = alloc_chrdev_region(&memalloc_major, 0, 1, ENC_MEM_CHRDEV_NAME);
    if (ret)
    {
        dev_err(&dev->dev, "alloc_chrdev_region failed!\n");
        goto err;
    }
    dev_info(&dev->dev, "major:%d minor:%d\n", MAJOR(memalloc_major), MINOR(memalloc_major));

    // add a character device
    cdev_init(&enc_mem_chrdev_cdev, &enc_memalloc_fops);
    enc_mem_chrdev_cdev.owner = THIS_MODULE;
    err = cdev_add(&enc_mem_chrdev_cdev, memalloc_major, 1);
    if (err)
    {
        dev_err(&dev->dev, " cdev_add failed!\n");
        goto err;
    }

    // create the device class
    enc_mem_chrdev_class = class_create(THIS_MODULE, ENC_MEM_CHRDEV_NAME);
    if (IS_ERR(enc_mem_chrdev_class))
    {
        dev_err(&dev->dev, " class_create failed!\n");
        goto err;
    }

    // create the device node in /dev
    enc_mem_chrdev_device = device_create(enc_mem_chrdev_class, NULL, memalloc_major,
                                          NULL, ENC_MEM_CHRDEV_NAME);
    if (NULL == enc_mem_chrdev_device)
    {
        dev_err(&dev->dev, " device_create failed!\n");
        goto err;
    }

    memnp = of_parse_phandle(dev->dev.of_node, "memory-region", 0);
    if (!memnp)
    {
        dev_err(&dev->dev, "no memory-region node\n");
        return -EINVAL;
    }

    rc = of_address_to_resource(memnp, 0, &res_mem);
    of_node_put(memnp);

    if (rc)
    {
        dev_err(&dev->dev, "failed to translate memory-region to a resource\n");
        return -EINVAL;
    }
    ENC_HLINA_START_ADDRESS_ENC = res_mem.start;
    ENC_HLINA_END_ADDRESS_ENC = res_mem.end;

    dev_info(&dev->dev, "SW build %d \n", MEMALLOC_SW_BUILD);
    dev_info(&dev->dev, "linear memory base = 0x%lx \n", ENC_HLINA_START_ADDRESS_ENC);
    dev_info(&dev->dev, "linear memory size = %ld MB \n",
           (ENC_HLINA_END_ADDRESS_ENC-ENC_HLINA_START_ADDRESS_ENC+1) / 1024);

    ResetMems(dev, 1);

    /* We keep a register of out customers, reset it */
    for (i = 0; i < MAX_OPEN; i++)
    {
        eid[i] = ID_UNUSED;
    }

    return 0;

err:
    dev_err(&dev->dev, "module not inserted\n");
    unregister_chrdev_region(memalloc_major, 1);
    return result;
}

static int emem_chrdev_remove(struct platform_device *dev)
{
    dev_info(&dev->dev, " mem chrdev remove!\n");

    cdev_del(&enc_mem_chrdev_cdev);
    unregister_chrdev_region(memalloc_major, 1);

    device_destroy(enc_mem_chrdev_class, memalloc_major);
    class_destroy(enc_mem_chrdev_class);

    return 0;
}

static const struct of_device_id emem_chrdev_of_match[] = {
	{ .compatible = "sunplus,q645-hantro-emem", },
    { .compatible = "sunplus,sp7350-hantro-emem",},
	{},
};

static struct platform_driver emem_chrdev_platform_driver = {
    .probe = emem_chrdev_probe,
    .remove = emem_chrdev_remove,
    .driver = {
        .name = ENC_MEM_CHRDEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = emem_chrdev_of_match,
    },
};

int __init enc_memalloc_init(void)
{
    int ret = 0;

    ret = platform_driver_register(&emem_chrdev_platform_driver);
    if (ret)
        pr_err("enc memalloc: init failed!\n");

    return ret;
}

void __exit enc_memalloc_exit(void)
{
    platform_driver_unregister(&emem_chrdev_platform_driver);
    pr_info("enc memalloc: module removed\n");
    return;
}

/* Cycle through the buffers we have, give the first free one */
static int AllocMemory(unsigned int *busaddr, unsigned int size, struct file *filp)
{
    int i = 0;

    *busaddr = 0;

    for (i = 0; i < max_chunks; i++)
    {
        if (!hlina_chunks[i].used && (hlina_chunks[i].size >= size))
        {
            *busaddr = hlina_chunks[i].bus_address;
            hlina_chunks[i].used = 1;
            hlina_chunks[i].file_id = *((int *)(filp->private_data));
            break;
        }
    }
    /* If no free chunk, try to create new chunk after the last one. */
    if (*busaddr == 0)
    {
        long nextba = hlina_chunks[max_chunks - 1].bus_address + hlina_chunks[max_chunks - 1].size;
        long memavail = ENC_HLINA_END_ADDRESS_ENC - nextba;
        int chunksize = (size / 4096 + 1) * 4096;

        if ((memavail >= chunksize) && (max_chunks < MAX_CHUNKS))
        {
            *busaddr = hlina_chunks[max_chunks].bus_address = nextba;
            hlina_chunks[max_chunks].used = 1;
            hlina_chunks[max_chunks].file_id = *((int *)(filp->private_data));
            hlina_chunks[max_chunks].size = chunksize;
            max_chunks++;
        }
    }

    if ((*busaddr + hlina_chunks[i].size) > ENC_HLINA_END_ADDRESS_ENC)
    {
        pr_err("FAILED allocation, linear memory overflow\n");
        *busaddr = 0;
    }
    else if (*busaddr == 0)
    {
        pr_err("FAILED allocation, no free chunk of size = %d, max_chunks = %d\n",
               size, max_chunks);
    }

    if (*busaddr == 0)
    {
        int total = 0;

        /* Dump chunk allocation */
        for (i = 0; i < max_chunks; i++)
        {
            total += hlina_chunks[i].size;
        }
    }

    return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned long busaddr)
{
    int i = 0;

    for (i = 0; i < max_chunks; i++)
    {
        if (hlina_chunks[i].bus_address == busaddr)
        {
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
        }
    }

    if (dynamic)
    {
        /* Rewind all the trailing free chunks */
        for (i = max_chunks - 1; i >= 0; i--)
        {
            if (hlina_chunks[i].used == 1)
            {
                max_chunks = i + 1;
                break;
            }
        }

        /* All chunks unused, reset to one configured chunk. */
        if (i < 0)
            max_chunks = 1;
    }
    return 0;
}

/* Reset "used" status */
void ResetMems(struct platform_device *dev, int bprobe)
{
    int i = 0;
    unsigned long ba = ENC_HLINA_START_ADDRESS_ENC;

    if (dynamic)
    {
        /* Dynamic chunk allocation, reset to single chunk. */
        for (i = 0; i < MAX_CHUNKS; i++)
        {
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
            hlina_chunks[i].size = 0;
        }

        hlina_chunks[0].bus_address = ba;
        hlina_chunks[0].size = 4096;
        ba += hlina_chunks[0].size;
        max_chunks = 1;
    }
    else
    {
        /* Fixed chunks, reset them all. */
        for (i = 0; i < max_chunks; i++)
        {
            hlina_chunks[i].bus_address = ba;
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
            hlina_chunks[i].size = 4096 * size_table[i];

            ba += hlina_chunks[i].size;
        }
    }

    if (!bprobe) return;

    if (dev)
        dev_info(&dev->dev, "chunk size = %ld KB, chunks = %d configured\n",
           (ba - ENC_HLINA_START_ADDRESS_ENC) / 1024, max_chunks);
    else
        pr_info("chunk size = %ld KB, chunks = %d configured\n",
           (ba - ENC_HLINA_START_ADDRESS_ENC) / 1024, max_chunks);

    if (ba > ENC_HLINA_END_ADDRESS_ENC)
    {
        if (dev)
            dev_warn(&dev->dev, "Linear memory top limit 0x%lx exceeded!\n", ENC_HLINA_END_ADDRESS_ENC);
        else
            pr_warn("Linear memory top limit 0x%lx exceeded!\n", ENC_HLINA_END_ADDRESS_ENC);
    }
}

module_init(enc_memalloc_init);
module_exit(enc_memalloc_exit);

