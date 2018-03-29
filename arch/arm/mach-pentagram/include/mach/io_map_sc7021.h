#ifndef __IO_MAP_SC7021_H__
#define __IO_MAP_SC7021_H__

/* PA */
#define	PA_B_REG		0x9C000000
#define	SIZE_B_REG		SZ_32M
#define	PA_A_REG		0x9EC00000
#define	SIZE_A_REG		SZ_4M

#define PA_IOB_ADDR(x)		((x) + PA_B_REG)
#define PA_IOA_ADDR(x)		((x) + PA_A_REG)

/*  VA */
#define VA_B_REG		0xF8000000
#define VA_A_REG		(VA_B_REG + SIZE_B_REG)

#define VA_IOB_ADDR(x)		((x) + VA_B_REG)
#define VA_IOA_ADDR(x)		((x) + VA_A_REG)

/* RGST */

#define B_SYSTEM_BASE           VA_IOB_ADDR(0 * 32 * 4)
#define A_SYSTEM_BASE           VA_IOA_ADDR(0 * 32 * 4)
#define A_SYS_COUNTER_BASE      (A_SYSTEM_BASE + 0x28000) /* 9ec2_8000 */

#endif
