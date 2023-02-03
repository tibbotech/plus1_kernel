/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for TCON block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_TCON_H__
#define __SP7350_DISP_TCON_H__

/*TCON_TPG_CTRL*/
#define SP7350_TCON_TPG_PATTERN			GENMASK(5, 2)
#define SP7350_TCON_TPG_PATTERN_SET(pat)	FIELD_PREP(GENMASK(5, 2), pat)
#define SP7350_TCON_TPG_PATTERN_H_1_BAR		0x0
#define SP7350_TCON_TPG_PATTERN_H_2_RAMP	0x1
#define SP7350_TCON_TPG_PATTERN_H_3_ODD		0x2
#define SP7350_TCON_TPG_PATTERN_V_1_BAR		0x3
#define SP7350_TCON_TPG_PATTERN_V_2_RAMP	0x4
#define SP7350_TCON_TPG_PATTERN_V_3_ODD		0x5
#define SP7350_TCON_TPG_PATTERN_HV_1_CHK	0x6
#define SP7350_TCON_TPG_PATTERN_HV_2_FRAME	0x7
#define SP7350_TCON_TPG_PATTERN_HV_3_MOI_A	0x8
#define SP7350_TCON_TPG_PATTERN_HV_4_MOI_B	0x9
#define SP7350_TCON_TPG_PATTERN_HV_5_CONTR	0xa
#define SP7350_TCON_TPG_MODE			GENMASK(1, 0)
#define SP7350_TCON_TPG_MODE_SET(mod)		FIELD_PREP(GENMASK(1, 0), mod)
#define SP7350_TCON_TPG_MODE_NORMAL		0x0
#define SP7350_TCON_TPG_MODE_INTERNAL		0x1
#define SP7350_TCON_TPG_MODE_EXTERNAL		0x2

/*
 * Init SP7350 TCON Setting
 */
void sp7350_tcon_init(void);

/*
 * Show SP7350 TCON Info
 */
void sp7350_tcon_reg_info(void);
void sp7350_tcon_decrypt_info(void);
void sp7350_tcon_resolution_chk(void);

/*
 * SP7350 TCON BIST Settings
 */
void sp7350_tcon_bist_info(void);
void sp7350_tcon_bist_set(int bist_mode, int tcon_bist_pat);

#endif	//__SP7350_DISP_TCON_H__
