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

void sp7350_mipitx_init(void)
{
	;//TBD
}

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

	value = readl(disp_dev->base + MIPITX_INFO_STATUS); //G204.28
	pr_info("  MIPITX_INFO_STATUS MIPITX %s (TX_PHY %s)\n",
		FIELD_GET(GENMASK(24,24), value)?"ON":"OFF",
		FIELD_GET(GENMASK(0,0), value)?"X":"O");

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

	value = readl(disp_dev->base + MIPITX_INFO_STATUS); //G204.28
	pr_info("  MIPITX_INFO_STATUS MIPITX %s (TX_PHY %s)\n",
		FIELD_GET(GENMASK(24,24), value)?"ON":"OFF",
		FIELD_GET(GENMASK(0,0), value)?"X":"O");

	value = readl(disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
	pr_info("  ULPS wake up delay=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));

}
EXPORT_SYMBOL(sp7350_mipitx_resolution_chk);

void sp7350_mipitx_timing_set(void)
{
	;//TBD
}

void sp7350_mipitx_timing_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, mipitx_csi_dt, mipitx_csi_fe;
	u32 mipitx_mode, mipitx_mode1, mipitx_analog_en;

	pr_info("MIPITX Timing Get\n");
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

	value = readl(disp_dev->base + MIPITX_INFO_STATUS); //G204.28
	pr_info("  MIPITX_INFO_STATUS MIPITX %s (PHY %s)\n",
		FIELD_GET(GENMASK(24,24), value)?"ON":"OFF",
		FIELD_GET(GENMASK(0,0), value)?"X":"O");

	value = readl(disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
	pr_info("  ULPS wake up delay=%04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(15,0), value), FIELD_GET(GENMASK(15,0), value));
}

static const u32 sp7350_pllh_pstdiv_int[] = {
	25, 30, 35, 40, 50, 55, 60, 70, 75, 80, 90, 100, 105, 110, 120, 125
};

static const u32 sp7350_pllh_mipitx_sel_int[] = {
	1,2,0,4,0,0,0,8,0,0,0,0,0,0,0,16,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void sp7350_mipitx_pllclk_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;
	u32 tmp_value1, tmp_value2, tmp_value3;

	value1 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_14);
	value2 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_25);

	tmp_value1 = 25 * ((FIELD_GET(GENMASK(15,15), value1)?2:1) * 
		(FIELD_GET(GENMASK(14,7), value1) + 64)) /
		(FIELD_GET(GENMASK(2,1), value1)?2:1);
	tmp_value2 = (tmp_value1 *10 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	tmp_value3 = (tmp_value1 *1000 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));

	pr_info("     PLLH FVCO %04d MHz , pix_clk %03d.%02d MHz\n",
		tmp_value1, tmp_value2, (tmp_value3 - tmp_value2*100));

}

static const char * const sp7350_txpll_prediv[] = {
	"DIV1", "DIV2", "DIV5", "DIV8",
};
static const u32 sp7350_txpll_prediv_int[] = {
	1,2,5,8
};

static const char * const sp7350_txpll_pstdiv[] = {
	"DIV1", "DIV2", "DIV4", "DIV8", "DIV16"
};
static const u32 sp7350_txpll_pstdiv_int[] = {
	1, 2, 4, 8, 16
};

static const char * const sp7350_txpll_endiv5[] = {
	"DIV1", "DIV5"
};
static const u32 sp7350_txpll_endiv5_int[] = {
	1, 5
};

void sp7350_mipitx_txpll_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;
	u32 tmp_value1, tmp_value2, tmp_value3;

	value1 = readl(disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11

	tmp_value1 = 25 * ((FIELD_GET(GENMASK(4,4), value1)?2:1) * 
		(FIELD_GET(GENMASK(13,8), value1))) /
		(sp7350_txpll_prediv_int[FIELD_GET(GENMASK(1,0), value1)]);

	tmp_value2 = (tmp_value1)/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)]));
	tmp_value3 = (tmp_value1 *100 )/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)]));
	pr_info("    TXPLL FVCO %04d MHz , bit_clk %03d.%02d MHz\n", 
		tmp_value1, tmp_value2, (tmp_value3 - (tmp_value2*100)));

	tmp_value2 = (tmp_value1)/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)])*8);
	tmp_value3 = (tmp_value1 *100 )/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)])*8);
	pr_info("    TXPLL ---- ---- --- , byteclk %03d.%02d MHz\n", 
		tmp_value2, (tmp_value3 - (tmp_value2*100)));

	value2 = readl(disp_dev->base + MIPITX_LP_CK); //G204.04
	tmp_value2 = (tmp_value1)/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)])*8*(FIELD_GET(GENMASK(5,0), value2)+1));
	tmp_value3 = (tmp_value1 *100 )/ ((sp7350_txpll_pstdiv_int[FIELD_GET(GENMASK(18,16), value1)]) * 
		(sp7350_txpll_endiv5_int[FIELD_GET(GENMASK(20,20), value1)])*8*(FIELD_GET(GENMASK(5,0), value2)+1));
	pr_info("    TXPLL ---- ---- --- , LPCDclk %03d.%02d MHz\n", 
		tmp_value2, (tmp_value3 - (tmp_value2*100)));

}

