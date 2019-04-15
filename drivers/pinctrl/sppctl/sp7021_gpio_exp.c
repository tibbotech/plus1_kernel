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

u32 gpio_in_val(u32 bit)
{
	u32 value = 0;

	gpio_in(bit, &value);

	return value;
}
EXPORT_SYMBOL(gpio_in_val);

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
