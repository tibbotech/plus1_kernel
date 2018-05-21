#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#define NUM_ICM 4

#define TRACE	printk("%s:%d\n", __FUNCTION__, __LINE__)

struct sp_icm_reg {
	//u32 cfg0;	// config0
	u32 enable:1;
	u32 reload:1; // reload setting
	u32 intclr:1; // write 1 clear interrupt
	u32 muxsel:3; // select input signal source
	u32 clksel:3; // select clock source for counter
	u32 tstscl:6; // test signal clock prescale
	u32 rsv0:1;
	u32 msk0:16;

	//u32 cfg1;	// config1
	u32 eemode:2; // edge mode: 0 rising / 1 falling / 2 both
	u32 etimes:4; // event times (0~15)
	u32 dtimes:3; // debounce times (0~7)
	u32 rsv1:4;
	u32 fioclr:1; // fifo clear
	u32 fioept:1; // fifo empty
	u32 fioful:1; // fifo full
	u32 msk1:16;

	//u32 cfg2;	// config2
	u32 cntscl;	// counter clock prescale (0~255)
	u32 cnt;	// counter, read from fifo

	u32 pwh;	// pulse width high
	u32 pwl;	// pulse width low
};

struct sp_icm_dev {
	volatile struct sp_icm_reg *reg;
	int irq;
};

static struct sp_icm_dev sp_icm;

static irqreturn_t sp_icm_isr(int irq, void *dev_id)
{
	volatile struct sp_icm_reg *icm = &sp_icm.reg[irq - sp_icm.irq];

	TRACE;
	icm->intclr = 1; // clear interrupt
	TRACE;
	while (!icm->fioept) printk("%08x\n", icm->cnt); // read counter
	TRACE;

	return IRQ_HANDLED;
}

static int sp_icm_open(struct inode *inode, struct file *filp)
{
	volatile struct sp_icm_reg *icm;
	volatile u32 *dd;

	TRACE;

	icm = sp_icm.reg;
	dd = (void *)icm;
	icm->muxsel = 4;	// select input source: test signal
	icm->clksel = 4;	// select clock source: sysclk
	icm->tstscl = 10;	// test signal prescale: sysclk / (tstscl + 1) * 2
	icm->eemode = 0;	// event mode: rising
	icm->etimes = 1;	// event times
	icm->enable = 1;
	printk("icm(%p): %08x %08x %08x %08x %08x\n", icm, dd[0], dd[1], dd[2], dd[3], dd[4]);

	TRACE;
	msleep(1000);
	TRACE;

	return 0;
}

static int sp_icm_release(struct inode *inode, struct file *filp)
{
	TRACE;
	sp_icm.reg->enable = 0;
	return 0;
}

static long sp_icm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg_)
{
	TRACE;
	return 0;
}

static const struct file_operations sp_icm_fops = {
	.owner = THIS_MODULE,
	.open = sp_icm_open,
	.release = sp_icm_release,
	.unlocked_ioctl = sp_icm_ioctl,
};

static struct miscdevice sp_icm_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sp_icm",
	.fops = &sp_icm_fops,
};

static int sp_icm_probe(struct platform_device *pdev)
{
	struct sp_icm_dev *dev = &sp_icm;
	struct resource *res_mem, *res_irq;
	void __iomem *membase;
	int i = 0;
	int ret = 0;

	TRACE;
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	membase = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(membase))
		return PTR_ERR(membase);

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq) {
		ret = -ENODEV;
		goto out;
	}

	dev->reg = membase;
	dev->irq = res_irq->start;
	platform_set_drvdata(pdev, dev);

	while (i < NUM_ICM) {
		ret = request_irq(dev->irq + i, sp_icm_isr, IRQF_TRIGGER_RISING, "sp_icm", dev);
		if (ret) goto out;
		i++;
	}

	ret = misc_register(&sp_icm_dev);
	TRACE;

out:
	if (ret) {
		TRACE;
		while (i--) free_irq(dev->irq + i, dev);
		devm_iounmap(&pdev->dev, membase);
	}

	return ret;
}

static int sp_icm_remove(struct platform_device *pdev)
{
	struct sp_icm_dev *dev = platform_get_drvdata(pdev);
	int i = NUM_ICM;

	misc_deregister(&sp_icm_dev);
	while (i--) free_irq(dev->irq + i, dev);
	devm_iounmap(&pdev->dev, (void *)dev->reg);

	return 0;
}

static const struct of_device_id sp_icm_of_match[] = {
	{ .compatible = "sunplus,sp-icm" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_crypto_of_match);

static struct platform_driver sp_icm_driver = {
	.probe		= sp_icm_probe,
	.remove		= sp_icm_remove,
	.driver		= {
		.name	= "sp_icm",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_icm_of_match),
	},
};

static int __init sp_icm_module_init(void)
{
	platform_driver_register(&sp_icm_driver);
	return 0;
}

static void __exit sp_icm_module_exit(void)
{
	platform_driver_unregister(&sp_icm_driver);
}

module_init(sp_icm_module_init);
module_exit(sp_icm_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus ICM driver");
