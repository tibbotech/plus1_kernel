/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DISP_VPP_H__
#define __DISP_VPP_H__

#include <media/sunplus/disp/sp7021/display.h>

void DRV_VPP_Init(void *pInHWReg1, void *pInHWReg2);
int vpost_setting(int x, int y, int input_w, int input_h, int output_w, int output_h);
#if defined(TTL_USER_MODE_SUPPORT) || defined(HDMI_USER_MODE_SUPPORT)
    #if defined(TTL_USER_MODE_DTS) || defined(HDMI_USER_MODE_DTS)
void sp_disp_set_ttl_vpp(void);
#endif
#endif
int ddfch_setting(int luma_addr, int chroma_addr, int w, int h, int is_yuv422);

extern void sp_disp_set_ttl_vpp(void);
#endif	//__DISP_VPP_H__

