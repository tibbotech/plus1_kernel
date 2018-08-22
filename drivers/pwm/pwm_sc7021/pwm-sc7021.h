#ifndef __PWM_SC7021_H__
#define __PWM_SC7021_H__

#define STATIC_ASSERT(b) extern int _static_assert[b ? 1 : -1]

#ifndef ENABLE
	#define ENABLE			1
#endif

#ifndef DISABLE
	#define DISABLE			0
#endif

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
typedef unsigned char	UINT8;
typedef char			INT8;
typedef unsigned short	UINT16;
typedef short			INT16;
typedef int				INT32;
typedef unsigned int	UINT32;
typedef unsigned char	BOOL;

struct _PWM_DD_REG_ {
	union {
		struct {
			UINT32 dd				:16;//b(0-15)
			UINT32 :16;
		};
		UINT32 idx_all;
	};
};
STATIC_ASSERT(sizeof(struct _PWM_DD_REG_) == 4);

struct _PWM_DU_REG_ {
	union {
		struct {
			UINT32 pwm_du			:8; 	//b(0-7)
			UINT32 pwm_du_dd_sel	:2; 	//b(8-9)
			UINT32					:6; 	//b(10-15)
			UINT32 :16;
		};
		UINT32 idx_all;
	};
};
STATIC_ASSERT(sizeof(struct _PWM_DU_REG_) == 4);

struct _PWM_REG_ {
	//GROUP 244
	union {
		struct {
			UINT32 pwm_en			:8;	//b(0-7)
			UINT32 pwm_bypass		:8;	//b(8-15)
			UINT32 :16;
		};
		UINT32 grp244_0;
	};
	union {
		struct {
			UINT32 pwm_cnt0_en		:1;	//b(0)
			UINT32 pwm_cnt1_en		:1;	//b(1)
			UINT32 pwm_cnt2_en		:1;	//b(2)
			UINT32 pwm_cnt3_en		:1;	//b(3)
			UINT32 pwm_clk54_en		:1;	//b(4)
			UINT32					:3;	//b(5-7)
			UINT32 pwm_dd0_sync_off	:1;	//b(8)
			UINT32 pwm_dd1_sync_off	:1;	//b(9)
			UINT32 pwm_dd2_sync_off	:1;	//b(10)
			UINT32 pwm_dd3_sync_off	:1;	//b(11)
			UINT32					:4;	//b(12-15)
			UINT32 :16;
		};
		UINT32 grp244_1;
	};
	struct _PWM_DD_REG_ pwm_dd[4];		//G244.2~5
#if 0
	union {
		struct {
			UINT32 pwm_dd0			:16;	//b(0-15)
			UINT32 :16;
		};
		UINT32 grp244_2;
	};
	union {
		struct {
			UINT32 pwm_dd1			:16;	//b(0-15)
			UINT32 :16;
		};
		UINT32 grp244_3;
	};
	union {
		struct {
			UINT32 pwm_dd2			:16;	//b(0-15)
			UINT32 :16;
		};
		UINT32 grp244_4;
	};
	union {
		struct {
			UINT32 pwm_dd3			:16;	//b(0-15)
			UINT32 :16;
		};
		UINT32 grp244_5;
	};
#endif
	struct _PWM_DU_REG_ pwm_du[8];
#if 0
	union {
		struct {
			UINT32 pwm_du0			:8;		//b(0-7)
			UINT32 pwm_du0_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_6;
	};
	union {
		struct {
			UINT32 pwm_du1			:8;		//b(0-7)
			UINT32 pwm_du1_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_7;
	};
	union {
		struct {
			UINT32 pwm_du2			:8;		//b(0-7)
			UINT32 pwm_du2_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_8;
	};
	union {
		struct {
			UINT32 pwm_du3			:8;		//b(0-7)
			UINT32 pwm_du3_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_9;
	};
	union {
		struct {
			UINT32 pwm_du4			:8;		//b(0-7)
			UINT32 pwm_du4_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_10;
	};
	union {
		struct {
			UINT32 pwm_du5			:8;		//b(0-7)
			UINT32 pwm_du5_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_11;
	};
	union {
		struct {
			UINT32 pwm_du6			:8;		//b(0-7)
			UINT32 pwm_du6_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_12;
	};
	union {
		struct {
			UINT32 pwm_du7			:8;		//b(0-7)
			UINT32 pwm_du7_dd_sel	:2;		//b(8-9)
			UINT32					:6;		//b(10-15)
			UINT32 :16;
		};
		UINT32 grp244_13;
	};
#endif
	UINT32 grp244_14_31[18];
};
STATIC_ASSERT(sizeof(struct _PWM_REG_) == (32 * 4));

#endif	/*__PWM_SC7021_H__ */

