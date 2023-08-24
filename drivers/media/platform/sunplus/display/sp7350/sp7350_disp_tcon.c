// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for TCON block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

static const char * const tpg_mode[] = {
	"TPG off", "TPG internal", "TPG external", "TPG external"};
static const char * const tpg_pattern[] = {
	"H_1_COLORBAR", "H_2_RAMP", "H_3_ODDEVEN",
	"V_1_COLORBAR", "V_2_RAMP", "V_3_ODDEVEN",
	"HV_1_CHECK", "HV_2_FRAME", "HV_3_MOIRE_A","HV_4_MOIRE_B", "HV_5_CONTRAST",
	"H_1_COLORBAR", "H_1_COLORBAR", "H_1_COLORBAR", "H_1_COLORBAR", "H_1_COLORBAR"};
static const char * const tcon_pix_en_sel[] = {
	"1_TCON_CLK", "1/2_TCON_CLK", "1/3_TCON_CLK", "1/4_TCON_CLK",
	"1/6_TCON_CLK", "1/8_TCON_CLK", "1/12_TCON_CLK", "1/16_TCON_CLK",
	"1/24_TCON_CLK", "1/32_TCON_CLK", "1_TCON_CLK", "1_TCON_CLK",
	"1_TCON_CLK", "1_TCON_CLK", "1_TCON_CLK", "1_TCON_CLK"};
static const char * const tcon_out_package[] = {
	"DSI-RGB565", "CSI-RGB565", "CSI-YUY2/-10", "DSI-RGB666-18b",
	"DSI-RGB888", "DSI-RGB666-24b", "CSI-RGB888", "unknown"};

void sp7350_tcon_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value = 0;

	#if 1
	//writel(0x00008127, disp_dev->base + TCON_TCON0);
	//writel(0x00008011, disp_dev->base + TCON_TCON1);
	//writel(0x00000011, disp_dev->base + TCON_TCON2); //don't care
	//writel(0x00002002, disp_dev->base + TCON_TCON3); //don't care
	//writel(0x00000000, disp_dev->base + TCON_TCON4); //fixed , don't change it
	//writel(0x00000004, disp_dev->base + TCON_TCON5); //don't care

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {
		value = 0;
		if (disp_dev->mipitx_format == 0x0)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_DSI_RGB565);
		else if (disp_dev->mipitx_format == 0x1)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_DSI_RGB666_18);
		else if (disp_dev->mipitx_format == 0x2)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_DSI_RGB666_24);
		else if (disp_dev->mipitx_format == 0x3)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_DSI_RGB888);
		else
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_DSI_RGB888);

		value |= SP7350_TCON_DOT_RGB888_MASK |
			SP7350_TCON_DOT_ORDER_SET(SP7350_TCON_DOT_ORDER_RGB) |
			SP7350_TCON_HVIF_EN | SP7350_TCON_YU_SWAP;
	} else {
		value = 0;
		if (disp_dev->mipitx_format == 0x0)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_RGB565);
		else if (disp_dev->mipitx_format == 0x1)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_YUY2);
		else if (disp_dev->mipitx_format == 0x2)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_YUY2);
		else if (disp_dev->mipitx_format == 0x3)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_RGB888);
		else
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_RGB888);

		if ((disp_dev->mipitx_format == 0x1) || (disp_dev->mipitx_format == 0x2))
			value |= SP7350_TCON_DITHER_YUV_EN;

		value |= SP7350_TCON_DOT_RGB888_MASK |
			SP7350_TCON_DOT_ORDER_SET(SP7350_TCON_DOT_ORDER_RGB) |
			SP7350_TCON_HVIF_EN | SP7350_TCON_YU_SWAP;
		#if 0
		if (disp_dev->mipitx_data_bit == 16)
			//value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_RGB565);
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_YUY2);
		else if (disp_dev->mipitx_data_bit == 18)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_YUY2);
		else if (disp_dev->mipitx_data_bit == 24)
			value |= SP7350_TCON_OUT_PACKAGE_SET(SP7350_TCON_OUT_PACKAGE_CSI_RGB888);
		#endif
		//value = 0x008157; //CSI YUY2 and bit6 must enable
	}
	writel(value, disp_dev->base + TCON_TCON0);

	value = 0;
	value |= (SP7350_TCON_EN | SP7350_TCON_YUV_UV_SWAP |
		SP7350_TCON_STHLR_DLY_SET(SP7350_TCON_STHLR_DLY_1T));
	writel(value, disp_dev->base + TCON_TCON1);

	value = 0;
	//value |= SP7350_TCON_HDS_FILTER;
	writel(value, disp_dev->base + TCON_TCON2); //don't care

	value = 0;
	//value |= SP7350_TCON_OEH_POL;
	writel(value, disp_dev->base + TCON_TCON3); //don't care

	value = 0;
	value |= SP7350_TCON_PIX_EN_SEL_SET(SP7350_TCON_PIX_EN_DIV_1_CLK_TCON);
	writel(value, disp_dev->base + TCON_TCON4); //fixed , don't change it

	value = 0;
	value |= SP7350_TCON_CHK_SUM_EN;
	writel(value, disp_dev->base + TCON_TCON5); //don't care
	#endif

