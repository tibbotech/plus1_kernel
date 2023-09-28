/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for TGEN block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_TGEN_H__
#define __SP7350_DISP_TGEN_H__

/*TGEN_RESET*/
#define SP7350_TGEN_RESET		BIT(0)

/*TGEN_USER_INT1_CONFIG*/
#define SP7350_TGEN_USER_INT1_CONFIG	GENMASK(11, 0)
/*TGEN_USER_INT2_CONFIG*/
#define SP7350_TGEN_USER_INT2_CONFIG	GENMASK(11, 0)

/*TGEN_DTG_CONFIG*/
#define SP7350_TGEN_FORMAT		GENMASK(10, 8)
#define SP7350_TGEN_FORMAT_SET(fmt)	FIELD_PREP(GENMASK(10, 8), fmt)
#define SP7350_TGEN_FORMAT_480P			0x0
#define SP7350_TGEN_FORMAT_576P			0x1
#define SP7350_TGEN_FORMAT_720P			0x2
#define SP7350_TGEN_FORMAT_1080P		0x3
#define SP7350_TGEN_FORMAT_64X64_360X100	0x6
#define SP7350_TGEN_FORMAT_64X64_144X100	0x7
#define SP7350_TGEN_FPS			GENMASK(5, 4)
#define SP7350_TGEN_FPS_SET(fps)	FIELD_PREP(GENMASK(5, 4), fps)
#define SP7350_TGEN_FPS_59P94HZ		0x0
#define SP7350_TGEN_FPS_50HZ		0x1
#define SP7350_TGEN_FPS_24HZ		0x2
#define SP7350_TGEN_USER_MODE		BIT(0)
#define SP7350_TGEN_INTERNAL		0x0
#define SP7350_TGEN_USER_DEF		0x1

/*TGEN_DTG_ADJUST1*/
#define SP7350_TGEN_DTG_ADJ_MASKA	GENMASK(13, 8)
#define SP7350_TGEN_DTG_ADJ_MASKB	GENMASK(5, 0)
#define SP7350_TGEN_DTG_ADJ1_VPP0(adj)	FIELD_PREP(GENMASK(13, 8), adj)
#define SP7350_TGEN_DTG_ADJ1_VPP1(adj)	FIELD_PREP(GENMASK(5, 0), adj)
/*TGEN_DTG_ADJUST2*/
#define SP7350_TGEN_DTG_ADJ2_OSD3(adj)	FIELD_PREP(GENMASK(13, 8), adj)
#define SP7350_TGEN_DTG_ADJ2_OSD2(adj)	FIELD_PREP(GENMASK(5, 0), adj)
/*TGEN_DTG_ADJUST3*/
#define SP7350_TGEN_DTG_ADJ3_OSD1(adj)	FIELD_PREP(GENMASK(13, 8), adj)
#define SP7350_TGEN_DTG_ADJ3_OSD0(adj)	FIELD_PREP(GENMASK(5, 0), adj)
/*TGEN_DTG_ADJUST4*/
#define SP7350_TGEN_DTG_ADJ4_PTG(adj)	FIELD_PREP(GENMASK(13, 8), adj)
#define SP7350_TGEN_DTG_ADJ4_VPP2(adj)	FIELD_PREP(GENMASK(5, 0), adj)

/* for tgen_input_adj */
#define SP7350_TGEN_DTG_ADJ_VPP0	0x0
#define SP7350_TGEN_DTG_ADJ_VPP1	0x1
#define SP7350_TGEN_DTG_ADJ_VPP2	0x2
#define SP7350_TGEN_DTG_ADJ_OSD0	0x3
#define SP7350_TGEN_DTG_ADJ_OSD1	0x4
#define SP7350_TGEN_DTG_ADJ_OSD2	0x5
#define SP7350_TGEN_DTG_ADJ_OSD3	0x6
#define SP7350_TGEN_DTG_ADJ_PTG		0x7
#define SP7350_TGEN_DTG_ADJ_ALL		0x8

struct sp7350_tgen_timing {
	int usr;
	int fps;
	int fmt;
	u16 htt;
	u16 vtt;
	u16 hact;
	u16 vact;
	u16 vbp;
};

/*
 * Init SP7350 TGEN Setting
 */
void sp7350_tgen_init(void);

/*
 * Show SP7350 TGEN Info
 */
void sp7350_tgen_decrypt_info(void);
void sp7350_tgen_resolution_chk(void);


/*
 * SP7350 TGEN Timing Generator Settings
 */
void sp7350_tgen_reset(void);
void sp7350_tgen_set_user_int1(u32 count);
void sp7350_tgen_set_user_int2(u32 count);
u32 sp7350_tgen_get_current_line_count(void);

void sp7350_tgen_timing_set_dsi(void);
void sp7350_tgen_timing_set_csi(void);
void sp7350_tgen_timing_get(void);

/*
 * SP7350 TGEN Timing Adjust Settings
 */
void sp7350_tgen_input_adjust(int tgen_input_adj, u32 adj_value);

/*
 * SP7350 TGEN register store/restore
 */
void sp7350_tgen_store(void);
void sp7350_tgen_restore(void);

#endif	//__SP7350_DISP_TGEN_H__

