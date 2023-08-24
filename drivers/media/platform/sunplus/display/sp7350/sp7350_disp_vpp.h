/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for VPP layer
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_VPP_H__
#define __SP7350_DISP_VPP_H__

/*IMGREAD_GLOBAL_CONTROL*/
#define SP7350_VPP_IMGREAD_FETCH_EN             BIT(31)

/*IMGREAD_CONFIG*/
#define SP7350_VPP_IMGREAD_HDS_LPF              BIT(24)
#define SP7350_VPP_IMGREAD_CRMA_REPEAT          BIT(20)
#define SP7350_VPP_IMGREAD_DATA_FMT             GENMASK(18, 16)
#define SP7350_VPP_IMGREAD_DATA_FMT_SEL(fmt)    FIELD_PREP(GENMASK(18, 16), fmt)
#define SP7350_VPP_IMGREAD_DATA_FMT_UYVY        0x1
#define SP7350_VPP_IMGREAD_DATA_FMT_YUY2        0x2
#define SP7350_VPP_IMGREAD_DATA_FMT_NV16	0x3
#define SP7350_VPP_IMGREAD_DATA_FMT_NV24	0x6
#define SP7350_VPP_IMGREAD_DATA_FMT_NV12	0x7
#define SP7350_VPP_IMGREAD_YC_SWAP              BIT(12)
#define SP7350_VPP_IMGREAD_UV_SWAP              BIT(11)
#define SP7350_VPP_IMGREAD_UPDN_FLIP            BIT(7)
#define SP7350_VPP_IMGREAD_BIST_MASK            GENMASK(5, 4)
#define SP7350_VPP_IMGREAD_BIST_EN              BIT(5)
#define SP7350_VPP_IMGREAD_BIST_MODE            BIT(4)
#define SP7350_VPP_IMGREAD_FM_MODE              BIT(1)
#define SP7350_VPP_IMGREAD_FIELD_ID             BIT(0)

/*VSCL_CONFIG2*/
#define SP7350_VPP_VSCL_BUFR_BW_LIMIT	        BIT(9)

#define SP7350_VPP_VSCL_BIST_MASK	        GENMASK(8, 7)
#define SP7350_VPP_VSCL_BIST_MODE	        BIT(8)
#define SP7350_VPP_VSCL_BIST_EN		        BIT(7)

#define SP7350_VPP_VSCL_CHKSUM_EN	        BIT(6)
#define SP7350_VPP_VSCL_BUFR_EN		        BIT(4)

#define SP7350_VPP_VSCL_VINT_EN		        BIT(3)
#define SP7350_VPP_VSCL_HINT_EN		        BIT(2)
#define SP7350_VPP_VSCL_DCTRL_EN	        BIT(1)
#define SP7350_VPP_VSCL_ACTRL_EN	        BIT(0)

/*VSCL_HINT_CTRL*/
#define SP7350_VPP_VSCL_HINT_FLT_EN	        BIT(1)
/*VSCL_HINT_HFACTOR_HIGH*/
#define SP7350_VPP_VSCL_HINT_HFACTOR_HIGH	GENMASK(8, 0)
/*VSCL_HINT_HFACTOR_LOW*/
#define SP7350_VPP_VSCL_HINT_HFACTOR_LOW	GENMASK(15, 0)
/*VSCL_HINT_INITF_HIGH*/
#define SP7350_VPP_VSCL_HINT_INITF_PN	        BIT(6)
#define SP7350_VPP_VSCL_HINT_INITF_HIGH	        GENMASK(5, 0)
/*VSCL_HINT_INITF_LOW*/
#define SP7350_VPP_VSCL_HINT_INITF_LOW	        GENMASK(15, 0)

/*VSCL_VINT_CTRL*/
#define SP7350_VPP_VSCL_VINT_FLT_EN	        BIT(1)
/*VSCL_VINT_VFACTOR_HIGH*/
#define SP7350_VPP_VSCL_VINT_VFACTOR_HIGH	GENMASK(8, 0)
/*VSCL_VINT_VFACTOR_LOW*/
#define SP7350_VPP_VSCL_VINT_VFACTOR_LOW	GENMASK(15, 0)
/*VSCL_VINT_INITF_HIGH*/
#define SP7350_VPP_VSCL_VINT_INITF_PN	        BIT(6)
#define SP7350_VPP_VSCL_VINT_INITF_HIGH	        GENMASK(5, 0)
/*VSCL_VINT_INITF_LOW*/
#define SP7350_VPP_VSCL_VINT_INITF_LOW	        GENMASK(15, 0)

