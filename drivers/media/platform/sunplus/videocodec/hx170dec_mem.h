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

#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <linux/ioctl.h>

#undef PDEBUG
#ifdef MEMALLOC_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) pr_info(fmt, ##args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...)
#endif

typedef struct {
  unsigned int bus_address;
  unsigned int size;
  unsigned int translationOffset;
} MemallocParams;

typedef struct {
  unsigned int busAddress;
  unsigned int size;
  unsigned int translationOffset;
} MemDmabufParam;

#define MEMALLOC_IOC_MAGIC 'k'

#define MEMALLOC_IOCXGETBUFFER _IOWR(MEMALLOC_IOC_MAGIC, 1, int)
#define MEMALLOC_IOCSFREEBUFFER _IOW(MEMALLOC_IOC_MAGIC, 2, int)
#define MEMALLOC_IOCHARDRESET _IO(MEMALLOC_IOC_MAGIC, 15) /* debugging tool */
#define MEMALLOC_IOC_MAXNR 15

#endif /* MEMALLOC_H */
