// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for DMIX block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include <linux/bitfield.h>
#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

static const char * const dmix_layer_name[] = {
	"BG", "L1", "L2", "L3", "L4", "L5", "L6"};
static const char * const dmix_fg_sel[] = {
	"VPP0", "VPP1", "VPP2", "OSD0", "OSD1", "OSD2", "OSD3", "PTG"};
static const char * const ptg_border_pattern[] = {
	"colorbar", "colorbar", "border", "region"};
static const char * const ptg_pattern_type[] = {
	"COLORBAR", "BORDER", "SNOW", "REGION"};
static const char * const dmix_layer_mode[] = {
	"blending", "transparent", "opacity", "none"};
static const char * const dmix_layer_mode1[] = {
	"+", "-", "O", "x"};
static const char * const dmix_layer_mode2[] = {
	"[+]blending", "[-]transparent", "[O]opacity", "[x]unknown"};
static const char * const dmix_layer_sel[] = {
	"VPP0", "VPP1", "VPP2", "OSD0", "OSD1","OSD2","OSD3","PTG ","none",};
static const char * const dmix_pix_en_sel[] = {
	"VENC_DMIX", "VDACIF_DMIX", "TCON0_DMIX", "TCON1_DMIX",
	"TCON2_DMIX","TGEN_DMIX","DVE","VENC_DMIX"};

void sp7350_dmix_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	/* DMIX setting FG_SEL
	 * L6   L5   L4   L3   L2   L1   BG
	 * OSD0 OSD1 OSD2 OSD3 ---- VPP0 PTG
	 */
	value = 0;
	value |= SP7350_DMIX_L6_FG_SEL(SP7350_DMIX_OSD0_SEL) |
		SP7350_DMIX_L5_FG_SEL(SP7350_DMIX_OSD1_SEL) |
		SP7350_DMIX_L4_FG_SEL(SP7350_DMIX_OSD2_SEL) |
		SP7350_DMIX_L3_FG_SEL(SP7350_DMIX_OSD3_SEL) |
		SP7350_DMIX_L2_FG_SEL(SP7350_DMIX_VPP1_SEL) |
		SP7350_DMIX_L1_FG_SEL(SP7350_DMIX_VPP0_SEL) |
		SP7350_DMIX_BG_FG_SEL(SP7350_DMIX_PTG_SEL);
	writel(value, disp_dev->base + DMIX_LAYER_CONFIG_0);
	//writel(0x34561070, disp_dev->base + DMIX_LAYER_CONFIG_0); //(default setting)

	/* DMIX setting MODE_SEL
	 * L6   L5   L4   L3   L2   L1   BG
	 * OSD0 OSD1 OSD2 OSD3 ---- VPP0 PTG
	 * (AlphaBlend / Transparent / Opacity setting)
	 */
	value = 0;
	value |= SP7350_DMIX_L6_MODE_SEL(SP7350_DMIX_TRANSPARENT) |
		SP7350_DMIX_L5_MODE_SEL(SP7350_DMIX_TRANSPARENT) |
		SP7350_DMIX_L4_MODE_SEL(SP7350_DMIX_TRANSPARENT) |
		SP7350_DMIX_L3_MODE_SEL(SP7350_DMIX_TRANSPARENT) |
		SP7350_DMIX_L2_MODE_SEL(SP7350_DMIX_TRANSPARENT) |
		SP7350_DMIX_L1_MODE_SEL(SP7350_DMIX_TRANSPARENT);
	writel(value, disp_dev->base + DMIX_LAYER_CONFIG_1); //All AlphaBlend
	//writel(0x00000000, disp_dev->base + DMIX_LAYER_CONFIG_1); //All AlphaBlend
	//writel(0x00000555, disp_dev->base + DMIX_LAYER_CONFIG_1); //All Transparent

	/* DMIX PTG(default border pixel len=0 (BGC))
	 */
	value = 0;
	value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_BORDER) |
		SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_00);
	writel(value, disp_dev->base + DMIX_PTG_CONFIG_0); //BackGround only
	//writel(0x00002000, disp_dev->base + DMIX_PTG_CONFIG_0); //BackGround only
	//writel(0x00002001, disp_dev->base + DMIX_PTG_CONFIG_0); //BackGround + Border pix = 1
	//writel(0x00002007, disp_dev->base + DMIX_PTG_CONFIG_0); //BackGround + Border pix = 7

	/* DMIX PTG(BackGround Color Setting)
	 */
	value =0;
	value |= SP7350_DMIX_PTG_BLACK;
	writel(value, disp_dev->base + DMIX_PTG_CONFIG_2); //red for BackGround
	//writel(0x004040f0, disp_dev->base + DMIX_PTG_CONFIG_2); //red for BackGround
	//writel(0x00101010, disp_dev->base + DMIX_PTG_CONFIG_2); //green for BackGround
	//writel(0x0029f06e, disp_dev->base + DMIX_PTG_CONFIG_2); //blue for BackGround
	//writel(0x00108080, disp_dev->base + DMIX_PTG_CONFIG_2); //black for BackGround (default setting)

	/* DMIX PIXEL_EN_SEL
	 */
	value =0;
	value |= SP7350_DMIX_SOURCE_SEL(SP7350_DMIX_TCON0_DMIX);
	writel(value, disp_dev->base + DMIX_SOURCE_SEL);
	//writel(0x00000002, disp_dev->base + DMIX_SOURCE_SEL); //PIXEL_EN_SEL=TCON0(don't change)

}

