/*
 * GPIO Driver for SunPlus/Tibbo SC7021 controller
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
 * TODO: irq hanling, pinconf (inv)
 */

#ifndef SC7021_GPIO_DEFS_H
#define SC7021_GPIO_DEFS_H

#define KINF(fmt,args...) printk( KERN_INFO MNAME": "fmt,##args)
#define KERR(fmt,args...) printk( KERN_ERR MNAME": "fmt,##args)

#ifdef SC7021_GPIO_DEBUG
#define DBGM(x) ( debug_mask & x)
#define KDBG(fmt,args...) printk( KERN_DEBUG MNAME": "fmt,##args)
#else
#define KDBG(fmt,args...) 
#endif

#define SC7021_GPIO_MAXN 128

#define SC7021_GPIO_OFF_GFR 0x00
#define SC7021_GPIO_OFF_CTL 0x00
#define SC7021_GPIO_OFF_OE 0x20
#define SC7021_GPIO_OFF_OUT 0x40
#define SC7021_GPIO_OFF_IN 0x60
// base1=380, IINV=380
#define SC7021_GPIO_OFF_IINV 0x00
// base1=380, OINV=3A0
#define SC7021_GPIO_OFF_OINV 0x20
// base1=380, OINV=3C0
#define SC7021_GPIO_OFF_OD 0x40

typedef struct sc7021gpio_chip_T {
 spinlock_t lock;
 struct gpio_chip chip;
 void __iomem *base0;   // MASTER , OE , OUT , IN
 void __iomem *base1;   // I_INV , O_INV , OD
 void __iomem *base2;   // GPIO_FIRST
 int irq[ 8];
 struct device *devp;
} sc7021gpio_chip_t;

static const char *sc7021gpio_list_names[] = {
 "GPIO_P0_00",
 "GPIO_P0_01",
 "GPIO_P0_02",
 "GPIO_P0_03",
 "GPIO_P0_04",
 "GPIO_P0_05",
 "GPIO_P0_06",
 "GPIO_P0_07",
 "GPIO_P1_00",
 "GPIO_P1_01",
 "GPIO_P1_02",
 "GPIO_P1_03",
 "GPIO_P1_04",
 "GPIO_P1_05",
 "GPIO_P1_06",
 "GPIO_P1_07",
 "GPIO_P2_00",
 "GPIO_P2_01",
 "GPIO_P2_02",
 "GPIO_P2_03",
 "GPIO_P2_04",
 "GPIO_P2_05",
 "GPIO_P2_06",
 "GPIO_P2_07",
 "GPIO_P3_00",
 "GPIO_P3_01",
 "GPIO_P3_02",
 "GPIO_P3_03",
 "GPIO_P3_04",
 "GPIO_P3_05",
 "GPIO_P3_06",
 "GPIO_P3_07",
 "GPIO_P4_00",
 "GPIO_P4_01",
 "GPIO_P4_02",
 "GPIO_P4_03",
 "GPIO_P4_04",
 "GPIO_P4_05",
 "GPIO_P4_06",
 "GPIO_P4_07",
 "GPIO_P5_00",
 "GPIO_P5_01",
 "GPIO_P5_02",
 "GPIO_P5_03",
 "GPIO_P5_04",
 "GPIO_P5_05",
 "GPIO_P5_06",
 "GPIO_P5_07",
 "GPIO_P6_00",
 "GPIO_P6_01",
 "GPIO_P6_02",
 "GPIO_P6_03",
 "GPIO_P6_04",
 "GPIO_P6_05",
 "GPIO_P6_06",
 "GPIO_P6_07",
 "GPIO_P7_00",
 "GPIO_P7_01",
 "GPIO_P7_02",
 "GPIO_P7_03",
 "GPIO_P7_04",
 "GPIO_P7_05",
 "GPIO_P7_06",
 "GPIO_P7_07",
 "GPIO_P8_00",
 "GPIO_P8_01",
 "GPIO_P8_02",
 "GPIO_P8_03",
 "GPIO_P8_04",
 "GPIO_P8_05",
 "GPIO_P8_06",
 "GPIO_P8_07",
};

#endif // SC7021_GPIO_DEFS_H
