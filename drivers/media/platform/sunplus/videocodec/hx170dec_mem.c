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

#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "hx170dec_mem.h"

#define MEMALLOC_SW_MINOR 0
#define MEMALLOC_SW_MAJOR 1
#define MEMALLOC_SW_BUILD ((MEMALLOC_SW_MAJOR * 1000) + MEMALLOC_SW_MINOR)

/* memory size in MBs for MEMALLOC_DYNAMIC */
unsigned long DEC_HLINA_START_ADDRESS;
unsigned long DEC_HLINA_END_ADDRESS;

#ifndef HLINA_TRANSL_OFFSET
#define HLINA_TRANSL_OFFSET 0x0
#endif

/* the size of chunk in MEMALLOC_DYNAMIC */
#define CHUNK_SIZE (PAGE_SIZE * 4)
#define DEC_MEM_CHRDEV_NAME "memalloc_d"

/* user space SW will substract HLINA_TRANSL_OFFSET from the bus address
 * and decoder HW will use the result as the address translated base
 * address. The SW needs the original host memory bus address for memory
 * mapping to virtual address. */

unsigned long addr_transl = HLINA_TRANSL_OFFSET;
// unsigned int dec_reserved_saddress = 0xcc000000;
// unsigned int dec_reserved_length   = 0x04000000;

static int memalloc_major = 0; /* dynamic */

// module_param(dec_reserved_saddress, uint, 0);
// module_param(addr_transl, ulong, 0);

static DEFINE_SPINLOCK(mem_lock);

typedef struct hlinc
{
  u64 bus_address;
  u32 reserved;
  const struct file *filp; /* Client that allocated this chunk */
} hlina_chunk;

static hlina_chunk *hlina_chunks = NULL;
static size_t chunks = 0;

static int AllocMemory(unsigned int *busaddr, unsigned int size,
                       const struct file *filp);
static int FreeMemory(unsigned int busaddr, const struct file *filp);
static void ResetMems(void);

static struct class *dec_mem_chrdev_class = NULL;
static struct device *dec_mem_chrdev_device = NULL;
static struct cdev dec_mem_chrdev_cdev;

static long dec_memalloc_ioctl(struct file *filp, unsigned int cmd,
                               unsigned long arg)
{
  int ret = 0;
  MemallocParams memparams;
  unsigned int busaddr;

  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC)
    return -ENOTTY;
  if (_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR)
    return -ENOTTY;

  if (_IOC_DIR(cmd) & _IOC_READ)
    ret = !access_ok((void *)arg, _IOC_SIZE(cmd));
  else if (_IOC_DIR(cmd) & _IOC_WRITE)
    ret = !access_ok((void *)arg, _IOC_SIZE(cmd));
  if (ret)
    return -EFAULT;

  spin_lock(&mem_lock);

  switch (cmd)
  {
  case MEMALLOC_IOCHARDRESET:
    ResetMems();
    break;
  case MEMALLOC_IOCXGETBUFFER:
    ret = copy_from_user(&memparams, (MemallocParams *)arg,
                         sizeof(MemallocParams));
    if (ret)
      break;

    ret = AllocMemory(&memparams.bus_address, memparams.size, filp);

    memparams.translationOffset = addr_transl;

    ret |= copy_to_user((MemallocParams *)arg, &memparams,
                        sizeof(MemallocParams));

    break;
  case MEMALLOC_IOCSFREEBUFFER:
    __get_user(busaddr, (unsigned int *)arg);
    ret = FreeMemory(busaddr, filp);
    break;
  }

  spin_unlock(&mem_lock);

  return ret ? -EFAULT : 0;
}

static int dec_memalloc_open(struct inode *inode, struct file *filp)
{
  return 0;
}

static int dec_memalloc_release(struct inode *inode, struct file *filp)
{
  int i = 0;

  for (i = 0; i < chunks; i++)
  {
    spin_lock(&mem_lock);
    if (hlina_chunks[i].filp == filp)
    {
      hlina_chunks[i].filp = NULL;
      hlina_chunks[i].reserved = 0;
    }
    spin_unlock(&mem_lock);
  }
  return 0;
}

/* VFS methods */

static struct file_operations dec_memalloc_fops = {
    .owner = THIS_MODULE,
    .open = dec_memalloc_open,
    .release = dec_memalloc_release,
    .compat_ioctl = dec_memalloc_ioctl,
    .unlocked_ioctl = dec_memalloc_ioctl,
    .fasync = NULL,
};