void sp7350_mipitx_pllclk_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2;

	value = 0x80000000; //init clock
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14

	//init PLLH setting
	value = 0xffff40be; //PLLH BNKSEL = 0x2 (2000~2500MHz)
	writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_15); //AO_G3.15
	value = 0xffff0009; //PLLH
	writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_16); //AO_G3.16
	/*
	 * PLLH Fvco = 2150MHz (fixed)
	 *                             2150
	 * MIPITX pixel CLK = ----------------------- = 59.72MHz
	 *                     PST_DIV * MIPITX_SEL
	 */
	value = 0xffff0b50; //PLLH PST_DIV = div9 (default)
	writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_14); //G205.14
	value = 0x07800180; //PLLH MIPITX_SEL = div4
	writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_25); //G205.25

	//init TXPLL setting
	value = 0x00000003; //TXPLL enable and reset
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL5); //G205.10
	/*
	 * PRESCAL = 1, FBKDIV = 48, PRE_DIV = 1, EN_DIV5 = 0, PRE_DIV = 2, POST_DIV = 1
	 *                    25 * PRESCAL * FBKDIV            25 * 48
	 * MIPITX bit CLK = ------------------------------ = ----------- = 600MHz
	 *                   PRE_DIV * POST_DIV * 5^EN_DIV5       2
	 */
	value1 = 0x00003001; //TXPLL MIPITX CLK = 600MHz
	value2 = 0x00000140; //TXPLL BNKSEL = 0x0 (320~640MHz)
	writel(value1, disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11
	writel(value2, disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12
	/*
	 *                      600  
	 * MIPITX LP CLK = ------------ = 8.3MHz
	 *                   8 * div9
	 */
	value = 0x00000008; //(600/8/div9)=8.3MHz
	writel(value, disp_dev->base + MIPITX_LP_CK); //G204.04

	value = 0x00000000; //init clock done
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14

	//sp7350_mipitx_pllclk_get();
	//sp7350_mipitx_txpll_get();

}

/*
 * sp_mipitx_phy_pllclk_dsi[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-7, MIPITX PRESCALE & FBKDIV & PREDIV & POSTDIV & EN_DIV5 & BNKSEL(TXPLL)
 * y = 8-10, MIPITX PSTDIV & MIPITX_SEL & BNKSEL (PLLH)
 *
 * XTAL--[PREDIV]--------------------------->[EN_DIV5]--[POSTDIV]-->Fckout
 *                 |                       |
 *                 |<--FBKDIV<--PRESCALE<--|
 *
 *                25 * PRESCALE * FBKDIV
 *    Fckout = -----------------------------
 *              PREDIV * POSTDIV * 5^EN_DIV5
 */
static const u32 sp_mipitx_phy_pllclk_dsi[11][11] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   Q1   Q2   Q3 */
	{ 720,  480, 0x0, 0x1B, 0x0, 0x2, 0x0, 0x1, 0x4, 0xf, 0x0}, /* 480P */
	{ 720,  576, 0x0, 0x20, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 576P */
	{1280,  720, 0x0, 0x24, 0x0, 0x1, 0x0, 0x1, 0x8, 0x3, 0x0}, /* 720P */
	{1920, 1080, 0x0, 0x24, 0x0, 0x0, 0x0, 0x1, 0x8, 0x1, 0x1}, /* 1080P */
	{ 480, 1280, 0x0, 0x0c, 0x0, 0x0, 0x0, 0x0, 0x5, 0x7, 0x0}, /* 480x1280 */
	{ 128,  128, 0x1, 0x1f, 0x1, 0x4, 0x1, 0x1, 0x0, 0x0, 0x0}, /* 128x128 */
	{  64, 2880, 0x0, 0x29, 0x0, 0x0, 0x1, 0x2, 0x0, 0x0, 0x0}, /* 64x2880 */
	{3840,   64, 0x0, 0x1f, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 3840x64 */
	{3840, 2880, 0x0, 0x3c, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0}, /* 3840x2880 */
	{ 800,  480, 0x0, 0x23, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 800x480 */
	{1024,  600, 0x1, 0x3d, 0x1, 0x1, 0x1, 0x3, 0x0, 0x0, 0x0}  /* 1024x600 */
};

/*
 * sp_mipitx_phy_pllclk_csi[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-7, MIPITX PRESCALE & FBKDIV & PREDIV & POSTDIV & EN_DIV5 & BNKSEL(TXPLL)
 * y = 8-10, MIPITX PSTDIV & MIPITX_SEL & BNKSEL (PLLH)
 *
 * XTAL--[PREDIV]--------------------------->[EN_DIV5]--[POSTDIV]-->Fckout
 *                 |                       |
 *                 |<--FBKDIV<--PRESCALE<--|
 *
 *                25 * PRESCALE * FBKDIV
 *    Fckout = -----------------------------
 *              PREDIV * POSTDIV * 5^EN_DIV5
 */
static const u32 sp_mipitx_phy_pllclk_csi[7][11] = {
	/* (w   h)   P1   P2    P3   P4   P5   P6   Q1   Q2   Q3 */
	{  64,   64, 0x0, 0x29, 0x1, 0x4, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 64x64 */
	{ 128,  128, 0x1, 0x1f, 0x1, 0x4, 0x1, 0x1, 0x0, 0x0, 0x0}, /* 128x128 */
	{ 720,  480, 0x0, 0x0c, 0x0, 0x0, 0x0, 0x0, 0x5, 0x7, 0x0}, /* 480P */
	{ 720,  576, 0x0, 0x20, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0}, /* 576P */
	{1280,  720, 0x0, 0x0c, 0x0, 0x0, 0x0, 0x0, 0x5, 0x7, 0x0}, /* 720P */
	{1920, 1080, 0x0, 0x0c, 0x0, 0x0, 0x0, 0x0, 0x5, 0x7, 0x0}, /* 1080P */
	{3840, 2880, 0x0, 0x0c, 0x0, 0x0, 0x0, 0x0, 0x5, 0x7, 0x0}, /* 3840x2880 */
};

void sp7350_mipitx_pllclk_set(int mode, int width, int height)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value;

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {

		for (i = 0; i < 11; i++) {
			if ((sp_mipitx_phy_pllclk_dsi[i][0] == disp_dev->out_res.width) &&
				(sp_mipitx_phy_pllclk_dsi[i][1] == disp_dev->out_res.height)) {
					time_cnt = i;
					break;
			}
		}

		value = 0;
		value |= (0x00780000 | (sp_mipitx_phy_pllclk_dsi[time_cnt][8] << 3));
		writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_14); //AO_G3.14

		value = 0;
		value |= (0x07800000 | (sp_mipitx_phy_pllclk_dsi[time_cnt][9] << 7));
		writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_25); //AO_G3.25

		value = 0x00000000;
		value |= (SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_dsi[time_cnt][6]) |
				SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_dsi[time_cnt][5]) |
				SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_dsi[time_cnt][3]) |
				SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_dsi[time_cnt][2]) |
				SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_dsi[time_cnt][4]));
		writel(value, disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11

		value = readl(disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12
		value &= ~(SP7350_MIPITX_MIPI_PHY_BNKSEL_MASK);
		value |= SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_dsi[time_cnt][7]);
		writel(value, disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12

	} else {

		for (i = 0; i < 7; i++) {
			if ((sp_mipitx_phy_pllclk_csi[i][0] == disp_dev->out_res.width) &&
				(sp_mipitx_phy_pllclk_csi[i][1] == disp_dev->out_res.height)) {
					time_cnt = i;
					break;
			}
		}
		value = 0;
		value |= (0x00780000 | (sp_mipitx_phy_pllclk_csi[time_cnt][8] << 3));
		writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_14); //AO_G3.14

		value = 0;
		value |= (0x07800000 | (sp_mipitx_phy_pllclk_csi[time_cnt][9] << 7));
		writel(value, disp_dev->ao_moon3 + MIPITX_AO_MOON3_25); //AO_G3.25

		value = 0x00000000;
		value |= (SP7350_MIPITX_MIPI_PHY_EN_DIV5(sp_mipitx_phy_pllclk_csi[time_cnt][6]) |
				SP7350_MIPITX_MIPI_PHY_POSTDIV(sp_mipitx_phy_pllclk_csi[time_cnt][5]) |
				SP7350_MIPITX_MIPI_PHY_FBKDIV(sp_mipitx_phy_pllclk_csi[time_cnt][3]) |
				SP7350_MIPITX_MIPI_PHY_PRESCALE(sp_mipitx_phy_pllclk_csi[time_cnt][2]) |
				SP7350_MIPITX_MIPI_PHY_PREDIV(sp_mipitx_phy_pllclk_csi[time_cnt][4]));
		writel(value, disp_dev->base + MIPITX_ANALOG_CTRL6); //G205.11

		value = readl(disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12
		value &= ~(SP7350_MIPITX_MIPI_PHY_BNKSEL_MASK);
		value |= SP7350_MIPITX_MIPI_PHY_BNKSEL(sp_mipitx_phy_pllclk_csi[time_cnt][7]);
		writel(value, disp_dev->base + MIPITX_ANALOG_CTRL7); //G205.12

	}
}

