/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for DMIX block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_DMIX_H__
#define __SP7350_DISP_DMIX_H__

/*DMIX_LAYER_CONFIG_0*/
#define SP7350_DMIX_L6_FG_SEL(sel)	FIELD_PREP(GENMASK(31, 28), sel)
#define SP7350_DMIX_L5_FG_SEL(sel)	FIELD_PREP(GENMASK(27, 24), sel)
#define SP7350_DMIX_L4_FG_SEL(sel)	FIELD_PREP(GENMASK(23, 20), sel)
#define SP7350_DMIX_L3_FG_SEL(sel)	FIELD_PREP(GENMASK(19, 16), sel)
#define SP7350_DMIX_L2_FG_SEL(sel)	FIELD_PREP(GENMASK(15, 12), sel)
#define SP7350_DMIX_L1_FG_SEL(sel)	FIELD_PREP(GENMASK(11, 8), sel)
#define SP7350_DMIX_BG_FG_SEL(sel)	FIELD_PREP(GENMASK(7, 4), sel)
#define SP7350_DMIX_VPP0_SEL		0x0
#define SP7350_DMIX_VPP1_SEL		0x1
#define SP7350_DMIX_VPP2_SEL		0x2
#define SP7350_DMIX_OSD0_SEL		0x3
#define SP7350_DMIX_OSD1_SEL		0x4
#define SP7350_DMIX_OSD2_SEL		0x5
#define SP7350_DMIX_OSD3_SEL		0x6
#define SP7350_DMIX_PTG_SEL		0x7
/*DMIX_LAYER_CONFIG_1*/
#define SP7350_DMIX_L6_MODE_SEL(sel)	FIELD_PREP(GENMASK(11, 10), sel)
#define SP7350_DMIX_L5_MODE_SEL(sel)	FIELD_PREP(GENMASK(9, 8), sel)
#define SP7350_DMIX_L4_MODE_SEL(sel)	FIELD_PREP(GENMASK(7, 6), sel)
#define SP7350_DMIX_L3_MODE_SEL(sel)	FIELD_PREP(GENMASK(5, 4), sel)
#define SP7350_DMIX_L2_MODE_SEL(sel)	FIELD_PREP(GENMASK(3, 2), sel)
#define SP7350_DMIX_L1_MODE_SEL(sel)	FIELD_PREP(GENMASK(1, 0), sel)
#define SP7350_DMIX_BLENDING		0x0
#define SP7350_DMIX_TRANSPARENT		0x1
#define SP7350_DMIX_OPACITY		0x2

/*DMIX_PTG_CONFIG_0*/
#define SP7350_DMIX_PTG_COLOR_BAR_ROTATE	BIT(15)
#define SP7350_DMIX_PTG_COLOR_BAR_SNOW		BIT(14)
#define SP7350_DMIX_PTG_BORDER_PATTERN(sel)	FIELD_PREP(GENMASK(13, 12), sel)
#define SP7350_DMIX_PTG_COLOR_BAR	0x0
#define SP7350_DMIX_PTG_BORDER		0x2
#define SP7350_DMIX_PTG_REGION		0x3
#define SP7350_DMIX_PTG_BORDER_PIX(sel)		FIELD_PREP(GENMASK(2, 0), sel)
#define SP7350_DMIX_PTG_BORDER_PIX_00	0x0
#define SP7350_DMIX_PTG_BORDER_PIX_01	0x1
#define SP7350_DMIX_PTG_BORDER_PIX_02	0x2
#define SP7350_DMIX_PTG_BORDER_PIX_03	0x3
#define SP7350_DMIX_PTG_BORDER_PIX_04	0x4
#define SP7350_DMIX_PTG_BORDER_PIX_05	0x5
#define SP7350_DMIX_PTG_BORDER_PIX_06	0x6
#define SP7350_DMIX_PTG_BORDER_PIX_07	0x7

/*DMIX_PTG_CONFIG_1*/
#define SP7350_DMIX_PTG_V_DOT_SET(sel)		FIELD_PREP(GENMASK(7, 4), sel)
#define SP7350_DMIX_PTG_H_DOT_SET(sel)		FIELD_PREP(GENMASK(3, 0), sel)
#define SP7350_DMIX_PTG_DOT_00	0x0
#define SP7350_DMIX_PTG_DOT_01	0x1
#define SP7350_DMIX_PTG_DOT_02	0x2
#define SP7350_DMIX_PTG_DOT_03	0x3
#define SP7350_DMIX_PTG_DOT_04	0x4
#define SP7350_DMIX_PTG_DOT_05	0x5
#define SP7350_DMIX_PTG_DOT_06	0x6
#define SP7350_DMIX_PTG_DOT_07	0x7
#define SP7350_DMIX_PTG_DOT_08	0x8
#define SP7350_DMIX_PTG_DOT_09	0x9
#define SP7350_DMIX_PTG_DOT_10	0xa
#define SP7350_DMIX_PTG_DOT_11	0xb
#define SP7350_DMIX_PTG_DOT_12	0xc
#define SP7350_DMIX_PTG_DOT_13	0xd
#define SP7350_DMIX_PTG_DOT_14	0xe
#define SP7350_DMIX_PTG_DOT_15	0xf