#if 0
	value = readl(disp_dev->base + MIPITX_INFO_STATUS); //G204.28
	//pr_info("  MIPITX_INFO_STATUS MIPITX %s (TX_PHY %s)\n",
	//	FIELD_GET(GENMASK(24,24), value)?"ON":"OFF",
	//	FIELD_GET(GENMASK(0,0), value)?"X":"O");
	if ((FIELD_GET(GENMASK(24,24), value) == 1) && (FIELD_GET(GENMASK(0,0), value) == 0)) {
		pr_info("  MIPITX working, skip tcon setting\n");
		return;
	}
#endif

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		sp7350_tcon_timing_set_dsi();
	else if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI)
		sp7350_tcon_timing_set_csi();
	else
		pr_info("undefined mode for tcon init\n");

}

void sp7350_tcon_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, tcon0_value, tcon1_value, tcon2_value, tcon3_value, tcon5_value;
	u32 value1, value2, value3, value4;

	/* TCON Control Setting
	*/
	pr_info("[TCON Control Setting]\n");
	tcon0_value = readl(disp_dev->base + TCON_TCON0); //G199.00
	pr_info("G199.00 TC_TCON0 0x%08x\n", tcon0_value);
	pr_info("  HVIF_EN[%s]\n", FIELD_GET(GENMASK(1,1), tcon0_value)?"HV_SYNC":"Pulse");
	pr_info("  DITHER_YUV_EN[%s]\n", FIELD_GET(GENMASK(6,6), tcon0_value)?"O":"X");
	pr_info("  RGB OUT[%s]\n", FIELD_GET(GENMASK(15,15), tcon0_value)?"RGB888":"RGB666");
	pr_info("  OUT_PACKAGE[%s]\n",  tcon_out_package[FIELD_GET(GENMASK(5,3), tcon0_value)]);

	tcon1_value = readl(disp_dev->base + TCON_TCON1); //G199.01
	pr_info("G199.01 TC_TCON1 0x%08x\n", tcon1_value);
	pr_info("  [%s] TCON Module\n", FIELD_GET(GENMASK(15,15), tcon1_value)?"ON":"OFF");
	pr_info("  YU_SWAP[%s] UV_SWAP[%s]\n",
		FIELD_GET(GENMASK(0,0), tcon0_value)?"O":"X",
		FIELD_GET(GENMASK(4,4), tcon1_value)?"O":"X");

	tcon2_value = readl(disp_dev->base + TCON_TCON2); //G199.02
	pr_info("G199.02 TC_TCON2 0x%08x\n", tcon2_value);
	//pr_info("  TC_CHP1_POL[%s]\n",
	//	FIELD_GET(GENMASK(2,2), tcon2_value)?"P":"N");
	//pr_info("  horizon fileter[%s]\n",
	//	FIELD_GET(GENMASK(0,0), tcon2_value)?"[0,1/4,1/2,1/4,0]":"[0,0,1,0,0]");

	tcon3_value = readl(disp_dev->base + TCON_TCON3); //G199.03
	pr_info("G199.03 TC_TCON3 0x%08x\n", tcon3_value);
	pr_info("  Digital RGB OUT[%s]\n",
		FIELD_GET(GENMASK(11,11), tcon3_value)?"X":"O");
	pr_info("  POL HSync[%s]VSync[%s]DE[%s]\n",
		FIELD_GET(GENMASK(5,5), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(4,4), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(1,1), tcon3_value)?"P":"N");
		
	value = readl(disp_dev->base + TCON_TCON4); //G200.15
	pr_info("G200.15 TC_TCON4 0x%08x\n", value);
	pr_info("  PIX_EN_SEL :%s\n", tcon_pix_en_sel[FIELD_GET(GENMASK(3,0), value)]);
	
	tcon5_value = readl(disp_dev->base + TCON_TCON5); //G200.26
	pr_info("G200.26 TC_TCON5 0x%08x\n", tcon5_value);
	pr_info("  COLOR_SPACE[%s][%s]\n",
		FIELD_GET(GENMASK(1,1), tcon5_value)?"O":"X",
		FIELD_GET(GENMASK(0,0), tcon5_value)?"BT.709":"BT.601");
	pr_info("  TCON_AFIFO_DIS[%s]\n",
		FIELD_GET(GENMASK(3,3), tcon5_value)?"O":"X");

	value = readl(disp_dev->base + TCON_DITHER_TVOUT); //G200.22
	pr_info("G200.22 TCON_DITHER_TVOUT 0x%08x\n", value);
	pr_info("  DITHER Set[565 %s][%s][%s][%s][%s]\n",
		FIELD_GET(GENMASK(7,7), value)?"en":"dis",
		FIELD_GET(GENMASK(6,6), value)?"robust":"matched",
		FIELD_GET(GENMASK(3,3), value)?"new":"-",
		FIELD_GET(GENMASK(1,1), value)?"method1":"method2",
		FIELD_GET(GENMASK(0,0), value)?"RGB":"R only");

	#if 0
	/* TCON Gamma Correction
	*/
	pr_info("[TCON Gamma Correction]\n");
	value1 = readl(disp_dev->base + TCON_GAMMA0); //G200.00
	pr_info("G200.00 TCON_GAMMA0 0x%08x\n", value1);
	//pr_info("  GM_EN[%s]\n",
	//	FIELD_GET(GENMASK(0,0), value1)?"O":"X");
	value2 = readl(disp_dev->base + TCON_GAMMA1); //G200.27
	////pr_info("G200.27 TCON_GAMMA1 0x%08x\n", value2);
	//pr_info("  GM_ADDR 0x%04lx\n", FIELD_GET(GENMASK(8,0), value2));
	value3 = readl(disp_dev->base + TCON_GAMMA2); //G200.28
	////pr_info("G200.28 TCON_GAMMA2 0x%08x\n", value3);
	//pr_info("  GM_DATA 0x%04lx\n", FIELD_GET(GENMASK(11,0), value3));
	pr_info("  GM_EN[%s]GM_ADDR 0x%04lx GM_DATA 0x%04lx\n",
		FIELD_GET(GENMASK(0,0), value1)?"O":"X",
		FIELD_GET(GENMASK(8,0), value2),
		FIELD_GET(GENMASK(11,0), value3));
	#endif

	/* TCON Parameter list
	*/
	pr_info("[TCON Timing list]\n");
	value1 = readl(disp_dev->base + TCON_OEV_START); //G199.12
	value2 = readl(disp_dev->base + TCON_OEV_END); //G199.13
	pr_info("  [O] VSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_HSYNC_START); //G199.23
	value2 = readl(disp_dev->base + TCON_HSYNC_END); //G199.24
	pr_info("  [O] HSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_STVU_START); //G199.20
	value2 = readl(disp_dev->base + TCON_STVU_END); //G199.21
	pr_info("  [O] VTOP_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_DE_HSTART); //G199.25
	value2 = readl(disp_dev->base + TCON_DE_HEND); //G199.26
	pr_info("  [O] DE_HSTART/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_DE_VSTART); //G199.27
	value2 = readl(disp_dev->base + TCON_DE_VEND); //G199.28
	pr_info("  [O] DE_VSTART/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	/* TCON TPG Test Pattern Gen
	*/
	pr_info("[TCON TPG Test Pattern Gen]\n");
	value1 = readl(disp_dev->base + TCON_TPG_CTRL); //G200.06 TPG_CTRL
	//pr_info("  [TPG] Mode :%s\n", tpg_mode[FIELD_GET(GENMASK(1,0), value1)]);
	//pr_info("  [TPG] Pattern :%s\n", tpg_pattern[FIELD_GET(GENMASK(5,2), value1)]);
	pr_info("  [%s] :%s\n", tpg_mode[FIELD_GET(GENMASK(1,0), value1)],
		tpg_pattern[FIELD_GET(GENMASK(5,2), value1)]);

	value1 = readl(disp_dev->base + TCON_TPG_ALINE_START); //G200.21
	pr_info("  [TPG] ALINE_START %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(11,0), value1), FIELD_GET(GENMASK(11,0), value1));
	value1 = readl(disp_dev->base + TCON_TPG_HCOUNT); //G200.07
	value2 = readl(disp_dev->base + TCON_TPG_VCOUNT); //G200.08
	value3 = readl(disp_dev->base + TCON_TPG_HACT_COUNT); //G200.09
	value4 = readl(disp_dev->base + TCON_TPG_VACT_COUNT); //G200.10
	pr_info("  [TPG] H/VSTEP %04ld %04ld\n",
		FIELD_GET(GENMASK(14,12), value2), FIELD_GET(GENMASK(14,12), value4));	
	pr_info("  [TPG] H/VCOUNT %04d(0x%04x) %04ld(0x%04lx)\n",
		value1, value1, FIELD_GET(GENMASK(11,0), value2), FIELD_GET(GENMASK(11,0), value2));
	pr_info("  [TPG] H/V_ACT_COUNT %04d(0x%04x) %04ld(0x%04lx)\n",
		value3, value3, FIELD_GET(GENMASK(11,0), value4), FIELD_GET(GENMASK(11,0), value4));

	value1 = readl(disp_dev->base + TCON_TPG_RGB_DATA); //G200.19
	value2 = readl(disp_dev->base + TCON_TPG_RGB_DATA_2); //G200.20
	pr_info("  [TPG] WHITE_BG : %s\n",
		FIELD_GET(GENMASK(15,15), value1)?"En set white BG":"Dis BG depends");
	pr_info("  [TPG] R/G/B (0x%02lx)(0x%02lx)(0x%02lx)\n",
		FIELD_GET(GENMASK(7,0), value1),
		FIELD_GET(GENMASK(15,8), value2),
		FIELD_GET(GENMASK(7,0), value2));

	/* TCON Debug info
	*/
	pr_info("[TCON Debug info]\n");
	value1 = readl(disp_dev->base + TCON_DET_BP_LCNT); //G200.11
	value2 = readl(disp_dev->base + TCON_DET_FP_LCNT); //G200.12
	value3 = readl(disp_dev->base + TCON_DET_HCOUNT); //G200.14
	pr_info("  [RU] VBP VFP HCOUNT (%04ld)(%04ld)(%04ld)\n",
		FIELD_GET(GENMASK(7,0), value1),
		FIELD_GET(GENMASK(11,0), value2),
		FIELD_GET(GENMASK(12,0), value3));
	value1 = readl(disp_dev->base + TCON_HACT_LATCH); //G200.23
	value2 = readl(disp_dev->base + TCON_VTOTAL); //G200.24
	pr_info("  [RU] HACT VTOTAL (%04ld)(%04ld)\n",
		FIELD_GET(GENMASK(12,0), value1),
		FIELD_GET(GENMASK(11,0), value2));
	value1 = readl(disp_dev->base + TCON_CHECKSUM); //G200.29
	pr_info("  [RU] TCON_CHECK_SUM [%s] (0x%04lx)\n",
		FIELD_GET(GENMASK(2,2), tcon5_value)?"O":"X", FIELD_GET(GENMASK(11,0), value1));

	#if 0
	/* TCON Bit Swap Setting
	*/
	pr_info("[TCON Bit Swap Setting]\n");
	value1 = readl(disp_dev->base + TCON_BIT_SWAP_CFG); //G201.13
	pr_info("G201.13 TCON_BIT_SWAP_CFG 0x%08x\n", value1);
	pr_info("  BIT_SWAP_EN[%s]\n",
		FIELD_GET(GENMASK(0,0), value1)?"O":"X");
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G7); //G201.21
	//pr_info("  B23/B22/B21 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B23/B22/B21 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G6); //G201.20
	//pr_info("  B20/B19/B18 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B20/B19/B18 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G5); //G201.19
	//pr_info("  B17/B16/B15 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B17/B16/B15 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G4); //G201.18
	//pr_info("  B14/B13/B12 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B14/B13/B12 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G3); //G201.17
	//pr_info("  B11/B10/B09 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B11/B10/B09 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G2); //G201.16
	//pr_info("  B08/B07/B06 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B08/B07/B06 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G1); //G201.15
	//pr_info("  B05/B04/B03 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B05/B04/B03 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	value2 = readl(disp_dev->base + TCON_BIT_SWAP_G0); //G201.14
	//pr_info("  B02/B01/B00 0x%04lx 0x%04lx 0x%04lx\n",
	pr_info("  B02/B01/B00 DIN[%02ld] DIN[%02ld] DIN[%02ld]\n",
		FIELD_GET(GENMASK(14,10), value2),
		FIELD_GET(GENMASK(9,5), value2),
		FIELD_GET(GENMASK(4,0), value2));
	#endif
}
EXPORT_SYMBOL(sp7350_tcon_decrypt_info);

void sp7350_tcon_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	//u32 tcon1_value, value, value1, value2, value3;
	u32 tcon1_value, value1, value2;

	tcon1_value = readl(disp_dev->base + TCON_TCON1); //G199.01
	pr_info("TCON resolution chk\n");
	/* TCON Parameter list
	*/
	//pr_info("TCON Timing list\n");
	value1 = readl(disp_dev->base + TCON_OEV_START); //G199.12
	value2 = readl(disp_dev->base + TCON_OEV_END); //G199.13
	pr_info("  [O] VSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_HSYNC_START); //G199.23
	value2 = readl(disp_dev->base + TCON_HSYNC_END); //G199.24
	pr_info("  [O] HSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_STVU_START); //G199.20
	value2 = readl(disp_dev->base + TCON_STVU_END); //G199.21
	pr_info("  [O] VTOP_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_DE_HSTART); //G199.25
	value2 = readl(disp_dev->base + TCON_DE_HEND); //G199.26
	pr_info("  [O] DE_HSTART/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_DE_VSTART); //G199.27
	value2 = readl(disp_dev->base + TCON_DE_VEND); //G199.28
	pr_info("  [O] DE_VSTART/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
}
EXPORT_SYMBOL(sp7350_tcon_resolution_chk);

void sp7350_tcon_bist_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3, value4;

	/* TCON TPG Test Pattern Gen
	*/
	pr_info("TCON TPG Test Pattern Gen\n");
	value1 = readl(disp_dev->base + TCON_TPG_CTRL); //G200.06 TPG_CTRL
	//pr_info("  [TPG] Mode :%s\n", tpg_mode[FIELD_GET(GENMASK(1,0), value1)]);
	//pr_info("  [TPG] Pattern :%s\n", tpg_pattern[FIELD_GET(GENMASK(5,2), value1)]);
	pr_info("  [%s] :%s\n", tpg_mode[FIELD_GET(GENMASK(1,0), value1)],
		tpg_pattern[FIELD_GET(GENMASK(5,2), value1)]);

	value1 = readl(disp_dev->base + TCON_TPG_HCOUNT); //G200.07
	value2 = readl(disp_dev->base + TCON_TPG_VCOUNT); //G200.08
	value3 = readl(disp_dev->base + TCON_TPG_HACT_COUNT); //G200.09
	value4 = readl(disp_dev->base + TCON_TPG_VACT_COUNT); //G200.10
	pr_info("  [TPG] H/VSTEP %ld %ld\n",
		FIELD_GET(GENMASK(14,12), value2), FIELD_GET(GENMASK(14,12), value4));	
	pr_info("  [TPG] H/V_CNT     %d(0x%04x) %ld(0x%04lx)\n",
		value1, value1, FIELD_GET(GENMASK(11,0), value2), FIELD_GET(GENMASK(11,0), value2));
	pr_info("  [TPG] H/V_ACT_CNT %d(0x%04x) %ld(0x%04lx)\n",
		value3, value3, FIELD_GET(GENMASK(11,0), value4), FIELD_GET(GENMASK(11,0), value4));
	value1 = readl(disp_dev->base + TCON_TPG_ALINE_START); //G200.21
	pr_info("  [TPG] ALINE_START %ld(0x%04lx)\n",
		FIELD_GET(GENMASK(11,0), value1), FIELD_GET(GENMASK(11,0), value1));

	value1 = readl(disp_dev->base + TCON_TPG_RGB_DATA); //G200.19
	value2 = readl(disp_dev->base + TCON_TPG_RGB_DATA_2); //G200.20
	pr_info("  [TPG] WHITE_BG : %s\n",
		FIELD_GET(GENMASK(15,15), value1)?"En set white BG":"Dis BG depends");
	pr_info("  [TPG] R/G/B (0x%02lx)(0x%02lx)(0x%02lx)\n",
		FIELD_GET(GENMASK(7,0), value1),
		FIELD_GET(GENMASK(15,8), value2),
		FIELD_GET(GENMASK(7,0), value2));

}
EXPORT_SYMBOL(sp7350_tcon_bist_info);

void sp7350_tcon_bist_set(int bist_mode, int tcon_bist_pat)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	pr_info("%s\n",__func__);
	value = readl(disp_dev->base + TCON_TPG_CTRL);
	value &= ~(SP7350_TCON_TPG_PATTERN | SP7350_TCON_TPG_MODE);
	value |= SP7350_TCON_TPG_PATTERN_SET(tcon_bist_pat) |
		SP7350_TCON_TPG_MODE_SET(bist_mode);
	writel(value , disp_dev->base + TCON_TPG_CTRL);

}
EXPORT_SYMBOL(sp7350_tcon_bist_set);

/*
 * sp_tcon_para_dsi[x][y]
 * y = 0-1, TCON width & height
 * y = 2-11, TCON DE_H & Vsync_H & Hsync & DE_V & VTOP_V
 */
static const u32 sp_tcon_para_dsi[11][12] = {
	/* (w   h)    DE_H       Vsync_H     Hsync       DE_V        VTOP_V     */
	{ 720,  480,    0,  719,  850,  854,  850,  854,    0,    0,  524,    0}, /* 480P */
	{ 720,  576,    0,  719,  856,  856,  856,  860,    0,    0,  624,    0}, /* 576P */
	{1280,  720,    0, 1279, 1642, 1646, 1642, 1646,    0,    0,  749,    0}, /* 720P */
	{1920, 1080,    0, 2047, 2192, 2196, 2192, 2196,    0,    0, 1124,    0}, /* 1080P */
	//{  64,   64,    0,   63,  353,  353,  353,  356,    0,    0,   99,    0}, /* 64x64 */
	{ 480, 1280,    0,  479,  612,  616,  612,  616,    0,    0, 1313,    0}, /* 480x1280 */
	{ 128,  128,    0,  127,  352,  352,  352,  356,    0,    0,  149,    0}, /* 128x128 */
	{  64, 2880,    0,   63,  352,  352,  352,  356,    0,    0, 3199,    0}, /* 64x2880 */
	{3840,   64,    0, 3839, 4600, 4600, 4600, 4604,    0,    0,   99,    0}, /* 3840x64 */
	{3840, 2880,    0, 3839, 4600, 4600, 4600, 4604,    0,    0, 3199,    0}, /* 3840x2880 */
	{ 800,  480,    0,  799,  920,  920,  920,  924,    0,    0,  524,    0}, /* 800x480 */
	{1024,  600,    0, 1023, 1336, 1336, 1336, 1340,    0,    0,  634,    0}  /* 1024x600 */
};

/*
 * sp_tcon_tpg_para_dsi[x][y]
 * y = 0-1, TCON width & height
 * y = 2-9, TCON Hstep & Vstep & Hcnt & Vcnt & Hact & Vact & A_LINE & DITHER
 */
static const u32 sp_tcon_tpg_para_dsi[11][10] = {
	/* (w   h)    Hstep Vstep Hcnt  Vcnt  Hact  Vact A_LINE DITHER */
	{ 720,  480,    4,    4,  857,  524,  719,  479,  17, 0x01}, /* 480P */
	{ 720,  576,    4,    4,  863,  624,  719,  575,  17, 0x41}, /* 576P */
	{1280,  720,    4,    4, 1649,  749, 1279,  719,  17, 0x41}, /* 720P */
	{1920, 1080,    4,    4, 2199, 1124, 1919, 1079,  17, 0x01}, /* 1080P */
	//{  64,   64,    4,    4,  359,   99,   63,   63,  17, 0xC1}, /* 64x64 */
	{ 480, 1280,    4,    4,  619, 1313,  479, 1279,  17, 0x01}, /* 480x1280 */
	{ 128,  128,    4,    4,  359,  149,  127,  127,  17, 0x49}, /* 128x128 */
	{  64, 2880,    4,    4,  359, 3199,   63, 2879,  17, 0xC1}, /* 64x2880 */
	{3840,   64,    4,    4, 4607,   99, 3839,   63,  17, 0x01}, /* 3840x64 */
	{3840, 2880,    4,    4, 4607, 3199, 3839, 2879,  17, 0x01}, /* 3840x2880 */
	{ 800,  480,    4,    4,  927,  524,  799,  479,  17, 0x01}, /* 800x480 */
	{1024,  600,    4,    4, 1343,  634, 1023,  599,  17, 0x01}  /* 1024x600 */
};

void sp7350_tcon_timing_set_dsi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value = 0;

	for (i = 0; i < 11; i++) {
		if ((sp_tcon_para_dsi[i][0] == disp_dev->out_res.width) &&
			(sp_tcon_para_dsi[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	//pr_info("%s (w h)(%d %d)\n", __func__,
	//	sp_tcon_para_dsi[time_cnt][0], sp_tcon_para_dsi[time_cnt][1]);
	/*
	 * TCON H&V timing parameter
	 */
	writel(sp_tcon_para_dsi[time_cnt][2], disp_dev->base + TCON_DE_HSTART); //DE_HSTART
	writel(sp_tcon_para_dsi[time_cnt][3], disp_dev->base + TCON_DE_HEND); //DE_HEND

	writel(sp_tcon_para_dsi[time_cnt][4], disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
	writel(sp_tcon_para_dsi[time_cnt][5], disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND

	writel(sp_tcon_para_dsi[time_cnt][6], disp_dev->base + TCON_HSYNC_START); //HSYNC_START
	writel(sp_tcon_para_dsi[time_cnt][7], disp_dev->base + TCON_HSYNC_END); //HSYNC_END

	writel(sp_tcon_para_dsi[time_cnt][8], disp_dev->base + TCON_DE_VSTART); //DE_VSTART
	writel(sp_tcon_para_dsi[time_cnt][9], disp_dev->base + TCON_DE_VEND); //DE_VEND

	writel(sp_tcon_para_dsi[time_cnt][10], disp_dev->base + TCON_STVU_START); //VTOP_VSTART
	writel(sp_tcon_para_dsi[time_cnt][11], disp_dev->base + TCON_STVU_END); //VTOP_VEND

	/*
	 * TPG(Test Pattern Gen) parameter
	 */
	writel(sp_tcon_tpg_para_dsi[time_cnt][4], disp_dev->base + TCON_TPG_HCOUNT);
	value |= (sp_tcon_tpg_para_dsi[time_cnt][2] << 12) | sp_tcon_tpg_para_dsi[time_cnt][5];
	writel(value, disp_dev->base + TCON_TPG_VCOUNT);
	writel(sp_tcon_tpg_para_dsi[time_cnt][6], disp_dev->base + TCON_TPG_HACT_COUNT);
	value = 0;
	value |= (sp_tcon_tpg_para_dsi[time_cnt][3] << 12) | sp_tcon_tpg_para_dsi[time_cnt][7];
	writel(value, disp_dev->base + TCON_TPG_VACT_COUNT);

	writel(sp_tcon_tpg_para_dsi[time_cnt][8], disp_dev->base + TCON_TPG_ALINE_START);

	//writel(sp_tcon_tpg_para_dsi[time_cnt][9], disp_dev->base + TCON_DITHER_TVOUT);

}

/*
 * sp_tcon_para_csi[x][y]
 * y = 0-1, TCON width & height
 * y = 2-11, TCON DE_H & Vsync_H & Hsync & DE_V & VTOP_V
 */
static const u32 sp_tcon_para_csi[7][12] = {
	/* (w   h)    DE_H       Vsync_H     Hsync       DE_V        VTOP_V     */
	{  64,   64,    0,   63,  353,  353,  353,  356,    0,    0,   99,    0}, /* 64x64 */
	{ 128,  128,    0,  127,  352,  352,  352,  356,    0,    0,  149,    0}, /* 128x128 */
	{ 720,  480,    0,  719,  850,  850,  850,  854,    0,    0,  524,    0}, /* 480P */
	{ 720,  576,    0,  719,  856,  856,  856,  860,    0,    0,  624,    0}, /* 576P */
	{1280,  720,    0, 1279, 1642, 1642, 1642, 1646,    0,    0,  749,    0}, /* 720P */
	{1920, 1080,    0, 2047, 2192, 2192, 2192, 2196,    0,    0, 1124,    0}, /* 1080P */
	{3840, 2880,    0, 3839, 4600, 4600, 4600, 4604,    0,    0, 3199,    0}, /* 3840x2880 */
};

/*
 * sp_tcon_tpg_para_csi[x][y]
 * y = 0-1, TCON width & height
 * y = 2-9, TCON Hstep & Vstep & Hcnt & Vcnt & Hact & Vact & A_LINE & DITHER
 */
static const u32 sp_tcon_tpg_para_csi[7][10] = {
	/* (w   h)    Hstep Vstep Hcnt  Vcnt  Hact  Vact A_LINE DITHER */
	{  64,   64,    4,    4,  359,   99,   63,   63,  17, 0xC1}, /* 64x64 */
	{ 128,  128,    4,    4,  359,  149,  127,  127,  17, 0x49}, /* 128x128 */
	{ 720,  480,    4,    4,  857,  524,  719,  479,  17, 0x01}, /* 480P */
	{ 720,  576,    4,    4,  863,  624,  719,  575,  17, 0x41}, /* 576P */
	{1280,  720,    4,    4, 1649,  749, 1279,  719,  17, 0x41}, /* 720P */
	{1920, 1080,    4,    4, 2199, 1124, 1919, 1079,  17, 0x01}, /* 1080P */
	{3840, 2880,    4,    4, 4607, 3199, 3839, 2879,  17, 0x01}, /* 3840x2880 */
};

void sp7350_tcon_timing_set_csi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value = 0;

	for (i = 0; i < 7; i++) {
		if ((sp_tcon_para_csi[i][0] == disp_dev->out_res.width) &&
			(sp_tcon_para_csi[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	//pr_info("%s (w h)(%d %d)\n", __func__,
	//	sp_tcon_para_csi[time_cnt][0], sp_tcon_para_csi[time_cnt][1]);
	/*
	 * TCON H&V timing parameter
	 */
	writel(sp_tcon_para_csi[time_cnt][2], disp_dev->base + TCON_DE_HSTART); //DE_HSTART
	writel(sp_tcon_para_csi[time_cnt][3], disp_dev->base + TCON_DE_HEND); //DE_HEND

	writel(sp_tcon_para_csi[time_cnt][4], disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
	writel(sp_tcon_para_csi[time_cnt][5], disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND

	writel(sp_tcon_para_csi[time_cnt][6], disp_dev->base + TCON_HSYNC_START); //HSYNC_START
	writel(sp_tcon_para_csi[time_cnt][7], disp_dev->base + TCON_HSYNC_END); //HSYNC_END

	writel(sp_tcon_para_csi[time_cnt][8], disp_dev->base + TCON_DE_VSTART); //DE_VSTART
	writel(sp_tcon_para_csi[time_cnt][9], disp_dev->base + TCON_DE_VEND); //DE_VEND

	writel(sp_tcon_para_csi[time_cnt][10], disp_dev->base + TCON_STVU_START); //VTOP_VSTART
	writel(sp_tcon_para_csi[time_cnt][11], disp_dev->base + TCON_STVU_END); //VTOP_VEND

	/*
	 * TPG(Test Pattern Gen) parameter
	 */
	writel(sp_tcon_tpg_para_csi[time_cnt][4], disp_dev->base + TCON_TPG_HCOUNT);
	value |= (sp_tcon_tpg_para_csi[time_cnt][2] << 12) | sp_tcon_tpg_para_csi[time_cnt][5];
	writel(value, disp_dev->base + TCON_TPG_VCOUNT);
	writel(sp_tcon_tpg_para_csi[time_cnt][6], disp_dev->base + TCON_TPG_HACT_COUNT);
	value = 0;
	value |= (sp_tcon_tpg_para_csi[time_cnt][3] << 12) | sp_tcon_tpg_para_csi[time_cnt][7];
	writel(value, disp_dev->base + TCON_TPG_VACT_COUNT);

	writel(sp_tcon_tpg_para_csi[time_cnt][8], disp_dev->base + TCON_TPG_ALINE_START);

	if (disp_dev->mipitx_format == 0x0)
		;//TBD
	else if (disp_dev->mipitx_format == 0x1)
		writel(0x00000008, disp_dev->base + TCON_DITHER_TVOUT);
	else if (disp_dev->mipitx_format == 0x2)
		writel(0x00000008, disp_dev->base + TCON_DITHER_TVOUT);
	else if (disp_dev->mipitx_format == 0x3)
		;//TBD
	else
		;//TBD
	//writel(sp_tcon_tpg_para_csi[time_cnt][9], disp_dev->base + TCON_DITHER_TVOUT);

}

void sp7350_tcon_timing_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3, value4;

	pr_info("TCON Timing Get\n");
	value1 = readl(disp_dev->base + TCON_OEV_START); //G199.12
	value2 = readl(disp_dev->base + TCON_OEV_END); //G199.13
	pr_info("  [h&v] VSYNC_H %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_HSYNC_START); //G199.23
	value2 = readl(disp_dev->base + TCON_HSYNC_END); //G199.24
	pr_info("  [h&v] HSYNC   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_DE_HSTART); //G199.25
	value2 = readl(disp_dev->base + TCON_DE_HEND); //G199.26
	pr_info("  [h&v] DE_H    %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	//value1 = readl(disp_dev->base + TCON_DE_VSTART); //G199.27
	//value2 = readl(disp_dev->base + TCON_DE_VEND); //G199.28
	//pr_info("  [h&v] DE_V    %04d(0x%04x) %04d(0x%04x)\n",
	//	value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_STVU_START); //G199.20
	value2 = readl(disp_dev->base + TCON_STVU_END); //G199.21
	pr_info("  [h&v] VTOP_V  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);

	value1 = readl(disp_dev->base + TCON_TPG_HCOUNT); //G200.07
	value2 = readl(disp_dev->base + TCON_TPG_VCOUNT); //G200.08
	value3 = readl(disp_dev->base + TCON_TPG_HACT_COUNT); //G200.09
	value4 = readl(disp_dev->base + TCON_TPG_VACT_COUNT); //G200.10
	pr_info("  [TPG] H/VSTEP     %ld %ld\n",
		FIELD_GET(GENMASK(14,12), value2), FIELD_GET(GENMASK(14,12), value4));	
	pr_info("  [TPG] H/V_CNT     %d(0x%04x) %ld(0x%04lx)\n",
		value1, value1, FIELD_GET(GENMASK(11,0), value2), FIELD_GET(GENMASK(11,0), value2));
	pr_info("  [TPG] H/V_ACT_CNT %d(0x%04x) %ld(0x%04lx)\n",
		value3, value3, FIELD_GET(GENMASK(11,0), value4), FIELD_GET(GENMASK(11,0), value4));
	value1 = readl(disp_dev->base + TCON_TPG_ALINE_START); //G200.21
	pr_info("  [TPG] ALINE_START %d(0x%04x)\n",
		value1, value1);

	value1 = readl(disp_dev->base + TCON_DET_HCOUNT); //G200.14
	value2 = readl(disp_dev->base + TCON_VTOTAL); //G200.24
	pr_info("\n  [TPG_DET] HCOUNT VTOTAL %d(0x%04x) %d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_DET_BP_LCNT); //G200.11
	value2 = readl(disp_dev->base + TCON_DET_FP_LCNT); //G200.12
	pr_info("  [TPG_DET] BP/FP_LCNT  %d(0x%04x) %d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_HACT_LATCH); //G200.23
	pr_info("  [TPG_DET] HACT %d(0x%04x)\n",
		value1, value1);
}