void sp7350_dmix_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3;
	u32 ptg_rotate, ptg_source_sel, ptg_border_on, ptg_border_pix;
	u32 layer6_mode, layer5_mode, layer4_mode, layer3_mode, layer2_mode, layer1_mode;
	u32 layer6_sel, layer5_sel, layer4_sel, layer3_sel, layer2_sel, layer1_sel, bg_sel;

	pr_info("DMIX G198 reg decryption\n");
	//pr_info("DMIX Layer Status\n");
	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	layer6_sel = FIELD_GET(GENMASK(31,28), value1);
	layer5_sel = FIELD_GET(GENMASK(27,24), value1);
	layer4_sel = FIELD_GET(GENMASK(23,20), value1);
	layer3_sel = FIELD_GET(GENMASK(19,16), value1);
	layer2_sel = FIELD_GET(GENMASK(15,12), value1);
	layer1_sel = FIELD_GET(GENMASK(11,8), value1);
	bg_sel = FIELD_GET(GENMASK(7,4), value1);
	pr_info("  G198.00 LayerSel 0x%08x\n", value1);
	value2 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);
	layer6_mode = FIELD_GET(GENMASK(11,10), value2);
	layer5_mode = FIELD_GET(GENMASK(9,8), value2);
	layer4_mode = FIELD_GET(GENMASK(7,6), value2);
	layer3_mode = FIELD_GET(GENMASK(5,4), value2);
	layer2_mode = FIELD_GET(GENMASK(3,2), value2);
	layer1_mode = FIELD_GET(GENMASK(1,0), value2);
	pr_info("  G198.01 LayerMode 0x%08x\n", value2);

	pr_info("    L6[%s]%s\n", dmix_layer_sel[layer6_sel], dmix_layer_mode2[layer6_mode]);
	pr_info("    L5[%s]%s\n", dmix_layer_sel[layer5_sel], dmix_layer_mode2[layer5_mode]);
	pr_info("    L4[%s]%s\n", dmix_layer_sel[layer4_sel], dmix_layer_mode2[layer4_mode]);
	pr_info("    L3[%s]%s\n", dmix_layer_sel[layer3_sel], dmix_layer_mode2[layer3_mode]);
	pr_info("    L2[%s]%s\n", dmix_layer_sel[layer2_sel], dmix_layer_mode2[layer2_mode]);
	pr_info("    L1[%s]%s\n", dmix_layer_sel[layer1_sel], dmix_layer_mode2[layer1_mode]);
	pr_info("    BG[%s][O]\n", dmix_layer_sel[bg_sel]);

	value1 = readl(disp_dev->base + DMIX_PTG_CONFIG_0);
	pr_info("  G198.09 PTG_CFG_0 0x%08x\n", value1);
	ptg_rotate = FIELD_GET(GENMASK(15,15), value1);
	ptg_source_sel = FIELD_GET(GENMASK(14,14), value1);
	ptg_border_on = FIELD_GET(GENMASK(13,12), value1);
	ptg_border_pix = FIELD_GET(GENMASK(2,0), value1);
	pr_info("    SRC_SEL[%s]\n", ptg_source_sel?"snow":"bar/bor");
	pr_info("    ROT[%s] PAT[%s] BOR_PIX:%d\n",
	ptg_rotate?"90":"0",
	ptg_border_pattern[ptg_border_on],
	ptg_border_pix);

	value2 = readl(disp_dev->base + DMIX_PTG_CONFIG_1);
	pr_info("  G198.10 PTG_CFG_1 0x%08x\n", value2);
	pr_info("    SEED2[%02ld] SEED1[%02ld] V_DOT[%02ld] H_DOT[%02ld]\n",
		FIELD_GET(GENMASK(25,24), value2),
		FIELD_GET(GENMASK(23,16), value2),
		FIELD_GET(GENMASK(7,4), value2),
		FIELD_GET(GENMASK(3,0), value2));
	value3 = readl(disp_dev->base + DMIX_PTG_CONFIG_2);
	pr_info("  G198.11 PTG_CFG_2 0x%08x\n", value3);
	pr_info("    Y/CB/CR (%03ld/%03ld/%03ld)(0x%02lx/0x%02lx/0x%02lx)\n",
		FIELD_GET(GENMASK(23,16), value3),
		FIELD_GET(GENMASK(15,8), value3),
		FIELD_GET(GENMASK(7,0), value3),
		FIELD_GET(GENMASK(23,16), value3),
		FIELD_GET(GENMASK(15,8), value3),
		FIELD_GET(GENMASK(7,0), value3));

	value1 = readl(disp_dev->base + DMIX_SOURCE_SEL);
	pr_info("  G198.20 DMIX_SOURCE_SEL 0x%08x\n", value1);
	pr_info("    DMIX source sel[%s_PIX_EN]\n",
		dmix_pix_en_sel[FIELD_GET(GENMASK(2,0), value1)]);

}
EXPORT_SYMBOL(sp7350_dmix_decrypt_info);

