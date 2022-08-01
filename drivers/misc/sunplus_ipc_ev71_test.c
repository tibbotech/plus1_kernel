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
	#define NUM_IRQ 18
#else
	#define NUM_IRQ 18
#endif

#define IPC_TEST_FUNC_DEBUG
#ifdef IPC_TEST_FUNC_DEBUG
#define DBG_INFO(fmt, args ...)	pr_info("K_IPC_TEST: " fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif
#define INTERRUPT_MSG
#define EV71_SELF_TEST 0 //set 1 if want to do mailbox self test in EV71 side 

static struct sp_ipc_test_dev sp_ipc_test;
static u32 irq_trigged;

static irqreturn_t sp_ipc_test_isr(int irq, void *dev_id)
{
	unsigned int value,i;

	DBG_INFO("!!!%d@CPU%d!!!\n", irq, smp_processor_id());	
#ifdef INTERRUPT_MSG//check irq sent to CA55 from CA55-EV71 Mailbox
	#if (EV71_SELF_TEST == 0)
    switch (irq) {
		case 57:  //swirq 0 to 1
			for (i=4;i < 24; i++){
				if (readl(&sp_ipc_test.reg[1]) & (0x01 << i)){  //check normal data lock status to determine which data is written
					value = readl(&sp_ipc_test.reg[i]);//read cpu0 to 1 normal_reg which is locked
					DBG_INFO("data%d = %d\n", i, value);
					break;
				}
			}
			if (i >= 24) DBG_INFO("No normal data is written\n");
			writel(0, &sp_ipc_test.reg[0]); // de-assert CPU0_TO_CPU1_INT  CA55_IRQ_57  EV71_IRQ_33
			irq_trigged = 1;
			break;
		case 56:  //swirq 1 to 0
			for (i=4;i < 24; i++){
				if (readl(&sp_ipc_test.reg[33]) & (0x01 << i)){  //check normal data lock status to determine which data is written
					value = readl(&sp_ipc_test.reg[i+32]);//read cpu1 to 0 normal_reg which is locked
					DBG_INFO("data%d = %d\n", i+32, value);
					break;
				}
			}
			if (i >= 24) DBG_INFO("No normal data is written\n");
			writel(0, &sp_ipc_test.reg[32]); // de-assert CPU1_TO_CPU0_INT CA55_IRQ_56	
			irq_trigged = 1;
			break;
		case 48:
		case 49:
		case 50:
        case 51:
		case 52:
		case 53:
		case 54:
		case 55:
			if (readl(&sp_ipc_test.reg[1]) & (0x01 << (irq-24))){  //check data lock status to determine if data is written or not			
				value = readl(&sp_ipc_test.reg[(irq-24)]); //read CPU0_TO_CPU1 CPU1_DIRECT_REG00~07
				DBG_INFO("data%d: %x\n",(irq-24), value);
			}
			else{
				DBG_INFO("No direct data is written\n");
			}
			irq_trigged = 1;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:	
			if (readl(&sp_ipc_test.reg[33]) & (0x01 << (irq-16))){  //check data lock status 
				value = readl(&sp_ipc_test.reg[(irq+16)]); //read CPU1_TO_CPU0 CPU0_DIRECT_REG00~07
				DBG_INFO("data%d: %x\n",(irq+16),value);
			}
			else{
				DBG_INFO("No direct data is written\n");
			}			
			irq_trigged = 1;
			break;
    	}
	#endif	
#endif
	return IRQ_HANDLED;
}

static void test_help(void)
{
	DBG_INFO(
		"sp_ipc test:\n"
		"  echo <reg:0~63> [value] > /sys/module/sunplus_ipc_test/parameters/test\n"
		"  regs:\n"
		"     0~31: G258 CA552EV71\n"
		"    32~63: G259 EV712CA55\n"
	);
}

