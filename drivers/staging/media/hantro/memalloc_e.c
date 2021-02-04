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
--  Abstract : Allocate memory blocks
--
------------------------------------------------------------------------------*/

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
/* Our header */
#include "memalloc_e.h"

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Google Finland Oy");
MODULE_DESCRIPTION("RAM allocation");

/* start of reserved linear memory area */
#ifndef HLINA_START_ADDRESS
#define HLINA_START_ADDRESS 0x02000000  /* 32MB */
#endif

/* end of reserved linear memory area */
#ifndef HLINA_END_ADDRESS
#define HLINA_END_ADDRESS   0x08000000  /* 128MB */
#endif

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
unsigned long alloc_e_base = HLINA_START_ADDRESS;

/* selects the memory allocation method, i.e. which allocation scheme table is used */
unsigned int alloc_method = MEMALLOC_BASIC;

static int memalloc_major = 0;  /* dynamic */

int id[MAX_OPEN] = { ID_UNUSED };

/* module_param(name, type, perm) */
module_param(alloc_method, uint, 0);

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
    24500
};

/* PC PCI test */
unsigned int size_table_3[] = {
    1, 1,
    57, 57, 57, 57, 57,
    225, 225, 225, 225, 225,
    384, 384,
    1013, 1013, 1013, 1013, 1013, 1013,
    16000
};

static int max_chunks;
static int dynamic = 0;     /* Allocate chunks dynamically */

static hlina_chunk hlina_chunks[MAX_CHUNKS];

static int AllocMemory(unsigned long *busaddr, unsigned int size, struct file *filp);
static int FreeMemory(unsigned long busaddr);
static void ResetMems(void);

static long memalloc_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int err = 0;
    int ret;

    PDEBUG("ioctl cmd 0x%08x\n", cmd);

    if(filp == NULL || arg == 0)
    {
        return -EFAULT;
    }
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if(_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC)
        return -ENOTTY;
    if(_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR)
        return -ENOTTY;

    if(_IOC_DIR(cmd) & _IOC_READ)
        // err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
        err = !access_ok((void *) arg, _IOC_SIZE(cmd));
    else if(_IOC_DIR(cmd) & _IOC_WRITE)
        // err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
        err = !access_ok((void *) arg, _IOC_SIZE(cmd));
    if(err)
        return -EFAULT;

    switch (cmd)
    {
    case MEMALLOC_IOCHARDRESET:

        PDEBUG("HARDRESET\n");
        ResetMems();

        break;

    case MEMALLOC_IOCXGETBUFFER:
        {
            int result;
            MemallocParams memparams;

            PDEBUG("GETBUFFER\n");
            spin_lock(&mem_lock);

            if(__copy_from_user
               (&memparams, (const void *) arg, sizeof(memparams)))
            {
                result = -EFAULT;
            }
            else
            {
                result =
                    AllocMemory(&memparams.busAddress, memparams.size, filp);

                if(__copy_to_user((void *) arg, &memparams, sizeof(memparams)))
                {
                    result = -EFAULT;
                }
            }

            spin_unlock(&mem_lock);

            return result;
        }
    case MEMALLOC_IOCSFREEBUFFER:
        {

            unsigned long busaddr;

            PDEBUG("FREEBUFFER\n");
            spin_lock(&mem_lock);
            __get_user(busaddr, (unsigned long *) arg);
            ret = FreeMemory(busaddr);

            spin_unlock(&mem_lock);
            return ret;
        }
    }
    return 0;
}

static int memalloc_open(struct inode *inode, struct file *filp)
{
    int i = 0;

    for(i = 0; i < MAX_OPEN + 1; i++)
    {

        if(i == MAX_OPEN)
            return -1;
        if(id[i] == ID_UNUSED)
        {
            id[i] = i;
            filp->private_data = id + i;
            break;
        }
    }
    PDEBUG("dev opened, instance=%d\n", i);
    return 0;

}

