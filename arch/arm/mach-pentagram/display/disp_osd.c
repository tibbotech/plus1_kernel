/**************************************************************************
 *                                                                        *
 *         Copyright (c) 2018 by Sunplus Inc.                             *
 *                                                                        *
 *  This software is copyrighted by and is the property of Sunplus        *
 *  Inc. All rights are reserved by Sunplus Inc.                          *
 *  This software may only be used in accordance with the                 *
 *  corresponding license agreement. Any unauthorized use, duplication,   *
 *  distribution, or disclosure of this software is expressly forbidden.  *
 *                                                                        *
 *  This Copyright notice MUST not be removed or modified without prior   *
 *  written consent of Sunplus Technology Co., Ltd.                       *
 *                                                                        *
 *  Sunplus Inc. reserves the right to modify this software               *
 *  without notice.                                                       *
 *                                                                        *
 *  Sunplus Inc.                                                          *
 *  19, Innovation First Road, Hsinchu Science Park                       *
 *  Hsinchu City 30078, Taiwan, R.O.C.                                    *
 *                                                                        *
 **************************************************************************/
/**
 * @file disp_osd.c
 * @brief
 * @author PoChou Chen
 */
/**************************************************************************
 *                         H E A D E R   F I L E S
 **************************************************************************/
#include <asm/io.h>
#include <asm/cacheflush.h>
#include "reg_disp.h"
#include "hal_disp.h"

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/
// OSD Header config0
#define OSD_HDR_C0_CULT				0x80
#define OSD_HDR_C0_TRANS			0x40
	
// OSD Header config1
#define OSD_HDR_C1_BS				0x10
#define OSD_HDR_C1_BL2				0x04
#define OSD_HDR_C1_BL				0x01
	
// OSD control register
#define OSD_CTRL_COLOR_MODE_YUV		(0 << 10)
#define OSD_CTRL_COLOR_MODE_RGB		(1 << 10)
#define OSD_CTRL_NOT_FETCH_EN		(1 << 8)
#define OSD_CTRL_CLUT_FMT_GBRA		(0 << 7)
#define OSD_CTRL_CLUT_FMT_ARGB		(1 << 7)
#define OSD_CTRL_LATCH_EN			(1 << 5)
#define OSD_CTRL_A32B32_EN			(1 << 4)
#define OSD_CTRL_FIFO_DEPTH			(7 << 0)

// OSD region dirty flag for SW latch
#define REGION_ADDR_DIRTY			(1 << 0)
#define REGION_GSCL_DIRTY			(1 << 1)

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#ifdef DEBUG_MSG
	#define DEBUG(fmt, arg...) diag_printf("[%s:%d] "fmt, __FUNCTION__, __LINE__, ##arg)
	#define MSG(fmt, arg...) diag_printf("[%s:%d] "fmt, __FUNCTION__, __LINE__, ##arg)
#else
	#define DEBUG(fmt, arg...)
	#define MSG(fmt, arg...)
#endif
#define ERRDISP(fmt, arg...) diag_printf("[Disp][%s:%d] "fmt, __FUNCTION__, __LINE__, ##arg)
#define WARNING(fmt, arg...) diag_printf("[Disp][%s:%d] "fmt, __FUNCTION__, __LINE__, ##arg)
#define INFO(fmt, arg...) diag_printf("[Disp][%s:%d] "fmt, __FUNCTION__, __LINE__, ##arg)

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
typedef struct HW_OSD_Header_s   
{
	u8 config0;	//config0 includes:
	// [bit 7] cu	: color table update
	// [bit 6] ft	: force transparency
	// [bit 5:4]	: reserved
	// [bit 3:0] md : bitmap format (color mode)

	u8 reserved0; // reserved bits.

	u8 config1;	//config1 includes:
	// [bit 7:5]	: reserved
	// [bit 4] b_s	: byte swap enable
	// [bit 3] KY	: reserved
	// [bit 2] bl2	: region blend alpha enable (multiplier)
	// [bit 1]		: reserved
	// [bit 0] bl	: region blend alpha enable (replace)

	u8 blend_level;	//region blend level value

	u16 v_size;		//vertical display region size (line number)
	u16 h_size;		//horizontal display region size (pixel number)

	u16 disp_start_row;		//region vertical start row (0~(y-1))
	u16 disp_start_column;	//region horizontal start column (0~(x-1))

	u8 keying_R;
	u8 keying_G;
	u8 keying_B;
	u8 keying_A;

	u16 data_start_row;
	u16 data_start_column;

	u8 reserved2;
	u8 csc_mode_sel; //color space converter mode sel
	u16 data_width;	//source bitmap crop width

	u32 link_next;
	u32 link_data;

	u32 reserved3[24];	// need 128 bytes for HDR
} HW_OSD_Header_t;
STATIC_ASSERT(sizeof(HW_OSD_Header_t) == 128);

