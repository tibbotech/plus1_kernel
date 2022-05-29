// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/kernel.h>

/* MAILBOX2 CPU0(CA55) to CPU2(CM4)*/
#define DIRECT_CPU0_TO_CPU2_0 0x60
#define DIRECT_CPU0_TO_CPU2_1 0x64
#define DIRECT_CPU0_TO_CPU2_2 0x68
#define DIRECT_CPU0_TO_CPU2_3 0x6C
#define DIRECT_CPU0_TO_CPU2_4 0x70
#define DIRECT_CPU0_TO_CPU2_5 0x74
#define DIRECT_CPU0_TO_CPU2_6 0x78
#define DIRECT_CPU0_TO_CPU2_7 0x7C

/* MAILBOX2 CPU2(CM4) to CPU0(CA55)*/
#define DIRECT_CPU2_TO_CPU0_0 0x60
#define DIRECT_CPU2_TO_CPU0_1 0x64
#define DIRECT_CPU2_TO_CPU0_2 0x68
#define DIRECT_CPU2_TO_CPU0_3 0x6C
#define DIRECT_CPU2_TO_CPU0_4 0x70
#define DIRECT_CPU2_TO_CPU0_5 0x74
#define DIRECT_CPU2_TO_CPU0_6 0x78
#define DIRECT_CPU2_TO_CPU0_7 0x7C

struct sp_ipc_test_dev {
	void __iomem *mailbox2_cpu0_to_cpu2;
	void __iomem *mailbox2_cpu2_to_cpu0;
};

#ifdef CONFIG_SOC_Q645
	#define NUM_IRQ 17
#else
	#define NUM_IRQ 18
#endif

//#define IPC_TEST_FUNC_DEBUG
#ifdef IPC_TEST_FUNC_DEBUG
#define DBG_INFO(fmt, args ...)	pr_info("K_IPC_TEST: " fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif

static struct sp_ipc_test_dev *sp_ipc_test;

static irqreturn_t sp_ipc_test_isr(int irq, void *dev_id)
{
	int value;

	DBG_INFO("!!!%d@CPU%d!!!\n", irq, smp_processor_id());
	value = readl(sp_ipc_test->mailbox2_cpu0_to_cpu2+DIRECT_CPU0_TO_CPU2_7);
	DBG_INFO("%08x\n", value);
	return IRQ_HANDLED;
}

static void test_help(void)
{
	DBG_INFO(
		"sp_ipc test:\n"
		"  echo <reg:0~63> [value] > /sys/module/sp_ipc_test/parameters/test\n"
		"  regs:\n"
		"     0~31: G258 A2B\n"
		"    32~63: G259 B2A\n"
	);
}

static int test_set(const char *val, const struct kernel_param *kp)
{
	int i, cnt;
	u32 value;

	cnt = sscanf(val, "%d %d", &i, &value);
	switch (cnt) {
	case 0:
		test_help();
		break;
	case 1:
		DBG_INFO("%08x\n", readl(sp_ipc_test->mailbox2_cpu0_to_cpu2+DIRECT_CPU0_TO_CPU2_0));
		break;
	case 2:
		writel(value, sp_ipc_test->mailbox2_cpu0_to_cpu2+DIRECT_CPU0_TO_CPU2_0);
		break;
	}

	return 0;
}

static const struct kernel_param_ops test_ops = {
	.set = test_set,
};
module_param_cb(test, &test_ops, NULL, 0600);

static const struct of_device_id sp_ipc_test_of_match[] = {
	{ .compatible = "sunplus,q645-ipc-test" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_ipc_test_of_match);

static int sp_ipc_test_probe(struct platform_device *pdev)
{
	struct sp_ipc_test_dev *dev;
	struct resource *res_mem, *res_irq;
	int i, irq_number, ret = 0;

	sp_ipc_test = devm_kzalloc(&pdev->dev, sizeof(struct sp_ipc_test_dev), GFP_KERNEL);
    if (!sp_ipc_test)
	{
		ret = -ENOMEM;
		goto fail_kmalloc;
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu0_to_cpu2");
	sp_ipc_test->mailbox2_cpu0_to_cpu2 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_ipc_test->mailbox2_cpu0_to_cpu2))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_ipc_test->mailbox2_cpu0_to_cpu2);
	}

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu2_to_cpu0");
	sp_ipc_test->mailbox2_cpu2_to_cpu0 = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(sp_ipc_test->mailbox2_cpu2_to_cpu0))
	{
		dev_err(&pdev->dev, "ioremap fail\n");
		return PTR_ERR(sp_ipc_test->mailbox2_cpu2_to_cpu0);
	}

	for (i = 0; i < NUM_IRQ; i++) {
		res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res_irq) {
			DBG_INFO("%s:%d\n", __func__, __LINE__);
			return -ENODEV;
		}
		irq_number = platform_get_irq(pdev, i);
		DBG_INFO("irq_number=%d\n", irq_number);
		ret = devm_request_irq(&pdev->dev, irq_number, sp_ipc_test_isr, 0, "sp_ipc_test", dev);
		if (ret) {
			DBG_INFO("%s:%d\n", __func__, __LINE__);
			return -ENODEV;
		}
		DBG_INFO("%s:%d %d\n", __func__, __LINE__, ret);
	}
	return ret;

	fail_kmalloc:
	return ret;
}

static struct platform_driver sp_ipc_test_driver = {
	.probe		= sp_ipc_test_probe,
	.driver		= {
		.name	= "sp_ipc_test",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_ipc_test_of_match),
	},
};

module_platform_driver(sp_ipc_test_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus ipc_test driver");
