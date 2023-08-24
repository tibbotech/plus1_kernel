// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for VPP layer
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include <linux/bitfield.h>
#include <linux/module.h>
#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

extern unsigned char vpp0_data_array;

static const char * const imgread_fmt_sel[] = {
			"RGB888(X)", "RGB565(X)", "YUY2", "NV16",
			"Y-only(X)", "4 bytes(X)", "NV24", "NV12"
			};

void sp7350_vpp_init(void)
{

}

void sp7350_vpp_decrypt_info(void)
{
	pr_info("VPP G185-G188 reg decryption\n");
	sp7350_vpp_imgread_resolution_chk();
	sp7350_vpp_vscl_resolution_chk();

}
EXPORT_SYMBOL(sp7350_vpp_decrypt_info);

void sp7350_vpp_imgread_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 vpp_layer_en, img_bist_en, img_bist_mode;
	u32 img_yc_swap, img_uv_swap, img_fmt_sel;
	u32 value1, value2;

	value1 = readl(disp_dev->base + IMGREAD_GLOBAL_CONTROL);
	vpp_layer_en = FIELD_GET(GENMASK(31, 31), value1);

	value1 = readl(disp_dev->base + IMGREAD_CONFIG);
	img_fmt_sel = FIELD_GET(GENMASK(18, 16), value1);
	img_yc_swap = FIELD_GET(GENMASK(12, 12), value1);
	img_uv_swap = FIELD_GET(GENMASK(11, 11), value1);
	img_bist_en = FIELD_GET(GENMASK(5, 5), value1);
	img_bist_mode = FIELD_GET(GENMASK(4, 4), value1);

	pr_info("VPP G185 Resolution chk\n");
	pr_info("  IMGREAD[layer%s][bist%s]: %s\n",
		vpp_layer_en?"EN ":"DIS",
		vpp_layer_en?(img_bist_en?"ON ":"OFF"):"OFF",
		img_bist_mode?"Border":"COLORBAR");
	pr_info("    YC_SWAP[%s]UV_SWAP[%s] UPDN_FLIP[%s]\n",
		FIELD_GET(GENMASK(12, 12), value1)?"En ":"Dis",
		FIELD_GET(GENMASK(11, 11), value1)?"En ":"Dis",
		FIELD_GET(GENMASK(7, 7), value1)?"En ":"Dis");
	if ((img_yc_swap == 1) && (img_uv_swap == 0) && (img_fmt_sel == 2))
		pr_info("    IMGREAD_FMT_SEL: %s (UYVY)\n",
		imgread_fmt_sel[FIELD_GET(GENMASK(18, 16), value1)]);
	else
		pr_info("    IMGREAD_FMT_SEL: %s\n",
			imgread_fmt_sel[FIELD_GET(GENMASK(18, 16), value1)]);

	value1 = readl(disp_dev->base + IMGREAD_CROP_START);
	value2 = readl(disp_dev->base + IMGREAD_FRAME_SIZE);
	pr_info("    W_OFS %04ld(0x%04lx) W %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value2),
		FIELD_GET(GENMASK(12, 0), value2));
	pr_info("    H_OFS %04ld(0x%04lx) H %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(27, 16), value1),
		FIELD_GET(GENMASK(27, 16), value1),
		FIELD_GET(GENMASK(27, 16), value2),
		FIELD_GET(GENMASK(27, 16), value2));
	value1 = readl(disp_dev->base + IMGREAD_LINE_STRIDE_SIZE);
	pr_info("    LINE_STRIDE SIZE1 %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(13, 0), value1),
		FIELD_GET(GENMASK(13, 0), value1));
	pr_info("    LINE_STRIDE SIZE2 %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(29, 16), value1),
		FIELD_GET(GENMASK(29, 16), value1));

	value1 = readl(disp_dev->base + IMGREAD_DATA_ADDRESS_1);
	value2 = readl(disp_dev->base + IMGREAD_DATA_ADDRESS_2);
	pr_info("    ADDR1(0x%08lx) ADDR2(0x%04lx)\n",
		FIELD_GET(GENMASK(31, 0), value1),
		FIELD_GET(GENMASK(31, 0), value2));

}
EXPORT_SYMBOL(sp7350_vpp_imgread_resolution_chk);