static int test_set(const char *val, const struct kernel_param *kp)
{
	int i, cnt;
	u32 value;

	cnt = sscanf(val, "%d %d", &i, &value);
	switch (cnt) {
	case 0: //no parameter
		test_help();
		break;
	case 1: //one parameter
		DBG_INFO("%08x\n", sp_ipc_test.reg[i]);
		break;
	case 2: //two parameter
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
	{ .compatible = "sunplus,q645-ipc-ca55-ev71-test" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_ipc_test_of_match);

static int sp_ipc_test_probe(struct platform_device *pdev)
{
	struct sp_ipc_test_dev *dev = &sp_ipc_test;
	struct resource *res_mem, *res_irq;
	unsigned int i, irq_number, ret = 0, timeout;

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

	DBG_INFO("write data to trigger irq\n");
	irq_trigged = 0;

#ifdef CONFIG_SOC_Q645
	#if (EV71_SELF_TEST == 0)
    for (i=4;i < 24; i++){
		if (readl(&dev->reg[1]) & (0x01 << i)){  //check normal data lock status 
			DBG_INFO("data%d lock\n", i);
		}
		else 
		{
			writel(i, &dev->reg[i]);//write cpu0 to 1 normal_reg00~19

			writel(i, &dev->reg[i]);//write cpu0 to 1 normal_reg00~19 again, force to be overwritten status 
			if (readl(&dev->reg[2]) & (0x01 << i)){
				DBG_INFO("data%d is overwirtten\n", i);
				writel((readl(&dev->reg[2]) & (!(0x01 << i))), &dev->reg[2]);//clear data overwritten status
			}

			DBG_INFO("swirq to 1\n");
			writel(1, &dev->reg[0]); // trigger CPU0_TO_CPU1_INT  CA55_IRQ_57  EV71_IRQ_33
			
			for (timeout = 0;timeout < 10000;timeout += 100){
				if (irq_trigged) {
	                irq_trigged = 0;
					break;
				}
				udelay(100);
			}
			if (timeout >= 10000){
				DBG_INFO("swirq%d to 1 timeout\n", i);
			}
			if (readl(&dev->reg[1]) & (0x01 << i)){  //check normal data lock status 
				DBG_INFO("data%d lock\n", i);
			}			
		}
	}

    for (i=4;i < 24; i++){
		if (readl(&dev->reg[33]) & (0x01 << i)){  //check normal data lock status 
			DBG_INFO("data%d lock\n", i+32);
		}
		else 
		{
			writel(i+32, &dev->reg[i+32]);//write cpu1 to 0 normal_reg00~19

			writel(i+32, &dev->reg[i+32]);//write cpu1 to 0 normal_reg00~19 again, force to be overwritten status 
			if (readl(&dev->reg[34]) & (0x01 << i)){
				DBG_INFO("data%d is overwirtten\n", i+32);
				writel((readl(&dev->reg[34]) & (!(0x01 << i))), &dev->reg[34]);//clear data overwritten status
			}

			DBG_INFO("swirq to 0\n");
			writel(1, &dev->reg[32]); // trigger CPU1_TO_CPU0_INT CA55_IRQ_56	
			
			for (timeout = 0;timeout < 10000;timeout += 100){
				if (irq_trigged) {
	                irq_trigged = 0;
					break;
				}
				udelay(100);
			}
			if (timeout >= 10000){
				DBG_INFO("swirq%d to 1 timeout\n", i);
			}
			if (readl(&dev->reg[33]) & (0x01 << i)){  //check normal data lock status 
				DBG_INFO("data%d lock\n", i+32);
			}			
		}
	}
	

    for (i = 0; i < 8; i ++){
		if (readl(&dev->reg[1]) & (0x01 << (i+24))){  //check direct data lock status 
			DBG_INFO("data%d lock\n", i+24);
		}
		else 
		{
		    DBG_INFO("hwirq%d to 1\n", i);
			writel(0x12345670+i, &dev->reg[i+24]); // write cpu0_to_cpu1_direct_reg00~07, trigger CPU0_TO_CPU1 DIRECT_INT0~7

			writel(0x12345670+i, &dev->reg[i+24]); // write cpu0_to_cpu1_direct_reg again, force to be overwritten status 
			if (readl(&dev->reg[2]) & (0x01 << (i+24))){
				DBG_INFO("data%d is overwirtten\n", i+24);
				writel((readl(&dev->reg[2]) & (!(0x01 << (i+24)))), &dev->reg[2]);//clear data overwritten status
			}

			for (timeout = 0;timeout < 10000;timeout += 100){
				if (irq_trigged) {
	                irq_trigged = 0;
					break;
				}
				udelay(100);
			}
			if (timeout >= 10000){
				DBG_INFO("hwirq%d to 1 timeout\n", i);
			}
			if (readl(&dev->reg[1]) & (0x01 << (i+24))){  //check data lock status 
				DBG_INFO("data%d lock\n", i+24);
			}		
		}		
	}


    for (i = 0; i < 8; i ++){
		if (readl(&dev->reg[33]) & (0x01 << (i+24))){  //check data lock status 
			DBG_INFO("data%d lock\n", 56+i);
		}
		else 
		{
		    DBG_INFO("hwirq%d to 0\n", i);
			writel(0x87654320+i, &dev->reg[i+56]); // write cpu1_to_cpu0_direct_reg00~07, trigger CPU1_TO_CPU0 DIRECT_INT0~7.

			writel(0x87654320+i, &dev->reg[i+56]); // write cpu1_to_cpu0_direct_reg00~07 again, force to be overwritten status 
			if (readl(&dev->reg[34]) & (0x01 << (i+24))){
				DBG_INFO("data%d is overwirtten\n", i+56);
				writel((readl(&dev->reg[34]) & (!(0x01 << (i+24)))), &dev->reg[34]);//clear data overwritten status
			}
			
			for (timeout = 0;timeout < 10000;timeout += 100){
				if (irq_trigged) {
	                irq_trigged = 0;
					break;
				}
				udelay(100);
			}
			if (timeout >= 10000){
				DBG_INFO("hwirq%d to 0 timeout\n", i);
			}
			if (readl(&dev->reg[33]) & (0x01 << (i+24))){  //check data lock status 
				DBG_INFO("data%d lock\n", i+56);
			}		
		}		
	}

	#endif
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