typedef struct _Region_Manager_t_
{
	DRV_Region_Info_t			RegionInfo;

	enum DRV_OsdRegionFormat_e	Format;
	u32							Align;
	u32							NumBuff;
	u32							DataPhyAddr;
	u8							*DataAddr;
	u8							*Hdr_ClutAddr;	//palette addr in osd header
	u32							BmpSize;
	u32							CurrBufID;

	// SW latch
	u32							DirtyFlag;
	u8							*PaletteAddr;	//other side palette addr, Gearing with swap buffer.

	HW_OSD_Header_t				*pHWRegionHdr;
	u32 reserved[4]; //For gsl allocate buffer case. The structure size should be 32 alignment.
} Region_Manager_t;
STATIC_ASSERT((sizeof(Region_Manager_t) % 4) == 0);

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static DISP_OSD_REG_t *pOSDReg;
static DISP_GPOST_REG_t *pGPOSTReg;

static Region_Manager_t *gpWinRegion;
static u8 *gpOsdHeader;

/**************************************************************************
 *             F U N C T I O N    I M P L E M E N T A T I O N S           *
 **************************************************************************/
void DRV_OSD_Init(void *pInHWReg1, void *pInHWReg2)
{
	pOSDReg = (DISP_OSD_REG_t *)pInHWReg1;
	pGPOSTReg = (DISP_GPOST_REG_t *)pInHWReg2;
}

DRV_Status_e DRV_OSD_SetClut(DRV_OsdRegionHandle_t region, UINT32 *pClutDataPtr)
{
	Region_Manager_t *pRegionManager = (Region_Manager_t *)region;
	UINT32 copysize = 0;

	if (pRegionManager && pClutDataPtr)
	{
		switch(pRegionManager->Format)
		{
			case DRV_OSD_REGION_FORMAT_8BPP:
				copysize = 256 * 4;
				break;
			default:
				goto Return;
		}
		memcpy(pRegionManager->Hdr_ClutAddr, pClutDataPtr, copysize);

		return DRV_SUCCESS;
	}

Return:
	ERRDISP("Incorrect region handle, pClutDataPtr 0x%x\n", (UINT32)pClutDataPtr);
	return DRV_ERR_INVALID_HANDLE;
}

void DRV_OSD_IRQ(void)
{
	Region_Manager_t *pRegionManager = gpWinRegion;
	HW_OSD_Header_t *pHWOSDhdr;

	if (!pRegionManager)
		return;

	if (pRegionManager->pHWRegionHdr)
	{
		pHWOSDhdr = (HW_OSD_Header_t *)pRegionManager->pHWRegionHdr;

		if (pRegionManager->DirtyFlag & REGION_ADDR_DIRTY)
		{
			pRegionManager->DirtyFlag &= ~REGION_ADDR_DIRTY;
			pHWOSDhdr->link_data = SWAP32((UINT32)((UINT32)pRegionManager->DataPhyAddr + pRegionManager->BmpSize * (pRegionManager->CurrBufID & 0xf)));
			if (pRegionManager->PaletteAddr)
				(void)DRV_OSD_SetClut((DRV_OsdRegionHandle_t)pRegionManager, (UINT32 *)pRegionManager->PaletteAddr);
			flush_cache_all();
		}
	}
}

