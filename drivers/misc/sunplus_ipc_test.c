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

struct sp_ipc_test_dev {
	u32 *reg;
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
#define INTERRUPT_MSG

static struct sp_ipc_test_dev sp_ipc_test;

static irqreturn_t sp_ipc_test_isr(int irq, void *dev_id)
{
	int value;

	DBG_INFO("!!!%d@CPU%d!!!\n", irq, smp_processor_id());
#ifdef INTERRUPT_MSG
	value = readl(&sp_ipc_test.reg[56]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[57]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[58]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[59]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[60]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[61]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[62]); //trigger CPU2_TO_CPU0 DIRECT_INT
	value = readl(&sp_ipc_test.reg[63]); //trigger CPU2_TO_CPU0 DIRECT_INT
#endif
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
		DBG_INFO("%08x\n", sp_ipc_test.reg[i]);
		break;
	case 2:
		sp_ipc_test.reg[i] = value;
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
	struct sp_ipc_test_dev *dev = &sp_ipc_test;
	struct resource *res_mem, *res_irq;
	int i, irq_number, ret = 0;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	dev->reg = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR((void *)dev->reg))
		return PTR_ERR((void *)dev->reg);


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

#ifdef CONFIG_SOC_Q645
	writel(1, &dev->reg[0]); // trigger CPU0_TO_CPU2 INT
	writel(0x12345624, &dev->reg[24]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345625, &dev->reg[25]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345626, &dev->reg[26]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345627, &dev->reg[27]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345628, &dev->reg[28]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345629, &dev->reg[29]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345630, &dev->reg[30]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(0x12345631, &dev->reg[31]); // trigger CPU0_TO_CPU2 DIRECT_INT
	writel(1, &dev->reg[32]); // trigger CPU2_TO_CPU0 INT
	writel(0x87654356, &dev->reg[56]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654357, &dev->reg[57]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654358, &dev->reg[58]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654359, &dev->reg[59]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654360, &dev->reg[60]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654361, &dev->reg[61]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654362, &dev->reg[62]); //trigger CPU2_TO_CPU0 DIRECT_INT
	writel(0x87654363, &dev->reg[63]); //trigger CPU2_TO_CPU0 DIRECT_INT
#else
	dev->reg[0] = 1; // trigger A2B INT
	dev->reg[31] = 0x12345678; // trigger A2B DIRECT_INT
	dev->reg[32] = 1; // trigger B2A INT
	dev->reg[63] = 0x87654321; // trigger B2A DIRECT_INT
#endif
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