void sp7350_vpp_vscl_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;
	u32 vpp_layer_en, vscl_bist_en, vscl_bist_mode;
	u32 vscl_xlen, vscl_ylen, vscl_output_w, vscl_output_h;

	pr_info("VPP G186 G187 Resolution chk\n");

	value1 = readl(disp_dev->base + IMGREAD_GLOBAL_CONTROL);
	vpp_layer_en = FIELD_GET(GENMASK(31, 31), value1);

	value2 = readl(disp_dev->base + VSCL_CONFIG2);
	vscl_bist_en = FIELD_GET(GENMASK(7, 7), value2);
	vscl_bist_mode = FIELD_GET(GENMASK(8, 8), value2);

	pr_info("    VSCL[layer%s][bist%s]: %s\n",
		vpp_layer_en?"EN ":"DIS",
		vpp_layer_en?(vscl_bist_en?"ON ":"OFF"):"OFF",
		vscl_bist_mode?"Border":"COLORBAR");

	value1 = readl(disp_dev->base + VSCL_ACTRL_I_XLEN);
	value2 = readl(disp_dev->base + VSCL_ACTRL_I_YLEN);
	pr_info("    ACTRL XLEN %04ld(0x%04lx) YLEN %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(11, 0), value2),
		FIELD_GET(GENMASK(11, 0), value2));
	value1 = readl(disp_dev->base + VSCL_ACTRL_S_XSTART);
	value2 = readl(disp_dev->base + VSCL_ACTRL_S_YSTART);
	pr_info("    ACTRL ST_X %04ld(0x%04lx) ST_Y %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(11, 0), value2),
		FIELD_GET(GENMASK(11, 0), value2));
	vscl_xlen = readl(disp_dev->base + VSCL_ACTRL_S_XLEN);
	vscl_ylen = readl(disp_dev->base + VSCL_ACTRL_S_YLEN);
	pr_info("    ACTRL OT_X %04ld(0x%04lx) OT_Y %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), vscl_xlen),
		FIELD_GET(GENMASK(12, 0), vscl_xlen),
		FIELD_GET(GENMASK(11, 0), vscl_ylen),
		FIELD_GET(GENMASK(11, 0), vscl_ylen));

	value1 = readl(disp_dev->base + VSCL_DCTRL_O_XLEN);
	value2 = readl(disp_dev->base + VSCL_DCTRL_O_YLEN);
	pr_info("    DCTRL XLEN %04ld(0x%04lx) YLEN %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(11, 0), value2),
		FIELD_GET(GENMASK(11, 0), value2));
	value1 = readl(disp_dev->base + VSCL_DCTRL_D_XSTART);
	value2 = readl(disp_dev->base + VSCL_DCTRL_D_YSTART);
	pr_info("    DCTRL ST_X %04ld(0x%04lx) ST_Y %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(12, 0), value1),
		FIELD_GET(GENMASK(11, 0), value2),
		FIELD_GET(GENMASK(11, 0), value2));
	vscl_output_w = readl(disp_dev->base + VSCL_DCTRL_D_XLEN);
	vscl_output_h = readl(disp_dev->base + VSCL_DCTRL_D_YLEN);
	pr_info("    DCTRL OT_X %04ld(0x%04lx) OT_Y %04ld(0x%04lx)\n",
		FIELD_GET(GENMASK(12, 0), vscl_output_w),
		FIELD_GET(GENMASK(12, 0), vscl_output_w),
		FIELD_GET(GENMASK(11, 0), vscl_output_h),
		FIELD_GET(GENMASK(11, 0), vscl_output_h));

	pr_info("    (x-axis, y-axis) = (%02d.%03d x,%02d.%03d x) \n",
		vscl_output_w/vscl_xlen, (vscl_output_w * 1000 / vscl_xlen)-(vscl_output_w/vscl_xlen)*1000,
		vscl_output_h/vscl_ylen, (vscl_output_h * 1000 / vscl_ylen)-(vscl_output_h/vscl_ylen)*1000);

	value1 = readl(disp_dev->base + VSCL_HINT_CTRL);
	value2 = readl(disp_dev->base + VSCL_VINT_CTRL);
	pr_info("  HINT_CTRL / VINT_CTRL : 0x%04x 0x%04x\n", value1, value2);

	value1 = readl(disp_dev->base + VSCL_HINT_HFACTOR_HIGH);
	value2 = readl(disp_dev->base + VSCL_HINT_HFACTOR_LOW);
	pr_info("    HFACTOR_(HIGH/LOW): (0x%04x 0x%04x)\n", value1, value2);
	pr_info("    hfactor: %08ld (0x%04lx)\n",
		((FIELD_GET(GENMASK(8, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)),
		((FIELD_GET(GENMASK(8, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)));
	value1 = readl(disp_dev->base + VSCL_HINT_INITF_HIGH);
	value2 = readl(disp_dev->base + VSCL_HINT_INITF_LOW);
	pr_info("    HINITF_(HIGH/LOW) : (0x%04x 0x%04x)\n", value1, value2);
	pr_info("    hinitf: %s %08ld (0x%04lx)\n",
		(FIELD_GET(GENMASK(6, 6), value1)?"[plus ]":"[minux]"),
		((FIELD_GET(GENMASK(5, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)),
		((FIELD_GET(GENMASK(5, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)));

	value1 = readl(disp_dev->base + VSCL_VINT_VFACTOR_HIGH);
	value2 = readl(disp_dev->base + VSCL_VINT_VFACTOR_LOW);
	pr_info("    VFACTOR_(HIGH/LOW): (0x%04x 0x%04x)\n", value1, value2);
	pr_info("    vfactor: %08ld (0x%04lx)\n",
		((FIELD_GET(GENMASK(8, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)),
		((FIELD_GET(GENMASK(8, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)));
	value1 = readl(disp_dev->base + VSCL_VINT_INITF_HIGH);
	value2 = readl(disp_dev->base + VSCL_VINT_INITF_LOW);
	pr_info("    VINITF_(HIGH/LOW) : (0x%04x 0x%04x)\n", value1, value2);
	pr_info("    vinitf: %s %08ld (0x%04lx)\n",
		(FIELD_GET(GENMASK(6, 6), value1)?"[plus ]":"[minux]"),
		((FIELD_GET(GENMASK(5, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)),
		((FIELD_GET(GENMASK(5, 0), value1) << 16) + FIELD_GET(GENMASK(15, 0), value2)));

}
EXPORT_SYMBOL(sp7350_vpp_vscl_resolution_chk);

void sp7350_vpp_layer_onoff(int onoff)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	//pr_info("sp7350_vpp_layer: VPP0 %s\n", onoff?"ON":"OFF");
	value = readl(disp_dev->base + IMGREAD_GLOBAL_CONTROL);
	if (onoff)
		value |= SP7350_VPP_IMGREAD_FETCH_EN;
	else
		value &= ~(SP7350_VPP_IMGREAD_FETCH_EN);

	writel(value, disp_dev->base + IMGREAD_GLOBAL_CONTROL);

}
EXPORT_SYMBOL(sp7350_vpp_layer_onoff);

void sp7350_vpp_bist_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, vpp_layer_en;
	u32 img_bist_en, img_bist_mode;
	u32 vscl_bist_en, vscl_bist_mode;

	value = readl(disp_dev->base + IMGREAD_GLOBAL_CONTROL);
	vpp_layer_en = FIELD_GET(GENMASK(31, 31), value);

	value = readl(disp_dev->base + IMGREAD_CONFIG);
	img_bist_en = FIELD_GET(GENMASK(5, 5), value);
	img_bist_mode = FIELD_GET(GENMASK(4, 4), value);

	value = readl(disp_dev->base + VSCL_CONFIG2);
	vscl_bist_en = FIELD_GET(GENMASK(7, 7), value);
	vscl_bist_mode = FIELD_GET(GENMASK(8, 8), value);

	pr_info("VPP G185 G186 BIST info\n");
	pr_info("  IMGREAD[layer%s][bist%s]: %s\n",
		vpp_layer_en?"EN ":"DIS",
		vpp_layer_en?(img_bist_en?"ON ":"OFF"):"OFF",
		img_bist_mode?"Border":"COLORBAR");

	pr_info("     VSCL[layer%s][bist%s]: %s\n",
		vpp_layer_en?"EN ":"DIS",
		vpp_layer_en?(vscl_bist_en?"ON ":"OFF"):"OFF",
		vscl_bist_mode?"Border":"COLORBAR");

}
EXPORT_SYMBOL(sp7350_vpp_bist_info);

void sp7350_vpp_bist_set(int img_vscl_sel, int bist_en, int vpp_bist_type)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	if (img_vscl_sel == 0) {
		pr_info("sp7350_vpp_bist_set: VPP0 for imgread\n");
		value = readl(disp_dev->base + IMGREAD_CONFIG);
		value &= ~(SP7350_VPP_IMGREAD_BIST_MASK);
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_YUY2);
		writel((value | bist_en << 5 | vpp_bist_type << 4), disp_dev->base + IMGREAD_CONFIG);
	} else if (img_vscl_sel == 1) {
		pr_info("sp7350_vpp_bist_set: VPP0 for vscl\n");
		value = readl(disp_dev->base + VSCL_CONFIG2);
		value &= ~(SP7350_VPP_VSCL_BIST_MASK);
		writel((value | bist_en << 7 | vpp_bist_type << 8), disp_dev->base + VSCL_CONFIG2);
	} else {
		pr_info("sp7350_vpp_bist_set: undef\n");
	}

}
EXPORT_SYMBOL(sp7350_vpp_bist_set);

/*
 * 0: none setting
 * 1: method 1 , set imgread xstart&ystart
 * 2: method 2 , set vscl xstart&ystart
 */
#define SP7350_VPP_SCALE_METHOD		1
/*
 * 0: use (factor - 1) , closet
 * 1: use (factor) , round up
 */
#define SP7350_VPP_FACTOR_METHOD	0

int sp7350_vpp_imgread_set(u32 data_addr1, int x, int y, int w, int h, int yuv_fmt)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	value = readl(disp_dev->base + IMGREAD_GLOBAL_CONTROL);
	value |= SP7350_VPP_IMGREAD_FETCH_EN;
	writel(value, disp_dev->base + IMGREAD_GLOBAL_CONTROL);

	value = readl(disp_dev->base + IMGREAD_CONFIG);
	value &= ~(SP7350_VPP_IMGREAD_DATA_FMT |
		SP7350_VPP_IMGREAD_YC_SWAP | SP7350_VPP_IMGREAD_UV_SWAP);
	if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_YUY2)
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_YUY2);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV12)
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_NV12);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV16)
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_NV16);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV24)
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_NV24);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_UYVY)
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_YUY2) |
			SP7350_VPP_IMGREAD_YC_SWAP;
	else
		value |= SP7350_VPP_IMGREAD_DATA_FMT_SEL(SP7350_VPP_IMGREAD_DATA_FMT_YUY2);

	value |= SP7350_VPP_IMGREAD_FM_MODE;

	if (disp_dev->out_res.mipitx_mode == SP7350_MIPITX_DSI)
		;//TBD
	else
		value |= SP7350_VPP_IMGREAD_UV_SWAP; //for CSI YUY2 test only

	writel(value, disp_dev->base + IMGREAD_CONFIG);

	value = 0;
	#if (SP7350_VPP_SCALE_METHOD == 1) //method 1 , set imgread xstart&ystart
	value |= (y << 16) | (x << 0);
	#endif
	writel(value, disp_dev->base + IMGREAD_CROP_START);

	value = 0;
	value |= (h << 16) | (w << 0);
	writel(value, disp_dev->base + IMGREAD_FRAME_SIZE);

	value = 0;
	if ((yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_YUY2) ||
		 (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_UYVY))
		value |= (0 << 16) | (w * 2);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV12)
		//value |= ((w / 2) << 16) | (w);
		value |= (w << 16) | (w);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV16)
		value |= (0 << 16) | (w);
	else if (yuv_fmt == SP7350_VPP_IMGREAD_DATA_FMT_NV24)
		value |= ((w * 2) << 16) | (w);
	else
		value |= (0 << 16) | (w * 2);
	writel(value, disp_dev->base + IMGREAD_LINE_STRIDE_SIZE);

	writel((u32)data_addr1,
		disp_dev->base + IMGREAD_DATA_ADDRESS_1);
	writel((u32)data_addr1 + (w * h),
		disp_dev->base + IMGREAD_DATA_ADDRESS_2);

	//pr_info("  imgread (%d, %d)\n", w, h);
	//pr_info("    luma=0x%08x, chroma=0x%08x\n",
	//	data_addr1, data_addr1 + (w * h));

	return 0;
}
EXPORT_SYMBOL(sp7350_vpp_imgread_set);

