// SPDX-License-Identifier: GPL-2.0-only
/*
 * ARM/ARM64 generic CPU idle driver.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 */
#define pr_fmt(fmt) "CPUidle arm: " fmt

#include <linux/cpu_cooling.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_data/cpuidle-sunplus-sp7021.h>
#include <linux/io.h>      //for support readl and writel
#include <mach/io_map.h>   //for support A_SYSTEM_BASE

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include "dt_idle_states.h"

extern int cpu_v7_do_idle(void);

void sp7021_central_suspend(void)
{
	void __iomem *regs = (void __iomem *)A_SYSTEM_BASE;

	/* power down
	 * core2 and core3 WFI (core0&1 can't power down in sp7021)
	 * G0.20(moon0.20 ca7_ctl_cfg).1=NSLEEP_CORES : 0 sleep core2&3 , 1 not sleep(default)
	 * G0.20(moon0.20 ca7_ctl_cfg).0=ISOLATE_CORES : 0 not isolate , 1 isolate(default)
	 * G0.2(moon0.2 ca7_sw_rst) default as 32"h667f
	 * G0.7(moon0.7 ca7_standbywfi).4=CA7_STANDBYWFIL2 RO
	 * G0.7(moon0.7 ca7_standbywfi).3~0=CA7_STANDBYWFI RO : 0 processor not in WFI lower power state
	 *                                                      1 processor in WFI lower power state
	 */
	while (((readl(regs + 0x1C))&(0xC)) != 0xC); /* wait CA7_STANDBYWFI[3:2]=2"b11 */
	/* MOON0 REG20(G0.20 ca7_ctl_cfg) : isolate cores and sleep core2&3 */
	writel(0x3, regs + 0x50); /* isolate cores */
	writel(0x1, regs + 0x50); /* sleep core2&3 */
	/* MOON0 REG2(G0.2 ca7_sw_rst) : core2&3 set reset
	 * b16=CA7CORE3POR_RST_B=0
	 * b15=CA7CORE2POR_RST_B=0
	 * b12=CA7CORE3_RST_B=0
	 * b11=CA7CORE2_RST_B=0
	 * b8=CA7DBG3_RST_B=0
	 * b7=CA7DBG2_RST_B=0
	 */
	writel(0x667f, regs + 0x8);
}

int sp7021_central_resume(void)
{
	void __iomem *regs = (void __iomem *)A_SYSTEM_BASE;

	/* power up*/
	/* MOON0 REG20(G0.20 ca7_ctl_cfg) : isolate cores and sleep core2&3 */
	writel(0x3, regs + 0x50); /* isolate cores */
	writel(0x2, regs + 0x50); /* not sleep core2&3 */
	/* MOON0 REG2(G0.2 ca7_sw_rst) : core2&3 set reset */
	/* b16=CA7CORE3POR_RST_B=1
	 * b15=CA7CORE2POR_RST_B=1
	 */
	writel(0x1e67f, regs + 0x8);
	/* b12=CA7CORE3_RST_B=1
	 * b11=CA7CORE2_RST_B=1
	 * b8=CA7DBG3_RST_B=1
	 * b7=CA7DBG2_RST_B=1
	 */
	writel(0x1ffff, regs + 0x8);

	return 0;
}
static int sp7021_wfi_finisher(unsigned long flags)
{
  //printk("sp7021_wfi_finisher");
	cpu_v7_do_idle();   // idle to WFI

	return -1;
}

static int sp7021_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	int ret;
	
  // if idx=0, call cpu_do_idle()
	if (!idx){
	  cpu_v7_do_idle();
		return idx;
	}
	
	// if idx>0, call cpu_suspend()
	ret = cpu_pm_enter();
	//sp7021_central_suspend();  //sleep cpu2&cpu3
  //printk("sp7021_central_suspend \n");
	if (!ret){
	/*
	 * Pass idle state index to cpuidle_suspend which in turn
	 * will call the CPU ops suspend protocol with idle index as a
	 * parameter.
	 */
	  ret = cpu_suspend(idx, sp7021_wfi_finisher);
	  //cpu_pm_exit();
	}
	//sp7021_central_resume();   //wakeup cpu2&cpu3
  //printk("sp7021_central_resume \n");
	cpu_pm_exit();
	
	return ret ? -1:idx;
}

static struct cpuidle_driver sp7021_idle_driver __initdata = {
	.name = "sp7021_idle",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	.states[0] = {
		.enter                  = sp7021_enter_idle_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.name                   = "WFI",
		.desc                   = "ARM WFI",
	}
};

static const struct of_device_id sp7021_idle_state_match[] __initconst = {
	{ .compatible = "sunplus,sp7021-idle-state",
	  .data = sp7021_enter_idle_state },
	{ },
};

/*
 * arm_idle_init - Initializes arm cpuidle driver
 *
 * Initializes arm cpuidle driver for all CPUs, if any CPU fails
 * to register cpuidle driver then rollback to cancel all CPUs
 * registeration.
 */
static int __init sp7021_idle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

  printk("sp7021_idle_init \n");
	drv = kmemdup(&sp7021_idle_driver, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->cpumask = (struct cpumask *)cpumask_of(cpu);
	/*
	 * Initialize idle states data, starting at index 1.  This
	 * driver is DT only, if no DT idle states are detected (ret
	 * == 0) let the driver initialization fail accordingly since
	 * there is no reason to initialize the idle driver if only
	 * wfi is supported.
	 */
	ret = dt_init_idle_driver(drv, sp7021_idle_state_match, 1);
	if (ret <= 0) 
		return ret ? : -ENODEV;
			
	ret = cpuidle_register_driver(drv);
  printk("cpuidle_register_driver");
	if (ret){
		pr_err("Failed to register cpuidle driver \n");
		return ret;
	}
	
	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		//ret = arm_cpuidle_init(cpu);
		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO)
		 */
		if (ret == -ENXIO)
			continue;
			
		if (ret){
			pr_err("CPU %d failed to init idle CPU ops \n", cpu);
			goto out_fail;
		}
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev){
			pr_err("Failed to allocate cpuidle device \n");
			ret = -ENOMEM;
			goto out_fail;
		}
		dev->cpu = cpu;
		
		ret = cpuidle_register_device(dev);
    printk("cpuidle_register_device \n");
		if (ret){
			pr_err("Failed to register cpuidle device for CPU %d \n", cpu);
			kfree(dev);
			goto out_fail;
		}	
	}

	return 0;

out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		cpuidle_unregister_device(dev);
		kfree(dev);
	}
  cpuidle_unregister_driver(drv);
  
	return ret;
}
static int __init idle_init(void)
{
	int ret;

	if (of_machine_is_compatible("sunplus,sp7021-achip")) {
		sp7021_idle_init();
		ret=0;
	}
	else
		ret= -1;
		
	if (ret) {
    printk("failed to cpuidle init\n");
		return ret;
	}

	return ret;
}
		
device_initcall(idle_init);

