#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <asm/io.h>
//#include <mach/hardware.h>
#include <mach/gpio_drv.h>
//#include <mach/sp_tset.h>
//#include <mach/module.h>
#include <mach/io_map.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>

/* #define GPIO_FUNC_DEBUG */
#define GPIO_KDBG_INFO
#define GPIO_KDBG_ERR

#ifdef GPIO_FUNC_DEBUG
	#define FUNC_DEBUG() printk(KERN_INFO "%s: %d %s()\n", __FILE__, __LINE__, __func__)
#else
	#define FUNC_DEBUG()
#endif

#ifdef GPIO_KDBG_INFO
	#define DBG_INFO(fmt, args ...) printk(KERN_INFO "K_GPIO: " fmt, ## args)
#else
	#define DBG_INFO(fmt, args ...)
#endif

#ifdef GPIO_KDBG_ERR
	#define DBG_ERR(fmt, args ...)  printk(KERN_ERR "K_GPIO: " fmt, ## args)
#else
	#define DBG_ERR(fmt, args ...)
#endif

#define NUM_GPIO    (1)
#define DEVICE_NAME             "sp_gpio"
#define NUM_GPIO_MAX    (256)
#define REG_GRP_OFS(GRP, OFFSET)        VA_IOB_ADDR((GRP) * 32 * 4 + (OFFSET) * 4) 
#define GPIO_FIRST(X)   (REG_GRP_OFS(101, (25+X)))
#define GPIO_MASTER(X)  (REG_GRP_OFS(6, (0+X)))
#define GPIO_OE(X)      (REG_GRP_OFS(6, (8+X)))
#define GPIO_OUT(X)     (REG_GRP_OFS(6, (16+X)))
#define GPIO_IN(X)      (REG_GRP_OFS(6, (24+X)))
#define GPIO_I_INV(X)   (REG_GRP_OFS(7, (0+X)))
#define GPIO_O_INV(X)   (REG_GRP_OFS(7, (8+X)))
#define GPIO_OD(X)      (REG_GRP_OFS(7, (16+X)))
#define GPIO_SFT_CFG(G,X)      (REG_GRP_OFS(G, (0+X)))

static DEFINE_SPINLOCK(slock_gpio);
static int dev_major;
static int dev_minor;
static struct cdev *dev_cdevp = NULL;
static struct class *p_gpio_class = NULL;


long gpio_input_invert_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) | 1 << ((bit & 0x0f) + 0x10));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_I_INV(idx)) | value), GPIO_I_INV(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_input_invert_1);

long gpio_input_invert_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}
	value = ((ioread32(GPIO_I_INV(idx)) | (1 << ((bit & 0x0f) + 0x10))) & ~( 1 << (bit & 0x0f)));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_I_INV(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_input_invert_0);

long gpio_output_invert_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) | 1 << ((bit & 0x0f) + 0x10));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_O_INV(idx)) | value), GPIO_O_INV(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_output_invert_1);

long gpio_output_invert_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}
	value = ((ioread32(GPIO_O_INV(idx)) | (1 << ((bit & 0x0f) + 0x10))) & ~( 1 << (bit & 0x0f)));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_O_INV(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_output_invert_0);

long gpio_open_drain_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) | 1 << ((bit & 0x0f) + 0x10));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_OD(idx)) | value), GPIO_OD(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_open_drain_1);

long gpio_open_drain_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}
	value = ((ioread32(GPIO_OD(idx)) | (1 << ((bit & 0x0f) + 0x10))) & ~( 1 << (bit & 0x0f)));
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_OD(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_open_drain_0);

long gpio_first_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 5;
	if (idx > 4) {
		return -EINVAL;
	}
	
	value = 1 << (bit & 0x1f);
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_FIRST(idx)) | value), GPIO_FIRST(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_first_1);