void DRV_OSD_Info(void)
{
	HW_OSD_Header_t *pOsdHdr = (HW_OSD_Header_t *)gpOsdHeader;

	ERRDISP("Region display-order is as follows:\n");

	diag_printf("    Check osd output: %d %d, region ouput:%d %d\n",
		pOSDReg->osd_hvld_width,
		pOSDReg->osd_vvld_height,
		SWAP16(pOsdHdr->h_size),
		SWAP16(pOsdHdr->v_size));

	diag_printf("header: (x, y)=(%d, %d) (w, h)=(%d, %d) data(x, y)=(%d, %d) data width=%d\n",
				SWAP16(pOsdHdr->disp_start_column), SWAP16(pOsdHdr->disp_start_row),
				SWAP16(pOsdHdr->h_size), SWAP16(pOsdHdr->v_size),
				SWAP16(pOsdHdr->data_start_column), SWAP16(pOsdHdr->data_start_row), SWAP16(pOsdHdr->data_width));
	diag_printf("cu:%d ft:%d bit format:%d link data:0x%x\n\n", (pOsdHdr->config0 & 0x80)?1:0, (pOsdHdr->config0 & 0x40)?1:0, (pOsdHdr->config0 & 0xf), SWAP32(pOsdHdr->link_data));
}

void DRV_OSD_HDR_Show(void)
{
	int *ptr = (int *)gpOsdHeader;
	int i;

	for (i = 0; i < 8; ++i)
		diag_printf("%d: 0x%08x\n", i, *(ptr+i));
}

void DRV_OSD_HDR_Write(int offset, int value)
{
	int *ptr = (int *)gpOsdHeader;

	*(ptr+offset) = value;
}

void DRV_OSD_GetColormode_Vars(struct colormode_t *var, enum DRV_OsdRegionFormat_e Fmt)
{
	switch(Fmt)
	{
		case DRV_OSD_REGION_FORMAT_8BPP:
			strcpy(var->name, "256color index");
			var->red.length		= 8;
			var->green.length	= 8;
			var->blue.length	= 8;
			var->transp.length	= 8;
			var->red.offset		= 8;
			var->green.offset	= 16;
			var->blue.offset	= 24;
			var->transp.offset	= 0;
			var->bits_per_pixel = 8;
			break;
		case DRV_OSD_REGION_FORMAT_RGB_565:
			strcpy(var->name, "RGB565");
			var->red.length		= 5;
			var->green.length	= 6;
			var->blue.length	= 5;
			var->transp.length	= 0;
			var->red.offset		= 11;
			var->green.offset	= 5;
			var->blue.offset	= 0;
			var->transp.offset	= 0;
			var->bits_per_pixel = 16;
			break;
		case DRV_OSD_REGION_FORMAT_ARGB_1555:
			strcpy(var->name, "ARGB1555");
			var->red.length		= 5;
			var->green.length	= 5;
			var->blue.length	= 5;
			var->transp.length	= 1;
			var->red.offset		= 10;
			var->green.offset	= 5;
			var->blue.offset	= 0;
			var->transp.offset	= 15;
			var->bits_per_pixel = 16;
			break;
		case DRV_OSD_REGION_FORMAT_RGBA_4444:
			strcpy(var->name, "RGBA4444");
			var->red.length		= 4;
			var->green.length	= 4;
			var->blue.length	= 4;
			var->transp.length	= 4;
			var->red.offset		= 12;
			var->green.offset	= 8;
			var->blue.offset	= 4;
			var->transp.offset	= 0;
			var->bits_per_pixel = 16;
			break;
		case DRV_OSD_REGION_FORMAT_ARGB_4444:
			strcpy(var->name, "ARGB4444");
			var->red.length		= 4;
			var->green.length	= 4;
			var->blue.length	= 4;
			var->transp.length	= 4;
			var->red.offset		= 8;
			var->green.offset	= 4;
			var->blue.offset	= 0;
			var->transp.offset	= 12;
			var->bits_per_pixel = 16;
			break;
		case DRV_OSD_REGION_FORMAT_RGBA_8888:
			strcpy(var->name, "RGBA8888");
			var->red.length		= 8;
			var->green.length	= 8;
			var->blue.length	= 8;
			var->transp.length	= 8;
			var->red.offset		= 24;
			var->green.offset	= 16;
			var->blue.offset	= 8;
			var->transp.offset	= 0;
			var->bits_per_pixel = 32;
			break;
		default:
		case DRV_OSD_REGION_FORMAT_ARGB_8888:
			strcpy(var->name, "ARGB8888");
			var->red.length		= 8;
			var->green.length	= 8;
			var->blue.length	= 8;
			var->transp.length	= 8;
			var->red.offset		= 16;
			var->green.offset	= 8;
			var->blue.offset	= 0;
			var->transp.offset	= 24;
			var->bits_per_pixel = 32;
			break;
	}
}

