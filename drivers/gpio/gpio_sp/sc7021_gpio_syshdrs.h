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

#ifndef SC7021_GPIO_SYSHDRS_H
#define SC7021_GPIO_SYSHDRS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/gpio/driver.h>

#include <mach/io_map.h>

#endif // SC7021_GPIO_SYSHDRS_H
