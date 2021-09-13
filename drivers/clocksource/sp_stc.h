/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef __SP_STC_H__
#define __SP_STC_H__

#include <mach/io_map.h>

struct stcReg_s {
	/* Group 12: STC */
	u32 stc_15_0;
	u32 stc_31_16;
	u32 stc_32;
	u32 stc_divisor;
	u32 rtc_15_0;
	u32 rtc_23_16;
	u32 rtc_divisor;
	u32 stc_config;
	u32 timer0_ctrl;
	u32 timer0_cnt;
	u32 timer1_ctrl;
	u32 timer1_cnt;
	u32 timerw_ctrl;
	u32 timerw_cnt;
	u32 g12_reserved_0[2];
	u32 timer2_ctrl;
	u32 timer2_divisor;
	u32 timer2_reload;
	u32 timer2_cnt;
	u32 timer3_ctrl;
	u32 timer3_divisor;
	u32 timer3_reload;
	u32 timer3_cnt;
	u32 stcl_0;
	u32 stcl_1;
	u32 stcl_2;
	u32 atc_0;
	u32 atc_1;
	u32 atc_2;
	u32 g12_reserved_1[2];
};

struct stc_avReg_s {
	/* Group 96, 97, 99: STC_AV0 - STC_AV2 */
	u32 stc_15_0;
	u32 stc_31_16;
	u32 stc_64;
	u32 stc_divisor;
	u32 rtc_15_0;
	u32 rtc_23_16;
	u32 rtc_divisor;
	u32 stc_config;
	u32 timer0_ctrl;
	u32 timer0_cnt;
	u32 timer1_ctrl;
	u32 timer1_cnt;
	u32 rsv_12;
	u32 rsv_13;
	u32 stc_47_32;
	u32 stc_63_48;
	u32 timer2_ctrl;
	u32 timer2_divisor;
	u32 timer2_reload;
	u32 timer2_cnt;
	u32 timer3_ctrl;
	u32 timer3_divisor;
	u32 timer3_reload;
	u32 timer3_cnt;
	u32 stcl_0;
	u32 stcl_1;
	u32 stcl_2;
	u32 atc_0;
	u32 atc_1;
	u32 atc_2;
	u32 rsv_30;
	u32 rsv_31;
};

#define SP_STC_TIMER0CTL_SRC_STC			(1 << 14)
#define SP_STC_TIMER0CTL_SRC_RTC			(2 << 14)
#define SP_STC_TIMER0CTL_RTP				(1 << 13)
#define SP_STC_TIMER0CTL_GO					(1 << 11)
#define SP_STC_TIMER0CTL_TM0_RELOAD_MASK	(0x07FF << 0)

#define SP_STC_AV_TIMER01_CTL_SRC_SYS		(0 << 14)
#define SP_STC_AV_TIMER01_CTL_SRC_STC		(1 << 14)
#define SP_STC_AV_TIMER01_CTL_SRC_RTC		(2 << 14)
#define SP_STC_AV_TIMER01_CTL_RTP			(1 << 13)
#define SP_STC_AV_TIMER01_CTL_GO			(1 << 11)
#define SP_STC_AV_TIMER01_CTL_RELOAD_MASK	(0x07FF << 0)

#define SP_STC_AV_TIMER23_CTL_SRC_SYS		(0 << 2)
#define SP_STC_AV_TIMER23_CTL_SRC_STC		(1 << 2)
#define SP_STC_AV_TIMER23_CTL_SRC_EXT		(3 << 2)
#define SP_STC_AV_TIMER23_CTL_RPT			(1 << 1)
#define SP_STC_AV_TIMER23_CTL_GO			(1 << 0)

#endif /* __SP_STC_H__ */