void sp7350_dmix_all_layer_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;
	u32 layer6_mode, layer5_mode, layer4_mode, layer3_mode, layer2_mode, layer1_mode;
	u32 layer6_sel, layer5_sel, layer4_sel, layer3_sel, layer2_sel, layer1_sel, bg_sel;

	pr_info("dmix all layer info\n");
	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	layer6_sel = FIELD_GET(GENMASK(31,28), value1);
	layer5_sel = FIELD_GET(GENMASK(27,24), value1);
	layer4_sel = FIELD_GET(GENMASK(23,20), value1);
	layer3_sel = FIELD_GET(GENMASK(19,16), value1);
	layer2_sel = FIELD_GET(GENMASK(15,12), value1);
	layer1_sel = FIELD_GET(GENMASK(11,8), value1);
	bg_sel = FIELD_GET(GENMASK(7,4), value1);
	//pr_info("  G198.00 LayerSel 0x%08x\n", value1);
	value2 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);
	layer6_mode = FIELD_GET(GENMASK(11,10), value2);
	layer5_mode = FIELD_GET(GENMASK(9,8), value2);
	layer4_mode = FIELD_GET(GENMASK(7,6), value2);
	layer3_mode = FIELD_GET(GENMASK(5,4), value2);
	layer2_mode = FIELD_GET(GENMASK(3,2), value2);
	layer1_mode = FIELD_GET(GENMASK(1,0), value2);
	//pr_info("  G198.01 LayerMode 0x%08x\n", value2);

	pr_info("    L6[%s]%s\n", dmix_layer_sel[layer6_sel], dmix_layer_mode2[layer6_mode]);
	pr_info("    L5[%s]%s\n", dmix_layer_sel[layer5_sel], dmix_layer_mode2[layer5_mode]);
	pr_info("    L4[%s]%s\n", dmix_layer_sel[layer4_sel], dmix_layer_mode2[layer4_mode]);
	pr_info("    L3[%s]%s\n", dmix_layer_sel[layer3_sel], dmix_layer_mode2[layer3_mode]);
	pr_info("    L2[%s]%s\n", dmix_layer_sel[layer2_sel], dmix_layer_mode2[layer2_mode]);
	pr_info("    L1[%s]%s\n", dmix_layer_sel[layer1_sel], dmix_layer_mode2[layer1_mode]);
	pr_info("    BG[%s][O]\n", dmix_layer_sel[bg_sel]);
}
EXPORT_SYMBOL(sp7350_dmix_all_layer_info);