static int memalloc_release(struct inode *inode, struct file *filp)
{

    int i = 0;

    for(i = 0; i < max_chunks; i++)
    {
        if(hlina_chunks[i].file_id == *((int *) (filp->private_data)))
        {
            if (hlina_chunks[i].used) {
                printk("memalloc_e: Unreleased at close, chunk %d at 0x%p size %d\n",
                        i, (void *)hlina_chunks[i].bus_address, hlina_chunks[i].size);
            }
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
        }
    }
    *((int *) filp->private_data) = ID_UNUSED;
    PDEBUG("dev closed\n");
    return 0;
}

/* VFS methods */
static struct file_operations memalloc_fops = {
  open:memalloc_open,
  release:memalloc_release,
  unlocked_ioctl:memalloc_ioctl,
};

int __init memalloc_init(void)
{
    int result;
    int i = 0;

#if (HLINA_END_ADDRESS <= HLINA_START_ADDRESS)
#error HLINA_END_ADDRESS and/or HLINA_START_ADDRESS not valid!
#endif

    PDEBUG("module init\n");
    printk("memalloc_e: H1 build %d \n", MEMALLOC_SW_BUILD);
    printk("memalloc_e: linear memory base = 0x%p \n", (void *)alloc_e_base);
    printk("memalloc_e: linear memory size =   %8d KB \n",
           (HLINA_END_ADDRESS - HLINA_START_ADDRESS) / 1024);

    switch (alloc_method)
    {

    case MEMALLOC_MAX_OUTPUT:
        size_table = size_table_1;
        max_chunks = (sizeof(size_table_1) / sizeof(*size_table_1));
        printk(KERN_INFO "memalloc_e: allocation method: MEMALLOC_MAX_OUTPUT\n");
        break;
    case MEMALLOC_X280:
        size_table = size_table_2;
        max_chunks = (sizeof(size_table_2) / sizeof(*size_table_2));
        printk(KERN_INFO "memalloc_e: allocation method: MEMALLOC_X280\n");
        break;
    case 3:
        size_table = size_table_3;
        max_chunks = (sizeof(size_table_3) / sizeof(*size_table_3));
        printk(KERN_INFO "memalloc_e: allocation method: PC PCI test\n");
        break;
    case 9:
        size_table = size_table_0;
        dynamic = 1;
        max_chunks = 1;
        printk(KERN_INFO "memalloc_e: allocation method: DYNAMIC\n");
        break;
    default:
        size_table = size_table_0;
        max_chunks = (sizeof(size_table_0) / sizeof(*size_table_0));
        printk(KERN_INFO "memalloc_e: allocation method: MEMALLOC_BASIC\n");
        break;
    }

    result = register_chrdev(memalloc_major, "memalloc_e", &memalloc_fops);
    if(result < 0)
    {
        PDEBUG("memalloc_e: unable to get major %d\n", memalloc_major);
        goto err;
    }
    else if(result != 0)    /* this is for dynamic major */
    {
        memalloc_major = result;
    }

    ResetMems();

    /* We keep a register of out customers, reset it */
    for(i = 0; i < MAX_OPEN; i++)
    {
        id[i] = ID_UNUSED;
    }

    return 0;

  err:
    PDEBUG("memalloc_e: module not inserted\n");
    unregister_chrdev(memalloc_major, "memalloc_e");
    return result;
}

void __exit memalloc_cleanup(void)
{

    PDEBUG("clenup called\n");

    unregister_chrdev(memalloc_major, "memalloc_e");

    printk(KERN_INFO "memalloc_e: module removed\n");
    return;
}

module_init(memalloc_init);
module_exit(memalloc_cleanup);