#define MIPITX_CMD_FIFO_FULL 0x00000001
#define MIPITX_CMD_FIFO_EMPTY 0x00000010
#define MIPITX_DATA_FIFO_FULL 0x00000100
#define MIPITX_DATA_FIFO_EMPTY 0x00001000

void check_cmd_fifo_full(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int mipitx_fifo_timeout = 0;
	u32 value = 0;

	value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
	//pr_info("fifo_status 0x%08x\n", value);
	while((value & MIPITX_CMD_FIFO_FULL) == MIPITX_CMD_FIFO_FULL) {
		if(mipitx_fifo_timeout > 10000) //over 1 second
		{
			pr_info("cmd fifo full timeout\n");
			break;
		}
		value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
		++mipitx_fifo_timeout;
		udelay(100);
	}
}

void check_cmd_fifo_empty(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int mipitx_fifo_timeout = 0;
	u32 value = 0;

	value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
	//pr_info("fifo_status 0x%08x\n", value);
	while((value & MIPITX_CMD_FIFO_EMPTY) != MIPITX_CMD_FIFO_EMPTY) {
		if(mipitx_fifo_timeout > 10000) //over 1 second
		{
			pr_info("cmd fifo empty timeout\n");
			break;
		}
		value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
		++mipitx_fifo_timeout;
		udelay(100);
	}
}

void check_data_fifo_full(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int mipitx_fifo_timeout = 0;
	u32 value = 0;

	value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
	//pr_info("fifo_status 0x%08x\n", value);
	while((value & MIPITX_DATA_FIFO_FULL) == MIPITX_DATA_FIFO_FULL) {
		if(mipitx_fifo_timeout > 10000) //over 1 second
		{
			pr_info("data fifo full timeout\n");
			break;
		}
		value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
		++mipitx_fifo_timeout;
		udelay(100);
	}
}

void check_data_fifo_empty(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int mipitx_fifo_timeout = 0;
	u32 value = 0;

	value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
	//pr_info("fifo_status 0x%08x\n", value);
	while((value & MIPITX_DATA_FIFO_EMPTY) != MIPITX_DATA_FIFO_EMPTY) {
		if(mipitx_fifo_timeout > 10000) //over 1 second
		{
			pr_info("data fifo empty timeout\n");
			break;
		}
		value = readl(disp_dev->base + MIPITX_CMD_FIFO); //G204.16
		++mipitx_fifo_timeout;
		udelay(100);
	}
}

