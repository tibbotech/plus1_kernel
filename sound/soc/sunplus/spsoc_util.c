/*
 * ALSA SoC AUD7021 UTILITY MACRO
 *
 * Author:	 <@sunplus.com>
 *
*/

#include "spsoc_util.h"

//#define SYNCHRONIZE_IO		__asm__ __volatile__ ("dmb" : : : "memory")
#define SYNCHRONIZE_IO		__asm__ __volatile__ ("" : : : "memory")

void delay_ms(UINT32 ms_count)
{
#if 0
	/* can not be used in interrupt (very important)*/
	msleep(ms_count);
#else
	/* cpu may buzy wait */
	mdelay(ms_count);
#endif
}