void sp7350_dmix_bist_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3;
	u32 ptg_rotate, ptg_source_sel, ptg_border_on, ptg_border_pix, ptg_pat_type;
	u32 layer6_mode, layer5_mode, layer4_mode, layer3_mode, layer2_mode, layer1_mode;
	u32 layer6_sel, layer5_sel, layer4_sel, layer3_sel, layer2_sel, layer1_sel, bg_sel;

	pr_info("DMIX G198 BIST info\n");
	value1 = readl(disp_dev->base + DMIX_PTG_CONFIG_0);
	value2 = readl(disp_dev->base + DMIX_PTG_CONFIG_1);
	value3 = readl(disp_dev->base + DMIX_PTG_CONFIG_2);
	ptg_rotate = FIELD_GET(GENMASK(15,15), value1);
	ptg_source_sel = FIELD_GET(GENMASK(14,14), value1);
	ptg_border_on = FIELD_GET(GENMASK(13,12), value1);
	ptg_border_pix = FIELD_GET(GENMASK(2,0), value1);

	pr_info("  BIST CFG 0x%08x 0x%08x 0x%08x\n", value1, value2, value3);

	if(ptg_source_sel == 1)
		ptg_pat_type = 2; //snow
	else {
		if ((ptg_border_on == 0) || (ptg_border_on == 1))
			ptg_pat_type = 0; //colorbar
		else if (ptg_border_on == 2)
			ptg_pat_type = 1; //border
		else if (ptg_border_on == 3)
			ptg_pat_type = 3; //region
	}

	pr_info("     PTG [LayerEN ][bistON ]: %s\n", ptg_pattern_type[ptg_pat_type]);
	if (ptg_pat_type == 0)
		pr_info("       BAR  [<--][ROT %s] \n", ptg_rotate?"90":"0");
	else if (ptg_pat_type == 1)
		pr_info("       BOR  [<--][PIX %d] \n", ptg_border_pix);
	else if (ptg_pat_type == 2)
		pr_info("       SNOW [<--][H %02ld][V %02ld] \n",
			FIELD_GET(GENMASK(3,0), value2),
			FIELD_GET(GENMASK(7,4), value2));
	else if (ptg_pat_type == 3)
		pr_info("       REGION [<--] \n");
	pr_info("       Y/CB/CR (0x%02lx/0x%02lx/0x%02lx)\n",
		FIELD_GET(GENMASK(23,16), value3),
		FIELD_GET(GENMASK(15,8), value3),
		FIELD_GET(GENMASK(7,0), value3));	

	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	layer6_sel = FIELD_GET(GENMASK(31,28), value1);
	layer5_sel = FIELD_GET(GENMASK(27,24), value1);
	layer4_sel = FIELD_GET(GENMASK(23,20), value1);
	layer3_sel = FIELD_GET(GENMASK(19,16), value1);
	layer2_sel = FIELD_GET(GENMASK(15,12), value1);
	layer1_sel = FIELD_GET(GENMASK(11,8), value1);
	bg_sel = FIELD_GET(GENMASK(7,4), value1);
	//pr_info("DMIX G198 LayerSel 0x%08x\n", value1);
	value2 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);
	layer6_mode = FIELD_GET(GENMASK(11,10), value2);
	layer5_mode = FIELD_GET(GENMASK(9,8), value2);
	layer4_mode = FIELD_GET(GENMASK(7,6), value2);
	layer3_mode = FIELD_GET(GENMASK(5,4), value2);
	layer2_mode = FIELD_GET(GENMASK(3,2), value2);
	layer1_mode = FIELD_GET(GENMASK(1,0), value2);
	//pr_info("DMIX G198 LayerMode 0x%08x\n", value2);
	pr_info("  Layer_(Sel/Mode) 0x%08x 0x%08x\n", value1, value2);

	pr_info("    %s %s %s %s %s %s %s\n",
		dmix_layer_sel[layer6_sel],
		dmix_layer_sel[layer5_sel],
		dmix_layer_sel[layer4_sel],
		dmix_layer_sel[layer3_sel],
		dmix_layer_sel[layer2_sel],
		dmix_layer_sel[layer1_sel],
		dmix_layer_sel[bg_sel]);
	pr_info("    L6[%s]L5[%s]L4[%s]L3[%s]L2[%s]L1[%s]BG[O]\n",
		dmix_layer_mode1[layer6_mode],
		dmix_layer_mode1[layer5_mode],
		dmix_layer_mode1[layer4_mode],
		dmix_layer_mode1[layer3_mode],
		dmix_layer_mode1[layer2_mode],
		dmix_layer_mode1[layer1_mode]);

}
EXPORT_SYMBOL(sp7350_dmix_bist_info);

