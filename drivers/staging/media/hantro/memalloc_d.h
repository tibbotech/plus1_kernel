/* Copyright 2018 VeriSilicon. All Rights Reserved. */
/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <linux/ioctl.h>

#undef PDEBUG
#ifdef MEMALLOC_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) printk(KERN_INFO "memalloc: " fmt, ##args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...)
#endif

typedef struct {
  unsigned long bus_address;
  unsigned int size;
  unsigned long translation_offset;
} MemallocParams;

#define MEMALLOC_IOC_MAGIC 'k'

#define MEMALLOC_IOCXGETBUFFER _IOWR(MEMALLOC_IOC_MAGIC, 1, MemallocParams*)
#define MEMALLOC_IOCSFREEBUFFER _IOW(MEMALLOC_IOC_MAGIC, 2, unsigned long*)

#define MEMALLOC_IOCHARDRESET _IO(MEMALLOC_IOC_MAGIC, 15) /* debugging tool */
#define MEMALLOC_IOC_MAXNR 15

#endif /* MEMALLOC_H */
