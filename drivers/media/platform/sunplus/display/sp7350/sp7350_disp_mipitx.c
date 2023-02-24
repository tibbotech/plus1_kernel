// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for MIPITX block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

static const char * const mipitx_dsi_fmt[] = {
	"DSI-RGB565-16b", "DSI-RGB666-18b", "DSI-RGB666-24b", "DSI-RGB888-24b"};
static const char * const mipitx_csi_fmt[] = {
	"CSI-YUY2", "CSI-YUY2-10", "CSI-RBG565", "CSI-RBG888"};
static const char * const mipitx_video_fmt[] = {
	"non-burst sync pulse", "non-burst sync event", "none", "none"};
static const char * const mipitx_video_pkt[] = {
	"DSI-RGB565/CSI-16bits", "DSI-RGB666-18bits", "DSI-RGB666-24bits",
	"DSI-RGB888/CSI-24bits", "CSI-YUV422-20bits", "none", "none", "none"};

/*
 * sp_mipitx_input_timing[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-9, MIPITX HSA & HFP & HBP & HACT & VSA & VFP & VBP & VACT
 */
static const u32 sp_mipitx_input_timing[11][10] = {
	/* (w   h)   HSA  HFP HBP HACT VSA  VFP  VBP   VACT */
	{ 720,  480, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  480}, /* 480P */
	{ 720,  576, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x2B,  576}, /* 576P */
	{1280,  720, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x18,  720}, /* 720P */
	{1920, 1080, 0x4,  0, 0x5,  0, 0x1, 0x3, 0x28, 1080}, /* 1080P */
	{  64,   64, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x1c,   64}, /* 64x64 */
	//{  64,   64, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x11,   64}, /* 64x64 ZEBU */
	{ 128,  128, 0x4,  0, 0x5,  0, 0x1, 0x2, 0x12,  128}, /* 128x128 */
	{  64, 2880, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x28, 2880}, /* 64x2880 */
	{3840,   64, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x1c,   64}, /* 3840x64 */
	{3840, 2880, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x26, 2880}, /* 3840x2880 */
	{ 800,  480, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  480}, /* 800x480 */
	{1024,  600, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  600}  /* 1024x600 */
};

/*
 * sp_mipitx_output_timing[x]
 * x = 0-10, MIPITX phy
 *   T_HS-EXIT / T_LPX
 *   T_CLK-PREPARE / T_CLK-ZERO
 *   T_CLK-TRAIL / T_CLK-PRE / T_CLK-POST
 *   T_HS-TRAIL / T_HS-PREPARE / T_HS-ZERO
 *   ULPS_DELAY
 */
static const u32 sp_mipitx_output_timing[11] = {
	0x10,  /* T_HS-EXIT */
	0x08,  /* T_LPX */
	0x10,  /* T_CLK-PREPARE */
	0x10,  /* T_CLK-ZERO */
	0x05,  /* T_CLK-TRAIL */
	0x12,  /* T_CLK-PRE */
	0x20,  /* T_CLK-POST */
	0x05,  /* T_HS-TRAIL */
	0x05,  /* T_HS-PREPARE */
	0x10,  /* T_HS-ZERO */
	0xaf,  /* ULPS_DELAY */
};

/*
 * sp_mipitx_phy_pllclk_norm[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-6, MIPITX PRESCALE & FBKDIV & PREDIV & POSTDIV & EN_DIV5
 * y = 7-12, MIPITX ICH & RS & CS & CP & VCO_G & BNKSEL
 *
 * XTAL--[PREDIV]------->[PFD/CP/LF/VCO]------>[EN_DIV5]--[POSTDIV]-->Fckout
 *                 |                       |
 *                 |<--FBKDIV<--PRESCALE<--|
 *
 *                25 * PRESCALE * FBKDIV
 *    Fckout = -----------------------------
 *              PREDIV * POSTDIV * 5^EN_DIV5
 */