void sp7350_dmix_ptg_set(int pattern_sel, int bg_color_yuv)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2;

	value = readl(disp_dev->base + DMIX_PTG_CONFIG_0);
	value &= ~(GENMASK(15,12) | GENMASK(2,0));
	value1 = readl(disp_dev->base + DMIX_PTG_CONFIG_1);
	value1 &= ~(GENMASK(7,4) | GENMASK(3,0));
	value2 = readl(disp_dev->base + DMIX_PTG_CONFIG_2);
	value2 &= ~GENMASK(23,0);

	switch (pattern_sel) {
		case SP7350_DMIX_BIST_BGC:
		case SP7350_DMIX_BIST_BORDER:
			value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_BORDER) |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value2 |= bg_color_yuv;
			break;
		case SP7350_DMIX_BIST_BORDER_NONE:
			value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_BORDER) |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_00);
			value2 |= bg_color_yuv;
			break;
		case SP7350_DMIX_BIST_BORDER_ONE:
			value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_BORDER) |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_01);
			value2 |= bg_color_yuv;
			break;
		case SP7350_DMIX_BIST_COLORBAR_ROT0:
			value |= SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value2 |= SP7350_DMIX_PTG_BLACK;
			break;
		case SP7350_DMIX_BIST_COLORBAR_ROT90:
			value |= SP7350_DMIX_PTG_COLOR_BAR_ROTATE |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value2 |= SP7350_DMIX_PTG_BLACK;
			break;
		case SP7350_DMIX_BIST_SNOW:
			value |= SP7350_DMIX_PTG_COLOR_BAR_SNOW |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value1 |= SP7350_DMIX_PTG_V_DOT_SET(SP7350_DMIX_PTG_DOT_00) |
				SP7350_DMIX_PTG_H_DOT_SET(SP7350_DMIX_PTG_DOT_00);
			value2 |= SP7350_DMIX_PTG_BLACK;
			break;
		case SP7350_DMIX_BIST_SNOW_HALF:
			value |= SP7350_DMIX_PTG_COLOR_BAR_SNOW |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value1 |= SP7350_DMIX_PTG_V_DOT_SET(SP7350_DMIX_PTG_DOT_07) |
				SP7350_DMIX_PTG_H_DOT_SET(SP7350_DMIX_PTG_DOT_07);
			value2 |= SP7350_DMIX_PTG_BLACK;
			break;			
		case SP7350_DMIX_BIST_SNOW_MAX:
			value |= SP7350_DMIX_PTG_COLOR_BAR_SNOW |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value1 |= SP7350_DMIX_PTG_V_DOT_SET(SP7350_DMIX_PTG_DOT_15) |
				SP7350_DMIX_PTG_H_DOT_SET(SP7350_DMIX_PTG_DOT_15);
			value2 |= SP7350_DMIX_PTG_BLACK;
			break;
		case SP7350_DMIX_BIST_REGION:
			value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_REGION) |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value2 |= bg_color_yuv;
			break;	
		default:
			value |= SP7350_DMIX_PTG_BORDER_PATTERN(SP7350_DMIX_PTG_BORDER) |
				SP7350_DMIX_PTG_BORDER_PIX(SP7350_DMIX_PTG_BORDER_PIX_07);
			value2 |= bg_color_yuv;
			break;
	}
	writel(value, disp_dev->base + DMIX_PTG_CONFIG_0);
	writel(value1, disp_dev->base + DMIX_PTG_CONFIG_1);
	writel(value2, disp_dev->base + DMIX_PTG_CONFIG_2);
}