long gpio_first_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 5;
	if (idx > 4) {
		return -EINVAL;
	}

	value = 1 << (bit & 0x1f);
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_FIRST(idx)) & (~value)), GPIO_FIRST(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);
	return 0;
}
EXPORT_SYMBOL(gpio_first_0);

long gpio_master_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) | 1 << ((bit & 0x0f) + 0x10));
	//if (gpio_check_first(idx, value) == -EINVAL) {
	//	return -EINVAL;
	//}
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_MASTER(idx)) | value), GPIO_MASTER(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_master_1);

long gpio_master_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}
	
	value = ((ioread32(GPIO_MASTER(idx)) | (1 << ((bit & 0x0f) + 0x10)) ) & ~( 1 << (bit & 0x0f)));
	//if (gpio_check_first(idx, value) == -EINVAL) {
	//	return -EINVAL;
	//}
		
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_MASTER(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_master_0);

long gpio_set_oe(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) |  1 << ((bit & 0x0f) + 0x10));

#if 0
	if (gpio_check_first(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_master(idx, value) == -EINVAL) {
		return -EINVAL;
	}
#endif
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_OE(idx)) | value), GPIO_OE(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_set_oe);

long gpio_clr_oe(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
  if (idx > 8) {
		return -EINVAL;
	}
	
	value = ((ioread32(GPIO_OE(idx)) | (1 << ((bit & 0x0f) + 0x10)) ) & ~( 1 << (bit & 0x0f)));
#if 0
	if (gpio_check_first(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_master(idx, value) == -EINVAL) {
		return -EINVAL;
	}
#endif
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_OE(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_clr_oe);

long gpio_out_1(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = (1 << (bit & 0x0f) | 1 << ((bit & 0x0f) + 0x10));
	
#if 0
	if (gpio_check_first(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_master(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_oe(idx, value) == -EINVAL) {
		return -EINVAL;
	}
#endif
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32((ioread32(GPIO_OUT(idx)) | value), GPIO_OUT(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_out_1);

long gpio_out_0(u32 bit)
{
	u32 idx, value;
	unsigned long flags;

	idx = bit >> 4;
	if (idx > 8) {
		return -EINVAL;
	}

	value = ((ioread32(GPIO_OUT(idx)) | (1 << ((bit & 0x0f) + 0x10)) ) & ~( 1 << (bit & 0x0f)));
#if 0
	if (gpio_check_first(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_master(idx, value) == -EINVAL) {
		return -EINVAL;
	}
	if (gpio_check_oe(idx, value) == -EINVAL) {
		return -EINVAL;
	}
#endif
	spin_lock_irqsave(&slock_gpio, flags);
	iowrite32(value, GPIO_OUT(idx));
	spin_unlock_irqrestore(&slock_gpio, flags);

	return 0;
}
EXPORT_SYMBOL(gpio_out_0);

long gpio_in(u32 bit, u32 *gpio_in_value)
{

	u32 idx, value;
	unsigned long flags;
	
	idx = bit >> 5;
	if (idx > 5) {
		return -EINVAL;
	}
	
	value = 1 << (bit & 0x1f);
	
	spin_lock_irqsave(&slock_gpio, flags);
	*gpio_in_value = (ioread32(GPIO_IN(idx)) & value) ? 1 : 0;
	spin_unlock_irqrestore(&slock_gpio, flags);
		
	return 0;
}
EXPORT_SYMBOL(gpio_in);

long gpio_pin_mux_sel(PMXSEL_ID id, u32 sel)
{
	u32 grp ,idx, max_value, reg_val, mask, bit_num;
	unsigned long flags;
	
	grp = (id >> 24) & 0xff;
	if (grp > 0x03) {
		return -EINVAL;
	}	
	
	idx = (id >> 16) & 0xff;
	if (idx > 0x1f) {
		return -EINVAL;
	}
	
	max_value = (id >> 8) & 0xff;
	if (sel > max_value) {
		return -EINVAL;
	}
	
	bit_num = id & 0xff;
	
	if (max_value == 1) {
		mask = 0x01 << bit_num;
	}
	else if ((max_value == 2) || (max_value == 3)) {
		mask = 0x03 << bit_num;
	}
	else {
		mask = 0x7f << bit_num;
	}	

	spin_lock_irqsave(&slock_gpio, flags);	
	reg_val = ioread32(GPIO_SFT_CFG(grp,idx));
	reg_val |= mask << 0x10 ;
	reg_val &= (~mask);	
	reg_val = ((sel << bit_num) | (mask << 0x10));		
	iowrite32(reg_val, GPIO_SFT_CFG(grp,idx));
	spin_unlock_irqrestore(&slock_gpio, flags);
	

	return 0;
}
EXPORT_SYMBOL(gpio_pin_mux_sel);

long gpio_pin_mux_get(PMXSEL_ID id, u32 *sel)
{
	u32 grp , idx, max_value, reg_val, mask, bit_num;
	unsigned long flags;
	
	grp = (id >> 24) & 0xff;
	
	idx = (id >> 16) & 0xff;
	if (idx > 0x11) {
		return -EINVAL;
	}

	max_value = (id >> 8) & 0xff;
	if (sel > max_value) {
		return -EINVAL;
	}
	
	bit_num = id & 0xff;

	if (max_value == 1) {
		mask = 0x01 << bit_num;
	}
	else if ((max_value == 2) || (max_value == 3)) {
		mask = 0x03 << bit_num;
	}
	else {
		mask = 0x7f << bit_num;
	}

	spin_lock_irqsave(&slock_gpio, flags);
	reg_val = ioread32(GPIO_SFT_CFG(grp,idx));
	reg_val &= mask;
	*sel = (reg_val >> bit_num);
	spin_unlock_irqrestore(&slock_gpio, flags);
	return 0;
}
EXPORT_SYMBOL(gpio_pin_mux_get);

static long gpio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	u32 bit;
	u32 gpio_in_value = 0;
	PMXSEL_T pin_mux;

	FUNC_DEBUG();


	if (cmd == GPIO_IOC_PIN_MUX_SEL) {
		if (unlikely(copy_from_user(&pin_mux, (void *)arg, sizeof(pin_mux)))) {
			return -EFAULT;
		}
	} else {
		if (unlikely(copy_from_user(&bit, (void *)arg, sizeof(bit)))) {
			return -EFAULT;
		}
	}

	switch (cmd) {
	case GPIO_IOC_INPUT_INVERT_1:
		err = gpio_input_invert_1(bit);
		break;
	case GPIO_IOC_INPUT_INVERT_0:
		err = gpio_input_invert_0(bit);
		break;
	case GPIO_IOC_OUTPUT_INVERT_1:
		err = gpio_output_invert_1(bit);
		break;
	case GPIO_IOC_OUTPUT_INVERT_0:
		err = gpio_output_invert_0(bit);
		break;
	case GPIO_IOC_OPEN_DRAIN_1:
		err = gpio_open_drain_1(bit);
		break;
	case GPIO_IOC_OPEN_DRAIN_0:
		err = gpio_open_drain_0(bit);
		break;
	case GPIO_IOC_FIRST_1:
		err = gpio_first_1(bit);
		break;
	case GPIO_IOC_FIRST_0:
		err = gpio_first_0(bit);
		break;
	case GPIO_IOC_MASTER_1:
		err = gpio_master_1(bit);
		break;
	case GPIO_IOC_MASTER_0:
		err = gpio_master_0(bit);
		break;
	case GPIO_IOC_SET_OE:
		err = gpio_set_oe(bit);
		break;
	case GPIO_IOC_CLR_OE:
		err = gpio_clr_oe(bit);
		break;
	case GPIO_IOC_OUT_1:
		err = gpio_out_1(bit);
		break;
	case GPIO_IOC_OUT_0:
		err = gpio_out_0(bit);
		break;
	case GPIO_IOC_IN:
		err = gpio_in(bit, &gpio_in_value);
		err |= copy_to_user((void *)arg, &gpio_in_value, sizeof(gpio_in_value));
		break;
	case GPIO_IOC_PIN_MUX_GET:
		err = gpio_pin_mux_get((PMXSEL_ID)pin_mux.id, &pin_mux.val);
		err |= copy_to_user((void *)arg, &pin_mux.val, sizeof(pin_mux.val));
		break;
	case GPIO_IOC_PIN_MUX_SEL:
		err = gpio_pin_mux_sel((PMXSEL_ID)pin_mux.id, pin_mux.val);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static irqreturn_t sunplus_gpio_irq(int irq, void *args)
{
	printk("sunplus_gpio_irq\n");
	return IRQ_HANDLED;
}


static int gpio_open(struct inode *inode, struct file *filp)
{
	FUNC_DEBUG();
	return 0;
}

static int gpio_release(struct inode *inode, struct file *filp)
{
	FUNC_DEBUG();
	return 0;
}

static int sunplus_gpio_platform_driver_probe_of(struct platform_device *pdev)
{
#if 0
	int ret,npins,i;
	struct resource *ires;
	
	npins = platform_irq_count(pdev);
	for (i = 0; i < npins; i++) {
		ires = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!ires) {
			return -ENODEV;
		}		
		ret = request_irq(ires->start, sunplus_gpio_irq, 0, "sp-gpio", ires);
		if (ret) { 
			printk("request_irq() failed (%d)\n", ret); 
		}
	}
#endif
	return 0;
}

static const struct of_device_id sp_gpio_of_match[] = {
	{ .compatible = "sunplus,sp-gpio" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_gpio_of_match);

static struct file_operations fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = gpio_ioctl,
	.open           = gpio_open,
	.release        = gpio_release
};

static struct platform_driver sp_gpio_driver = {
	.probe		= sunplus_gpio_platform_driver_probe_of,
	.driver = {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_gpio_of_match),
	}
};

static struct platform_device sp_gpio_device = {
	.name	= "sp_gpio",
	.id	= 0,
};



static int __init gpio_drv_init(void)
{
	dev_t dev = 0;
	int ret;
	ret = alloc_chrdev_region(&dev, 0, NUM_GPIO, DEVICE_NAME);
	if (ret) {
		return ret;
	}

  dev_major = MAJOR(dev);
	dev_minor = MINOR(dev);
	
	dev_cdevp = kzalloc(sizeof(struct cdev), GFP_KERNEL);
	if (dev_cdevp == NULL) {
		goto failed00;
	}
	
	p_gpio_class = class_create(THIS_MODULE, DEVICE_NAME);
	cdev_init(dev_cdevp, &fops);
	dev_cdevp->owner = THIS_MODULE;
	ret = cdev_add(dev_cdevp, dev, NUM_GPIO);
	if (ret < 0) {
		goto failed00;
	}
	device_create(p_gpio_class, NULL, dev, NULL, DEVICE_NAME);
	
	platform_device_register(&sp_gpio_device);
	platform_driver_register(&sp_gpio_driver);

	return 0;	
	
	failed00:
	if (dev_cdevp) {
		kfree(dev_cdevp);
		dev_cdevp = NULL;
	}

	unregister_chrdev_region(dev, NUM_GPIO);
	return -ENODEV;
}
 
static void __exit gpio_drv_exit(void)
{
	dev_t dev;

	FUNC_DEBUG();

	dev = MKDEV(dev_major, dev_minor);
	if (dev_cdevp) {
		cdev_del(dev_cdevp);
		kfree(dev_cdevp);
		dev_cdevp = NULL;
	}

	unregister_chrdev_region(dev, NUM_GPIO);
	platform_device_unregister(&sp_gpio_device);
	platform_driver_unregister(&sp_gpio_driver);
}
 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunplus Technology");
MODULE_DESCRIPTION("Sunplus GPIO driver");
 
module_init(gpio_drv_init);
module_exit(gpio_drv_exit);
