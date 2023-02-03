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
	u32 tcon_width, tcon_height;
	//u32 value = 0;

	tcon_width = 720; //64 128 720 1280 1920 3840
	tcon_height = 480; //64 128 480 576 720 1080 2880

	writel(0x00008127, disp_dev->base + TCON_TCON0);
	writel(0x00008001, disp_dev->base + TCON_TCON1);
	writel(0x00000011, disp_dev->base + TCON_TCON2);
	writel(0x00002002, disp_dev->base + TCON_TCON3);

	if ((tcon_width == 64) && (tcon_height == 64)) {
		writel(0x00000161, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000161, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000063, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000161, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x00000164, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x0000003f, disp_dev->base + TCON_DE_HEND); //DE_HEND
		writel(0x0000001c, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		writel(0x0000005c, disp_dev->base + TCON_DE_VEND); //DE_VEND
	} else if ((tcon_width == 64) && (tcon_height == 2880)) {
		writel(0x00000160, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000160, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000c7f, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000160, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x00000164, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x0000003f, disp_dev->base + TCON_DE_HEND); //DE_HEND
		writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND
	} else if ((tcon_width == 128) && (tcon_height == 128)) {
		writel(0x00000160, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000160, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000095, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000160, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x00000164, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x0000007f, disp_dev->base + TCON_DE_HEND); //DE_HEND
		//writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		//writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND
	} else if ((tcon_width == 720) && (tcon_height == 480)) {
		writel(0x00000352, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000352, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x0000020c, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000352, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x00000356, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x000002cf, disp_dev->base + TCON_DE_HEND); //DE_HEND
		//writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		//writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND
	} else if ((tcon_width == 720) && (tcon_height == 576)) {
		writel(0x00000358, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000358, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000270, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000358, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x0000035c, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x000002cf, disp_dev->base + TCON_DE_HEND); //DE_HEND
		//writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		//writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND		
	} else if ((tcon_width == 1280) && (tcon_height == 720)) {
		writel(0x0000066a, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x0000066a, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x000002ed, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x0000066a, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x0000066e, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x000004ff, disp_dev->base + TCON_DE_HEND); //DE_HEND
		//writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		//writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND	
	} else if ((tcon_width == 1920) && (tcon_height == 1080)) {
		writel(0x00000890, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x00000890, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000464, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x00000890, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x00000894, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x000007ff, disp_dev->base + TCON_DE_HEND); //DE_HEND
		writel(0x00000000, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		writel(0x000007ff, disp_dev->base + TCON_DE_VEND); //DE_VEND			
	} else if ((tcon_width == 3840) && (tcon_height == 64)) {
		writel(0x000011f8, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x000011f8, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000063, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x000011f8, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x000011fc, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x00000eff, disp_dev->base + TCON_DE_HEND); //DE_HEND
		writel(0x0000001c, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		writel(0x0000005d, disp_dev->base + TCON_DE_VEND); //DE_VEND		
	} else if ((tcon_width == 3840) && (tcon_height == 2880)) {
		writel(0x000011f8, disp_dev->base + TCON_OEV_START); //TC_VSYNC_HSTART
		writel(0x000011f8, disp_dev->base + TCON_OEV_END); //TC_VSYNC_HEND
		writel(0x00000c7f, disp_dev->base + TCON_STVU_START); //VTOP_VSTART
		writel(0x00000000, disp_dev->base + TCON_STVU_END); //VTOP_VEND

		writel(0x000011f8, disp_dev->base + TCON_HSYNC_START); //HSYNC_START
		writel(0x000011fc, disp_dev->base + TCON_HSYNC_END); //HSYNC_END

		writel(0x00000000, disp_dev->base + TCON_DE_HSTART); //DE_HSTART
		writel(0x00000eff, disp_dev->base + TCON_DE_HEND); //DE_HEND
		writel(0x00000028, disp_dev->base + TCON_DE_VSTART); //DE_VSTART
		writel(0x00000b69, disp_dev->base + TCON_DE_VEND); //DE_VEND		
	}

	/* TPG(Test Pattern Gen) parameter
	*/
	if ((tcon_width == 64) && (tcon_height == 64)) {
		//writel(0x00000000, disp_dev->base + TCON_F_RST_LINE);
		//writel(0x00000000, disp_dev->base + TCON_RESERVED_200_05);
		//writel(0x00000000, disp_dev->base + TCON_TPG_CTRL);
		writel(0x00000167, disp_dev->base + TCON_TPG_HCOUNT);
		writel(0x00004063, disp_dev->base + TCON_TPG_VCOUNT);
		writel(0x0000003f, disp_dev->base + TCON_TPG_HACT_COUNT);
		writel(0x0000403f, disp_dev->base + TCON_TPG_VACT_COUNT);

		writel(0x000000C1, disp_dev->base + TCON_DITHER_TVOUT);

	} else if ((tcon_width == 64) && (tcon_height == 2880)) {
		writel(0x000000C1, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 128) && (tcon_height == 128)) {
		writel(0x00000049, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 720) && (tcon_height == 480)) {
		writel(0x00000359, disp_dev->base + TCON_TPG_HCOUNT); //857
		writel(0x0000420c, disp_dev->base + TCON_TPG_VCOUNT); //524
		writel(0x000002cf, disp_dev->base + TCON_TPG_HACT_COUNT); //719
		writel(0x000041df, disp_dev->base + TCON_TPG_VACT_COUNT); //479

		writel(0x00000001, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 720) && (tcon_height == 576)) {
		writel(0x00000041, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 1280) && (tcon_height == 720)) {
		writel(0x00000041, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 1920) && (tcon_height == 1080)) {
		writel(0x00000001, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	} else if ((tcon_width == 3840) && (tcon_height == 64)) {
		//TBD
	} else if ((tcon_width == 3840) && (tcon_height == 2880)) {
		writel(0x00000001, disp_dev->base + TCON_DITHER_TVOUT);
		//TBD
	}
	writel(0x00000000, disp_dev->base + TCON_TCON4);
	writel(0x00000004, disp_dev->base + TCON_TCON5);

	//pr_info("tcon G199.00 TCON_TCON0 0x%08x\n", readl(disp_dev->base + TCON_TCON0));
	//pr_info("tcon G199.01 TCON_TCON1 0x%08x\n", readl(disp_dev->base + TCON_TCON1));
	//pr_info("tcon G199.02 TCON_TCON2 0x%08x\n", readl(disp_dev->base + TCON_TCON2));
	//pr_info("tcon G199.03 TCON_TCON3 0x%08x\n", readl(disp_dev->base + TCON_TCON3));
	//pr_info("tcon G200.15 TCON_TCON4 0x%08x\n", readl(disp_dev->base + TCON_TCON4));
	//pr_info("tcon G200.26 TCON_TCON5 0x%08x\n", readl(disp_dev->base + TCON_TCON5));
	//pr_info("tcon G200.00 GAMMA 0x%08x\n", readl(disp_dev->base + TCON_GAMMA0));
	//pr_info("tcon G200.06 TPG 0x%08x\n", readl(disp_dev->base + TCON_TPG_CTRL));
	//pr_info("tcon G200.22 DITHER 0x%08x\n", readl(disp_dev->base + TCON_DITHER_TVOUT));
}

void sp7350_tcon_reg_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, j;

	for (j = 0; j < 5; j++) {
		pr_info("TCON G%d Dump info\n", j+199);
		for (i = 0; i < 8; i++) {
			pr_info("0x%08x 0x%08x 0x%08x 0x%08x\n",
					readl(disp_dev->base + (j << 7) + TCON_TCON0 + (i * 4 + 0) * 4),
					readl(disp_dev->base + (j << 7) + TCON_TCON0 + (i * 4 + 1) * 4),
					readl(disp_dev->base + (j << 7) + TCON_TCON0 + (i * 4 + 2) * 4),
					readl(disp_dev->base + (j << 7) + TCON_TCON0 + (i * 4 + 3) * 4));
		}
	}
}
EXPORT_SYMBOL(sp7350_tcon_reg_info);

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
	pr_info("  RGB OUT[%s]\n", FIELD_GET(GENMASK(15,15), tcon0_value)?"RGB888":"RGB666");
	pr_info("  DITHER_YUV_EN[%s]\n", FIELD_GET(GENMASK(6,6), tcon0_value)?"O":"X");
	pr_info("  HVIF_EN[%s]\n", FIELD_GET(GENMASK(1,1), tcon0_value)?"HVsync_DE":"Pulse");
	pr_info("  OUT_PACKAGE[%s]\n",  tcon_out_package[FIELD_GET(GENMASK(5,3), tcon0_value)]);

	tcon1_value = readl(disp_dev->base + TCON_TCON1); //G199.01
	pr_info("G199.01 TC_TCON1 0x%08x\n", tcon1_value);
	pr_info("  [%s] TCON Module\n", FIELD_GET(GENMASK(15,15), tcon1_value)?"ON":"OFF");
	pr_info("  YU_SWAP[%s] UV_SWAP[%s]\n",
		FIELD_GET(GENMASK(0,0), tcon0_value)?"O":"X",
		FIELD_GET(GENMASK(4,4), tcon1_value)?"O":"X");

	tcon2_value = readl(disp_dev->base + TCON_TCON2); //G199.02
	pr_info("G199.02 TC_TCON2 0x%08x\n", tcon2_value);
	pr_info("  TC_CHP1_POL[%s]\n",
		FIELD_GET(GENMASK(2,2), tcon2_value)?"P":"N");
	pr_info("  horizon fileter[%s]\n",
		FIELD_GET(GENMASK(0,0), tcon2_value)?"[0,1/4,1/2,1/4,0]":"[0,0,1,0,0]");

	tcon3_value = readl(disp_dev->base + TCON_TCON3); //G199.03
	pr_info("G199.03 TC_TCON3 0x%08x\n", tcon3_value);
	pr_info("  Digital RGB OUT[%s]\n",
		FIELD_GET(GENMASK(11,11), tcon3_value)?"X":"O");
	pr_info("  POL Set[%s][%s][%s][%s][%s][%s]\n",
		FIELD_GET(GENMASK(5,5), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(4,4), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(3,3), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(2,2), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(1,1), tcon3_value)?"P":"N",
		FIELD_GET(GENMASK(0,0), tcon3_value)?"P":"N");

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

	/* TCON Parameter list
	*/
	pr_info("[TCON Parameter list]\n");
	value1 = readl(disp_dev->base + TCON_STR_LINE); //G199.04
	value2 = readl(disp_dev->base + TCON_END_LINE); //G199.06
	value3 = readl(disp_dev->base + TCON_STV_SLINE); //G199.08
	pr_info("  [%s] Act LStart/End  %04d(0x%04x) %04d(0x%04x)\n",
		FIELD_GET(GENMASK(6,6), tcon1_value)?"O":"X",value1, value1, value2, value2);
	pr_info("  [%s] STV L num %04d(0x%04x)\n",
		FIELD_GET(GENMASK(5,5), tcon1_value)?"O":"X",value3, value3);

	value = readl(disp_dev->base + TCON_F_RST_LINE); //G200.04
	pr_info("  [%s] VCOUNT rst L num %04d(0x%04x)\n",
		FIELD_GET(GENMASK(7,7), tcon1_value)?"O":"X",value, value);

	value1 = readl(disp_dev->base + TCON_OEH_START); //G199.10
	value2 = readl(disp_dev->base + TCON_OEH_END); //G199.11
	pr_info("  [-] OEH_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_OEV_START); //G199.12
	value2 = readl(disp_dev->base + TCON_OEV_END); //G199.13
	pr_info("  [O] OEV_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_VCOM_START); //G199.16
	value2 = readl(disp_dev->base + TCON_VCOM_END); //G199.17
	pr_info("  [-] VCOM_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_CKV_START); //G199.18
	value2 = readl(disp_dev->base + TCON_CKV_END); //G199.19
	pr_info("  [-] CKV_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_STVU_START); //G199.20
	value2 = readl(disp_dev->base + TCON_STVU_END); //G199.21
	pr_info("  [-] STVU_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_STHL_START); //G199.31
	value2 = readl(disp_dev->base + TCON_STHL_END); //G199.22
	pr_info("  [-] STHL_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_HSYNC_START); //G199.23
	value2 = readl(disp_dev->base + TCON_HSYNC_END); //G199.24
	pr_info("  [O] HSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
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

}
EXPORT_SYMBOL(sp7350_tcon_decrypt_info);

void sp7350_tcon_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 tcon1_value, value, value1, value2, value3;

	tcon1_value = readl(disp_dev->base + TCON_TCON1); //G199.01
	pr_info("TCON resolution chk\n");
	/* TCON Parameter list
	*/
	pr_info("  [Parameter list]\n");
	value1 = readl(disp_dev->base + TCON_STR_LINE); //G199.04
	value2 = readl(disp_dev->base + TCON_END_LINE); //G199.06
	value3 = readl(disp_dev->base + TCON_STV_SLINE); //G199.08
	pr_info("  [%s] Act LStart/End  %04d(0x%04x) %04d(0x%04x)\n",
		FIELD_GET(GENMASK(6,6), tcon1_value)?"O":"X",value1, value1, value2, value2);
	pr_info("  [%s] STV L num %04d(0x%04x)\n",
		FIELD_GET(GENMASK(5,5), tcon1_value)?"O":"X",value3, value3);

	value = readl(disp_dev->base + TCON_F_RST_LINE); //G200.04
	pr_info("  [%s] VCOUNT rst L num %04d(0x%04x)\n",
		FIELD_GET(GENMASK(7,7), tcon1_value)?"O":"X",value, value);

	value1 = readl(disp_dev->base + TCON_OEH_START); //G199.10
	value2 = readl(disp_dev->base + TCON_OEH_END); //G199.11
	pr_info("  [-] OEH_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_OEV_START); //G199.12
	value2 = readl(disp_dev->base + TCON_OEV_END); //G199.13
	pr_info("  [O] OEV_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_VCOM_START); //G199.16
	value2 = readl(disp_dev->base + TCON_VCOM_END); //G199.17
	pr_info("  [-] VCOM_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_CKV_START); //G199.18
	value2 = readl(disp_dev->base + TCON_CKV_END); //G199.19
	pr_info("  [-] CKV_START/END   %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_STVU_START); //G199.20
	value2 = readl(disp_dev->base + TCON_STVU_END); //G199.21
	pr_info("  [-] STVU_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_STHL_START); //G199.31
	value2 = readl(disp_dev->base + TCON_STHL_END); //G199.22
	pr_info("  [-] STHL_START/END  %04d(0x%04x) %04d(0x%04x)\n",
		value1, value1, value2, value2);
	value1 = readl(disp_dev->base + TCON_HSYNC_START); //G199.23
	value2 = readl(disp_dev->base + TCON_HSYNC_END); //G199.24
	pr_info("  [O] HSYNC_START/END %04d(0x%04x) %04d(0x%04x)\n",
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

	value1 = readl(disp_dev->base + TCON_TPG_ALINE_START); //G200.21
	pr_info("  [TPG] ALINE_START %ld(0x%04lx)\n",
		FIELD_GET(GENMASK(11,0), value1), FIELD_GET(GENMASK(11,0), value1));
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
	//writel((value | tcon_bist_pat << 2 | bist_mode << 0), disp_dev->base + TCON_TPG_CTRL);

}
EXPORT_SYMBOL(sp7350_tcon_bist_set);