void sp7350_dmix_layer_init(int layer, int fg_sel, int layer_mode)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1;
	int input;

	//pr_info("Init layer %s = %s to %s\n",
	//	dmix_layer_name[layer], dmix_fg_sel[fg_sel], dmix_layer_mode[layer_mode]);

	value = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);

	/* Set layer mode */
	if (layer != SP7350_DMIX_BG) {
		/* Clear and set layer mode bit */
		value1 &= ~(GENMASK(1, 0) << ((layer - 1) << 1));
		value1 |= (layer_mode << ((layer - 1) << 1));
	}
	/* Set layer input select */
	value &= ~(GENMASK(3, 0) << ((layer * 4) + 4));
	value |= (fg_sel << ((layer * 4) + 4));

	writel(value, disp_dev->base + DMIX_LAYER_CONFIG_0);
	writel(value1, disp_dev->base + DMIX_LAYER_CONFIG_1);

	switch (fg_sel) {
		case SP7350_DMIX_VPP0:
			input = SP7350_TGEN_DTG_ADJ_VPP0;
			break;
		case SP7350_DMIX_OSD0:
			input = SP7350_TGEN_DTG_ADJ_OSD0;
			break;
		case SP7350_DMIX_OSD1:
			input = SP7350_TGEN_DTG_ADJ_OSD1;
			break;
		case SP7350_DMIX_OSD2:
			input = SP7350_TGEN_DTG_ADJ_OSD2;
			break;
		case SP7350_DMIX_OSD3:
			input = SP7350_TGEN_DTG_ADJ_OSD3;
			break;
		case SP7350_DMIX_PTG:
			input = SP7350_TGEN_DTG_ADJ_PTG;
			break;
		default:
			input = SP7350_TGEN_DTG_ADJ_ALL;
			break;
	}
	if (input != SP7350_TGEN_DTG_ADJ_ALL) {
		if (input == SP7350_TGEN_DTG_ADJ_PTG)
			sp7350_tgen_input_adjust(input, 0x10);
		else
			sp7350_tgen_input_adjust(input, 0x10 - ((layer - SP7350_DMIX_L1) << 1));
	}
}

void sp7350_dmix_layer_set(int fg_sel, int layer_mode)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, tmp_value;
	u32 layer, i;

	/* Find Layer number */
	value = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	for (i = 0; i < SP7350_DMIX_MAX_LAYER; i++) {
		//tmp_value = FIELD_GET(GENMASK(3, 0), value);
		tmp_value = (value >> (i * 4 + 4)) & GENMASK(3, 0);
		if (tmp_value == fg_sel) {
			layer = i;
			break;
		}
	}
	//pr_info("Set layer %s (%s) to %s\n",
	//	dmix_layer_name[layer], dmix_fg_sel[fg_sel], dmix_layer_mode[layer_mode]);

	/* Set layer mode */
	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);
	if (layer != SP7350_DMIX_BG) {
		value1 &= ~(GENMASK(1, 0) << ((layer - 1) << 1));
		value1 |= (layer_mode << ((layer - 1) << 1));
	}
	writel(value1, disp_dev->base + DMIX_LAYER_CONFIG_1);

}

void sp7350_dmix_layer_cfg_set(int layer_id)
{
	/* this is for V4L2 one output test only
	 * don't use it in case of open over one output
	 */
	if (layer_id == 0) {
		;//TBD
	} else if (layer_id == 1) {
		;//TBD
		//sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
	} else if (layer_id == 2) {
		;//TBD
		//sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
		//sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
	} else if (layer_id == 3) {
		;//TBD
		//sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
		//sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
		//sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
	} else if (layer_id == 4) {
		sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
		sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
	} else
		pr_info("undefined layer\n");

}

void sp7350_dmix_layer_cfg_store(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, i;

	for(i = 0; i < 32 ; i++) {
		value = readl(disp_dev->base + DISP_DMIX_REG + i * 4);
		disp_dev->tmp_dmix.reg[i] = value;
	}

}

void sp7350_dmix_layer_cfg_restore(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	writel(disp_dev->tmp_dmix.reg[0], disp_dev->base + DMIX_LAYER_CONFIG_0);
	writel(disp_dev->tmp_dmix.reg[1], disp_dev->base + DMIX_LAYER_CONFIG_1);
	writel(disp_dev->tmp_dmix.reg[2], disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_0);
	writel(disp_dev->tmp_dmix.reg[3], disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_1);

	/* Add for PM function */
	writel(disp_dev->tmp_dmix.reg[20], disp_dev->base + DMIX_SOURCE_SEL);

}