/*DMIX_PTG_CONFIG_2*/
#define SP7350_DMIX_PTG_BLACK		0x108080
#define SP7350_DMIX_PTG_RED		0x4040f0
#define SP7350_DMIX_PTG_GREEN		0x101010
#define SP7350_DMIX_PTG_BLUE		0x29f06e

/*DMIX_SOURCE_SEL*/
#define SP7350_DMIX_SOURCE_SEL(sel)		FIELD_PREP(GENMASK(2, 0), sel)
#define SP7350_DMIX_TCON0_DMIX		0x2

/*DMIX_ADJUST_CONFIG_0*/
#define SP7350_DMIX_LUMA_ADJ_EN		BIT(17)
#define SP7350_DMIX_CRMA_ADJ_EN		BIT(16)

#define DMIX_LUMA_OFFSET_MIN	(-50)
#define DMIX_LUMA_OFFSET_MAX	(50)
#define DMIX_LUMA_SLOPE_MIN	(0.60)
#define DMIX_LUMA_SLOPE_MAX	(1.40)

/* for fg_sel */
#define SP7350_DMIX_VPP0	0x0
#define SP7350_DMIX_VPP1	0x1 //unsupported
#define SP7350_DMIX_VPP2	0x2 //unsupported
#define SP7350_DMIX_OSD0	0x3
#define SP7350_DMIX_OSD1	0x4
#define SP7350_DMIX_OSD2	0x5
#define SP7350_DMIX_OSD3	0x6
#define SP7350_DMIX_PTG		0x7

/* for layer_mode */
#define SP7350_DMIX_BG	0x0 //BG
#define SP7350_DMIX_L1	0x1 //VPP0
#define SP7350_DMIX_L2	0x2 //VPP1 (unsupported)
#define SP7350_DMIX_L3	0x3 //OSD3
#define SP7350_DMIX_L4	0x4 //OSD2
#define SP7350_DMIX_L5	0x5 //OSD1
#define SP7350_DMIX_L6	0x6 //OSD0
#define SP7350_DMIX_MAX_LAYER	7

/* for pattern_sel */
#define SP7350_DMIX_BIST_BGC		0x0
#define SP7350_DMIX_BIST_COLORBAR_ROT0	0x1
#define SP7350_DMIX_BIST_COLORBAR_ROT90	0x2
#define SP7350_DMIX_BIST_BORDER_NONE	0x3
#define SP7350_DMIX_BIST_BORDER_ONE	0x4
#define SP7350_DMIX_BIST_BORDER		0x5
#define SP7350_DMIX_BIST_SNOW		0x6
#define SP7350_DMIX_BIST_SNOW_MAX	0x7
#define SP7350_DMIX_BIST_SNOW_HALF	0x8
#define SP7350_DMIX_BIST_REGION		0x9

struct sp7350_dmix_plane_alpha {
	int layer;
	unsigned int enable;
	unsigned int fix_alpha;
	unsigned int alpha_value;
};

/*
 * Init SP7350 DMIX Setting
 */
void sp7350_dmix_init(void);

/*
 * Show SP7350 DMIX Info
 */
void sp7350_dmix_decrypt_info(void);
void sp7350_dmix_all_layer_info(void);
void sp7350_dmix_layer_cfg_set(int layer_id);
void sp7350_dmix_layer_cfg_store(void);
void sp7350_dmix_layer_cfg_restore(void);

/*
 * SP7350 DMIX PTG Settings
 */
void sp7350_dmix_bist_info(void);
void sp7350_dmix_ptg_set(int pattern_sel, int bg_color_yuv);

/*
 * SP7350 DMIX Layer Settings
 */
void sp7350_dmix_layer_init(int layer, int fg_sel, int layer_mode);
void sp7350_dmix_layer_set(int fg_sel, int layer_mode);
void sp7350_dmix_layer_info(int layer);

/*
 * SP7350 DMIX Layer Alpha Setting
 */
void sp7350_dmix_plane_alpha_config(struct sp7350_dmix_plane_alpha *plane);

/*
 * SP7350 DMIX LUMA/CRMA Adjustment
 */
void sp7350_dmix_color_adj_onoff(int enable);

#endif	/* __SP7350_DISP_DMIX_H__ */

