// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for TGEN block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

static const char * const sp7350_tgen_usr[] = {"Internal", "User Mode"};
static const char * const sp7350_tgen_fmt[] = {
	"480P", "576P", "720P", "1080P", "rsv", "rsv",
	"64x64(360x100)", "64x64(144x100)"};
static const char * const sp7350_tgen_fps[] = {"60Hz", "50Hz", "24Hz", "rsv"};

static const char * const sp7350_pllh_pstdiv[] = {
	"DIV2.5", "DIV3", "DIV3.5", "DIV4", "DIV5", "DIV5.5", "DIV6", "DIV7",
	"DIV7.5", "DIV8", "DIV9", "DIV10", "DIV10.5", "DIV11", "DIV12", "DIV12.5"
};
static const char * const sp7350_pllh_mipitx_sel[] = {
	"DIV1", "DIV2", "undef", "DIV4", "undef", "undef", "undef", "DIV8",
	"undef", "undef", "undef", "undef", "undef", "undef", "undef", "DIV16"
	"undef", "undef", "undef", "undef", "undef", "undef", "undef", "undef",
	"undef", "undef", "undef", "undef", "undef", "undef", "undef", "undef"
};

static const u32 sp7350_pllh_pstdiv_int[] = {
	25, 30, 35, 40, 50, 55, 60, 70, 75, 80, 90, 100, 105, 110, 120, 125
};

static const u32 sp7350_pllh_mipitx_sel_int[] = {
	1,2,0,4,0,0,0,8,0,0,0,0,0,0,0,16,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void sp7350_tgen_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, dtg_fmt, set_width, set_height;

	writel(0x00000000, disp_dev->base + TGEN_CONFIG);
	writel(0x0000000a, disp_dev->base + TGEN_USER_INT1_CONFIG);
	writel(0x0000000a, disp_dev->base + TGEN_USER_INT2_CONFIG);

#if 1
	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	dtg_fmt = FIELD_GET(GENMASK(10,8), value);
	set_width = disp_dev->out_res.width;
	set_height = disp_dev->out_res.height;
	if (((dtg_fmt == 0x00) && (set_width == 720) && (set_height == 480)) ||
		((dtg_fmt == 0x01) && (set_width == 720) && (set_height == 576)) ||
		((dtg_fmt == 0x02) && (set_width == 1280) && (set_height == 720)) ||
		((dtg_fmt == 0x03) && (set_width == 1920) && (set_height == 1080))) {
		value = readl(disp_dev->base + MIPITX_INFO_STATUS); //G204.28
		if ((FIELD_GET(GENMASK(24,24), value) == 1) && (FIELD_GET(GENMASK(0,0), value) == 0)) {
			//pr_info("  MIPITX working, skip tgen setting\n");
			return;
		}
	}
#endif

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		sp7350_tgen_timing_set_dsi();
	else if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_CSI)
		sp7350_tgen_timing_set_csi();
	else
		pr_info("undefined mode for tgen init\n");

	writel(0x0000100d, disp_dev->base + TGEN_DTG_ADJUST1);
	writel(0x00000000, disp_dev->base + TGEN_SOURCE_SEL);

	/* Write 1 to reset DTG timing bit
	 */
	sp7350_tgen_reset();

}