void sp7350_dmix_layer_info(int layer)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;
	u32 fg_sel, mod_sel;

	value1 = readl(disp_dev->base + DMIX_LAYER_CONFIG_0);
	value2 = readl(disp_dev->base + DMIX_LAYER_CONFIG_1);

	switch (layer) {
		case SP7350_DMIX_L6:
			fg_sel = FIELD_GET(GENMASK(31, 28), value1);
			mod_sel = FIELD_GET(GENMASK(11, 10), value2);
			break;
		case SP7350_DMIX_L5:
			fg_sel = FIELD_GET(GENMASK(27, 24), value1);
			mod_sel = FIELD_GET(GENMASK(9, 8), value2);
			break;
		case SP7350_DMIX_L4:
			fg_sel = FIELD_GET(GENMASK(23, 20), value1);
			mod_sel = FIELD_GET(GENMASK(7, 6), value2);
			break;
		case SP7350_DMIX_L3:
			fg_sel = FIELD_GET(GENMASK(19, 16), value1);
			mod_sel = FIELD_GET(GENMASK(5, 4), value2);
			break;
		case SP7350_DMIX_L2:
			fg_sel = FIELD_GET(GENMASK(15, 12), value1);
			mod_sel = FIELD_GET(GENMASK(3, 2), value2);
			break;
		case SP7350_DMIX_L1:
			fg_sel = FIELD_GET(GENMASK(11, 8), value1);
			mod_sel = FIELD_GET(GENMASK(1, 0), value2);
			break;
		case SP7350_DMIX_BG:
			fg_sel = FIELD_GET(GENMASK(7, 4), value1);
			//mod_sel = FIELD_GET(GENMASK(0, 0), value2);
			break;
		default:
			break;
	}
	pr_info("%s layer = %s : %s\n",
		dmix_layer_name[layer],
		dmix_fg_sel[fg_sel],
		layer?"--":dmix_layer_mode[mod_sel]);
		
}

void sp7350_dmix_plane_alpha_config(struct sp7350_dmix_plane_alpha *plane)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2;

	//pr_info("layer%d: En%d, Fix%d, 0x%x\n",
	//	plane->layer, plane->enable, plane->fix_alpha, plane->alpha_value);

	value1 = readl(disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_0);
	value2 = readl(disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_1);

	switch (plane->layer) {
		case SP7350_DMIX_L1:
			value1 &= ~(GENMASK(15, 8));
			value1 |= FIELD_PREP(GENMASK(15, 15), plane->enable) |
				FIELD_PREP(GENMASK(14, 14), plane->fix_alpha) |
				FIELD_PREP(GENMASK(13, 8), plane->alpha_value);
			break;
		case SP7350_DMIX_L2:
			break;
		case SP7350_DMIX_L3:
			value2 &= ~(GENMASK(31, 24));
			value2 |= FIELD_PREP(GENMASK(31, 31), plane->enable) |
				FIELD_PREP(GENMASK(30, 30), plane->fix_alpha) |
				FIELD_PREP(GENMASK(29, 24), plane->alpha_value);
			break;
		case SP7350_DMIX_L4:
			value2 &= ~(GENMASK(23, 16));
			value2 |= FIELD_PREP(GENMASK(23, 23), plane->enable) |
				FIELD_PREP(GENMASK(22, 22), plane->fix_alpha) |
				FIELD_PREP(GENMASK(21, 16), plane->alpha_value);
			break;
		case SP7350_DMIX_L5:
			value2 &= ~(GENMASK(15, 8));
			value2 |= FIELD_PREP(GENMASK(15, 15), plane->enable) |
				FIELD_PREP(GENMASK(14, 14), plane->fix_alpha) |
				FIELD_PREP(GENMASK(13, 8), plane->alpha_value);
			break;
		case SP7350_DMIX_L6:
			value2 &= ~(GENMASK(7, 0));
			value2 |= FIELD_PREP(GENMASK(7, 7), plane->enable) |
				FIELD_PREP(GENMASK(6, 6), plane->fix_alpha) |
				FIELD_PREP(GENMASK(5, 0), plane->alpha_value);
			break;
		default:
			break;
	}
	writel(value1, disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_0);
	writel(value2, disp_dev->base + DMIX_PLANE_ALPHA_CONFIG_1);

}

void sp7350_dmix_color_adj_onoff(int enable)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value = readl(disp_dev->base + DMIX_ADJUST_CONFIG_0);

	if (enable)
		value |= SP7350_DMIX_LUMA_ADJ_EN | SP7350_DMIX_CRMA_ADJ_EN;
	else
		value &= ~(SP7350_DMIX_LUMA_ADJ_EN | SP7350_DMIX_CRMA_ADJ_EN);

	writel(value, disp_dev->base + DMIX_ADJUST_CONFIG_0);
}