void sp7350_mipitx_gpio_set(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (!IS_ERR(disp_dev->reset_gpio)) {
		/* reset panel */
		#if 0 //active low
		gpiod_set_value(disp_dev->reset_gpio, 0);
		mdelay(15);
		gpiod_set_value(disp_dev->reset_gpio, 1);
		mdelay(15);
		gpiod_set_value(disp_dev->reset_gpio, 0);
		mdelay(25);
		#else //active high
		gpiod_set_value(disp_dev->reset_gpio, 0);
		mdelay(15);
		gpiod_set_value(disp_dev->reset_gpio, 1);
		mdelay(25);
		#endif
	}

}

/*
 * MIPI DSI (Display Command Set) for SP7350
 */
static void sp7350_dcs_write_buf(const void *data, size_t len)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i;
	u8 *data1;
	u32 value, data_cnt;

	data1 = (u8 *)data;

	if (len == 0) {
		check_cmd_fifo_full();
		value = 0x00000003;
		writel(value, disp_dev->base + MIPITX_SPKT_HEAD); //G204.09
	} else if (len == 1) {
		check_cmd_fifo_full();
		value = 0x00000013 | (data1[0] << 8);
		writel(value, disp_dev->base + MIPITX_SPKT_HEAD); //G204.09
	} else if (len == 2) {
		check_cmd_fifo_full();
		value = 0x00000023 | (data1[0] << 8) | (data1[1] << 16);
		writel(value, disp_dev->base + MIPITX_SPKT_HEAD); //G204.09
	} else if ((len >= 3) && (len <= 64)) {
		check_cmd_fifo_full();
		value = 0x00000029 | ((u32)len << 8);
		writel(value, disp_dev->base + MIPITX_LPKT_HEAD); //G204.10

		if (len % 4) data_cnt = ((u32)len / 4) + 1;
		else data_cnt = ((u32)len / 4);

		for (i = 0; i < data_cnt; i++) {
			check_data_fifo_full();
			value = 0x00000000;
			if (i * 4 + 0 >= len) data1[i * 4 + 0] = 0x00;
			if (i * 4 + 1 >= len) data1[i * 4 + 1] = 0x00;
			if (i * 4 + 2 >= len) data1[i * 4 + 2] = 0x00;
			if (i * 4 + 3 >= len) data1[i * 4 + 3] = 0x00;
			value |= ((data1[i * 4 + 3] << 24) | (data1[i * 4 + 2] << 16) |
				 (data1[i * 4 + 1] << 8) | (data1[i * 4 + 0] << 0));
			writel(value, disp_dev->base + MIPITX_LPKT_PAYLOAD); //G204.11
		}
	} else {
		pr_info("data length over %ld\n", len);
	}
}

#define sp7350_dcs_write_seq(seq...)			\
({							\
	static const u8 d[] = { seq };			\
	sp7350_dcs_write_buf(d, ARRAY_SIZE(d));	\
})