static const u32 sp_mipitx_phy_pllclk_norm_16b[11][13] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   P7   P8   P9   P10  P11 */
	{ 720,  480, 0x1, 0x16, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 480P */
	{ 720,  576, 0x1, 0x16, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 576P */
	{1280,  720, 0x0, 0x1e, 0x0, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x1}, /* 16bit 720P */
	{1920, 1080, 0x1, 0x18, 0x1, 0x1, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 1080P */
	{  64,   64, 0x0, 0x1c, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 64x64 */
	{ 128,  128, 0x1, 0x15, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 128x128 */
	{  64, 2880, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 64x2880 */
	{3840,   64, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 3840x64 */
	{3840, 2880, 0x0, 0x3c, 0x0, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}, /* 16bit 3840x2880 */
	{ 800,  480, 0x1, 0x18, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 16bit 800x480 */
	{1024,  600, 0x1, 0x29, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x2}  /* 16bit 1024x600 */
};
static const u32 sp_mipitx_phy_pllclk_norm_18b[11][13] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   P7   P8   P9   P10  P11 */
	{ 720,  480, 0x0, 0x18, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 480P */
	{ 720,  576, 0x0, 0x18, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 576P */
	{1280,  720, 0x0, 0x21, 0x0, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x1}, /* 18bit 720P */
	{1920, 1080, 0x1, 0x0d, 0x1, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 1080P */
	{  64,   64, 0x0, 0x1f, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 64x64 */
	{ 128,  128, 0x1, 0x17, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 128x128 */
	{  64, 2880, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 64x2880 */
	{3840,   64, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 3840x64 */
	{3840, 2880, 0x0, 0x3c, 0x0, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}, /* 18bit 3840x2880 */
	{ 800,  480, 0x0, 0x1a, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 18bit 800x480 */
	{1024,  600, 0x1, 0x2e, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x2}  /* 18bit 1024x600 */
};
static const u32 sp_mipitx_phy_pllclk_norm_20b[11][13] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   P7   P8   P9   P10  P11 */
	{ 720,  480, 0x0, 0x1b, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 480P */
	{ 720,  576, 0x0, 0x1b, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 576P */
	{1280,  720, 0x0, 0x25, 0x0, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x1}, /* 20bit 720P */
	{1920, 1080, 0x1, 0x0f, 0x1, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 1080P */
	{  64,   64, 0x0, 0x22, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 64x64 */
	{ 128,  128, 0x1, 0x1a, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x1}, /* 20bit 128x128 */
	{  64, 2880, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 64x2880 */
	{3840,   64, 0x0, 0x30, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 3840x64 */
	{3840, 2880, 0x0, 0x3c, 0x0, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}, /* 20bit 3840x2880 */
	{ 800,  480, 0x0, 0x1d, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 20bit 800x480 */
	{1024,  600, 0x1, 0x33, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}  /* 20bit 1024x600 */
};
static const u32 sp_mipitx_phy_pllclk_norm_24b[11][13] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   P7   P8   P9   P10  P11 */
	{ 720,  480, 0x0, 0x20, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 480P */
	{ 720,  576, 0x0, 0x20, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 576P */
	{1280,  720, 0x0, 0x2c, 0x0, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x2}, /* 24bit 720P */
	{1920, 1080, 0x1, 0x12, 0x1, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 1080P */
	{  64,   64, 0x0, 0x29, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 64x64 */
	{ 128,  128, 0x1, 0x1f, 0x1, 0x4, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x1}, /* 24bit 128x128 */
	{  64, 2880, 0x0, 0x29, 0x0, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x2}, /* 24bit 64x2880 */
	{3840,   64, 0x0, 0x1f, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 3840x64 */
	{3840, 2880, 0x0, 0x3c, 0x0, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}, /* 24bit 3840x2880 */
	{ 800,  480, 0x0, 0x23, 0x1, 0x0, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 24bit 800x480 */
	{1024,  600, 0x1, 0x3d, 0x1, 0x1, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3}  /* 24bit 1024x600 */
};

/*
 * sp_mipitx_phy_pllclk_ssc[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-6, MIPITX PRESCALE & (NS) & PREDIV & POSTDIV & EN_DIV5
 * y = 7-12, MIPITX ICH & RS & CS & CP & VCO_G & BNKSEL
 * y = 13-15, MIPITX MDS (CLK & MUL & RATIO)
 *
 * XTAL--[PREDIV]------->[PFD/CP/LF/VCO]------>[EN_DIV5]--[POSTDIV]-->Fckout
 *                 |                       |
 *                 |<--NS(MDS)<--PRESCALE<-|
 *
 *                25 * PRESCALE * NS
 *    Fckout = -----------------------------
 *              PREDIV * POSTDIV * 5^EN_DIV5
 */
static const u32 sp_mipitx_phy_pllclk_ssc[11][16] = {
	/* (w   h)   P1   Q2   P3   P4   P5   P6   P7   P8   P9   P10  P11  Q12  Q13  Q14 */
	{ 720,  480, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 480P */
	{ 720,  576, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 576P */
	{1280,  720, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 720P */
	{1920, 1080, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 1080P */
	{  64,   64, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 64x64 */
	{ 128,  128, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 128x128 */
	{  64, 2880, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 64x2880 */
	{3840,   64, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 3840x64 */
	{3840, 2880, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 3840x2880 */
	{ 800,  480, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}, /* 800x480 */
	{1024,  600, 0x0, 0x0, 0x3, 0x0, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0, 0x3, 0x3, 0x0, 0x1}  /* 1024x600 */
};

/*
 * sp_mipitx_phy_para[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-3, MIPITX REG_VSET & TX_LP_SR
 * y = 4-9, MIPITX GPO_DS & LPFS & REG_04_CSEL & REG_VSET_1P2 & BGS & CLK_EDGE
 * y = 10-12, MIPITX EN500P & EN250P & PHASE_DELAY
 */
static const u32 sp_mipitx_phy_para[11][13] = {
	/* (w   h)   P1   P2   P3   P4   P5   P6   P7   P8   P9   P10  P11 */
	{ 720,  480, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 480P */
	{ 720,  576, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 576P */
	{1280,  720, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 720P */
	{1920, 1080, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 1080P */
	{  64,   64, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 64x64 */
	{ 128,  128, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 128x128 */
	{  64, 2880, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 64x2880 */
	{3840,   64, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 3840x64 */
	{3840, 2880, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 3840x2880 */
	{ 800,  480, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}, /* 800x480 */
	{1024,  600, 0x3, 0x5, 0x1, 0x0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0}  /* 1024x600 */
};

void sp7350_mipitx_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	//u32 mipitx_width, mipitx_height;
	u32 value = 0;

	//mipitx_width = 720; //64 128 720 1280 1920 3840
	//mipitx_height = 480; //64 128 480 576 720 1080 2880
	//mipitx_lane = 4; //1 2 4
	//mipitx_data_bit = 24; //16 18 20 24
	//mipitx_csi_mode = 0; //0:output MIPI DSI TX , 1: output MIPI CSI TX

	writel(0x00000000, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Reset

	/*
	 * MIPITX Input Timing Setting
	 */
	if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_HSA_SET(0x6) |
			SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing[0][3]) |
			SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing[0][4]);
	else
		value |= SP7350_MIPITX_HSA_SET(sp_mipitx_input_timing[0][2]) |
			SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing[0][3]) |
			SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing[0][4]);
	writel(value, disp_dev->base + MIPITX_VM_HT_CTRL);

	value = 0;
	value |= SP7350_MIPITX_VSA_SET(sp_mipitx_input_timing[0][6]) |
		SP7350_MIPITX_VFP_SET(sp_mipitx_input_timing[0][7]) |
		SP7350_MIPITX_VBP_SET(sp_mipitx_input_timing[0][8]);
	writel(value, disp_dev->base + MIPITX_VM_VT0_CTRL);

	value = 0;
	value |= SP7350_MIPITX_VACT_SET(sp_mipitx_input_timing[0][9]);
	writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL);

	/*
	 * MIPITX PHY Timing Setting
	 */
	value = 0;
	value |= SP7350_MIPITX_T_HS_EXIT_SET(sp_mipitx_output_timing[0]) |
		SP7350_MIPITX_T_LPX_SET(sp_mipitx_output_timing[1]);
	writel(value, disp_dev->base + MIPITX_LANE_TIME_CTRL);

	value = 0;
	value |= SP7350_MIPITX_T_CLK_PREPARE_SET(sp_mipitx_output_timing[2]) |
		SP7350_MIPITX_T_CLK_ZERO_SET(sp_mipitx_output_timing[3]);
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL0);

	value = 0;
	value |= SP7350_MIPITX_T_CLK_TRAIL_SET(sp_mipitx_output_timing[4]) |
		SP7350_MIPITX_T_CLK_PRE_SET(sp_mipitx_output_timing[5]) |
		SP7350_MIPITX_T_CLK_POST_SET(sp_mipitx_output_timing[6]);
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL1);

	value = 0;
	value |= SP7350_MIPITX_T_HS_TRAIL_SET(sp_mipitx_output_timing[7]) |
		SP7350_MIPITX_T_HS_PREPARE_SET(sp_mipitx_output_timing[8]) |
		SP7350_MIPITX_T_HS_ZERO_SET(sp_mipitx_output_timing[9]);
	writel(value, disp_dev->base + MIPITX_DATA_TIME_CTRL0);

	value = 0;
	value |= SP7350_MIPITX_ULPS_DELAY_SET(sp_mipitx_output_timing[10]);
	writel(value, disp_dev->base + MIPITX_ULPS_DELAY);

	value = 0;
	value |= SP7350_MIPITX_FORMAT_VTF_SET(SP7350_MIPITX_VTF_SYNC_EVENT);
	value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB888);
	writel(value, disp_dev->base + MIPITX_FORMAT_CTRL);

	value = 0;
	value |= SP7350_MIPITX_BLANK_POWER_HSA |
		SP7350_MIPITX_BLANK_POWER_HBP;
	writel(value, disp_dev->base + MIPITX_BLANK_POWER_CTRL);

	value = 0;
	value |= SP7350_MIPITX_OP_CTRL_TXLDPT;
	writel(value, disp_dev->base + MIPITX_OP_CTRL);

	value = 0;
	if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_1_LANE);
	else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_2_LANE);
	else
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_4_LANE);
	value |= SP7350_MIPITX_CORE_CTRL_INPUT_EN |
		SP7350_MIPITX_CORE_CTRL_ANALOG_EN |
		SP7350_MIPITX_CORE_CTRL_DSI_EN;
	writel(value, disp_dev->base + MIPITX_CORE_CTRL);

	value = 0;
	value |= SP7350_MIPITX_PIXEL_CNT_SET(disp_dev->out_res.width) |
		SP7350_MIPITX_WORD_CNT_SET((disp_dev->out_res.width * disp_dev->mipitx_data_bit / 8));
	writel(value, disp_dev->base + MIPITX_WORD_CNT);

	value = 0;
	value |= SP7350_MIPITX_CLK_CTRL_CKHS_EN; /* CLK lane high speed enable */
	writel(value, disp_dev->base + MIPITX_CLK_CTRL);

	value = 0;
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {
		#ifdef SP7350_ZEBU_TEST
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422);
		#else
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);
		#endif
		value |= SP7350_MIPITX_MIPI_CSI_FE_SET(sp_mipitx_input_timing[0][7]-1) |
			SP7350_MIPITX_MIPI_CSI_MODE_EN;
	}
	writel(value, disp_dev->base + MIPITX_CTRL);

	writel(0x00000001, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Reset (return to Normal Mode)


}

void sp7350_mipitx_reg_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, j;

	for (j = 0; j < 2; j++) {
		pr_info("MIPITX G%d Dump info\n", j+204);
		for (i = 0; i < 8; i++) {
			pr_info("0x%08x 0x%08x 0x%08x 0x%08x\n",
					readl(disp_dev->base + (j << 7) + MIPITX_VM_HT_CTRL + (i * 4 + 0) * 4),
					readl(disp_dev->base + (j << 7) + MIPITX_VM_HT_CTRL + (i * 4 + 1) * 4),
					readl(disp_dev->base + (j << 7) + MIPITX_VM_HT_CTRL + (i * 4 + 2) * 4),
					readl(disp_dev->base + (j << 7) + MIPITX_VM_HT_CTRL + (i * 4 + 3) * 4));
		}
	}
}
EXPORT_SYMBOL(sp7350_mipitx_reg_info);

void sp7350_mipitx_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, mipitx_csi_dt, mipitx_csi_fe;
	u32 mipitx_mode, mipitx_mode1, mipitx_analog_en;

	pr_info("G204 G205 MIPITX Info\n");
	value = readl(disp_dev->base + MIPITX_CORE_CTRL); //G204.15
	mipitx_analog_en = FIELD_GET(GENMASK(24,24), value);
	pr_info("  CORE (INPUT[%s] MIPITX Module[%s])\n",
		FIELD_GET(GENMASK(28,28), value)?"En ":"Dis",
		FIELD_GET(GENMASK(0,0), value)?"En ":"Dis");

	pr_info("  CORE LANE_NUM=%02ld\n",
		FIELD_GET(GENMASK(5,4), value)+1);

	value = readl(disp_dev->base + MIPITX_CTRL); //G205.08
	mipitx_mode = FIELD_GET(GENMASK(0,0), value); //check G205.08[0] MIPITX CSI sw
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
	mipitx_mode1 = FIELD_GET(GENMASK(0,0), value); //check G205.06[0] MIPITX_RESET

	pr_info("  PHY [%s][%s] with [%s] Mode\n",
		mipitx_analog_en?"En ":"Dis",
		mipitx_mode1?"Normal Mode":"Under RESET",
		mipitx_mode?"CSI":"DSI");

	mipitx_csi_dt = FIELD_GET(GENMASK(9,4), value); //check G205.08[9:4] MIPITX_CSI_DT
	mipitx_csi_fe = FIELD_GET(GENMASK(19,12), value); //check G205.08[19:12] MIPITX_CSI_FE
	pr_info("  Param FE=%04d(CSI) DT=%04d(CSI)\n", mipitx_csi_fe, mipitx_csi_dt);

	value = readl(disp_dev->base + MIPITX_VM_HT_CTRL); //G204.00
	pr_info("  Param (HSA/HFP/HBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,24), value),
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(11,0), value));
	
	value = readl(disp_dev->base + MIPITX_VM_VT0_CTRL); //G204.01
	pr_info("  Param (VSA/VFP/VBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(15,8), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_VM_VT1_CTRL); //G204.02
	pr_info("  Param VACT= %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_FORMAT_CTRL); //G204.12
	pr_info("  video fmt VTF(tim)=%s\n", 
		mipitx_video_fmt[FIELD_GET(GENMASK(13,12), value)]);
	pr_info("  video fmt VPF(pkt)=%s\n", 
		mipitx_video_pkt[FIELD_GET(GENMASK(6,4), value)]);

	value = readl(disp_dev->base + MIPITX_WORD_CNT); //G204.19
	pr_info("  data PIXEL_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(31,16), value), FIELD_GET(GENMASK(31,16), value));
	pr_info("  data  WORD_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_LANE_TIME_CTRL); //G204.05
	pr_info("  tim T_HS-EXIT=(%04ld) T_LPX=(%04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL0); //G204.06
	pr_info("  tim T_CLK-(PREPARE/ZERO)=(%04ld %04ld)\n",
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL1); //G204.07
	pr_info("  tim T_CLK-(TRAIL/PRE/POST)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_DATA_TIME_CTRL0); //G204.08
	pr_info("  tim T_HS-(TRAIL/PREPARE/ZERO)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
	pr_info("  ULPS wake up delay=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_BLANK_POWER_CTRL); //G204.13
	pr_info("  blank (HSA/HBP/HFP/BLLP)_BM=[%s %s %s %s]spd\n",
		FIELD_GET(GENMASK(12,12), value)?"H":"L",
		FIELD_GET(GENMASK(8,8), value)?"H":"L",
		FIELD_GET(GENMASK(4,4), value)?"H":"L",
		FIELD_GET(GENMASK(0,0), value)?"H":"L");

	value = readl(disp_dev->base + MIPITX_OP_CTRL); //G204.14
	pr_info("  [%s]cmd trans at LP mode\n",
		FIELD_GET(GENMASK(20,20), value)?"En ":"Dis");

	value = readl(disp_dev->base + MIPITX_CLK_CTRL); //G204.30
	pr_info("  [%s]clk lane H spd mode\n",
		FIELD_GET(GENMASK(0,0), value)?"En ":"Dis");

}
EXPORT_SYMBOL(sp7350_mipitx_decrypt_info);

void sp7350_mipitx_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, mipitx_csi_dt, mipitx_csi_fe;
	u32 mipitx_mode, mipitx_mode1, mipitx_analog_en;

	pr_info("MIPITX resolution chk\n");
	value = readl(disp_dev->base + MIPITX_CORE_CTRL); //G204.15
	mipitx_analog_en = FIELD_GET(GENMASK(24,24), value);
	pr_info("  CORE (INPUT[%s] MIPITX Module[%s])\n",
		FIELD_GET(GENMASK(28,28), value)?"En ":"Dis",
		FIELD_GET(GENMASK(0,0), value)?"En ":"Dis");

	pr_info("  CORE LANE_NUM=%02ld\n",
		FIELD_GET(GENMASK(5,4), value)+1);

	value = readl(disp_dev->base + MIPITX_CTRL); //G205.08
	mipitx_mode = FIELD_GET(GENMASK(0,0), value); //check G205.08[0] MIPITX CSI sw
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
	mipitx_mode1 = FIELD_GET(GENMASK(0,0), value); //check G205.06[0] MIPITX_RESET

	pr_info("  PHY [%s][%s] with [%s] Mode\n",
		mipitx_analog_en?"En ":"Dis",
		mipitx_mode1?"Normal Mode":"Under RESET",
		mipitx_mode?"CSI":"DSI");

	mipitx_csi_dt = FIELD_GET(GENMASK(9,4), value); //check G205.08[9:4] MIPITX_CSI_DT
	mipitx_csi_fe = FIELD_GET(GENMASK(19,12), value); //check G205.08[19:12] MIPITX_CSI_FE
	pr_info("  Param FE=%04d(CSI) DT=%04d(CSI)\n", mipitx_csi_fe, mipitx_csi_dt);

	value = readl(disp_dev->base + MIPITX_VM_HT_CTRL); //G204.00
	pr_info("  Param (HSA/HFP/HBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,24), value),
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(11,0), value));
	
	value = readl(disp_dev->base + MIPITX_VM_VT0_CTRL); //G204.01
	pr_info("  Param (VSA/VFP/VBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(15,8), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_VM_VT1_CTRL); //G204.02
	pr_info("  Param VACT= %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_FORMAT_CTRL); //G204.12
	pr_info("  video fmt VTF(tim)=%s\n", 
		mipitx_video_fmt[FIELD_GET(GENMASK(13,12), value)]);
	pr_info("  video fmt VPF(pkt)=%s\n", 
		mipitx_video_pkt[FIELD_GET(GENMASK(6,4), value)]);

	value = readl(disp_dev->base + MIPITX_WORD_CNT); //G204.19
	pr_info("  data PIXEL_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(31,16), value), FIELD_GET(GENMASK(31,16), value));
	pr_info("  data  WORD_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_LANE_TIME_CTRL); //G204.05
	pr_info("  tim T_HS-EXIT=(%04ld) T_LPX=(%04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL0); //G204.06
	pr_info("  tim T_CLK-(PREPARE/ZERO)=(%04ld %04ld)\n",
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL1); //G204.07
	pr_info("  tim T_CLK-(TRAIL/PRE/POST)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_DATA_TIME_CTRL0); //G204.08
	pr_info("  tim T_HS-(TRAIL/PREPARE/ZERO)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
	pr_info("  ULPS wake up delay=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

}
EXPORT_SYMBOL(sp7350_mipitx_resolution_chk);

void sp7350_mipitx_timing_set(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value = 0;

	for (i = 0; i < 11; i++) {
		if ((sp_mipitx_input_timing[i][0] == disp_dev->out_res.width) &&
			(sp_mipitx_input_timing[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	writel(0x00000000, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Reset

	/*
	 * MIPITX Timing Setting
	 */
	value = 0;
	if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_HSA_SET(0x6) |
			SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing[time_cnt][3]) |
			SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing[time_cnt][4]);
	else
		value |= SP7350_MIPITX_HSA_SET(sp_mipitx_input_timing[time_cnt][2]) |
			SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing[time_cnt][3]) |
			SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing[time_cnt][4]);
	writel(value, disp_dev->base + MIPITX_VM_HT_CTRL);

	value = 0;
	value |= SP7350_MIPITX_VSA_SET(sp_mipitx_input_timing[time_cnt][6]) |
		SP7350_MIPITX_VFP_SET(sp_mipitx_input_timing[time_cnt][7]) |
		SP7350_MIPITX_VBP_SET(sp_mipitx_input_timing[time_cnt][8]);
	writel(value, disp_dev->base + MIPITX_VM_VT0_CTRL);

	value = 0;
	value |= SP7350_MIPITX_VACT_SET(sp_mipitx_input_timing[time_cnt][9]);
	writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL);

	/*
	 * MIPITX Packet Format Setting
	 */
	value = readl(disp_dev->base + MIPITX_FORMAT_CTRL);
	value &= ~SP7350_MIPITX_FORMAT_VPF_MASK;
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {
		if (disp_dev->mipitx_format == 0)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_16BITS);
		else if (disp_dev->mipitx_format == 4)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_YUV422_20BITS);
		else
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_24BITS);
	} else {
		if (disp_dev->mipitx_format == 0)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB565);
		else if (disp_dev->mipitx_format == 1)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB666_18BITS);
		else if (disp_dev->mipitx_format == 2)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB666_24BITS);
		else
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB888);
	}
	writel(value, disp_dev->base + MIPITX_FORMAT_CTRL);

	value = readl(disp_dev->base + MIPITX_CORE_CTRL);
	value &= ~SP7350_MIPITX_CORE_CTRL_LANE_NUM_MASK;
	if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_1_LANE);
	else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_2_LANE);
	else
		value |= SP7350_MIPITX_CORE_CTRL_LANE_NUM_SET(SP7350_MIPITX_4_LANE);
	writel(value, disp_dev->base + MIPITX_CORE_CTRL);

	value = readl(disp_dev->base + MIPITX_WORD_CNT);
	value &= ~(SP7350_MIPITX_PIXEL_CNT_MASK | SP7350_MIPITX_WORD_CNT_MASK);
	value |= SP7350_MIPITX_PIXEL_CNT_SET(disp_dev->out_res.width) |
		SP7350_MIPITX_WORD_CNT_SET((disp_dev->out_res.width * disp_dev->mipitx_data_bit / 8));
	writel(value, disp_dev->base + MIPITX_WORD_CNT);

	value = readl(disp_dev->base + MIPITX_CTRL);
	value &= ~(SP7350_MIPITX_MIPI_CSI_FE_MASK | SP7350_MIPITX_MIPI_CSI_DT_MASK);
	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {
		#ifdef SP7350_ZEBU_TEST
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422);
		#else
		if (disp_dev->mipitx_format == 4)
			value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422);
		else if (disp_dev->mipitx_format == 0)
			value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB565);
		else if (disp_dev->mipitx_format == 1)
			value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB666);
		else /*if (disp_dev->mipitx_format == 3)*/
			value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);
		#endif
		value |= SP7350_MIPITX_MIPI_CSI_FE_SET(sp_mipitx_input_timing[time_cnt][7]-1);
	}
	writel(value, disp_dev->base + MIPITX_CTRL);

	writel(0x00000001, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Normal
}

void sp7350_mipitx_timing_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, mipitx_csi_dt, mipitx_csi_fe;
	u32 mipitx_mode, mipitx_mode1, mipitx_analog_en;

	value = readl(disp_dev->base + MIPITX_CORE_CTRL); //G204.15
	mipitx_analog_en = FIELD_GET(GENMASK(24,24), value);
	pr_info("  CORE (INPUT[%s] MIPITX Module[%s])\n",
		FIELD_GET(GENMASK(28,28), value)?"En ":"Dis",
		FIELD_GET(GENMASK(0,0), value)?"En ":"Dis");

	pr_info("  CORE LANE_NUM=%02ld\n",
		FIELD_GET(GENMASK(5,4), value)+1);

	value = readl(disp_dev->base + MIPITX_CTRL); //G205.08
	mipitx_mode = FIELD_GET(GENMASK(0,0), value); //check G205.08[0] MIPITX CSI sw
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
	mipitx_mode1 = FIELD_GET(GENMASK(0,0), value); //check G205.06[0] MIPITX_RESET

	pr_info("  PHY [%s][%s] with [%s] Mode\n",
		mipitx_analog_en?"En ":"Dis",
		mipitx_mode1?"Normal Mode":"Under RESET",
		mipitx_mode?"CSI":"DSI");

	mipitx_csi_dt = FIELD_GET(GENMASK(9,4), value); //check G205.08[9:4] MIPITX_CSI_DT
	mipitx_csi_fe = FIELD_GET(GENMASK(19,12), value); //check G205.08[19:12] MIPITX_CSI_FE
	pr_info("  Param FE=%04d(CSI) DT=%04d(CSI)\n", mipitx_csi_fe, mipitx_csi_dt);

	value = readl(disp_dev->base + MIPITX_VM_HT_CTRL); //G204.00
	pr_info("  Param (HSA/HFP/HBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,24), value),
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(11,0), value));
	
	value = readl(disp_dev->base + MIPITX_VM_VT0_CTRL); //G204.01
	pr_info("  Param (VSA/VFP/VBP)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(15,8), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_VM_VT1_CTRL); //G204.02
	pr_info("  Param VACT= %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_FORMAT_CTRL); //G204.12
	pr_info("  video fmt VTF(tim)=%s\n", 
		mipitx_video_fmt[FIELD_GET(GENMASK(13,12), value)]);
	pr_info("  video fmt VPF(pkt)=%s\n", 
		mipitx_video_pkt[FIELD_GET(GENMASK(6,4), value)]);

	value = readl(disp_dev->base + MIPITX_WORD_CNT); //G204.19
	pr_info("  data PIXEL_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(31,16), value), FIELD_GET(GENMASK(31,16), value));
	pr_info("  data  WORD_CNT=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

	value = readl(disp_dev->base + MIPITX_LANE_TIME_CTRL); //G204.05
	pr_info("  tim T_HS-EXIT=(%04ld) T_LPX=(%04ld)\n",
		FIELD_GET(GENMASK(23,16), value),
		FIELD_GET(GENMASK(7,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL0); //G204.06
	pr_info("  tim T_CLK-(PREPARE/ZERO)=(%04ld %04ld)\n",
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_CLK_TIME_CTRL1); //G204.07
	pr_info("  tim T_CLK-(TRAIL/PRE/POST)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_DATA_TIME_CTRL0); //G204.08
	pr_info("  tim T_HS-(TRAIL/PREPARE/ZERO)=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,25), value),
		FIELD_GET(GENMASK(24,16), value),
		FIELD_GET(GENMASK(9,0), value));

	value = readl(disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
	pr_info("  ULPS wake up delay=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));
}

void sp7350_mipitx_phy_set(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value = 0;

	for (i = 0; i < 11; i++) {
		if ((sp_mipitx_phy_para[i][0] == disp_dev->out_res.width) &&
			(sp_mipitx_phy_para[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL1); //G205.04
	value &= ~(SP7350_MIPITX_MIPI_PHY_REG_VSET_MASK |
		SP7350_MIPITX_MIPI_PHY_TX_LP_SR_MASK);
	value |= SP7350_MIPITX_MIPI_PHY_REG_VSET(sp_mipitx_phy_para[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_TX_LP_SR(sp_mipitx_phy_para[time_cnt][3]);
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL1); //G205.04

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
	value &= ~(SP7350_MIPITX_MIPI_PHY_GPO_DS_MASK |
		SP7350_MIPITX_MIPI_PHY_LP_RX_LPF_MASK |
		SP7350_MIPITX_MIPI_PHY_REG04_CSEL_MASK |
		SP7350_MIPITX_MIPI_PHY_VCO_G_MASK |
		SP7350_MIPITX_MIPI_PHY_REG_VSET_1P2_MASK |
		SP7350_MIPITX_MIPI_PHY_BANDGAP_MASK |
		SP7350_MIPITX_MIPI_PHY_CLK_EDGE_SEL_MASK);
	#ifdef SP7350_MIPITX_PHY_SSC_MODE
	value |= SP7350_MIPITX_MIPI_PHY_VCO_G(sp_mipitx_phy_pllclk_ssc[time_cnt][11]);
	#else
	if (disp_dev->mipitx_data_bit == 16)
	value |= SP7350_MIPITX_MIPI_PHY_VCO_G(sp_mipitx_phy_pllclk_norm_16b[time_cnt][11]);
	else if (disp_dev->mipitx_data_bit == 18)
	value |= SP7350_MIPITX_MIPI_PHY_VCO_G(sp_mipitx_phy_pllclk_norm_18b[time_cnt][11]);
	else if (disp_dev->mipitx_data_bit == 20)
	value |= SP7350_MIPITX_MIPI_PHY_VCO_G(sp_mipitx_phy_pllclk_norm_20b[time_cnt][11]);
	else if (disp_dev->mipitx_data_bit == 24)
	value |= SP7350_MIPITX_MIPI_PHY_VCO_G(sp_mipitx_phy_pllclk_norm_24b[time_cnt][11]);
	#endif
	value |= SP7350_MIPITX_MIPI_PHY_GPO_DS(sp_mipitx_phy_para[time_cnt][4]) |
		SP7350_MIPITX_MIPI_PHY_LP_RX_LPF(sp_mipitx_phy_para[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_REG04_CSEL(sp_mipitx_phy_para[time_cnt][6]) |
		SP7350_MIPITX_MIPI_PHY_REG_VSET_1P2(sp_mipitx_phy_para[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BANDGAP(sp_mipitx_phy_para[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_CLK_EDGE_SEL(sp_mipitx_phy_para[time_cnt][9]);		
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL4); //G205.09
	value &= ~(SP7350_MIPITX_MIPI_PHY_EN500P_MASK |
		SP7350_MIPITX_MIPI_PHY_EN250P_MASK |
		SP7350_MIPITX_MIPI_PHY_PHASE_DELAY_MASK);
	value |= SP7350_MIPITX_MIPI_PHY_EN500P(sp_mipitx_phy_para[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_EN250P(sp_mipitx_phy_para[time_cnt][11]) |
		SP7350_MIPITX_MIPI_PHY_PHASE_DELAY(sp_mipitx_phy_para[time_cnt][12]);	
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL4); //G205.09

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11
	value &= ~(SP7350_MIPITX_MIPI_PHY_EN_DIV5_MASK |
		SP7350_MIPITX_MIPI_PHY_POSTDIV_MASK |
		SP7350_MIPITX_MIPI_PHY_FBKDIV_MASK |
		SP7350_MIPITX_MIPI_PHY_PRESCALE_MASK |
		SP7350_MIPITX_MIPI_PHY_PREDIV_MASK);
	#ifdef SP7350_MIPITX_PHY_SSC_MODE
	value |= SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_ssc[time_cnt][6]) |
		SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_ssc[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_FBKDIV(SP7350_MIPITX_FBKDIV_48) |
		SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_ssc[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_ssc[time_cnt][4]);
	#else
	if (disp_dev->mipitx_data_bit == 16) {
		if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][5]);
		else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][5] + 1);
		else if (disp_dev->mipitx_lane == 4)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][5] + 2);
	value |= SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_norm_16b[time_cnt][6]) |
		//SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][3]) |
		SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_norm_16b[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_norm_16b[time_cnt][4]);
	} else if (disp_dev->mipitx_data_bit == 18) {
		if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][5]);
		else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][5] + 1);
		else if (disp_dev->mipitx_lane == 4)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][5] + 2);
	value |= SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_norm_18b[time_cnt][6]) |
		//SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][3]) |
		SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_norm_18b[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_norm_18b[time_cnt][4]);
	} else if (disp_dev->mipitx_data_bit == 20) {
		if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][5]);
		else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][5] + 1);
		else if (disp_dev->mipitx_lane == 4)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][5] + 2);
	value |= SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_norm_20b[time_cnt][6]) |
		//SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][3]) |
		SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_norm_20b[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_norm_20b[time_cnt][4]);
	} else if (disp_dev->mipitx_data_bit == 24) {
		if (disp_dev->mipitx_lane == 1)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][5]);
		else if (disp_dev->mipitx_lane == 2)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][5] + 1);
		else if (disp_dev->mipitx_lane == 4)
		value |= SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][5] + 2);
	value |= SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_norm_24b[time_cnt][6]) |
		//SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][5]) |
		SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][3]) |
		SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_norm_24b[time_cnt][2]) |
		SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_norm_24b[time_cnt][4]);
	}
	#endif	
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12
	value &= ~(SP7350_MIPITX_MIPI_PHY_LPFCP_MASK |
		SP7350_MIPITX_MIPI_PHY_LPFCS_MASK |
		SP7350_MIPITX_MIPI_PHY_LPFRS_MASK |
		SP7350_MIPITX_MIPI_PHY_ICH_MASK |
		SP7350_MIPITX_MIPI_PHY_BNKSEL_MASK);
	#ifdef SP7350_MIPITX_PHY_SSC_MODE
	value |= SP7350_MIPITX_MIPI_PHY_LPFCP(sp_mipitx_phy_pllclk_ssc[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_LPFCS(sp_mipitx_phy_pllclk_ssc[time_cnt][9]) |
		SP7350_MIPITX_MIPI_PHY_LPFRS(sp_mipitx_phy_pllclk_ssc[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_ICH(sp_mipitx_phy_pllclk_ssc[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_ssc[time_cnt][12]);
	#else
	if (disp_dev->mipitx_data_bit == 16)
	value |= SP7350_MIPITX_MIPI_PHY_LPFCP(sp_mipitx_phy_pllclk_norm_16b[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_LPFCS(sp_mipitx_phy_pllclk_norm_16b[time_cnt][9]) |
		SP7350_MIPITX_MIPI_PHY_LPFRS(sp_mipitx_phy_pllclk_norm_16b[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_ICH(sp_mipitx_phy_pllclk_norm_16b[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_norm_16b[time_cnt][12]);
	else if (disp_dev->mipitx_data_bit == 18)
	value |= SP7350_MIPITX_MIPI_PHY_LPFCP(sp_mipitx_phy_pllclk_norm_18b[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_LPFCS(sp_mipitx_phy_pllclk_norm_18b[time_cnt][9]) |
		SP7350_MIPITX_MIPI_PHY_LPFRS(sp_mipitx_phy_pllclk_norm_18b[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_ICH(sp_mipitx_phy_pllclk_norm_18b[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_norm_18b[time_cnt][12]);
	else if (disp_dev->mipitx_data_bit == 20)
	value |= SP7350_MIPITX_MIPI_PHY_LPFCP(sp_mipitx_phy_pllclk_norm_20b[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_LPFCS(sp_mipitx_phy_pllclk_norm_20b[time_cnt][9]) |
		SP7350_MIPITX_MIPI_PHY_LPFRS(sp_mipitx_phy_pllclk_norm_20b[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_ICH(sp_mipitx_phy_pllclk_norm_20b[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_norm_20b[time_cnt][12]);
	else if (disp_dev->mipitx_data_bit == 24)
	value |= SP7350_MIPITX_MIPI_PHY_LPFCP(sp_mipitx_phy_pllclk_norm_24b[time_cnt][10]) |
		SP7350_MIPITX_MIPI_PHY_LPFCS(sp_mipitx_phy_pllclk_norm_24b[time_cnt][9]) |
		SP7350_MIPITX_MIPI_PHY_LPFRS(sp_mipitx_phy_pllclk_norm_24b[time_cnt][8]) |
		SP7350_MIPITX_MIPI_PHY_ICH(sp_mipitx_phy_pllclk_norm_24b[time_cnt][7]) |
		SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_norm_24b[time_cnt][12]);
	#endif
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL8); //G205.13
	value &= ~(SP7350_MIPITX_MIPI_PHY_MDS_MASK |
		SP7350_MIPITX_MIPI_PHY_NS_MASK |
		SP7350_MIPITX_MIPI_PHY_EN_SSC_MASK |
		SP7350_MIPITX_MIPI_PHY_EN_DNSSC_MASK);
#ifdef SP7350_MIPITX_PHY_SSC_MODE
	value |= SP7350_MIPITX_MIPI_PHY_MDS_CLK(sp_mipitx_phy_pllclk_ssc[time_cnt][13]) |
		SP7350_MIPITX_MIPI_PHY_MDS_CLK_MUL(sp_mipitx_phy_pllclk_ssc[time_cnt][14]) |
		SP7350_MIPITX_MIPI_PHY_MDS_CLK_RATIO(sp_mipitx_phy_pllclk_ssc[time_cnt][15]) |
		SP7350_MIPITX_MIPI_PHY_NS(sp_mipitx_phy_pllclk_ssc[time_cnt][3]) |
		SP7350_MIPITX_MIPI_PHY_EN_SSC(SP7350_MIPITX_SSC_EN) |
		SP7350_MIPITX_MIPI_PHY_EN_DNSSC(SP7350_MIPITX_DNSSC_EN);
#else
	value |= SP7350_MIPITX_MIPI_PHY_MDS_CLK(SP7350_MIPITX_MDS_CLK_78kHz) |
		SP7350_MIPITX_MIPI_PHY_MDS_CLK_MUL(SP7350_MIPITX_MDS_CLK_MUL1) |
		SP7350_MIPITX_MIPI_PHY_MDS_CLK_RATIO(SP7350_MIPITX_MDS_CLK_RATIO_0P42) |
		SP7350_MIPITX_MIPI_PHY_NS(SP7350_MIPITX_NS_DIV_256) |
		SP7350_MIPITX_MIPI_PHY_EN_SSC(SP7350_MIPITX_SSC_DIS) |
		SP7350_MIPITX_MIPI_PHY_EN_DNSSC(SP7350_MIPITX_DNSSC_DIS);
#endif
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL8); //G205.13

	//value = readl(disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14
	//value |= SP7350_MIPITX_MIPI_PHY_INITIAL(SP7350_MIPITX_INITIAL_EN);
	//writel(value, disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14

}
u32 sp_phy_pll_prediv[4] = {1, 2, 5, 8};
u32 sp_phy_pll_postdiv[8] = {1, 2, 4, 8, 16, 16, 16, 16};
u32 sp_phy_pll_endiv5[2] = {1, 5};
static const char * const sp_phy_pll_bnksel[] = {
	"320-640M", "640-1000M", "1000-1200M", "1200-1600M"};
void sp7350_mipitx_phy_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 pre_scale, fbkdiv;
	u32 prediv_tmp, prediv;
	u32 postdiv_tmp, postdiv;
	u32 endiv5_tmp, endiv5;
	u32 value, bnksel_tmp;
	//u32 mipitx_pllck_16b_tmp, mipitx_pllck_18b_tmp, mipitx_pllck_20b_tmp, mipitx_pllck_24b_tmp;
	//int i;

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL1); //G205.04
	pr_info("  BP REG_VSE TX_LP_SR=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(17,17), value),
		FIELD_GET(GENMASK(14,12), value),
		FIELD_GET(GENMASK(2,0), value));

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
	pr_info("  GPO_DS LP_RX_LPF REG04_CSEL=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(26,24), value),
		FIELD_GET(GENMASK(21,20), value),
		FIELD_GET(GENMASK(15,14), value));
	pr_info("  VCO_G REG_VSET_1P2 BANDGAP=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(13,12), value),
		FIELD_GET(GENMASK(10,8), value),
		FIELD_GET(GENMASK(6,4), value));
	pr_info("  CLK_EDGE_SEL RESET=(%04ld %04ld)\n",
		FIELD_GET(GENMASK(2,2), value),
		FIELD_GET(GENMASK(0,0), value));
	//value = readl(disp_dev->base + MIPITX_ANALOG_CTRL3); //G205.07 (RO)
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL4); //G205.09
	pr_info("  EN500P EN250P PHASE_DELAY=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(5,5), value),
		FIELD_GET(GENMASK(4,4), value),
		FIELD_GET(GENMASK(3,0), value));
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL5); //G205.10
	pr_info("  TEST RST_N_PLL EN_TXPLL=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(4,4), value),
		FIELD_GET(GENMASK(1,1), value),
		FIELD_GET(GENMASK(0,0), value));
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11

	pre_scale = FIELD_GET(GENMASK(4,4), value) + 1;
	fbkdiv = FIELD_GET(GENMASK(13,8), value);
	prediv_tmp = FIELD_GET(GENMASK(1,0), value);
	prediv = sp_phy_pll_prediv[prediv_tmp];
	postdiv_tmp = FIELD_GET(GENMASK(18,16), value);
	postdiv = sp_phy_pll_postdiv[postdiv_tmp];
	endiv5_tmp = FIELD_GET(GENMASK(20,20), value);
	endiv5 = sp_phy_pll_endiv5[endiv5_tmp];
	pr_info("  EN_DIV5 POSTDIV FBKDIV PRESCALE PREDIV=(%04ld %04ld %04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(20,20), value),
		FIELD_GET(GENMASK(18,16), value),
		FIELD_GET(GENMASK(13,8), value),
		FIELD_GET(GENMASK(4,4), value),
		FIELD_GET(GENMASK(1,0), value));
	pr_info("            (25M x %d x %d)\n", fbkdiv, pre_scale);
	pr_info("  Fckout=------------------= %dMHz\n",
		25*pre_scale*fbkdiv/prediv/postdiv/endiv5);
	pr_info("              (%d x %d x %d)\n", prediv, postdiv, endiv5);

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12
	bnksel_tmp = FIELD_GET(GENMASK(2,0), value);
	pr_info("  Fvco= %dMHz(bank %s) Fckout=%dMHz\n",
		25*pre_scale*fbkdiv/prediv,
		sp_phy_pll_bnksel[bnksel_tmp],
		25*pre_scale*fbkdiv/prediv/postdiv/endiv5);
	pr_info("  LPFCP LPFCS LPFRS ICH BNKSEL=(%04ld %04ld %04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(17,16), value),
		FIELD_GET(GENMASK(13,12), value),
		FIELD_GET(GENMASK(9,8), value),
		FIELD_GET(GENMASK(6,4), value),
		FIELD_GET(GENMASK(2,0), value));
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL8); //G205.13
	pr_info("  MDS (CLK MUL RTO) NS SSC SSC_DN=(%04ld %04ld %04ld %04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(22,20), value),
		FIELD_GET(GENMASK(19,19), value),
		FIELD_GET(GENMASK(18,16), value),
		FIELD_GET(GENMASK(15,8), value),
		FIELD_GET(GENMASK(1,1), value),
		FIELD_GET(GENMASK(0,0), value));
	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14
	pr_info("  INITIAL ANA_TEST OTP_FREE=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(31,31), value),
		FIELD_GET(GENMASK(29,29), value),
		FIELD_GET(GENMASK(28,28), value));
	#if 0
	for (i = 0; i < 11; i++) {
		mipitx_pllck_16b_tmp = 	25 * (sp_mipitx_phy_pllclk_norm_16b[i][2] + 1) *
			sp_mipitx_phy_pllclk_norm_16b[i][3] /
			sp_phy_pll_prediv[sp_mipitx_phy_pllclk_norm_16b[i][4]] /
			sp_phy_pll_postdiv[sp_mipitx_phy_pllclk_norm_16b[i][5]] /
			sp_phy_pll_endiv5[sp_mipitx_phy_pllclk_norm_16b[i][6]];
		mipitx_pllck_18b_tmp = 	25 * (sp_mipitx_phy_pllclk_norm_18b[i][2] + 1) *
			sp_mipitx_phy_pllclk_norm_18b[i][3] /
			sp_phy_pll_prediv[sp_mipitx_phy_pllclk_norm_18b[i][4]] /
			sp_phy_pll_postdiv[sp_mipitx_phy_pllclk_norm_18b[i][5]] /
			sp_phy_pll_endiv5[sp_mipitx_phy_pllclk_norm_18b[i][6]];
		mipitx_pllck_20b_tmp = 	25 * (sp_mipitx_phy_pllclk_norm_20b[i][2] + 1) *
			sp_mipitx_phy_pllclk_norm_20b[i][3] /
			sp_phy_pll_prediv[sp_mipitx_phy_pllclk_norm_20b[i][4]] /
			sp_phy_pll_postdiv[sp_mipitx_phy_pllclk_norm_20b[i][5]] /
			sp_phy_pll_endiv5[sp_mipitx_phy_pllclk_norm_20b[i][6]];
		mipitx_pllck_24b_tmp = 	25 * (sp_mipitx_phy_pllclk_norm_24b[i][2] + 1) *
			sp_mipitx_phy_pllclk_norm_24b[i][3] /
			sp_phy_pll_prediv[sp_mipitx_phy_pllclk_norm_24b[i][4]] /
			sp_phy_pll_postdiv[sp_mipitx_phy_pllclk_norm_24b[i][5]] /
			sp_phy_pll_endiv5[sp_mipitx_phy_pllclk_norm_24b[i][6]];

		pr_info("mipitx clock(%04d %04d) %04d %04d %04d %04d(MHz)\n",
		sp_mipitx_phy_pllclk_norm_16b[i][0], sp_mipitx_phy_pllclk_norm_16b[i][1],
			mipitx_pllck_16b_tmp,
			mipitx_pllck_18b_tmp,
			mipitx_pllck_20b_tmp,
			mipitx_pllck_24b_tmp);
	}
	#endif
}
