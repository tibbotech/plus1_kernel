/*
 * GPIO Driver for Sunplus I143 controller
 * Copyright (C) 2019 SunPlus Tech./Tibbo Tech.
 * Author: Dvorkin Dmitry <dvorkin@tibbo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SP7021_GPIO_H
#define SP7021_GPIO_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio/driver.h>
#include <linux/stringify.h>

#include "sppctl.h"


#define I143_GPIO_IRQS 8

#ifndef SPPCTL_H

#define MNAME "i143_gpio"
#define M_LIC "GPL v2"
#define M_AUT "Dvorkin Dmitry <dvorkin@tibbo.com>"
#define M_NAM "Sunplus I143 GPIO"
#define M_ORG "Sunplus Technology"
#define M_CPR "(C) 2019-2020"

#define KINF(pd,fmt,args...) { \
	if ((pd) != NULL) { dev_info((pd),""fmt,##args);  \
	} else { printk(KERN_INFO     MNAME": "fmt,##args); } }
#define KERR(pd,fmt,args...) { \
	if ((pd) != NULL) { dev_info((pd),""fmt,##args);  \
	} else { printk(KERN_ERR      MNAME": "fmt,##args); } }
#ifdef CONFIG_DEBUG_GPIO
#define KDBG(pd,fmt,args...) { \
	if ((pd) != NULL) { dev_info((pd),""fmt,##args);  \
	} else { printk(KERN_DEBUG    MNAME": "fmt,##args); } }
#else
#define KDBG(pd,fmt,args...)
#endif

#endif // SPPCTL_H

typedef struct sp7021gpio_chip_T {
	spinlock_t lock;
	struct gpio_chip chip;
	void __iomem *base0;   // MASTER , OE , OUT , IN
	void __iomem *base1;   // I_INV , O_INV , OD
	void __iomem *base2;   // GPIO_FIRST
	int irq[I143_GPIO_IRQS];
} sp7021gpio_chip_t;

extern const char * const sp7021gpio_list_s[];
extern const size_t GPIS_listSZ;

int sp7021_gpio_new(struct platform_device *_pd, void *_datap);
int sp7021_gpio_del(struct platform_device *_pd, void *_datap);

#define D_PIS(x) "GPIO" __stringify(x)

// FIRST: MUX=0, GPIO=1
enum muxF_MG {
	muxF_M = 0,
	muxF_G = 1,
	muxFKEEP = 2,
};
// MASTER: IOP=0,GPIO=1
enum muxM_IG {
	muxM_I = 0,
	muxM_G = 1,
	muxMKEEP = 2,
};

typedef enum muxF_MG muxF_MG_t;
typedef enum muxM_IG muxM_IG_t;

#endif // SP7021_GPIO_H
