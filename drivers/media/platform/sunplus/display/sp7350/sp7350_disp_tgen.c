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

void sp7350_tgen_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 tgen_width, tgen_height;

	tgen_width = 720;
	tgen_height = 480;
	writel(0x00000000, disp_dev->base + TGEN_CONFIG);
	writel(0x0000000a, disp_dev->base + TGEN_USER_INT1_CONFIG);
	writel(0x0000000a, disp_dev->base + TGEN_USER_INT2_CONFIG);

	if ((tgen_width == 64) && (tgen_height == 64)) {
		writel(0x00000600, disp_dev->base + TGEN_DTG_CONFIG); // 64x64
	} else if ((tgen_width == 64) && (tgen_height == 2880)) {
		writel(0x00000001, disp_dev->base + TGEN_DTG_CONFIG); // 64x2880(USER_MODE)
		writel(0x00000168, disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
		writel(0x00000040, disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
		writel(0x00000c80, disp_dev->base + TGEN_DTG_TOTAL_LINE);
		writel(0x00000b68, disp_dev->base + TGEN_DTG_FIELD_END_LINE);
		writel(0x00000029, disp_dev->base + TGEN_DTG_START_LINE);
	} else if ((tgen_width == 128) && (tgen_height == 128)) {
		writel(0x00000001, disp_dev->base + TGEN_DTG_CONFIG); // 128x128(USER_MODE)
		writel(0x00000168, disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
		writel(0x00000080, disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
		writel(0x00000096, disp_dev->base + TGEN_DTG_TOTAL_LINE);
		writel(0x00000094, disp_dev->base + TGEN_DTG_FIELD_END_LINE);
		writel(0x00000013, disp_dev->base + TGEN_DTG_START_LINE);
	} else if ((tgen_width == 720) && (tgen_height == 480)) {
		writel(0x00000000, disp_dev->base + TGEN_DTG_CONFIG); // 720x480
	} else if ((tgen_width == 720) && (tgen_height == 576)) {
		writel(0x00000100, disp_dev->base + TGEN_DTG_CONFIG); // 720x576
	} else if ((tgen_width == 1280) && (tgen_height == 720)) {
		writel(0x00000200, disp_dev->base + TGEN_DTG_CONFIG); // 1280x720
	} else if ((tgen_width == 1920) && (tgen_height == 1080)) {
		writel(0x00000300, disp_dev->base + TGEN_DTG_CONFIG); // 1920x1080
	} else if ((tgen_width == 3840) && (tgen_height == 64)) {
		writel(0x00000001, disp_dev->base + TGEN_DTG_CONFIG); // 3840x64(USER_MODE)
		writel(0x00001200, disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
		writel(0x00000F00, disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
		writel(0x00000064, disp_dev->base + TGEN_DTG_TOTAL_LINE);
		writel(0x0000005C, disp_dev->base + TGEN_DTG_FIELD_END_LINE);
		writel(0x0000001d, disp_dev->base + TGEN_DTG_START_LINE);
	} else if ((tgen_width == 3840) && (tgen_height == 2880)) {
		writel(0x00000001, disp_dev->base + TGEN_DTG_CONFIG); // 3840x2880(USER_MODE)
		writel(0x00001200, disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
		writel(0x00000F00, disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
		writel(0x00000c80, disp_dev->base + TGEN_DTG_TOTAL_LINE);
		writel(0x00000b68, disp_dev->base + TGEN_DTG_FIELD_END_LINE);
		//writel(0x00000029, disp_dev->base + TGEN_DTG_START_LINE);
		writel(0x00000027, disp_dev->base + TGEN_DTG_START_LINE);
	}

	writel(0x0000100d, disp_dev->base + TGEN_DTG_ADJUST1); //?? IC default setting 0x0000-100E
	writel(0x00000000, disp_dev->base + TGEN_SOURCE_SEL); //IC default settting already 0x0000-0000
	
	/* Write 1 to reset DTG timing bit
	 */
	sp7350_tgen_reset();
	//writel(0x00000001, disp_dev->base + TGEN_RESET); //Write 1 to reset DTG timing

	//sp7350_tgen_decrypt_info();

}

void sp7350_tgen_reg_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i;

	pr_info("TGEN G197 Dump info\n");
	for (i = 0; i < 8; i++)
		pr_info("0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(disp_dev->base + TGEN_CONFIG + (i * 4 + 0) * 4),
			readl(disp_dev->base + TGEN_CONFIG + (i * 4 + 1) * 4),
			readl(disp_dev->base + TGEN_CONFIG + (i * 4 + 2) * 4),
			readl(disp_dev->base + TGEN_CONFIG + (i * 4 + 3) * 4));

}
EXPORT_SYMBOL(sp7350_tgen_reg_info);

void sp7350_tgen_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2;
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
	pr_info("    [%s]  TOTAL_PIXEL %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	pr_info("    [%s] ACTIVE_PIXEL %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_TOTAL_LINE);
	pr_info("    [%s]  TOTAL_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	pr_info("    [%s] ACTIVE_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_START_LINE);
	pr_info("    [%s]  START_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);

	value = readl(disp_dev->base + TGEN_DTG_ADJUST1);
	pr_info("G197.23 TGEN_DTG_ADJUST1 0x%08x\n", value);
	value = readl(disp_dev->base + TGEN_SOURCE_SEL);
	pr_info("G197.29 TGEN_SOURCE_SEL 0x%08x\n", value);

}
EXPORT_SYMBOL(sp7350_tgen_decrypt_info);

void sp7350_tgen_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2;
	u32 user_mode;

	pr_info("TGEN resolution chk\n");
	value1 = readl(disp_dev->base + TGEN_USER_INT1_CONFIG);
	value2 = readl(disp_dev->base + TGEN_USER_INT2_CONFIG);
	pr_info("  USER_(INT1/INT2)_LCNT (0x%04x 0x%04x)\n", value1, value2);

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	//pr_info("G197.04 TGEN_DTG_CONFIG 0x%08x\n", value);
	user_mode = FIELD_GET(GENMASK(0,0), value);
	pr_info("  USER_MODE[%s] %s %s\n", 
		user_mode?"ON ":"OFF",
		user_mode?"---":sp7350_tgen_fmt[FIELD_GET(GENMASK(10,8), value)],
		user_mode?"---":sp7350_tgen_fps[FIELD_GET(GENMASK(5,4), value)]);

	value = readl(disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
	pr_info("    [%s]  TOTAL_PIXEL %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
	pr_info("    [%s] ACTIVE_PIXEL %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_TOTAL_LINE);
	pr_info("    [%s]  TOTAL_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_FIELD_END_LINE);
	pr_info("    [%s] ACTIVE_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);
	value = readl(disp_dev->base + TGEN_DTG_START_LINE);
	pr_info("    [%s]  START_LINE  %04d(0x%04x)\n", user_mode?"U":"X", value, value);

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

void sp7350_tgen_timing_set(struct sp7350_tgen_timing *tgen_tim)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	value &= ~(SP7350_TGEN_FORMAT | SP7350_TGEN_FPS | SP7350_TGEN_USER_MODE);
	if (tgen_tim->usr) {
		value |= SP7350_TGEN_FORMAT_SET(SP7350_TGEN_FORMAT_480P) |
			SP7350_TGEN_FPS_SET(SP7350_TGEN_FPS_59P94HZ) |
			SP7350_TGEN_USER_MODE;
		pr_info("tgen set with SP7350_TGEN_USER_DEF\n");

		/* TGEN USER MODE timing settings
		 */
		//writel((u32)tgen_tim->htt, disp_dev->base + TGEN_DTG_TOTAL_PIXEL);
		//writel((u32)tgen_tim->vtt, disp_dev->base + TGEN_DTG_TOTAL_LINE);
		//writel((u32)tgen_tim->v_bp, disp_dev->base + TGEN_DTG_START_LINE);
		//writel((u32)tgen_tim->hactive+2, disp_dev->base + TGEN_DTG_DS_LINE_START_CD_POINT);
		//writel((u32)(tgen_tim->vactive + tgen_tim->v_bp + 1), disp_dev->base + TGEN_DTG_FIELD_END_LINE);
		//pr_info("htt:%d, vtt:%d, h:%d, v:%d, bp:%d\n",
		//	tgen_tim->htt, tgen_tim->vtt, tgen_tim->hactive, tgen_tim->vactive, tgen_tim->v_bp);
	} else {
		value |= SP7350_TGEN_FORMAT_SET(tgen_tim->fmt) |
			SP7350_TGEN_FPS_SET(tgen_tim->fps);
		pr_info("tgen set with Internal Mode\n");
	}
	writel(value, disp_dev->base + TGEN_DTG_CONFIG);
	pr_info("tgen set tim %s (%s %s)\n",
		sp7350_tgen_usr[tgen_tim->usr],
		sp7350_tgen_fmt[tgen_tim->fmt],
		sp7350_tgen_fps[tgen_tim->fps]);
}

void sp7350_tgen_timing_get(struct sp7350_tgen_timing *tgen_tim)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, user_mode;

	value = readl(disp_dev->base + TGEN_DTG_CONFIG);
	user_mode = FIELD_GET(GENMASK(0, 0), value);
	if (user_mode) {
		tgen_tim->fmt = SP7350_TGEN_FORMAT_480P;
		tgen_tim->fps = SP7350_TGEN_FPS_59P94HZ;
		tgen_tim->usr = SP7350_TGEN_USER_DEF;
	} else {
		tgen_tim->fmt = FIELD_GET(GENMASK(10, 8), value);
		tgen_tim->fps = FIELD_GET(GENMASK(5, 4), value);
		tgen_tim->usr = SP7350_TGEN_INTERNAL;
	}
	pr_info("tgen cur tim %s (%s %s)\n",
		sp7350_tgen_usr[tgen_tim->usr],
		sp7350_tgen_fmt[tgen_tim->fmt],
		sp7350_tgen_fps[tgen_tim->fps]);
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
