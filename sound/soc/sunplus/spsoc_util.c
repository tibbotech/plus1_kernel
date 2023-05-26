// SPDX-License-Identifier: GPL-2.0
// ALSA SoC AUD7021 UTILITY MACRO
//
// Author:	 <@sunplus.com>
//
//

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/timer.h>

#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
#include "spsoc_util.h"
#elif IS_ENABLED(CONFIG_SND_SOC_AUD645)
#include "spsoc_util-645.h"
#endif
//#define SYNCHRONIZE_IO		__asm__ __volatile__ ("dmb" : : : "memory")
#define SYNCHRONIZE_IO	(__asm__ __volatile__ ("" : : : "memory"))

void delay_ms(UINT32 ms_count)
{
//#if 0
	// can not be used in interrupt (very important)
//	msleep(ms_count);
//#else
	// cpu may buzy wait
	mdelay(ms_count);
//#endif
}