static int dmem_chrdev_probe(struct platform_device *dev)
{
  int err = 0, ret = 0, mem_size;
  int rc;
  struct resource res_mem;
  struct device_node *memnp;

  // alloc character device number
  ret = alloc_chrdev_region(&memalloc_major, 0, 1, DEC_MEM_CHRDEV_NAME);
  if (ret)
  {
    dev_err(&dev->dev, "alloc_chrdev_region failed!\n");
    goto err;
  }
  dev_info(&dev->dev, "major:%d minor:%d\n", MAJOR(memalloc_major), MINOR(memalloc_major));

  // add a character device
  cdev_init(&dec_mem_chrdev_cdev, &dec_memalloc_fops);
  dec_mem_chrdev_cdev.owner = THIS_MODULE;
  err = cdev_add(&dec_mem_chrdev_cdev, memalloc_major, 1);
  if (err)
  {
    dev_err(&dev->dev, "cdev_add failed!\n");
    goto err;
  }

  // create the device class
  dec_mem_chrdev_class = class_create(THIS_MODULE, DEC_MEM_CHRDEV_NAME);
  if (IS_ERR(dec_mem_chrdev_class))
  {
    dev_err(&dev->dev, "class_create failed!\n");
    goto err;
  }

  // create the device node in /dev
  dec_mem_chrdev_device = device_create(dec_mem_chrdev_class, NULL, memalloc_major,
                                        NULL, DEC_MEM_CHRDEV_NAME);
  if (NULL == dec_mem_chrdev_device)
  {
    dev_err(&dev->dev, "device_create failed!\n");
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
  DEC_HLINA_START_ADDRESS = res_mem.start;
  DEC_HLINA_END_ADDRESS = res_mem.end;
  mem_size = DEC_HLINA_END_ADDRESS-DEC_HLINA_START_ADDRESS+1;

  dev_info(&dev->dev, "module init\n");
  dev_info(&dev->dev, "SW build %d \n", MEMALLOC_SW_BUILD);
  dev_info(&dev->dev, "linear memory base = 0x%lx \n", DEC_HLINA_START_ADDRESS);
  dev_info(&dev->dev, "linear memory size = %d MB \n", mem_size/1024/1024);

  chunks = mem_size/CHUNK_SIZE;
  hlina_chunks = (hlina_chunk *)vmalloc(chunks * sizeof(hlina_chunk));
  if (hlina_chunks == NULL)
  {
    dev_err(&dev->dev, "cannot allocate hlina_chunks\n");
    goto err;
  }

  dev_info(&dev->dev, "Total size %d MB; %ld chunks of size %lu\n", mem_size/1024/1024, chunks, CHUNK_SIZE);

  ResetMems();

  return 0;

err:
  dev_err(&dev->dev, "module not inserted\n");
  if (hlina_chunks != NULL)
    vfree(hlina_chunks);
  unregister_chrdev_region(memalloc_major, 1);
  return -ENODEV;
}

/* Cycle through the buffers we have, give the first free one */
static int AllocMemory(unsigned int *busaddr, unsigned int size,
                       const struct file *filp)
{
  int i = 0;
  int j = 0;
  unsigned int next_chunks = 0;

  /* calculate how many chunks we need; round up to chunk boundary */
  unsigned int alloc_chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

  *busaddr = 0;

  /* run through the chunk table */
  for (i = 0; i < chunks;)
  {
    next_chunks = 0;
    /* if this chunk is available */
    if (!hlina_chunks[i].reserved)
    {
      /* check that there is enough memory left */
      if (i + alloc_chunks > chunks)
        break;

      /* check that there is enough consecutive chunks available */
      for (j = i; j < i + alloc_chunks; j++)
      {
        if (hlina_chunks[j].reserved)
        {
          next_chunks = 1;
          /* skip the used chunks */
          i = j + hlina_chunks[j].reserved;
          break;
        }
      }

      /* if enough free memory found */
      if (!next_chunks)
      {
        *busaddr = hlina_chunks[i].bus_address;
        hlina_chunks[i].filp = filp;
        hlina_chunks[i].reserved = alloc_chunks;
        break;
      }
    }
    else
    {
      /* skip the used chunks */
      i += hlina_chunks[i].reserved;
    }
  }

  if (*busaddr == 0)
  {
    pr_err("memalloc: Allocation FAILED: size = %d\n", size);
    return -EFAULT;
  }
  return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned int busaddr, const struct file *filp)
{
  int i = 0;

  for (i = 0; i < chunks; i++)
  {
    /* user space SW has stored the translated bus address, add addr_transl to
     * translate back to our address space */
    if (hlina_chunks[i].bus_address == busaddr + addr_transl)
    {
      if (hlina_chunks[i].filp == filp)
      {
        hlina_chunks[i].filp = NULL;
        hlina_chunks[i].reserved = 0;
      }
      else
      {
        pr_warn("Owner mismatch while freeing memory!\n");
      }
      break;
    }
  }
  return 0;
}

/* Reset "used" status */
static void ResetMems(void)
{
  int i = 0;
  unsigned long ba = DEC_HLINA_START_ADDRESS;

  for (i = 0; i < chunks; i++)
  {
    hlina_chunks[i].bus_address = ba;
    hlina_chunks[i].filp = NULL;
    hlina_chunks[i].reserved = 0;

    ba += CHUNK_SIZE;
  }
}

static int dmem_chrdev_remove(struct platform_device *dev)
{
  dev_info(&dev->dev, " mem chrdev remove!\n");

  cdev_del(&dec_mem_chrdev_cdev);
  unregister_chrdev_region(memalloc_major, 1);

  device_destroy(dec_mem_chrdev_class, memalloc_major);
  class_destroy(dec_mem_chrdev_class);

  return 0;
}

static const struct of_device_id dmem_chrdev_of_match[] = {
    { .compatible = "sunplus,q645-hantro-dmem",},
    { .compatible = "sunplus,sp7350-hantro-dmem",},
    {},
};

static struct platform_driver dmem_chrdev_platform_driver = {
    .probe = dmem_chrdev_probe,
    .remove = dmem_chrdev_remove,
    .driver = {
        .name = DEC_MEM_CHRDEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = dmem_chrdev_of_match,
    },
};

int __init dec_memalloc_init(void)
{
  int ret = 0;
  ret = platform_driver_register(&dmem_chrdev_platform_driver);

  if (ret)
    pr_err("dec memalloc init failed!\n");

  return ret;
}

void __exit dec_memalloc_exit(void)
{
  platform_driver_unregister(&dmem_chrdev_platform_driver);
  pr_info("dec memalloc: module removed\n");
  return;
}

module_init(dec_memalloc_init);
module_exit(dec_memalloc_exit);

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Google Finland Oy");
MODULE_DESCRIPTION("Linear RAM allocation");
