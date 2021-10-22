/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Sunplus SP7021 SoC ICM(Input Capture Module) driver header file
 *
 * Author:	Hammer Hsieh <hammer.hsieh@sunplus.com>
 */

#ifndef __SUNPLUS_ICM_H__
#define __SUNPLUS_ICM_H__

/*
 * muxsel: input signal source select
 */
#define ICM_MUXSEL_INPUT0	0 /* input signal 0 */
#define ICM_MUXSEL_INPUT1	1 /* input signal 1 */
#define ICM_MUXSEL_INPUT2	2 /* input signal 2 */
#define ICM_MUXSEL_INPUT3	3 /* input signal 3 */
#define ICM_MUXSEL_TEST		4 /* test signal */

/*
 * clksel: internal counter clock select
 */
#define ICM_CLKSEL_EXT0		0 /* external clock 0 */
#define ICM_CLKSEL_EXT1		1 /* external clock 1 */
#define ICM_CLKSEL_EXT2		2 /* external clock 2 */
#define ICM_CLKSEL_EXT3		3 /* external clock 3 */
#define ICM_CLKSEL_SYS		4 /* system clock */
#define ICM_CLKSEL_27M		5 /* 27 MHz */
#define ICM_CLKSEL_32K		6 /* 32 kHz */

/*
 * eemode: edge event mode
 */
#define ICM_EEMODE_RISING	0 /* rising edge */
#define ICM_EEMODE_FALLING	1 /* falling edge */
#define ICM_EEMODE_BOTH		2 /* both edges */

/*
 * etimes: edge event times
 * set 0 ~ 15. if set 7, interrupt will be triggered after
 * 8 times edge event.
 *
 * dtimes: debounce times of input signal debounce filter
 * 0~6: (dtimes + 1) times
 * 7  : 16 times
 *
 * cntscl: internal counter clock prescaler: 0 ~ (2^32-1)
 * cnt_clk = clksel_clk / (cntscl + 1)
 * internal counter +1 @ cnt_clk rising edge
 *
 * tstscl: test signal clock prescaler: 0 ~ (2^32-1)
 * tst_clk = sysclk / (tstscl + 1)
 */
struct sunplus_icm_cfg {
	u32 muxsel; /* input signal source select */
	u32 clksel; /* internal counter clock select */
	u32 eemode; /* edge event mode */
	u32 etimes; /* edge event times */
	u32 dtimes; /* debounce times of input signal debounce filter */
	u32 cntscl; /* internal counter clock prescaler */
	u32 tstscl; /* test signal clock prescaler */
};

/*
 * icm fifo state
 */
#define ICM_FDDROP	0x1000 /* fifo data dropped */
#define ICM_FEMPTY	0x4000 /* fifo empty */
#define ICM_FFULL	0x8000 /* fifo full */

/*
 * icm callback function
 * cnt   : internal counter
 * fstate: fifo state
 */
typedef void (*sunplus_icm_cbf)(int icm, u32 cnt, u32 fstate);

/*
 * icm: 0 ~ 3 (icm0 ~ icm3)
 */
extern int sunplus_icm_config(int icm, struct sunplus_icm_cfg *cfg);
extern int sunplus_icm_reload(int icm);
extern int sunplus_icm_enable(int icm, sunplus_icm_cbf cbf);
extern int sunplus_icm_disable(int icm);
extern int sunplus_icm_pwidth(int icm, u32 *pwh, u32 *pwl);

#endif /* __SUNPLUS_ICM_H__ */