EXPORT_SYMBOL(DRV_OSD_Get_UI_Res);
int DRV_OSD_Get_UI_Res(struct UI_FB_Info_t *pinfo)
{
	if (!pOSDReg || !pGPOSTReg)
		return 1;

	/* todo reference Output size */
	pinfo->UI_width = 720;
	pinfo->UI_height = 480;
	pinfo->UI_bufNum = 2;
	pinfo->UI_bufAlign = 4096;
	pinfo->UI_ColorFmt = DRV_OSD_REGION_FORMAT_ARGB_8888;

	DRV_OSD_GetColormode_Vars(&pinfo->UI_Colormode, pinfo->UI_ColorFmt);

	return 0;
}

EXPORT_SYMBOL(DRV_OSD_Set_UI_Init);
void DRV_OSD_Set_UI_Init(struct UI_FB_Info_t *pinfo)
{
	u32 *osd_header;

	if (pinfo->UI_ColorFmt == DRV_OSD_REGION_FORMAT_8BPP)
		gpOsdHeader = kzalloc(sizeof(HW_OSD_Header_t) + 1024, GFP_KERNEL);
	else
		gpOsdHeader = kzalloc(sizeof(HW_OSD_Header_t), GFP_KERNEL);

	if (!gpOsdHeader) {
		diag_printf("kmalloc osd header fail\n");
		return;
	}

	gpWinRegion = kzalloc(sizeof(Region_Manager_t), GFP_KERNEL);

	if (!gpWinRegion) {
		diag_printf("kmalloc region header fail\n");
		return;
	}

	//gpWinRegion->RegionInfo

	gpWinRegion->Format = pinfo->UI_ColorFmt;
	gpWinRegion->Align = pinfo->UI_bufAlign;
	gpWinRegion->NumBuff = pinfo->UI_bufNum;
	gpWinRegion->DataPhyAddr = pinfo->UI_bufAddr;
	gpWinRegion->DataAddr = (u8 *)pinfo->UI_bufAddr;
	gpWinRegion->Hdr_ClutAddr = gpOsdHeader + sizeof(HW_OSD_Header_t);
	gpWinRegion->BmpSize = EXTENDED_ALIGNED(pinfo->UI_height * pinfo->UI_width * (pinfo->UI_Colormode.bits_per_pixel>>3), pinfo->UI_bufAlign);
	gpWinRegion->PaletteAddr = (u8 *)pinfo->UI_bufAddr_pal;
	gpWinRegion->pHWRegionHdr = (HW_OSD_Header_t *)gpOsdHeader;

	diag_printf("osd_header=0x%x 0x%x addr=0x%x\n", (u32)gpOsdHeader, __pa(gpOsdHeader), pinfo->UI_bufAddr);

	osd_header = (u32 *)gpOsdHeader;

	if (pinfo->UI_ColorFmt == DRV_OSD_REGION_FORMAT_8BPP)
		osd_header[0] = SWAP32(0x82001000);
	else
		osd_header[0] = SWAP32(0x00001000 | (pinfo->UI_ColorFmt << 24));

	osd_header[1] = SWAP32((pinfo->UI_height<< 16) | pinfo->UI_width);
	osd_header[2] = 0;
	osd_header[3] = 0;
	osd_header[4] = 0;
	if (pinfo->UI_ColorFmt == DRV_OSD_REGION_FORMAT_YUY2)
		osd_header[5] = SWAP32(0x00040000 | pinfo->UI_width);
	else
		osd_header[5] = SWAP32(0x00010000 | pinfo->UI_width);
	osd_header[6] = SWAP32(0xFFFFFFE0);
	osd_header[7] = SWAP32(pinfo->UI_bufAddr);

	//OSD
	pOSDReg->osd_ctrl = OSD_CTRL_COLOR_MODE_RGB | OSD_CTRL_CLUT_FMT_ARGB | OSD_CTRL_LATCH_EN | OSD_CTRL_A32B32_EN | OSD_CTRL_FIFO_DEPTH;
	pOSDReg->osd_base_addr = __pa(gpOsdHeader);
	pOSDReg->osd_hvld_offset = 0;
	pOSDReg->osd_vvld_offset = 0;
	pOSDReg->osd_hvld_width = pinfo->UI_width;
	pOSDReg->osd_vvld_height = pinfo->UI_height;
	pOSDReg->osd_bist_ctrl = 0x0;
	pOSDReg->osd_3d_h_offset = 0x0;
	pOSDReg->osd_src_decimation_sel = 0x0;

	pOSDReg->osd_en = 1;

	//GPOST bypass
	pGPOSTReg->gpost0_config = 0;
	pGPOSTReg->gpost0_master_en = 0;
	pGPOSTReg->gpost0_bg1 = 0x8010;
	pGPOSTReg->gpost0_bg2 = 0x0080;

	//GPOST PQ disable
	pGPOSTReg->gpost0_contrast_config = 0x0;
}

