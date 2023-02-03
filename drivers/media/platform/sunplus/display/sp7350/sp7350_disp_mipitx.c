// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for DVE block
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
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 mipitx_width, mipitx_height, mipitx_lane, mipitx_data_bit;
	u32 value = 0;

	mipitx_width = 720; //64 128 720 1280 1920 3840
	mipitx_height = 480; //64 128 480 576 720 1080 2880
	mipitx_lane = 4; //1 2 4
	mipitx_data_bit = 24; //16 18 20 24
	//mipitx_csi_mode = 0; //0:output MIPI DSI TX , 1: output MIPI CSI TX

	writel(0x00000000, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Reset

	if (mipitx_lane == 1)
		value |= (0x06 << 24); //HSA=0x4 or 0x6 depends on lane setting
	else if ((mipitx_lane == 2) || (mipitx_lane == 4))
		value |= (0x04 << 24); //HSA=0x4 or 0x6 depends on lane setting
	else {
		value |= (0x04 << 24); //HSA=0x4 or 0x6 depends on lane setting
		pr_info("not support lane num\n");
	}
	value |= (0x005 << 0); //HBP=0x005 fixed value
	value |= (0x080 << 12); //HFP=0x080 (no used)
	writel(value, disp_dev->base + MIPITX_VM_HT_CTRL); //HSA=4/6 HFP=x HBP=0x5
	//writel(0x04080005, disp_dev->base + MIPITX_VM_HT_CTRL); //HSA=4/6 HFP=x HBP=0x5

	if ((mipitx_width == 64) && (mipitx_height == 64)) {
		writel(0x0001061c, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x06 VBP=0x1c
		//writel(0x00010611, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x06 VBP=0x11
	} else if ((mipitx_width == 64) && (mipitx_height == 2880)) {
		writel(0x00010628, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x06 VBP=0x28
	} else if ((mipitx_width == 128) && (mipitx_height == 128)) {
		writel(0x00010212, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x02 VBP=0x12
	} else if ((mipitx_width == 720) && (mipitx_height == 480)) {
		writel(0x00010823, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x08 VBP=0x23
	} else if ((mipitx_width == 720) && (mipitx_height == 576)) {
		writel(0x0001042B, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x04 VBP=0x2B
	} else if ((mipitx_width == 1280) && (mipitx_height == 720)) {
		writel(0x00010418, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x04 VBP=0x18
	} else if ((mipitx_width == 1920) && (mipitx_height == 1080)) {
		writel(0x00010328, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x03 VBP=0x28
	} else if ((mipitx_width == 3840) && (mipitx_height == 64)) {
		writel(0x0001061c, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x06 VBP=0x1c
	} else if ((mipitx_width == 3840) && (mipitx_height == 2880)) {
		writel(0x00010626, disp_dev->base + MIPITX_VM_VT0_CTRL); //VSA=0x01 VFP=0x06 VBP=0x26
	}
	value = 0;
	value |= (mipitx_height << 0); //VACT=mipitx_height setting
	writel(value, disp_dev->base + MIPITX_VM_VT1_CTRL);

	/* MIPITX PHY Timing Setting
	*/
	writel(0x00100008, disp_dev->base + MIPITX_LANE_TIME_CTRL); //T_HS-EXIT T_LPX
	writel(0x00100010, disp_dev->base + MIPITX_CLK_TIME_CTRL0); //T_CLK-PREPARE T_CLK-ZERO
	writel(0x0a120020, disp_dev->base + MIPITX_CLK_TIME_CTRL1); //T_CLK-TRAIL T_CLK-PRE T_CLK-POST
	writel(0x0a050010, disp_dev->base + MIPITX_DATA_TIME_CTRL0); //T_HS-TRAIL T_HS-PREPARE T_HS-ZERO
	writel(0x000000af, disp_dev->base + MIPITX_ULPS_DELAY); //ULPS Wake Up Delay

	value = 0;
	//value = (0x00 << 12) | (0x03 << 4); //VTF=sync pulse VPF= packet fmt RGB888 24bit
	value = (0x01 << 12) | (0x03 << 4); //VTF=sync event VPF= packet fmt RGB888 24bit
	//writel(0x00001030, disp_dev->base + MIPITX_FORMAT_CTRL);
	writel(value, disp_dev->base + MIPITX_FORMAT_CTRL);

	writel(0x00001100, disp_dev->base + MIPITX_BLANK_POWER_CTRL);
	writel(0x00100000, disp_dev->base + MIPITX_OP_CTRL); //TXLDPT enable
	writel(0x11000031, disp_dev->base + MIPITX_CORE_CTRL); //INPUT enable PHY enable lane=4 DSI enable
	//writel(0x000000c0, disp_dev->base + MIPITX_WORD_CNT);
	//writel(0x004000c0, disp_dev->base + MIPITX_WORD_CNT); //for new version RTL 64x64
	//writel(0x02D00870, disp_dev->base + MIPITX_WORD_CNT); //for new version RTL 720x480
	value = 0;
	value = (mipitx_width << 16) | ((mipitx_width * mipitx_data_bit / 8) << 0); //PIXEL_CNT WORD_CNT
	writel(value, disp_dev->base + MIPITX_WORD_CNT); //for new version RTL PIXEL_CNT WORD_CNT setting

	writel(0x00000001, disp_dev->base + MIPITX_CLK_CTRL); //CLK lane high speed enable

	#if 0 //In case of MIPI CSI TX, TBD
	if (mipitx_csi_mode)
		writel(0x000001E1, disp_dev->base + MIPITX_CTRL); //if MIPI CSI, FE&DT&CSI sw setting
	else
		writel(0x00000000, disp_dev->base + MIPITX_CTRL); //if MIPI DSI, skip setting
	#endif
	writel(0x00000001, disp_dev->base + MIPITX_ANALOG_CTRL2); //PHY Reset (return to Normal Mode)

	//sp7350_mipitx_decrypt_info();

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
