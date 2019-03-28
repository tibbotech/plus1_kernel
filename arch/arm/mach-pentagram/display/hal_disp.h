#ifndef __HAL_DISP_H__
#define __HAL_DISP_H__
/**
 * @file hal_disp.h
 * @brief 
 * @author PoChou Chen
 */
#include "disp_dmix.h"
#include "disp_tgen.h"
#include "disp_dve.h"
#include "mach/display/disp_osd.h"
#include "mach/display/disp_vpp.h"

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/
#define REG_START_Q628_B			(0x9c000000)
#define REG_START_Q628_A			(0x9ec00000)

#define ALIGNED(x, n)				((x) & (~(n - 1)))
#define EXTENDED_ALIGNED(x, n)		(((x) + ((n) - 1)) & (~(n - 1)))

#define SWAP32(x)	((((UINT32)(x)) & 0x000000ff) << 24 \
					| (((UINT32)(x)) & 0x0000ff00) << 8 \
					| (((UINT32)(x)) & 0x00ff0000) >> 8 \
					| (((UINT32)(x)) & 0xff000000) >> 24)
#define SWAP16(x)	(((x) & 0x00ff) << 8 | ((x) >> 8))

#ifndef ENABLE
	#define ENABLE			1
#endif

#ifndef DISABLE
	#define DISABLE			0
#endif

#define diag_printf printk
extern int printk(const char *fmt, ...);

#define SUPPORT_DEBUG_MON

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
typedef struct _display_size_t {
	UINT16 width;
	UINT16 height;
} display_size_t;

typedef struct {
	void *pHWRegBase;

	//OSD
	spinlock_t osd_lock;
	wait_queue_head_t osd_wait;
	UINT32 osd_field_end_protect;

	//void *aio;
	void *bio;

	//clk
	struct clk *tgen_clk;
	struct clk *dmix_clk;
	struct clk *osd0_clk;
	struct clk *gpost0_clk;
	struct clk *vpost_clk;
	struct clk *ddfch_clk;
	struct clk *dve_clk;
	struct clk *hdmi_clk;

	display_size_t		panelRes;
#if 0
	display_size_t		UIRes;

	DRV_Sys_OutMode_Info_t DispOutMode;
	UINT8 DispOutModeUpdated;

#endif
} DISPLAY_WORKMEM;

extern DISPLAY_WORKMEM gDispWorkMem;

#endif	//__HAL_DISP_H__