int sp7350_vpp_vscl_set(int x, int y, int xlen, int ylen, int input_w, int input_h, int output_w, int output_h)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u64 factor64, factor_init64;
	u32 value;

	//pr_info("vpp vscl setting\n");

	value = readl(disp_dev->base + VSCL_CONFIG2);
	value |= (SP7350_VPP_VSCL_BUFR_EN |
		SP7350_VPP_VSCL_VINT_EN |
		SP7350_VPP_VSCL_HINT_EN |
		SP7350_VPP_VSCL_DCTRL_EN |
		SP7350_VPP_VSCL_ACTRL_EN);
	writel(value, disp_dev->base + VSCL_CONFIG2);

	//pr_info("  IN (%d, %d), OUT (%d, %d)\n", input_w, input_h, output_w, output_h);
	//pr_info("  crop from(%d, %d)to(%d, %d)\n", x, y, x + xlen, y + ylen);

	if ((xlen == 0) && (ylen == 0))
		pr_info("  no scaling\n");

	if (xlen < 64) xlen = 64;
	if (xlen > input_w) xlen = input_w;
	if (ylen < 64) ylen = 64;
	if (ylen > input_h) ylen = input_h;

	if ((x + xlen) > input_w) xlen = input_w - x;
	if ((y + ylen) > input_h) xlen = input_h - y;

	//pr_info("  IN (%d, %d), OUT (%d, %d)\n", input_w, input_h, output_w, output_h);
	//pr_info("  crop from(%d, %d)to(%d, %d)\n", x, y, x + xlen, y + ylen);

	//pr_info("    (x-axis, y-axis) = (%02d.%03d x,%02d.%03d x) \n",
	//	output_w/xlen, (output_w * 1000 / xlen)-(output_w/xlen)*1000,
	//	output_h/ylen, (output_h * 1000 / ylen)-(output_h/ylen)*1000);

	writel(input_w, disp_dev->base + VSCL_ACTRL_I_XLEN);
	writel(input_h, disp_dev->base + VSCL_ACTRL_I_YLEN);
	#if (SP7350_VPP_SCALE_METHOD == 2) //method 2 , set vscl xstart&ystart
	writel(x, disp_dev->base + VSCL_ACTRL_S_XSTART);
	writel(y, disp_dev->base + VSCL_ACTRL_S_YSTART);
	#endif

	writel(xlen, disp_dev->base + VSCL_ACTRL_S_XLEN);
	writel(ylen, disp_dev->base + VSCL_ACTRL_S_YLEN);

	writel(output_w, disp_dev->base + VSCL_DCTRL_O_XLEN);
	writel(output_h, disp_dev->base + VSCL_DCTRL_O_YLEN);
	writel(0, disp_dev->base + VSCL_DCTRL_D_XSTART);
	writel(0, disp_dev->base + VSCL_DCTRL_D_YSTART);
	writel(output_w, disp_dev->base + VSCL_DCTRL_D_XLEN);
	writel(output_h, disp_dev->base + VSCL_DCTRL_D_YLEN);

	/*
	 * VSCL SETTING for HORIZONTAL
	 */
	value = readl(disp_dev->base + VSCL_HINT_CTRL);
	value |= SP7350_VPP_VSCL_HINT_FLT_EN;
	writel(value, disp_dev->base + VSCL_HINT_CTRL);

	/* cal h_factor */
	if (xlen == output_w)
		factor64 = DIV64_U64_ROUND_CLOSEST((u64)xlen << 22, (u64)output_w);
	else {
		factor64 = DIV64_U64_ROUND_UP((u64)xlen << 22, (u64)output_w);
		#if (SP7350_VPP_FACTOR_METHOD == 0)
		factor64 -= 1;
		#endif
	}
	value = ((u32)factor64 >> 0) & 0x0000ffff;
	writel(value, disp_dev->base + VSCL_HINT_HFACTOR_LOW);
	value = ((u32)factor64 >> 16) & 0x000001ff;
	writel(value, disp_dev->base + VSCL_HINT_HFACTOR_HIGH);

	/* cal h_factor_init */
	if (xlen == output_w)
		factor_init64 = 0;
	else {
		factor_init64 = ((u64)xlen << 22) % ((u64)output_w);
		#if (SP7350_VPP_FACTOR_METHOD == 1)
		factor_init64 = output_w - factor_init64;
		#endif
	}
	value = ((u32)factor_init64 >> 0) & 0x0000ffff;
	writel(value, disp_dev->base + VSCL_HINT_INITF_LOW);
	value = ((u32)factor_init64 >> 16) & 0x0000003f;
	#if (SP7350_VPP_FACTOR_METHOD == 1)
	value |= SP7350_VPP_VSCL_HINT_INITF_PN;
	#endif
	writel(value, disp_dev->base + VSCL_HINT_INITF_HIGH);

	/*
	 * VSCL SETTING for VERTICAL
	 */
	value = readl(disp_dev->base + VSCL_VINT_CTRL);
	value |= SP7350_VPP_VSCL_VINT_FLT_EN;
	writel(value, disp_dev->base + VSCL_VINT_CTRL);

	/* cal v_factor */
	if (ylen == output_h)
		factor64 = DIV64_U64_ROUND_CLOSEST((u64)ylen << 22, (u64)output_h);
	else {
		factor64 = DIV64_U64_ROUND_UP((u64)ylen << 22, (u64)output_h);
		#if (SP7350_VPP_FACTOR_METHOD == 0)
		factor64 -= 1;
		#endif
	}
	value = ((u32)factor64 >> 0) & 0x0000ffff;
	writel(value, disp_dev->base + VSCL_VINT_VFACTOR_LOW);
	value = ((u32)factor64 >> 16) & 0x000001ff;
	writel(value, disp_dev->base + VSCL_VINT_VFACTOR_HIGH);

	/* cal v_factor_init */
	if (ylen == output_h)
		factor_init64 = 0;
	else {
		factor_init64 = ((u64)ylen << 22) % ((u64)output_h);
		#if (SP7350_VPP_FACTOR_METHOD == 1)
		factor_init64 = output_h - factor_init64;
		#endif
	}
	value = ((u32)factor_init64 >> 0) & 0x0000ffff;
	writel(value, disp_dev->base + VSCL_VINT_INITF_LOW);
	value = ((u32)factor_init64 >> 16) & 0x0000003f;
	#if (SP7350_VPP_FACTOR_METHOD == 1)
	value |= SP7350_VPP_VSCL_VINT_INITF_PN;
	#endif
	writel(value, disp_dev->base + VSCL_VINT_INITF_HIGH);


	return 0;
}
EXPORT_SYMBOL(sp7350_vpp_vscl_set);

