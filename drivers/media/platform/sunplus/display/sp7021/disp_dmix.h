/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DISP_DMIX_H__
#define __DISP_DMIX_H__

#include <media/sunplus/disp/sp7021/display.h>

#define DMIX_LUMA_OFFSET_MIN	(-50)
#define DMIX_LUMA_OFFSET_MAX	(50)
#define DMIX_LUMA_SLOPE_MIN		(0.60)
#define DMIX_LUMA_SLOPE_MAX		(1.40)

enum DRV_DMIX_InputSel_t {
	DRV_DMIX_VPP0 = 0,
	DRV_DMIX_VPP1,	//unsupported
	DRV_DMIX_VPP2,	//unsupported
	DRV_DMIX_OSD0,
	DRV_DMIX_OSD1,	//unsupported
	DRV_DMIX_OSD2,	//unsupported
	DRV_DMIX_OSD3,	//unsupported
	DRV_DMIX_PTG,
};

enum DRV_DMIX_LayerMode_t {
	DRV_DMIX_AlphaBlend,
	DRV_DMIX_Transparent,
	DRV_DMIX_Opacity,
};

enum DRV_DMIX_LayerId_t {
	DRV_DMIX_BG,
	DRV_DMIX_L1,
	DRV_DMIX_L2,	//unsupported
	DRV_DMIX_L3,	//unsupported
	DRV_DMIX_L4,	//unsupported
	DRV_DMIX_L5,	//unsupported
	DRV_DMIX_L6,
};

enum DRV_DMIX_TPG_t {
	DRV_DMIX_TPG_BGC,
	DRV_DMIX_TPG_V_COLORBAR,
	DRV_DMIX_TPG_H_COLORBAR,
	DRV_DMIX_TPG_BORDER,
	DRV_DMIX_TPG_SNOW,
	DRV_DMIX_TPG_MAX,
};

struct DRV_DMIX_PlaneAlpha_t {
	enum DRV_DMIX_LayerId_t Layer;
	unsigned int EnPlaneAlpha;
	unsigned int EnFixAlpha;
	unsigned int AlphaValue;
};

struct DRV_DMIX_Luma_Adj_t {
	unsigned int enable;
	unsigned int brightness;
	unsigned int contrast;
	//-------------
	unsigned short CP1_Dst;
	unsigned short CP1_Src;
	unsigned short CP2_Dst;
	unsigned short CP2_Src;
	unsigned short CP3_Dst;
	unsigned short CP3_Src;
	unsigned short Slope0;
	unsigned short Slope1;
	unsigned short Slope2;
	unsigned short Slope3;
};

struct DRV_DMIX_Chroma_Adj_t {
	unsigned int enable;
	unsigned short satcos;
	unsigned short satsin;
};

struct DRV_DMIX_Layer_Set_t {
	enum DRV_DMIX_LayerId_t Layer;
	enum DRV_DMIX_LayerMode_t LayerMode;
	enum DRV_DMIX_InputSel_t FG_Sel;
};

void DRV_DMIX_Pixel_En_Sel(void);
void DRV_DMIX_Init(void *pInReg);
enum DRV_Status_t DRV_DMIX_PTG_ColorBar(enum DRV_DMIX_TPG_t tpg_sel,
		int bg_color_yuv,
		int border_len);
void DRV_DMIX_PTG_Color_Set(unsigned int color);
void DRV_DMIX_PTG_Color_Set_YCbCr(unsigned char enable, unsigned char Y, unsigned char Cb, unsigned char Cr);
enum DRV_Status_t DRV_DMIX_Layer_Init(enum DRV_DMIX_LayerId_t Layer,
		enum DRV_DMIX_LayerMode_t LayerMode,
		enum DRV_DMIX_InputSel_t FG_Sel);
enum DRV_Status_t DRV_DMIX_Layer_Set(enum DRV_DMIX_LayerMode_t LayerMode,
		enum DRV_DMIX_InputSel_t FG_Sel);
void DRV_DMIX_Layer_Get(struct DRV_DMIX_Layer_Set_t *pLayerInfo);
enum DRV_Status_t DRV_DMIX_Plane_Alpha_Set(struct DRV_DMIX_PlaneAlpha_t *PlaneAlphaInfo);
void DRV_DMIX_PQ_OnOff(int OutId, int enable);
void DRV_DMIX_Luma_Adjust_Set(struct DRV_DMIX_Luma_Adj_t *LumaAdjInfo);
void DRV_DMIX_Luma_Adjust_Get(struct DRV_DMIX_Luma_Adj_t *LumaAdjInfo);
void DRV_DMIX_Chroma_Adjust_Set(struct DRV_DMIX_Chroma_Adj_t *ChromaAdjInfo);
void DRV_DMIX_Chroma_Adjust_Get(struct DRV_DMIX_Chroma_Adj_t *ChromaAdjInfo);

#endif	/* __DISP_DMIX_H__ */

