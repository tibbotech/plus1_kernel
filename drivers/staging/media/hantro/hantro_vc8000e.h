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
--  Abstract : 6280/7280/8270/8290 Encoder device driver (kernel module)
--
------------------------------------------------------------------------------*/

#ifndef _HX280ENC_H_
#define _HX280ENC_H_
#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */

/*
 * Macros to help debugging
 */

#undef PDEBUG   /* undef it, just in case */
#ifdef HX280ENC_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "hmp4e: " fmt, ## args)
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define HX280ENC_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
 /*
  * #define HX280ENC_IOCGBUFBUSADDRESS _IOR(HX280ENC_IOC_MAGIC,  1, unsigned long *)
  * #define HX280ENC_IOCGBUFSIZE       _IOR(HX280ENC_IOC_MAGIC,  2, unsigned int *)
  */
#define HX280ENC_IOCGHWOFFSET      _IOR(HX280ENC_IOC_MAGIC,  3, unsigned long *)
#define HX280ENC_IOCGHWIOSIZE      _IOR(HX280ENC_IOC_MAGIC,  4, unsigned int *)
#define HX280ENC_IOC_CLI           _IO(HX280ENC_IOC_MAGIC,  5)
#define HX280ENC_IOC_STI           _IO(HX280ENC_IOC_MAGIC,  6)
#define HX280ENC_IOCXVIRT2BUS      _IOWR(HX280ENC_IOC_MAGIC,  7, unsigned long *)

#define HX280ENC_IOCHARDRESET      _IO(HX280ENC_IOC_MAGIC, 8)   /* debugging tool */
#define HX280ENC_IOCGSRAMOFFSET    _IOR(HX280ENC_IOC_MAGIC,  9, unsigned long *)
#define HX280ENC_IOCGSRAMEIOSIZE    _IOR(HX280ENC_IOC_MAGIC,  10, unsigned int *)

#define HX280ENC_IOCH_ENC_RESERVE   _IOR(HX280ENC_IOC_MAGIC, 11,unsigned int *)
#define HX280ENC_IOCH_ENC_RELEASE   _IOR(HX280ENC_IOC_MAGIC, 12,unsigned int *)
#define HX280ENC_IOCG_CORE_WAIT     _IOR(HX280ENC_IOC_MAGIC, 13, unsigned int *)
#define HX280ENC_IOC_MAXNR 30

#endif /* !_HX280ENC_H_ */