#define SP7350_VPP_VPOST_WIN_ALPHA_VALUE	0xff
#define SP7350_VPP_VPOST_VPP_ALPHA_VALUE	0xff

int sp7350_vpp_vpost_set(int x, int y, int input_w, int input_h, int output_w, int output_h)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	/*
	 * VPOST SETTING
	 */
	value = readl(disp_dev->base + VPOST_CONFIG);
	value |= SP7350_VPP_VPOST_OPIF_EN;
	writel(value, disp_dev->base + VPOST_CONFIG);

	value = readl(disp_dev->base + VPOST_OPIF_CONFIG);
	//value |= SP7350_VPP_VPOST_WIN_ALPHA_EN;
	//value |= SP7350_VPP_VPOST_WIN_YUV_EN;
	value |= SP7350_VPP_VPOST_WIN_ALPHA_EN | SP7350_VPP_VPOST_WIN_YUV_EN;
	writel(value, disp_dev->base + VPOST_OPIF_CONFIG);

	/*set alpha value*/
	value = readl(disp_dev->base + VPOST_OPIF_ALPHA);
	value &= ~(SP7350_VPP_VPOST_WIN_ALPHA_MASK | SP7350_VPP_VPOST_VPP_ALPHA_MASK);
	value |= (SP7350_VPP_VPOST_WIN_ALPHA_SET(SP7350_VPP_VPOST_WIN_ALPHA_VALUE) |
		SP7350_VPP_VPOST_VPP_ALPHA_SET(SP7350_VPP_VPOST_VPP_ALPHA_VALUE));
	writel(value, disp_dev->base + VPOST_OPIF_ALPHA);

	/*set mask region*/
	value = readl(disp_dev->base + VPOST_OPIF_MSKTOP);
	value &= ~SP7350_VPP_VPOST_OPIF_TOP_MASK;
	value |= SP7350_VPP_VPOST_OPIF_TOP_SET(output_h >> 2);
	writel(value, disp_dev->base + VPOST_OPIF_MSKTOP);	

	value = readl(disp_dev->base + VPOST_OPIF_MSKBOT);
	value &= ~SP7350_VPP_VPOST_OPIF_BOT_MASK;
	value |= SP7350_VPP_VPOST_OPIF_BOT_SET(output_h >> 2);
	writel(value, disp_dev->base + VPOST_OPIF_MSKBOT);

	value = readl(disp_dev->base + VPOST_OPIF_MSKLEFT);
	value &= ~SP7350_VPP_VPOST_OPIF_LEFT_MASK;
	value |= SP7350_VPP_VPOST_OPIF_LEFT_SET(output_w >> 2);
	writel(value, disp_dev->base + VPOST_OPIF_MSKLEFT);

	value = readl(disp_dev->base + VPOST_OPIF_MSKRIGHT);
	value &= ~SP7350_VPP_VPOST_OPIF_RIGHT_MASK;
	value |= SP7350_VPP_VPOST_OPIF_RIGHT_SET(output_w >> 2);
	writel(value, disp_dev->base + VPOST_OPIF_MSKRIGHT);

	return 0;
}
EXPORT_SYMBOL(sp7350_vpp_vpost_set);