void sp7350_mipitx_panel_init(int mipitx_dev_id, int width, int height)
{

	//pr_info("mipitx id 0x%08x\n", mipitx_dev_id);

	if (mipitx_dev_id == 0x00001000) {
		pr_info("MIPITX DSI Panel : HXM0686TFT-001(%dx%d)\n", width, height);
		//Panel HXM0686TFT-001 IPS
		sp7350_mipitx_gpio_set();

		sp7350_dcs_write_seq(0xB9, 0xF1, 0x12, 0x87);
		sp7350_dcs_write_seq(0xB2, 0x40, 0x05, 0x78);
		sp7350_dcs_write_seq(0xB3, 0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00,
							0x00, 0x00, 0x00);
		sp7350_dcs_write_seq(0xB4, 0x80);
		sp7350_dcs_write_seq(0xB5, 0x09, 0x09);
		sp7350_dcs_write_seq(0xB6, 0x8D, 0x8D);
		sp7350_dcs_write_seq(0xB8, 0x26, 0x22, 0xF0, 0x63);
		sp7350_dcs_write_seq(0xBA, 0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44,
							0x25, 0x00, 0x91, 0x0A, 0x00, 0x00, 0x01, 0x4F,
							0x01, 0x00, 0x00, 0x37);
		sp7350_dcs_write_seq(0xBC, 0x47);
		sp7350_dcs_write_seq(0xBF, 0x02, 0x10, 0x00, 0x80, 0x04);
		sp7350_dcs_write_seq(0xC0, 0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12,
							0x70, 0x00);
		sp7350_dcs_write_seq(0xC1, 0x65, 0xC0, 0x32, 0x32, 0x77, 0xF4, 0x77,
							0x77, 0xCC, 0xCC, 0xFF, 0xFF, 0x11, 0x11, 0x00,
							0x00, 0x32);
		sp7350_dcs_write_seq(0xC7, 0x10, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
							0x00, 0xED, 0xC7, 0x00, 0xA5);
		sp7350_dcs_write_seq(0xC8, 0x10, 0x40, 0x1E, 0x03);
		sp7350_dcs_write_seq(0xCC, 0x0B);
		sp7350_dcs_write_seq(0xE0, 0x00, 0x04, 0x06, 0x32, 0x3F, 0x3F, 0x36,
							0x34, 0x06, 0x0B, 0x0E, 0x11, 0x11, 0x10, 0x12,
							0x10, 0x13, 0x00, 0x04, 0x06, 0x32, 0x3F, 0x3F,
							0x36, 0x34, 0x06, 0x0B, 0x0E, 0x11, 0x11, 0x10,
							0x12, 0x10, 0x13);
		sp7350_dcs_write_seq(0xE1, 0x11, 0x11, 0x91, 0x00, 0x00, 0x00, 0x00);
		sp7350_dcs_write_seq(0xE3, 0x03, 0x03, 0x0B, 0x0B, 0x00, 0x03, 0x00,
							0x00, 0x00, 0x00, 0xFF, 0x84, 0xC0, 0x10);
		sp7350_dcs_write_seq(0xE9, 0xC8, 0x10, 0x06, 0x05, 0x18, 0xD2, 0x81,
							0x12, 0x31, 0x23, 0x47, 0x82, 0xB0, 0x81, 0x23,
							0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
							0x04, 0x04, 0x00, 0x00, 0x00, 0x88, 0x0B, 0xA8,
							0x10, 0x32, 0x4F, 0x88, 0x88, 0x88, 0x88, 0x88,
							0x88, 0x0B, 0xA8, 0x10, 0x32, 0x4F, 0x88, 0x88,
							0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		sp7350_dcs_write_seq(0xEA, 0x96, 0x0C, 0x01, 0x01, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x4B, 0xA8,
							0x23, 0x01, 0x08, 0xF8, 0x88, 0x88, 0x88, 0x88,
							0x88, 0x4B, 0xA8, 0x23, 0x01, 0x08, 0xF8, 0x88,
							0x88, 0x88, 0x88, 0x23, 0x10, 0x00, 0x00, 0x92,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
							0x80, 0x81, 0x00, 0x00, 0x00, 0x00);
		sp7350_dcs_write_seq(0xEF, 0xFF, 0xFF, 0x01);

		sp7350_dcs_write_seq(0x36, 0x14);
		sp7350_dcs_write_seq(0x35, 0x00);
		sp7350_dcs_write_seq(0x11);
		mdelay(120);
		sp7350_dcs_write_seq(0x29);
		mdelay(20);
	} else {
		pr_info("undefined mipitx id 0x%08x\n", mipitx_dev_id);
	}
}

void sp7350_mipitx_phy_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = 0x00101330; //PHY Reset(under reset)
	if (disp_dev->mipitx_clk_edge)
		value |= SP7350_MIPITX_MIPI_PHY_CLK_EDGE_SEL(SP7350_MIPITX_FALLING);
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06

	if (disp_dev->mipitx_lane == 1)
		value = 0x11000001; //lane num = 1 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 2)
		value = 0x11000011; //lane num = 2 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 4)
		value = 0x11000031; //lane num = 4 and DSI_EN and ANALOG_EN
	writel(value, disp_dev->base + MIPITX_CORE_CTRL); //G204.15
	
	value = 0x00000000;
	if (disp_dev->mipitx_sync_timing)
		value |= SP7350_MIPITX_FORMAT_VTF_SET(SP7350_MIPITX_VTF_SYNC_EVENT);

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {
		if (disp_dev->mipitx_format == 0x0)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB565);
		else if (disp_dev->mipitx_format == 0x1)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB666_18BITS);
		else if (disp_dev->mipitx_format == 0x2)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB666_24BITS);
		else if (disp_dev->mipitx_format == 0x3)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB888);
		else
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_DSI_RGB888);
	} else { //if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {
		if (disp_dev->mipitx_format == 0x0)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_16BITS);
		else if (disp_dev->mipitx_format == 0x1)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_16BITS);
		else if (disp_dev->mipitx_format == 0x2)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_YUV422_20BITS);
		else if (disp_dev->mipitx_format == 0x3)
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_24BITS);
		else
			value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_24BITS);
	}
	writel(value, disp_dev->base + MIPITX_FORMAT_CTRL); //G204.12

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2);
	value |= SP7350_MIPITX_NORMAL; //PHY Reset(under normal mode)
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06
}

/*
 * sp_mipitx_output_timing[x]
 * x = 0-10, MIPITX phy
 *   T_HS-EXIT / T_LPX
 *   T_CLK-PREPARE / T_CLK-ZERO
 *   T_CLK-TRAIL / T_CLK-PRE / T_CLK-POST
 *   T_HS-TRAIL / T_HS-PREPARE / T_HS-ZERO
 */
static const u32 sp_mipitx_output_timing[10] = {
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
};

void sp7350_mipitx_lane_control_set(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value = 0;

	value |= ((sp_mipitx_output_timing[0] << 16) |
			(sp_mipitx_output_timing[1] << 0));
	writel(value, disp_dev->base + MIPITX_LANE_TIME_CTRL); //G204.05

	value = 0;
	value |= ((sp_mipitx_output_timing[2] << 16) |
			(sp_mipitx_output_timing[3] << 0));
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL0); //G204.06

	value = 0;
	value |= ((sp_mipitx_output_timing[4] << 25) |
			(sp_mipitx_output_timing[5] << 16) |
			(sp_mipitx_output_timing[6] << 0));
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL1); //G204.07

	value = 0;
	value |= ((sp_mipitx_output_timing[7] << 25) |
			(sp_mipitx_output_timing[8] << 16) |
			(sp_mipitx_output_timing[9] << 0));
	writel(value, disp_dev->base + MIPITX_DATA_TIME_CTRL0); //G204.08

	value = 0x00001100; //MIPITX Blanking Mode
	writel(value, disp_dev->base + MIPITX_BLANK_POWER_CTRL); //G204.13

	value = 0x00000001; //MIPITX CLOCK CONTROL (CK_HSEN)
	writel(value, disp_dev->base + MIPITX_CLK_CTRL); //G204.30
}

void sp7350_mipitx_cmd_mode_start(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = 0x00100000; //enable command transfer at LP mode
	writel(value, disp_dev->base + MIPITX_OP_CTRL); //G204.14

	if (disp_dev->mipitx_lane == 1)
		value = 0x11000003; //lane num = 1 and command mode start
	else if (disp_dev->mipitx_lane == 2)
		value = 0x11000013; //lane num = 2 and command mode start
	else if (disp_dev->mipitx_lane == 4)
		value = 0x11000033; //lane num = 4 and command mode start
	writel(value, disp_dev->base + MIPITX_CORE_CTRL); //G204.15

	value = 0x00520004; //TA GET/SURE/GO
	writel(value, disp_dev->base + MIPITX_BTA_CTRL); //G204.17

	//value = 0x0000c350; //fix
	//value = 0x000000af; //fix
	//value = 0x00000aff; //default
	//writel(value, disp_dev->base + MIPITX_ULPS_DELAY); //G204.29
}

