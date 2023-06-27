// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define TRACE	printk("!!! %s:%d !!!\n", __FUNCTION__, __LINE__)

#define MIN_VOLT_SHIFT		(100000)
#define MAX_VOLT_SHIFT		(200000)
#define MAX_VOLT_LIMIT		(1150000)
#define VOLT_TOL		(10000)

struct sp7350_cpu_dvfs_info {
	struct cpumask cpus;
	struct device *cpu_dev;
	struct clk *cpu_clk;
	struct regulator *proc_reg;
	struct list_head list_head;
};

/* refer 20221201-CA55_DVFS_Flow_guide(p5) */
#define MEMCTL_FAST	0x287
#define MEMCTL_DEFAULT	0x285
#define MEMCTL_VDDMIN	0x321
#define VDEFAULT	800000
extern void sp_clkc_ca55_memctl(u32 val);

static LIST_HEAD(dvfs_info_list);

static struct sp7350_cpu_dvfs_info *sp7350_cpu_dvfs_info_lookup(int cpu)
{
	struct sp7350_cpu_dvfs_info *info;

	list_for_each_entry(info, &dvfs_info_list, list_head) {
		if (cpumask_test_cpu(cpu, &info->cpus))
			return info;
	}

	return NULL;
}

static int sp7350_cpufreq_set_target(struct cpufreq_policy *policy,
				  unsigned int index)
{
	struct cpufreq_frequency_table *freq_table = policy->freq_table;
	struct clk *cpu_clk = policy->clk;
	struct sp7350_cpu_dvfs_info *info = policy->driver_data;
	struct device *cpu_dev = info->cpu_dev;
	struct dev_pm_opp *opp;
	long freq_hz;
	int vproc, old_vproc, ret;

	freq_hz = freq_table[index].frequency * 1000;

	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_hz);
	if (IS_ERR(opp)) {
		pr_err("cpu%d: failed to find OPP for %ld\n",
		policy->cpu, freq_hz);
		return PTR_ERR(opp);
	}
	vproc = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	if (info->proc_reg) {
		old_vproc = regulator_get_voltage(info->proc_reg);
		if (old_vproc < 0) {
			pr_err("%s: invalid Vproc value: %d\n", __func__, old_vproc);
			return old_vproc;
		}

		/*
		* If the new voltage is higher than the current voltage,
		* scale up voltage first.
		*/
		if (old_vproc < vproc) {
			ret = regulator_set_voltage(info->proc_reg, vproc, vproc + VOLT_TOL);
			if (ret) {
				pr_err("cpu%d: failed to scale up voltage!\n",
				policy->cpu);
				return ret;
			}
			sp_clkc_ca55_memctl((vproc == VDEFAULT) ? MEMCTL_DEFAULT : MEMCTL_FAST);
		}
	}

	/* Set the cpu_clk to target rate. */
	ret = clk_set_rate(cpu_clk, freq_hz);

	if (info->proc_reg) {
		/*
		* If the new voltage is lower than the current voltage,
		* scale down to the new voltage.
		*/
		if (vproc < old_vproc) {
			sp_clkc_ca55_memctl((vproc == VDEFAULT) ? MEMCTL_DEFAULT : MEMCTL_VDDMIN);
			ret = regulator_set_voltage(info->proc_reg, vproc, vproc + VOLT_TOL);
			if (ret) {
				pr_err("cpu%d: failed to scale down voltage!\n",
				policy->cpu);
				return ret;
			}
		}
	}

	return 0;
}

#define DYNAMIC_POWER "dynamic-power-coefficient"

static int sp7350_cpu_dvfs_info_init(struct sp7350_cpu_dvfs_info *info, int cpu)
{
	struct device *cpu_dev;
	struct regulator *proc_reg = ERR_PTR(-ENODEV);
	struct clk *cpu_clk = ERR_PTR(-ENODEV);
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		if (PTR_ERR(cpu_clk) == -EPROBE_DEFER)
			pr_warn("cpu clk for cpu%d not ready, retry.\n", cpu);
		else
			pr_err("failed to get cpu clk for cpu%d\n", cpu);

		ret = PTR_ERR(cpu_clk);
		return ret;
	}

	proc_reg = regulator_get_optional(cpu_dev, "proc");
	if (IS_ERR(proc_reg)) {
		if (PTR_ERR(proc_reg) == -EPROBE_DEFER)
			pr_warn("proc regulator for cpu%d not ready\n",
				cpu);
		else
			pr_warn("failed to get proc regulator for cpu%d\n",
			       cpu);
		proc_reg = NULL;
	}

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret) {
		pr_err("failed to get OPP-sharing information for cpu%d\n",
		       cpu);
		goto out_free_resources;
	}

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret) {
		pr_warn("no OPP table for cpu%d\n", cpu);
		goto out_free_resources;
	}

	info->cpu_dev = cpu_dev;
	info->proc_reg = proc_reg;
	info->cpu_clk = cpu_clk;

	return 0;