/* Cycle through the buffers we have, give the first free one */
static int AllocMemory(unsigned long *busaddr, unsigned int size, struct file *filp)
{

    int i = 0;

    *busaddr = 0;

    for(i = 0; i < max_chunks; i++)
    {
        if(!hlina_chunks[i].used && (hlina_chunks[i].size >= size))
        {
            *busaddr = hlina_chunks[i].bus_address;
            hlina_chunks[i].used = 1;
            hlina_chunks[i].file_id = *((int *) (filp->private_data));
            break;
        }
    }
    /* If no free chunk, try to create new chunk after the last one. */
    if (*busaddr == 0)
    {
        long nextba = hlina_chunks[max_chunks-1].bus_address + hlina_chunks[max_chunks-1].size;
        long memavail = HLINA_END_ADDRESS - nextba;
        int chunksize = (size/4096+1)*4096;

        if ((memavail >= chunksize) && (max_chunks < MAX_CHUNKS))
        {
            *busaddr = hlina_chunks[max_chunks].bus_address = nextba;
            hlina_chunks[max_chunks].used = 1;
            hlina_chunks[max_chunks].file_id = *((int *) (filp->private_data));
            hlina_chunks[max_chunks].size = chunksize;
            max_chunks++;

            PDEBUG("memalloc_e: Created new chunk %d at 0x%08x of size = %d\n",
               max_chunks-1, *busaddr, chunksize);
        }
    }

    if((*busaddr + hlina_chunks[i].size) > HLINA_END_ADDRESS)
    {
        printk("memalloc_e: FAILED allocation, linear memory overflow\n");
        *busaddr = 0;
    }
    else if(*busaddr == 0)
    {
        printk("memalloc_e: FAILED allocation, no free chunk of size = %d\n",
               size);
    }
    else
    {
        PDEBUG("memalloc_e: Allocated chunk %d; size requested %d, reserved: %d\n",
               i, size, hlina_chunks[i].size);
    }

    if(*busaddr == 0)
    {
        int total=0;

        /* Dump chunk allocation */
        for(i = 0; i < max_chunks; i++)
        {
            PDEBUG("memalloc_e:   Chunk %2d, size %8d bytes, used=%d\n",
                    i, hlina_chunks[i].size, hlina_chunks[i].used);
            total += hlina_chunks[i].size;
        }
        PDEBUG("memalloc_e:   Chunks total %8d bytes, available %08d bytes.\n",
                    total, HLINA_END_ADDRESS-HLINA_START_ADDRESS-total);
    }

    return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned long busaddr)
{
    int i = 0;

    for(i = 0; i < max_chunks; i++)
    {
        if(hlina_chunks[i].bus_address == busaddr)
        {
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
        }
    }

    if (dynamic) {
        /* Rewind all the trailing free chunks */
        for(i = max_chunks-1; i >= 0; i--) {
            if(hlina_chunks[i].used == 1) {
                max_chunks = i+1;
                break;
            }
        }

        /* All chunks unused, reset to one configured chunk. */
        if (i < 0) max_chunks = 1;
    }

    PDEBUG("memalloc_e: Freeing chunk at 0x%08x, max_chunks=%d\n",
            busaddr, max_chunks);

    return 0;
}

/* Reset "used" status */
void ResetMems(void)
{
    int i = 0;
    unsigned long ba = HLINA_START_ADDRESS;

    if (dynamic) {
        /* Dynamic chunk allocation, reset to single chunk. */
        for(i = 0; i < MAX_CHUNKS; i++)
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
        for(i = 0; i < max_chunks; i++)
        {
            hlina_chunks[i].bus_address = ba;
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
            hlina_chunks[i].size = 4096 * size_table[i];

            ba += hlina_chunks[i].size;
        }
    }

    printk("memalloc_e: %d bytes (%d KB) configured\n",
           ba - HLINA_START_ADDRESS, (ba - HLINA_START_ADDRESS) / 1024);

    if(ba > HLINA_END_ADDRESS)
    {
        printk("memalloc_e: WARNING! Linear memory top limit 0x%lx exceeded!\n",
                HLINA_END_ADDRESS);
    }

}