/*
 * sp_mipitx_input_timing_dsi[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-9, MIPITX HSA & HFP & HBP & HACT & VSA & VFP & VBP & VACT
 */
static const u32 sp_mipitx_input_timing_dsi[11][10] = {
	/* (w   h)   HSA  HFP HBP HACT VSA  VFP  VBP   VACT */
	{ 720,  480, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  480}, /* 480P */
	{ 720,  576, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x2B,  576}, /* 576P */
	{1280,  720, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x18,  720}, /* 720P */
	{1920, 1080, 0x4,  0, 0x5,  0, 0x1, 0x3, 0x28, 1080}, /* 1080P */
	{ 480, 1280, 0x4,  0, 0x4,  0, 0x1,0x10, 0x10, 1280}, /* 480x1280 */
	{ 128,  128, 0x4,  0, 0x5,  0, 0x1, 0x2, 0x12,  128}, /* 128x128 */
	{  64, 2880, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x28, 2880}, /* 64x2880 */
	{3840,   64, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x1c,   64}, /* 3840x64 */
	{3840, 2880, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x26, 2880}, /* 3840x2880 */
	{ 800,  480, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  480}, /* 800x480 */
	{1024,  600, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  600}  /* 1024x600 */
};

/*
 * sp_mipitx_input_timing_csi[x][y]
 * y = 0-1, MIPITX width & height
 * y = 2-9, MIPITX HSA & HFP & HBP & HACT & VSA & VFP & VBP & VACT
 */
static const u32 sp_mipitx_input_timing_csi[7][10] = {
	/* (w   h)   HSA  HFP HBP HACT VSA  VFP  VBP   VACT */
	{  64,   64, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x1c,   64}, /* 64x64 */
	{ 128,  128, 0x4,  0, 0x5,  0, 0x1, 0x2, 0x12,  128}, /* 128x128 */
	{ 720,  480, 0x4,  0, 0x5,  0, 0x1, 0x8, 0x23,  480}, /* 480P */
	{ 720,  576, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x2B,  576}, /* 576P */
	{1280,  720, 0x4,  0, 0x5,  0, 0x1, 0x4, 0x18,  720}, /* 720P */
	{1920, 1080, 0x4,  0, 0x5,  0, 0x1, 0x3, 0x28, 1080}, /* 1080P */
	{3840, 2880, 0x4,  0, 0x5,  0, 0x1, 0x6, 0x26, 2880}, /* 3840x2880 */
};

void sp7350_mipitx_video_mode_setting(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 width, height, data_bit;
	int i, time_cnt = 0;
	u32 value;

	width = disp_dev->out_res.width;
	height = disp_dev->out_res.height;
	data_bit = disp_dev->mipitx_data_bit;

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {

		for (i = 0; i < 11; i++) {
			if ((sp_mipitx_input_timing_dsi[i][0] == disp_dev->out_res.width) &&
				(sp_mipitx_input_timing_dsi[i][1] == disp_dev->out_res.height)) {
					time_cnt = i;
					break;
			}
		}

		value = 0;
		if (disp_dev->mipitx_lane == 1)
			value |= SP7350_MIPITX_HSA_SET(0x6) |
				SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing_dsi[time_cnt][3]) |
				SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing_dsi[time_cnt][4]);
		else
			value |= SP7350_MIPITX_HSA_SET(sp_mipitx_input_timing_dsi[time_cnt][2]) |
				SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing_dsi[time_cnt][3]) |
				SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing_dsi[time_cnt][4]);
		writel(value, disp_dev->base + MIPITX_VM_HT_CTRL);

		value = 0;
		value |= SP7350_MIPITX_VSA_SET(sp_mipitx_input_timing_dsi[time_cnt][6]) |
			SP7350_MIPITX_VFP_SET(sp_mipitx_input_timing_dsi[time_cnt][7]) |
			SP7350_MIPITX_VBP_SET(sp_mipitx_input_timing_dsi[time_cnt][8]);
		writel(value, disp_dev->base + MIPITX_VM_VT0_CTRL);

		value = 0;
		value |= SP7350_MIPITX_VACT_SET(sp_mipitx_input_timing_dsi[time_cnt][9]);
		writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL);

	} else { //if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI) {

		for (i = 0; i < 7; i++) {
			if ((sp_mipitx_input_timing_csi[i][0] == disp_dev->out_res.width) &&
				(sp_mipitx_input_timing_csi[i][1] == disp_dev->out_res.height)) {
					time_cnt = i;
					break;
			}
		}
		value = 0;
		if (disp_dev->mipitx_lane == 1)
			value |= SP7350_MIPITX_HSA_SET(0x6) |
				SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing_csi[time_cnt][3]) |
				SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing_csi[time_cnt][4]);
		else
			value |= SP7350_MIPITX_HSA_SET(sp_mipitx_input_timing_csi[time_cnt][2]) |
				SP7350_MIPITX_HFP_SET(sp_mipitx_input_timing_csi[time_cnt][3]) |
				SP7350_MIPITX_HBP_SET(sp_mipitx_input_timing_csi[time_cnt][4]);
		writel(value, disp_dev->base + MIPITX_VM_HT_CTRL);

		value = 0;
		value |= SP7350_MIPITX_VSA_SET(sp_mipitx_input_timing_csi[time_cnt][6]) |
			SP7350_MIPITX_VFP_SET(sp_mipitx_input_timing_csi[time_cnt][7]) |
			SP7350_MIPITX_VBP_SET(sp_mipitx_input_timing_csi[time_cnt][8]);
		writel(value, disp_dev->base + MIPITX_VM_VT0_CTRL);

		value = 0;
		value |= SP7350_MIPITX_VACT_SET(sp_mipitx_input_timing_csi[time_cnt][9]);
		writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL);

	}
	//MIPITX  Video Mode WordCount Setting
	value = 0;
	value |= ((width << 16) | ((width * data_bit) / 8)) ;
	writel(value, disp_dev->base + MIPITX_WORD_CNT); //G204.19

}