/*VPOST_CONFIG*/
#define SP7350_VPP_VPOST_ADJ_EN	                BIT(1)
#define SP7350_VPP_VPOST_CSPACE_EN	        BIT(2)
#define SP7350_VPP_VPOST_OPIF_EN	        BIT(3)
/*VPOST_CSPACE_CONFIG*/
#define SP7350_VPP_VPOST_CSPACE_MASK            GENMASK(2, 0)
#define SP7350_VPP_VPOST_CSPACE_BYPASS          0x0
#define SP7350_VPP_VPOST_CSPACE_601_TO_709      0x1
#define SP7350_VPP_VPOST_CSPACE_709_TO_601      0x2
#define SP7350_VPP_VPOST_CSPACE_JPG_TO_709      0x3
#define SP7350_VPP_VPOST_CSPACE_JPG_TO_601      0x4
/*VPOST_OPIF_CONFIG*/
#define SP7350_VPP_VPOST_MODE_MASK              GENMASK(5, 4)
#define SP7350_VPP_VPOST_MODE_COLORBAR          0x0
#define SP7350_VPP_VPOST_MODE_BORDER            0x1
#define SP7350_VPP_VPOST_MODE_BGCOLOR           0x2
#define SP7350_VPP_VPOST_MODE_GRAY_FADING       0x3
#define SP7350_VPP_VPOST_WIN_ALPHA_EN	        BIT(1)
#define SP7350_VPP_VPOST_WIN_YUV_EN	        BIT(0)
/*VPOST_OPIF_ALPHA*/
#define SP7350_VPP_VPOST_WIN_ALPHA_MASK         GENMASK(15, 8)
#define SP7350_VPP_VPOST_WIN_ALPHA_SET(val)     FIELD_PREP(GENMASK(15, 8), val)
#define SP7350_VPP_VPOST_VPP_ALPHA_MASK         GENMASK(7, 0)
#define SP7350_VPP_VPOST_VPP_ALPHA_SET(val)     FIELD_PREP(GENMASK(7, 0), val)
/*VPOST_OPIF_MSKTOP*/
#define SP7350_VPP_VPOST_OPIF_TOP_MASK          GENMASK(11, 0)
#define SP7350_VPP_VPOST_OPIF_TOP_SET(val)      FIELD_PREP(GENMASK(11, 0), val)
/*VPOST_OPIF_MSKBOT*/
#define SP7350_VPP_VPOST_OPIF_BOT_MASK          GENMASK(11, 0)
#define SP7350_VPP_VPOST_OPIF_BOT_SET(val)      FIELD_PREP(GENMASK(11, 0), val)
/*VPOST_OPIF_MSKLEFT*/
#define SP7350_VPP_VPOST_OPIF_LEFT_MASK         GENMASK(12, 0)
#define SP7350_VPP_VPOST_OPIF_LEFT_SET(val)     FIELD_PREP(GENMASK(12, 0), val)
/*VPOST_OPIF_MSKRIGHT*/
#define SP7350_VPP_VPOST_OPIF_RIGHT_MASK        GENMASK(12, 0)
#define SP7350_VPP_VPOST_OPIF_RIGHT_SET(val)    FIELD_PREP(GENMASK(12, 0), val)

/*
 * Init SP7350 VPP Setting
 */
void sp7350_vpp_init(void);

/*
 * Show SP7350 VPP Info
 */
void sp7350_vpp_decrypt_info(void);
void sp7350_vpp_imgread_resolution_chk(void);
void sp7350_vpp_vscl_resolution_chk(void);

/*
 * SP7350 VPP BIST Settings
 */
void sp7350_vpp_bist_info(void);
void sp7350_vpp_bist_set(int img_vscl_sel, int bist_en, int vpp_bist_type);

/*
 * SP7350 VPP Layer Settings
 */
void sp7350_vpp_layer_onoff(int onoff);
int sp7350_vpp_imgread_set(u32 data_addr1, int x, int y, int w, int h, int yuv_fmt);
int sp7350_vpp_vscl_set(int x, int y, int xlen, int ylen, int input_w, int input_h, int output_w, int output_h);
int sp7350_vpp_vpost_set(int x, int y, int input_w, int input_h, int output_w, int output_h);

#endif	//__SP7350_DISP_VPP_H__

