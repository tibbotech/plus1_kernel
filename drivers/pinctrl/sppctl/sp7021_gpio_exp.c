/*
 * GPIO Driver for SunPlus/Tibbo SP7021 controller - internal functions
 * Copyright (C) 2019 SunPlus Tech.
 * TODO: get rid of it : it is used by drivers/net/ethernet/sp/l2sw_hal.c:
 * GPIO_PIN_MUX_SEL(PMX_L2SW_CLK_OUT,40);
 */

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

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <mach/io_map.h>
#include "mach/gpio_drv.h"

static DEFINE_SPINLOCK(slock_gpio);

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

// used as GPIO_O_SET in 
// include/linux/usb/sp_usb.h
// drivers/mipicsi/mipicsi-core.c
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

// used as GPIO_O_SET in 
// include/linux/usb/sp_usb.h
// drivers/mipicsi/mipicsi-core.c
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

u32 gpio_in_val(u32 bit)
{
	u32 value = 0;

	gpio_in(bit, &value);

	return value;
}
EXPORT_SYMBOL(gpio_in_val);