int sp7350_vpp_resolution_init(struct sp_disp_device *disp_dev)
{
	/*
	 * enable vpp layer
	 */
	sp7350_vpp_layer_onoff(1);

	/*
	 * set vpp layer for imgread block
	 */
	sp7350_vpp_imgread_set((u32)virt_to_phys(&vpp0_data_array),
			disp_dev->vpp_res[0].x_ofs,
			disp_dev->vpp_res[0].y_ofs,
			disp_dev->vpp_res[0].width,
			disp_dev->vpp_res[0].height,
			disp_dev->vpp_res[0].color_mode);
	/*
	 * set vpp layer for vscl block
	 */
	sp7350_vpp_vscl_set(disp_dev->vpp_res[0].x_ofs, disp_dev->vpp_res[0].y_ofs,
			disp_dev->vpp_res[0].crop_w, disp_dev->vpp_res[0].crop_h,
			disp_dev->vpp_res[0].width, disp_dev->vpp_res[0].height,
			disp_dev->out_res.width, disp_dev->out_res.height);

	return 0;
}
EXPORT_SYMBOL(sp7350_vpp_resolution_init);

int sp7350_vpp_restore(struct sp_disp_device *disp_dev)
{
	/*
	 * set vpp layer for imgread block
	 */
	sp7350_vpp_imgread_set((u32)virt_to_phys(&vpp0_data_array),
			disp_dev->vpp_res[0].x_ofs,
			disp_dev->vpp_res[0].y_ofs,
			disp_dev->vpp_res[0].width,
			disp_dev->vpp_res[0].height,
			disp_dev->vpp_res[0].color_mode);
	/*
	 * set vpp layer for vscl block
	 */
	sp7350_vpp_vscl_set(disp_dev->vpp_res[0].x_ofs, disp_dev->vpp_res[0].y_ofs,
			disp_dev->vpp_res[0].crop_w, disp_dev->vpp_res[0].crop_h,
			disp_dev->vpp_res[0].width, disp_dev->vpp_res[0].height,
			disp_dev->out_res.width, disp_dev->out_res.height);

	return 0;
}
EXPORT_SYMBOL(sp7350_vpp_restore);
