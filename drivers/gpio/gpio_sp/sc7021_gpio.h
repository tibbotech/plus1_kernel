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

#ifndef SC7021_GPIO_H
#define SC7021_GPIO_H

#define MNAME "sc7021_gpio"
#define M_LIC "GPL v2"
#define M_AUT "Dvorkin Dmitry <dvorkin@tibbo.com>"
#define M_NAM "SC7021 GPIO module"
#define M_ORG "SunPlus/Tibbo Tech."
#define M_CPR "(C) 2019-2019"

#define SC7021_GPIO_DEBUG 1

#include "sc7021_gpio_syshdrs.h"
#include "sc7021_gpio_defs.h"

#include <mach/io_map.h>

//#define MOON_REG_BASE 0x9C000000
//#define MOON_REG_N(n) 0x80*(n)+MOON_REG_BASE
#define MOON_REG_N(n) PA_IOB_ADDR(0x80*(n))

//void sppctl_pin_set( sppctl_pdata_t *_p, uint8_t _pin, uint8_t _fun);


#endif // SC7021_GPIO_H