#if 0
UINT32 DRV_OSD_QueryUpdating(u32 region_handle)
{
	Region_Manager_t *pRegionManager = (Region_Manager_t *)region_handle;
	enum DRV_OsdWindow_e win_id;
	int IsUpdated = 0;

	if (!pRegionManager)
		return 0;

	for (win_id = DRV_OSD0; win_id < DRV_OSD_MAX; ++win_id)
	{
		Region_Manager_t *pFindRegion = gWinInfo[win_id].pWinRegion;

		while(pFindRegion)
		{
			if (pRegionManager->DataPhyAddr == pFindRegion->DataPhyAddr)
			{
				if (pFindRegion->DirtyFlag & REGION_ADDR_DIRTY)
				{
					IsUpdated |= (1 << win_id);
					break;
				}
			}
			pFindRegion = pFindRegion->pNextRegion;
		}
	}

	return IsUpdated;
}
#endif

#if 0
EXPORT_SYMBOL(DRV_OSD_WaitVSync);
UINT32 DRV_OSD_WaitVSync(u32 region_handle)
{
	Region_Manager_t *pRegionManager = (Region_Manager_t *)region_handle;
	enum DRV_OsdWindow_e win_id;
	int IsUpdated = 0;

	if (!pRegionManager)
		return 0;

	for (win_id = DRV_OSD0; win_id < DRV_OSD_MAX; ++win_id)
	{
		Region_Manager_t *pFindRegion = gWinInfo[win_id].pWinRegion;

		while(pFindRegion)
		{
			if (pRegionManager->DataPhyAddr == pFindRegion->DataPhyAddr)
			{
				if (pFindRegion->DirtyFlag & REGION_ADDR_DIRTY)
				{
					IsUpdated |= (1 << win_id);
					break;
				}
			}
			pFindRegion = pFindRegion->pNextRegion;
		}
	}

	return IsUpdated;
}
#endif

u32 DRV_OSD_SetVisibleBuffer(u32 bBufferId)
{
	Region_Manager_t *pRegionManager = gpWinRegion;

	if (!pRegionManager) {
		//ERRDISP("Invalid handle\n");
		return -1;
	}

	//DEBUG("bBufferId %d\n", bBufferId);

	pRegionManager->DirtyFlag |= REGION_ADDR_DIRTY;
	pRegionManager->CurrBufID = bBufferId;

	return 0;
}

