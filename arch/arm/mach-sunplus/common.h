/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef __ARCH_ARM_MACH_SUNPLUS_COMMON_H
#define __ARCH_ARM_MACH_SUNPLUS_COMMON_H

#include <linux/platform_data/cpuidle-sunplus.h>

extern void sp7021_cpu_power_down(int cpu);
extern void sp7021_pm_central_suspend(void);
extern int sp7021_pm_central_resume(void);
extern void sp7021_enter_aftr(void);

extern struct cpuidle_sp7021_data cpuidle_coupled_sp7021_data;

#endif /* __ARCH_ARM_MACH_SUNPLUS_COMMON_H */