void sp7350_mipitx_csi_setting(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, vfp;

	value = readl(disp_dev->base + MIPITX_VM_VT0_CTRL);
	vfp = FIELD_GET(GENMASK(15, 8), value);

	value = 0x00000000;
	value |= SP7350_MIPITX_MIPI_CSI_MODE_EN;
	value |= SP7350_MIPITX_MIPI_CSI_FE_SET((vfp - 1));
	if (disp_dev->mipitx_format == 0x0)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB565);
	else if (disp_dev->mipitx_format == 0x1)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422_8bit);
	else if (disp_dev->mipitx_format == 0x2)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422_10bit);
	else if (disp_dev->mipitx_format == 0x3)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);
	else
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);

	writel(value, disp_dev->base + MIPITX_CTRL); //G205.08

}

void sp7350_mipitx_video_mode_on(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = 0x11000000; //DSI_EN and ANALOG_EN
	//MIPITX SWITCH to Video Mode
	if (disp_dev->mipitx_lane == 1)
		value = 0x11000001; //lane num = 1 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 2)
		value = 0x11000011; //lane num = 2 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 4)
		value = 0x11000031; //lane num = 4 and DSI_EN and ANALOG_EN
	writel(value, disp_dev->base + MIPITX_CORE_CTRL); //G204.15

}

void sp7350_mipitx_phy_init_dsi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 width, height;

	width = disp_dev->out_res.width;
	height = disp_dev->out_res.height;

	sp7350_mipitx_phy_init();

	sp7350_mipitx_pllclk_init();

	//if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI) {
	if (disp_dev->mipitx_dev_id == 0x00001000) {
		sp7350_mipitx_cmd_mode_start();
		//transfer data from TX to RX (depends on panel manufacturer)
		sp7350_mipitx_panel_init(0x00001000, width, height);
	}

	sp7350_mipitx_pllclk_set(SP7350_MIPITX_HS_MODE, width, height);

	sp7350_mipitx_video_mode_setting();

	sp7350_mipitx_lane_control_set();

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI)
		sp7350_mipitx_csi_setting();

	sp7350_mipitx_video_mode_on();
}

