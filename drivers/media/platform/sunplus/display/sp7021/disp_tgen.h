/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DISP_TGEN_H__
#define __DISP_TGEN_H__

#include <media/sunplus/disp/sp7021/display.h>

enum DRV_TGEN_Input_t {
	DRV_TGEN_VPP0 = 0,
	DRV_TGEN_OSD0,
	DRV_TGEN_PTG,
	DRV_TGEN_ALL
};

enum DRV_VideoFormat_t {
	DRV_FMT_480P = 0,
	DRV_FMT_576P,
	DRV_FMT_720P,
	DRV_FMT_1080P,
	DRV_FMT_USER_MODE,
	DRV_FMT_MAX,
};

enum DRV_FrameRate_t {
	DRV_FrameRate_5994Hz = 0,
	DRV_FrameRate_50Hz,
	DRV_FrameRate_24Hz,
	DRV_FrameRate_MAX
};

struct DRV_SetTGEN_t {
	enum DRV_VideoFormat_t fmt;
	enum DRV_FrameRate_t fps;
	u16 htt;
	u16 vtt;
	u16 hactive;
	u16 vactive;
	u16 v_bp;
};

void DRV_TGEN_Init(void *pInHWReg);
void DRV_TGEN_GetFmtFps(enum DRV_VideoFormat_t *fmt, enum DRV_FrameRate_t *fps);
extern unsigned int DRV_TGEN_GetLineCntNow(void);
void DRV_TGEN_SetUserInt1(u32 count);
void DRV_TGEN_SetUserInt2(u32 count);
int DRV_TGEN_Set(struct DRV_SetTGEN_t *SetTGEN);
#if defined(TTL_USER_MODE_SUPPORT) || defined(HDMI_USER_MODE_SUPPORT)
    #if defined(TTL_USER_MODE_DTS) || defined(HDMI_USER_MODE_DTS)
void sp_disp_set_ttl_tgen(struct DRV_SetTGEN_t *SetTGEN);
#endif
#endif
void DRV_TGEN_Get(struct DRV_SetTGEN_t *GetTGEN);
void DRV_TGEN_Reset(void);
int DRV_TGEN_Adjust(enum DRV_TGEN_Input_t Input, u32 Adjust);

#endif	//__DISP_TGEN_H__

