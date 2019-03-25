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
 */

#include "sc7021_gpio.h"

const char * const sc7021gpio_list_names[] = {
 D_PIS(0,0), D_PIS(0,1), D_PIS(0,2), D_PIS(0,3), D_PIS(0,4), D_PIS(0,5), D_PIS(0,6), D_PIS(0,7),
 D_PIS(1,0), D_PIS(1,1), D_PIS(1,2), D_PIS(1,3), D_PIS(1,4), D_PIS(1,5), D_PIS(1,6), D_PIS(1,7),
 D_PIS(2,0), D_PIS(2,1), D_PIS(2,2), D_PIS(2,3), D_PIS(2,4), D_PIS(2,5), D_PIS(2,6), D_PIS(2,7),
 D_PIS(3,0), D_PIS(3,1), D_PIS(3,2), D_PIS(3,3), D_PIS(3,4), D_PIS(3,5), D_PIS(3,6), D_PIS(3,7),
 D_PIS(4,0), D_PIS(4,1), D_PIS(4,2), D_PIS(4,3), D_PIS(4,4), D_PIS(4,5), D_PIS(4,6), D_PIS(4,7),
 D_PIS(5,0), D_PIS(5,1), D_PIS(5,2), D_PIS(5,3), D_PIS(5,4), D_PIS(5,5), D_PIS(5,6), D_PIS(5,7),
 D_PIS(6,0), D_PIS(6,1), D_PIS(6,2), D_PIS(6,3), D_PIS(6,4), D_PIS(6,5), D_PIS(6,6), D_PIS(6,7),
 D_PIS(7,0), D_PIS(7,1), D_PIS(7,2), D_PIS(7,3), D_PIS(7,4), D_PIS(7,5), D_PIS(7,6), D_PIS(7,7),
 D_PIS(8,0), D_PIS(8,1), D_PIS(8,2), D_PIS(8,3), D_PIS(8,4), D_PIS(8,5), D_PIS(8,6), D_PIS(8,7),
};

const size_t sizeof_listG = sizeof( sc7021gpio_list_names)/sizeof( *( sc7021gpio_list_names));