void sp7350_mipitx_phy_init_csi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 width, height, data_bit;
	u32 value, value1, value2, vfp;

	pr_info("%s\n", __func__);

	width = disp_dev->out_res.width;
	height = disp_dev->out_res.height;
	data_bit = disp_dev->mipitx_data_bit;

	value = 0x00101330; //PHY Reset(under reset)
	if (disp_dev->mipitx_clk_edge)
		value |= SP7350_MIPITX_MIPI_PHY_CLK_EDGE_SEL(SP7350_MIPITX_FALLING);
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06

	if (disp_dev->mipitx_lane == 1)
		value = 0x11000001; //lane num = 1 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 2)
		value = 0x11000011; //lane num = 2 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 4)
		value = 0x11000031; //lane num = 4 and DSI_EN and ANALOG_EN
	writel(value, disp_dev->base + MIPITX_CORE_CTRL); //G204.15

	value = 0x00000000;
	if (disp_dev->mipitx_sync_timing)
		value |= SP7350_MIPITX_FORMAT_VTF_SET(SP7350_MIPITX_VTF_SYNC_EVENT);

	if (disp_dev->mipitx_format == 0x0)
		value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_16BITS);
	else if (disp_dev->mipitx_format == 0x1)
		value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_16BITS);
	else if (disp_dev->mipitx_format == 0x2)
		value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_YUV422_20BITS);
	else if (disp_dev->mipitx_format == 0x3)
		value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_24BITS);
	else
		value |= SP7350_MIPITX_FORMAT_VPF_SET(SP7350_MIPITX_VPF_CSI_24BITS);

	writel(value, disp_dev->base + MIPITX_FORMAT_CTRL); //G204.12

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL2);
	value |= SP7350_MIPITX_NORMAL; //PHY Reset(under normal mode)
	writel(value, disp_dev->base + MIPITX_ANALOG_CTRL2); //G205.06

	sp7350_mipitx_pllclk_init();

	value = 0x00000000;
	writel(value, disp_dev->base + MIPITX_OP_CTRL); //G204.14
	value = 0x00520004; //TA GET/SURE/GO
	writel(value, disp_dev->base + MIPITX_BTA_CTRL); //G204.17

	//value = 0x0000c350; //fix
	//value = 0x000000af; //fix
	//writel(value, disp_dev->base + MIPITX_ULPS_DELAY); //G204.29

	sp7350_mipitx_pllclk_set(SP7350_MIPITX_HS_MODE, width, height);

	//MIPITX  Video Mode Horizontal/Vertial Timing
	if ((width == 720) && (height == 480)) { // 720x480
		value1 = 0x04080005; //fix
		value2 = 0x00010823; //VSA=0x01 VFP=0x08 VBP=0x23
	} else if ((width == 800) && (height == 480)) { // 800x480
		value1 = 0x04080005; //fix
		value2 = 0x00010823; //VSA=0x01 VFP=0x08 VBP=0x23
	} else if ((width == 480) && (height == 1280)) { // 480x1280
		value1 = 0x04080004; //fix
		value2 = 0x00011010; //VSA=0x01 VFP=0x10 VBP=0x10
	} else if ((width == 1280) && (height == 480)) { // 1280x480
		value1 = 0x04080005; //fix
		value2 = 0x00010823; //VSA=0x01 VFP=0x08 VBP=0x23
	} else if ((width == 1280) && (height == 720)) { // 1280x720
		value1 = 0x04080005; //fix
		value2 = 0x00010418; //VSA=0x01 VFP=0x04 VBP=0x18
	} else if ((width == 1920) && (height == 1080)) { // 1920x1080
		value1 = 0x04080005; //fix
		value2 = 0x00010328; //VSA=0x01 VFP=0x03 VBP=0x28
	} else if ((width == 3840) && (height == 2880)) { // 3840x2880
		value1 = 0x04080005; //fix
		value2 = 0x00010628; //VSA=0x01 VFP=0x06 VBP=0x28
	} else {
		pr_info("TBD mipitx common setting\n");
	}
	writel(value1, disp_dev->base + MIPITX_VM_HT_CTRL); //G204.00
	writel(value2, disp_dev->base + MIPITX_VM_VT0_CTRL); //G204.01
	value = 0;
	value |= height;
	writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL); //G204.02

	value = readl(disp_dev->base + MIPITX_VM_VT0_CTRL);
	vfp = FIELD_GET(GENMASK(15, 8), value);

	//MIPITX  Video Mode WordCount Setting
	value = 0;
	value |= ((width << 16) | ((width * data_bit) / 8)) ;
	writel(value, disp_dev->base + MIPITX_WORD_CNT); //G204.19

	//MIPITX  LANE CLOCK DATA Timing
	value = 0x00100008;
	writel(value, disp_dev->base + MIPITX_LANE_TIME_CTRL); //G204.05
	value = 0x00100010;
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL0); //G204.06
	value = 0x0a120020;
	writel(value, disp_dev->base + MIPITX_CLK_TIME_CTRL1); //G204.07
	value = 0x0a050010;
	writel(value, disp_dev->base + MIPITX_DATA_TIME_CTRL0); //G204.08

	value = 0x00001100; //MIPITX Blanking Mode
	writel(value, disp_dev->base + MIPITX_BLANK_POWER_CTRL); //G204.13

	value = 0x00000001; //MIPITX CLOCK CONTROL (CK_HSEN)
	writel(value, disp_dev->base + MIPITX_CLK_CTRL); //G204.30

	//value = 0x000071E1; //MIPITX CONTROL (CSI TX Enable)
	value = 0x00000000;
	value |= SP7350_MIPITX_MIPI_CSI_MODE_EN;
	value |= SP7350_MIPITX_MIPI_CSI_FE_SET((vfp - 1));
	if (disp_dev->mipitx_format == 0x0)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB565);
	else if (disp_dev->mipitx_format == 0x1)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422_8bit);
	else if (disp_dev->mipitx_format == 0x2)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_YUV422_10bit);
	else if (disp_dev->mipitx_format == 0x3)
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);
	else
		value |= SP7350_MIPITX_MIPI_CSI_DT_SET(SP7350_MIPITX_MIPI_CSI_DT_RGB888);

	writel(value, disp_dev->base + MIPITX_CTRL); //G205.08

	//MIPITX SWITCH to Video Mode
	if (disp_dev->mipitx_lane == 1)
		value = 0x11000001; //lane num = 1 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 2)
		value = 0x11000011; //lane num = 2 and DSI_EN and ANALOG_EN
	else if (disp_dev->mipitx_lane == 4)
		value = 0x11000031; //lane num = 4 and DSI_EN and ANALOG_EN
	writel(value, disp_dev->base + MIPITX_CORE_CTRL); //G204.15

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

	value = readl(disp_dev->base + MIPITX_ANALOG_CTRL1); //G205.04
	pr_info("\n  BP REG_VSE TX_LP_SR=(%04ld %04ld %04ld)\n",
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
	//pr_info("  EN_DIV5 POSTDIV FBKDIV PRESCALE PREDIV=(%04ld %04ld %04ld %04ld %04ld)\n",
	//	FIELD_GET(GENMASK(20,20), value),
	//	FIELD_GET(GENMASK(18,16), value),
	//	FIELD_GET(GENMASK(13,8), value),
	//	FIELD_GET(GENMASK(4,4), value),
	//	FIELD_GET(GENMASK(1,0), value));
	pr_info("  FBKDIV PRESCALE =(%04ld %04ld)\n",
		FIELD_GET(GENMASK(13,8), value),
		FIELD_GET(GENMASK(4,4), value));
	pr_info("  EN_DIV5 POSTDIV PREDIV=(%04ld %04ld %04ld)\n",
		FIELD_GET(GENMASK(20,20), value),
		FIELD_GET(GENMASK(18,16), value),
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
	//value = readl(disp_dev->base + MIPITX_ANALOG_CTRL8); //G205.13
	//pr_info("  MDS (CLK MUL RTO) NS SSC SSC_DN=(%04ld %04ld %04ld %04ld %04ld %04ld)\n",
	//	FIELD_GET(GENMASK(22,20), value),
	//	FIELD_GET(GENMASK(19,19), value),
	//	FIELD_GET(GENMASK(18,16), value),
	//	FIELD_GET(GENMASK(15,8), value),
	//	FIELD_GET(GENMASK(1,1), value),
	//	FIELD_GET(GENMASK(0,0), value));
	//value = readl(disp_dev->base + MIPITX_ANALOG_CTRL9); //G205.14
	//pr_info("  INITIAL ANA_TEST OTP_FREE=(%04ld %04ld %04ld)\n",
	//	FIELD_GET(GENMASK(31,31), value),
	//	FIELD_GET(GENMASK(29,29), value),
	//	FIELD_GET(GENMASK(28,28), value));
}
