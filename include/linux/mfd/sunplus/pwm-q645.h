/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * PWM device header file for SUNPLUS SoCs
 *
 * Copyright (C) 2020 SUNPLUS Inc.
 *
 * Author:	PoChou Chen <pochou.chen@sunplus.com>
 *	Hammer Hsieh <hammer.hsieh@sunplus.com>
 *
 */

#ifndef __PWM_Q645_H__
#define __PWM_Q645_H__

#define STATIC_ASSERT(b) extern int _static_assert[b ? 1 : -1]

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
struct _PWM_DD_REG_ {
	union {
		struct {
			u32 dd	:18;//b(0-17)
			u32:14;//b(18-31)
		};
		u32 idx_all;
	};
};
STATIC_ASSERT(sizeof(struct _PWM_DD_REG_) == 4);

struct _PWM_DU_REG_ {
	union {
		struct {
			u32 pwm_du			:12;	//b(0-11)
			u32:4;						//b(12-15)
			u32	pwm_du_dd_sel	:2;		//b(16-17)
			u32:14;						//b(18-31)
		};
		u32 idx_all;
	};
};
STATIC_ASSERT(sizeof(struct _PWM_DU_REG_) == 4);

struct _PWM_REG_ {
	//GROUP 287
	union {
		struct {
			u32 pwm_en				:8;	//b(0-7)
			u32 pwm_bypass			:8;	//b(8-15)
			u32 pwm_pro_en			:4; //(bit16-19)
			u32 pwm_du_region		:4; //(bit20-23)
			u32 pwm_latch_mode		:1; //(bit24)
			u32 pwm_sync_start_mode	:1; //(bit25)
			u32:6;						//(bit26-31)
		};
		u32 grp287_0;
	};
	struct _PWM_DU_REG_ pwm_du[6];		//G287.1~6
	u32 grp287_7_14[8];					//G287.7 ~ 14 TBD
	u32 grp287_15[1];					//G287.15 TBD
	u32 pwm_version[1];					//G287.16 PWM VERSION
	struct _PWM_DD_REG_ pwm_dd[4];		//G287.17~20
	u32 grp287_21_31[11];				//G287.21~31 TBD

	//GROUP 288
	union {
		struct {
			u32 pwm_pro_end_int_mask	:4;	//b(0-3)
			u32 pwm_pro_user_int_mask	:4;	//b(4-7)
			u32	pwm_inv					:8;	//b(8-15)
			u32 pwm_pro_send			:4;	//b(16-19)
			u32 pwm_pro_reset			:4;	//b(20-23)
			u32:8;							//b(24-31)
		};
		u32 grp288_0;
	};
	u32 grp288_1_12[12];				//G288.1~12 pwm_pro_config TBD
	u32 grp288_13[1];					//G288.13 pwm_mode2 TBD
	u32 grp288_14_29[16];				//G288.14~29 pwm_adj_config TBD
	u32 grp288_30_31[2];				//G288.30~31 reserved
};
STATIC_ASSERT(sizeof(struct _PWM_REG_) == (32 * 4 * 2));

#endif	/*__PWM_Q645_H__ */