void sp7350_tgen_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2, value3, value4;
	u32 tmp_value1, tmp_value2, tmp_value3;
	u32 user_mode;

	pr_info("TGEN G197 reg decryption\n");
	value = readl(disp_dev->base + TGEN_CONFIG);
	pr_info("G197.00 TGEN_CONFIG 0x%08x\n", value);
	value1 = readl(disp_dev->base + TGEN_USER_INT1_CONFIG);
	value2 = readl(disp_dev->base + TGEN_USER_INT2_CONFIG);
	pr_info("USER_(INT1/INT2)_LCNT (0x%04x 0x%04x)\n", value1, value2);

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	pr_info("G197.04 TGEN_DTG_CONFIG 0x%08x\n", value);
	user_mode = FIELD_GET(GENMASK(0,0), value);
	pr_info("  USER_MODE[%s] %s %s\n", 
		user_mode?"ON ":"OFF",
		user_mode?"---":sp7350_tgen_fmt[FIELD_GET(GENMASK(10,8), value)],
		user_mode?"---":sp7350_tgen_fps[FIELD_GET(GENMASK(5,4), value)]);

	value = readl(disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	pr_info("  [%s]  TOTAL_PIXEL %04d(0x%04x)\n", user_mode?"USER":"X", value, value);
	value1 = readl(disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	pr_info("  [%s] ACTIVE_PIXEL %04d(0x%04x)\n", user_mode?"USER":"X", value1, value1);
	value2 = readl(disp_dev->base + TGEN_DTG_TOTAL_LINE);
	pr_info("  [%s]  TOTAL_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value2, value2);
	value3 = readl(disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	pr_info("  [%s] ACTIVE_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value3, value3);
	value4 = readl(disp_dev->base + TGEN_DTG_START_LINE);
	pr_info("  [%s]  START_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value4, value4);
	pr_info("     target  pixel_clk  %d(Hz)\n", value*value2*60);

#if 1 //get cur pixel clk setting
	value1 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_14);
	value2 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_25);

	tmp_value1 = 25 * ((FIELD_GET(GENMASK(15,15), value1)?2:1) * 
		(FIELD_GET(GENMASK(14,7), value1) + 64)) /
		(FIELD_GET(GENMASK(2,1), value1)?2:1);
	tmp_value2 = (tmp_value1 *10 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	tmp_value3 = (tmp_value1 *1000 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	pr_info("     current pixel_clk  %d.%d(MHz)\n", tmp_value2, (tmp_value3 - tmp_value2*100));
#endif

	value = readl(disp_dev->base + TGEN_DTG_ADJUST1);
	pr_info("G197.23 TGEN_DTG_ADJUST1 0x%08x\n", value);
	value = readl(disp_dev->base + TGEN_SOURCE_SEL);
	pr_info("G197.29 TGEN_SOURCE_SEL 0x%08x\n", value);

}
EXPORT_SYMBOL(sp7350_tgen_decrypt_info);

void sp7350_tgen_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2, value3, value4;
	u32 tmp_value1, tmp_value2, tmp_value3;
	u32 user_mode, dtg_fps, freq;

	pr_info("TGEN resolution chk\n");
	value1 = readl(disp_dev->base + TGEN_USER_INT1_CONFIG);
	value2 = readl(disp_dev->base + TGEN_USER_INT2_CONFIG);
	pr_info("  USER_(INT1/INT2)_LCNT (0x%04x 0x%04x)\n", value1, value2);

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	//pr_info("G197.04 TGEN_DTG_CONFIG 0x%08x\n", value);
	user_mode = FIELD_GET(GENMASK(0,0), value);
	dtg_fps = FIELD_GET(GENMASK(5, 4), value);
	pr_info("  USER_MODE[%s] %s %s\n", 
		user_mode?"ON ":"OFF",
		user_mode?"---":sp7350_tgen_fmt[FIELD_GET(GENMASK(10,8), value)],
		user_mode?"---":sp7350_tgen_fps[FIELD_GET(GENMASK(5,4), value)]);

	value = readl(disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	pr_info("  [%s]  TOTAL_PIXEL %04d(0x%04x)\n", user_mode?"USER":"X", value, value);
	value1 = readl(disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	pr_info("  [%s] ACTIVE_PIXEL %04d(0x%04x)\n", user_mode?"USER":"X", value1, value1);
	value2 = readl(disp_dev->base + TGEN_DTG_TOTAL_LINE);
	pr_info("  [%s]  TOTAL_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value2, value2);
	value3 = readl(disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	pr_info("  [%s] ACTIVE_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value3, value3);
	value4 = readl(disp_dev->base + TGEN_DTG_START_LINE);
	pr_info("  [%s]  START_LINE  %04d(0x%04x)\n", user_mode?"USER":"X", value4, value4);

	if (dtg_fps == 0) freq = 60;
	if (dtg_fps == 1) freq = 50;
	if (dtg_fps == 2) freq = 24;
	pr_info("     target  pixel_clk %d(Hz)\n", value*value2*freq);

#if 1 //get cur pixel clk setting
	value1 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_14);
	value2 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_25);

	tmp_value1 = 25 * ((FIELD_GET(GENMASK(15,15), value1)?2:1) * 
		(FIELD_GET(GENMASK(14,7), value1) + 64)) /
		(FIELD_GET(GENMASK(2,1), value1)?2:1);
	tmp_value2 = (tmp_value1 *10 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	tmp_value3 = (tmp_value1 *1000 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	pr_info("     current pixel_clk  %d.%d(MHz)\n", tmp_value2, (tmp_value3 - tmp_value2*100));
#endif

}
EXPORT_SYMBOL(sp7350_tgen_resolution_chk);

void sp7350_tgen_reset(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = readl(disp_dev->base + TGEN_RESET);
	value |= SP7350_TGEN_RESET;
	writel(value, disp_dev->base + TGEN_RESET);
}

void sp7350_tgen_set_user_int1(u32 count)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (count >= SP7350_TGEN_USER_INT1_CONFIG)
		count = SP7350_TGEN_USER_INT1_CONFIG;

	writel(count, disp_dev->base + TGEN_USER_INT1_CONFIG);
}

void sp7350_tgen_set_user_int2(u32 count)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (count >= SP7350_TGEN_USER_INT2_CONFIG)
		count = SP7350_TGEN_USER_INT2_CONFIG;
	writel(count , disp_dev->base + TGEN_USER_INT2_CONFIG);
}

u32 sp7350_tgen_get_current_line_count(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, line_cnt;

	value = readl(disp_dev->base + TGEN_DTG_STATUS1);
	line_cnt = FIELD_GET(GENMASK(11, 0), value);

	return line_cnt;
}
EXPORT_SYMBOL(sp7350_tgen_get_current_line_count);

/*
 * sp_tgen_para_dsi[x][y]
 * y = 0-1, TGEN width & height
 * y = 2-4, TGEN usr_mode & dtg_fps & dtg_fmt
 * y = 5-9, TGEN htt & hact & vtt & (vact+vbp+1) & vbp
 */
static const u32 sp_tgen_para_dsi[11][10] = {
	/* (w   h) (usr fps fmt) */
	{ 720,  480, 0, 0, 0,  858,  720,  525,  517, 36}, /* 480P 60Hz */
	{ 720,  576, 0, 1, 1,  864,  720,  625,  621, 44}, /* 576P 50Hz */
	{1280,  720, 0, 0, 2, 1650, 1280,  750,  746, 25}, /* 720P 60Hz */
	{1920, 1080, 0, 0, 3, 2200, 1920, 1125, 1122, 41}, /* 1080P 60Hz */
	//{  64,   64, 0, 0, 6,  360,   64,  100,   92, 29}, /* 64x64 60Hz */
	{ 480, 1280, 1, 0, 0,  620,  480, 1314, 1298, 17}, /* 480x1280 60Hz */
	/* (w   h) (usr - -)  htt   hact   vtt   vact+vbp+1 vbp*/
	{ 128,  128, 1, 0, 0,  360,  128,  150,  148, 19}, /* 128x128 60Hz */
	{  64, 2880, 1, 0, 0,  360,   64, 3200, 2920, 41}, /* 64x2880 60Hz */
	{3840,   64, 1, 0, 0, 4608, 3840,  100,   92, 29}, /* 3840x64 60Hz */
	{3840, 2880, 1, 0, 0, 4608, 3840, 3200, 2920, 39}, /* 3840x2880 60Hz */
	{ 800,  480, 1, 0, 0,  928,  800,  525,  501, 20}, /* 800x480 60Hz */
	{1024,  600, 1, 0, 0, 1344, 1024,  635,  625, 24}  /* 1024x600 60Hz */
};

void sp7350_tgen_timing_set_dsi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value;

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	value &= ~(SP7350_TGEN_FORMAT | SP7350_TGEN_FPS | SP7350_TGEN_USER_MODE);

	for (i = 0; i < 11; i++) {
		if ((sp_tgen_para_dsi[i][0] == disp_dev->out_res.width) &&
			(sp_tgen_para_dsi[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	//pr_info("  tgen timing (%dx%d)\n", sp_tgen_para_dsi[time_cnt][0], sp_tgen_para_dsi[time_cnt][1]);
	//pr_info("  (%d %d) (%d %d %d) (%d %d %d %d %d)\n",
	//	sp_tgen_para_dsi[time_cnt][0], sp_tgen_para_dsi[time_cnt][1],
	//	sp_tgen_para_dsi[time_cnt][2], sp_tgen_para_dsi[time_cnt][3], sp_tgen_para_dsi[time_cnt][4],
	//	sp_tgen_para_dsi[time_cnt][5], sp_tgen_para_dsi[time_cnt][6],
	//	sp_tgen_para_dsi[time_cnt][7], sp_tgen_para_dsi[time_cnt][8], sp_tgen_para_dsi[time_cnt][9]);

	value |= SP7350_TGEN_FORMAT_SET(sp_tgen_para_dsi[time_cnt][4]) |
			SP7350_TGEN_FPS_SET(sp_tgen_para_dsi[time_cnt][3]) |
			sp_tgen_para_dsi[time_cnt][2];
	writel(value, disp_dev->base + TGEN_DTG_CONFIG);

	writel(sp_tgen_para_dsi[time_cnt][5], disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	writel(sp_tgen_para_dsi[time_cnt][6], disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	writel(sp_tgen_para_dsi[time_cnt][7], disp_dev->base + TGEN_DTG_TOTAL_LINE);
	writel(sp_tgen_para_dsi[time_cnt][8], disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	writel(sp_tgen_para_dsi[time_cnt][9], disp_dev->base + TGEN_DTG_START_LINE);

	/* Write 1 to reset DTG timing bit
	 */
	sp7350_tgen_reset();

}

/*
 * sp_tgen_para_csi[x][y]
 * y = 0-1, TGEN width & height
 * y = 2-4, TGEN usr_mode & dtg_fps & dtg_fmt
 * y = 5-9, TGEN htt & hact & vtt & (vact+vbp+1) & vbp
 */
static const u32 sp_tgen_para_csi[7][10] = {
	/* (w   h) (usr fps fmt) */
	{  64,   64, 0, 0, 6,  360,   64,  100,   92, 29}, /* 64x64 60Hz */
	{ 128,  128, 1, 0, 0,  360,  128,  150,  148, 19}, /* 128x128 60Hz */
	{ 720,  480, 0, 0, 0,  858,  720,  525,  517, 36}, /* 480P 60Hz */
	{ 720,  576, 0, 1, 1,  864,  720,  625,  621, 44}, /* 576P 50Hz */
	{1280,  720, 0, 0, 2, 1650, 1280,  750,  746, 25}, /* 720P 60Hz */
	{1920, 1080, 0, 0, 3, 2200, 1920, 1125, 1122, 41}, /* 1080P 60Hz */
	{3840, 2880, 1, 0, 0, 4608, 3840, 3200, 2920, 39}, /* 3840x2880 60Hz */
};

void sp7350_tgen_timing_set_csi(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i, time_cnt = 0;
	u32 value;

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	value &= ~(SP7350_TGEN_FORMAT | SP7350_TGEN_FPS | SP7350_TGEN_USER_MODE);

	for (i = 0; i < 7; i++) {
		if ((sp_tgen_para_csi[i][0] == disp_dev->out_res.width) &&
			(sp_tgen_para_csi[i][1] == disp_dev->out_res.height)) {
				time_cnt = i;
				break;
		}
	}

	//pr_info("  tgen timing (%dx%d)\n", sp_tgen_para_csi[time_cnt][0], sp_tgen_para_csi[time_cnt][1]);
	//pr_info("  (%d %d) (%d %d %d) (%d %d %d %d %d)\n",
	//	sp_tgen_para_csi[time_cnt][0], sp_tgen_para_csi[time_cnt][1],
	//	sp_tgen_para_csi[time_cnt][2], sp_tgen_para_csi[time_cnt][3], sp_tgen_para_csi[time_cnt][4],
	//	sp_tgen_para_csi[time_cnt][5], sp_tgen_para_csi[time_cnt][6],
	//	sp_tgen_para_csi[time_cnt][7], sp_tgen_para_csi[time_cnt][8], sp_tgen_para_csi[time_cnt][9]);

	value |= SP7350_TGEN_FORMAT_SET(sp_tgen_para_csi[time_cnt][4]) |
			SP7350_TGEN_FPS_SET(sp_tgen_para_csi[time_cnt][3]) |
			sp_tgen_para_csi[time_cnt][2];
	writel(value, disp_dev->base + TGEN_DTG_CONFIG);

	writel(sp_tgen_para_csi[time_cnt][5], disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	writel(sp_tgen_para_csi[time_cnt][6], disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	writel(sp_tgen_para_csi[time_cnt][7], disp_dev->base + TGEN_DTG_TOTAL_LINE);
	writel(sp_tgen_para_csi[time_cnt][8], disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	writel(sp_tgen_para_csi[time_cnt][9], disp_dev->base + TGEN_DTG_START_LINE);

	/* Write 1 to reset DTG timing bit
	 */
	sp7350_tgen_reset();

}

void sp7350_tgen_timing_get(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, user_mode, dtg_fps, dtg_fmt, freq;
	u32 tmp_value1, tmp_value2, tmp_value3;
	u32 value1, value2, value3, value4, value5;

	pr_info("TGEN Timing Get\n");
	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	user_mode = FIELD_GET(GENMASK(0, 0), value);
	dtg_fps = FIELD_GET(GENMASK(5, 4), value);
	dtg_fmt = FIELD_GET(GENMASK(10, 8), value);
	pr_info("  tgen cur tim %s (%s %s)\n",
		sp7350_tgen_usr[user_mode],
		user_mode?"USER_DEF":sp7350_tgen_fmt[dtg_fmt],
		sp7350_tgen_fps[dtg_fps]);

	value1 = readl(disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	value2 = readl(disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	value3 = readl(disp_dev->base + TGEN_DTG_TOTAL_LINE);
	value4 = readl(disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	value5 = readl(disp_dev->base + TGEN_DTG_START_LINE);

	pr_info("  cur timing(%d %d %d %d %d)\n", value1, value2, value3, value4, value5);

	if (dtg_fps == 0) freq = 60;
	if (dtg_fps == 1) freq = 50;
	if (dtg_fps == 2) freq = 24;
	if (user_mode)
		pr_info("  target  pixel_clk %d(Hz)\n", value1*value3*freq);

#if 1 //get cur pixel clk setting
	value1 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_14);
	value2 = readl(disp_dev->ao_moon3 + MIPITX_AO_MOON3_25);

	tmp_value1 = 25 * ((FIELD_GET(GENMASK(15,15), value1)?2:1) * 
		(FIELD_GET(GENMASK(14,7), value1) + 64)) /
		(FIELD_GET(GENMASK(2,1), value1)?2:1);
	tmp_value2 = (tmp_value1 *10 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	tmp_value3 = (tmp_value1 *1000 )/ ((sp7350_pllh_pstdiv_int[FIELD_GET(GENMASK(6,3), value1)]) * 
		(sp7350_pllh_mipitx_sel_int[FIELD_GET(GENMASK(11,7), value2)]));
	pr_info("  current pixel_clk  %d.%d(MHz)\n", tmp_value2, (tmp_value3 - tmp_value2*100));
#endif

}

void sp7350_tgen_input_adjust(int tgen_input_adj, u32 adj_value)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3, value4;

	value1 = readl(disp_dev->base + TGEN_DTG_ADJUST1);
	value2 = readl(disp_dev->base + TGEN_DTG_ADJUST2);
	value3 = readl(disp_dev->base + TGEN_DTG_ADJUST3);
	value4 = readl(disp_dev->base + TGEN_DTG_ADJUST4);
	switch (tgen_input_adj) {
		case SP7350_TGEN_DTG_ADJ_VPP0:
			value1 &= ~(SP7350_TGEN_DTG_ADJ_MASKA);
			value1 |= SP7350_TGEN_DTG_ADJ1_VPP0(adj_value);
			writel(value1, disp_dev->base + TGEN_DTG_ADJUST1);
			break;
		case SP7350_TGEN_DTG_ADJ_VPP1:
			/* not supported */
			break;
		case SP7350_TGEN_DTG_ADJ_VPP2:
			/* not supported */
			break;
		case SP7350_TGEN_DTG_ADJ_OSD3:
			value2 &= ~(SP7350_TGEN_DTG_ADJ_MASKA);
			value2 |= SP7350_TGEN_DTG_ADJ2_OSD3(adj_value);
			writel(value2, disp_dev->base + TGEN_DTG_ADJUST2);
			break;
		case SP7350_TGEN_DTG_ADJ_OSD2:
			value2 &= ~(SP7350_TGEN_DTG_ADJ_MASKB);
			value2 |= SP7350_TGEN_DTG_ADJ2_OSD2(adj_value);
			writel(value2, disp_dev->base + TGEN_DTG_ADJUST2);
			break;
		case SP7350_TGEN_DTG_ADJ_OSD1:
			value3 &= ~(SP7350_TGEN_DTG_ADJ_MASKA);
			value3 |= SP7350_TGEN_DTG_ADJ3_OSD1(adj_value);
			writel(value3, disp_dev->base + TGEN_DTG_ADJUST3);
			break;
		case SP7350_TGEN_DTG_ADJ_OSD0:
			value3 &= ~(SP7350_TGEN_DTG_ADJ_MASKB);
			value3 |= SP7350_TGEN_DTG_ADJ3_OSD0(adj_value);
			writel(value3, disp_dev->base + TGEN_DTG_ADJUST3);
			break;
		case SP7350_TGEN_DTG_ADJ_PTG:
			value4 &= ~(SP7350_TGEN_DTG_ADJ_MASKA);
			value4 |= SP7350_TGEN_DTG_ADJ4_PTG(adj_value);
			writel(value4, disp_dev->base + TGEN_DTG_ADJUST4);
			break;
		case SP7350_TGEN_DTG_ADJ_ALL:
			break;
		default:
			break;
	}
}