out_free_resources:
	if (!IS_ERR_OR_NULL(proc_reg))
		regulator_put(proc_reg);
	if (!IS_ERR(cpu_clk))
		clk_put(cpu_clk);

	return ret;
}

static void sp7350_cpu_dvfs_info_release(struct sp7350_cpu_dvfs_info *info)
{
	if (!IS_ERR_OR_NULL(info->proc_reg))
		regulator_put(info->proc_reg);
	if (!IS_ERR(info->cpu_clk))
		clk_put(info->cpu_clk);
	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static int sp7350_cpufreq_init(struct cpufreq_policy *policy)
{
	struct sp7350_cpu_dvfs_info *info;
	struct cpufreq_frequency_table *freq_table;
	int ret;

	info = sp7350_cpu_dvfs_info_lookup(policy->cpu);
	if (!info) {
		pr_err("dvfs info for cpu%d is not initialized.\n",
		       policy->cpu);
		return -EINVAL;
	}

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret) {
		pr_err("failed to init cpufreq table for cpu%d: %d\n",
		       policy->cpu, ret);
		return ret;
	}

	cpumask_copy(policy->cpus, &info->cpus);
	policy->freq_table = freq_table;
	policy->driver_data = info;
	policy->clk = info->cpu_clk;

	dev_pm_opp_of_register_em(info->cpu_dev, policy->cpus);

	return 0;
}

static int sp7350_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct sp7350_cpu_dvfs_info *info = policy->driver_data;

	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &policy->freq_table);

	return 0;
}

static struct cpufreq_driver sp7350_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
		 CPUFREQ_IS_COOLING_DEV,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = sp7350_cpufreq_set_target,
	.get = cpufreq_generic_get,
	.init = sp7350_cpufreq_init,
	.exit = sp7350_cpufreq_exit,
	.name = "sp7350-cpufreq",
	.attr = cpufreq_generic_attr,
};

static int sp7350_cpufreq_probe(struct platform_device *pdev)
{
	struct sp7350_cpu_dvfs_info *info, *tmp;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		info = sp7350_cpu_dvfs_info_lookup(cpu);
		if (info)
			continue;

		info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		ret = sp7350_cpu_dvfs_info_init(info, cpu);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize dvfs info for cpu%d\n",
				cpu);
			goto release_dvfs_info_list;
		}

		list_add(&info->list_head, &dvfs_info_list);
	}

	ret = cpufreq_register_driver(&sp7350_cpufreq_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sp7350 cpufreq driver\n");
		goto release_dvfs_info_list;
	}

	return 0;

release_dvfs_info_list:
	list_for_each_entry_safe(info, tmp, &dvfs_info_list, list_head) {
		sp7350_cpu_dvfs_info_release(info);
		list_del(&info->list_head);
	}

	return ret;
}

static struct platform_driver sp7350_cpufreq_platdrv = {
	.driver = {
		.name	= "sp7350-cpufreq",
	},
	.probe		= sp7350_cpufreq_probe,
};

/* List of machines supported by this driver */
static const struct of_device_id sp7350_cpufreq_machines[] __initconst = {
	{ .compatible = "sunplus,sp7350", },
	{ }
};
MODULE_DEVICE_TABLE(of, sp7350_cpufreq_machines);

static int __init sp7350_cpufreq_driver_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;
	struct platform_device *pdev;
	int err;

	np = of_find_node_by_path("/");
	if (!np)
		return -ENODEV;

	match = of_match_node(sp7350_cpufreq_machines, np);
	of_node_put(np);
	if (!match) {
		pr_debug("Machine is not compatible with sp7350-cpufreq\n");
		return -ENODEV;
	}

	err = platform_driver_register(&sp7350_cpufreq_platdrv);
	if (err)
		return err;

	/*
	 * Since there's no place to hold device registration code and no
	 * device tree based way to match cpufreq driver yet, both the driver
	 * and the device registration codes are put here to handle defer
	 * probing.
	 */
	pdev = platform_device_register_simple("sp7350-cpufreq", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		pr_err("failed to register sp7350-cpufreq platform device\n");
		return PTR_ERR(pdev);
	}

	return 0;
}
device_initcall(sp7350_cpufreq_driver_init);

MODULE_DESCRIPTION("Sunplus SP7350 CPUFreq driver");
MODULE_AUTHOR("QinJian <qinjian@sunmedia.com.cn>");
MODULE_LICENSE("GPL v2");
