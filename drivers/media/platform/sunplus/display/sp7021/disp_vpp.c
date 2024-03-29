// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * @file disp_vpp.c
 * @brief
 * @author PoChou Chen
 */
/*******************************************************************************
 *                         H E A D E R   F I L E S
 *******************************************************************************/
#include <linux/module.h>

#include "reg_disp.h"
#include "hal_disp.h"

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static struct DISP_VPOST_REG_t *pVPOSTReg;
static struct DISP_DDFCH_REG_t *pDDFCHReg;
/**************************************************************************
 *             F U N C T I O N    I M P L E M E N T A T I O N S           *
 **************************************************************************/
void DRV_VPP_Init(void *pInHWReg1, void *pInHWReg2)
{
	pVPOSTReg = (struct DISP_VPOST_REG_t *)pInHWReg1;
	pDDFCHReg = (struct DISP_DDFCH_REG_t *)pInHWReg2;
}

int vpost_setting(int x, int y, int input_w, int input_h, int output_w, int output_h)
{
#if defined(TTL_USER_MODE_SUPPORT) || defined(HDMI_USER_MODE_SUPPORT)
	#if defined(TTL_USER_MODE_DTS) || defined(HDMI_USER_MODE_DTS)
	struct sp_disp_device *disp_dev = gDispWorkMem;
	int vpp_adj, user_mode;

	vpp_adj = (int)disp_dev->TTLPar.ttl_vpp_adj;
	user_mode = (int)disp_dev->TTLPar.set_user_mode;
	if (user_mode)
		pVPOSTReg->vpost_mas_sla = 0x1; //en user mode

	#else
		pVPOSTReg->vpost_mas_sla = 0x1; //en user mode
	#endif

	#if defined(TTL_USER_MODE_DTS) || defined(HDMI_USER_MODE_DTS)
	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = vpp_adj; //y active
	#else
#ifdef TTL_MODE_1280_720 //TBD
	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = 0; //y active
#endif
#ifdef TTL_MODE_1024_600
	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = 26; //y active
#endif
#ifdef TTL_MODE_800_480
	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = 29; //y active
#endif
#ifdef TTL_MODE_320_240
	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = 21; //y active
#endif
	#endif
#endif

	if ((input_w != output_w) || (input_h != output_h)) {
		pVPOSTReg->vpost_config1 = 0x11;
		pVPOSTReg->vpost_i_xstart = x;
		pVPOSTReg->vpost_i_ystart = y;
	} else {
		pVPOSTReg->vpost_config1 = 0x1;
		pVPOSTReg->vpost_i_xstart = 0;
		pVPOSTReg->vpost_i_ystart = 0;
	}

	pVPOSTReg->vpost_i_xlen = input_w;
	pVPOSTReg->vpost_i_ylen = input_h;
	pVPOSTReg->vpost_o_xlen = output_w;
	pVPOSTReg->vpost_o_ylen = output_h;

	pVPOSTReg->vpost_config2 = 4; //VPOST CHKSUM_EN
	//pVPOSTReg->vpost_config2 = 5; //VPOST CHKSUM_EN | BIST_EN //colorbar test pattern
	//pVPOSTReg->vpost_config2 = 7; //VPOST CHKSUM_EN | BIST_MMODE | BIST_EN //border test pattern

	return 0;
}
EXPORT_SYMBOL(vpost_setting);

#if defined(TTL_USER_MODE_SUPPORT) || defined(HDMI_USER_MODE_SUPPORT)
    #if defined(TTL_USER_MODE_DTS) || defined(HDMI_USER_MODE_DTS)
void sp_disp_set_ttl_vpp(void)
{
	struct sp_disp_device *disp_dev = gDispWorkMem;
	int vpp_adj;

	vpp_adj = (int)disp_dev->TTLPar.ttl_vpp_adj;

	pVPOSTReg->vpost_o_act_xstart = 0; //x active
	pVPOSTReg->vpost_o_act_ystart = vpp_adj; //y active

}
EXPORT_SYMBOL(sp_disp_set_ttl_vpp);
#endif
#endif

int ddfch_setting(int luma_addr, int chroma_addr, int w, int h, int yuv_fmt)
{
	sp_disp_dbg("ddfch luma=0x%x, chroma=0x%x (w %d,h %d)\n", luma_addr, chroma_addr, w, h);

	pDDFCHReg->ddfch_latch_en = 1;
	if (yuv_fmt == 0)
		pDDFCHReg->ddfch_mode_option = 0; //source yuv420 NV12
	else if (yuv_fmt == 1)
		pDDFCHReg->ddfch_mode_option = 0x400; //source yuv422 NV16
	else if (yuv_fmt == 2)
		pDDFCHReg->ddfch_mode_option = 0x800; //source yuv422 YUY2

	pDDFCHReg->ddfch_enable = 0xd0;
	pDDFCHReg->ddfch_luma_base_addr_0 = luma_addr>>10;
	pDDFCHReg->ddfch_crma_base_addr_0 = chroma_addr>>10;
	pDDFCHReg->ddfch_vdo_frame_size = EXTENDED_ALIGNED(w, 128); //video line pitch
	//pDDFCHReg->ddfch_vdo_crop_size = ((h<<16) | w); //y size & x size
	pDDFCHReg->ddfch_vdo_crop_offset = ((0 << 16) | 0);
	pDDFCHReg->ddfch_config_0 = 0x10000;
	pDDFCHReg->ddfch_bist = 0x80801002;
	pDDFCHReg->ddfch_vdo_crop_size = ((h<<16) | w); //y size & x size

	return 0;
}
EXPORT_SYMBOL(ddfch_setting);
