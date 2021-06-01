/*
 * Sunplus Q645 pinmux controller driver.
 * Copyright (C) Sunplus Tech/Tibbo Tech. 2021
 * Author: Wells Lu <wells.lu@sunplus.com>
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

#include "sppctl.h"

// function: GPIO. list of groups (pins)
const unsigned sppctlpins_G[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,
	 8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79
};

#define P(x) PINCTRL_PIN(x,D_PIS(x))

const struct pinctrl_pin_desc sppctlpins_all[] = {
	P( 0), P( 1), P( 2), P( 3), P( 4), P( 5), P( 6), P( 7),
	P( 8), P( 9), P(10), P(11), P(12), P(13), P(14), P(15),
	P(16), P(17), P(18), P(19), P(20), P(21), P(22), P(23),
	P(24), P(25), P(26), P(27), P(28), P(29), P(30), P(31),
	P(32), P(33), P(34), P(35), P(36), P(37), P(38), P(39),
	P(40), P(41), P(42), P(43), P(44), P(45), P(46), P(47),
	P(48), P(49), P(50), P(51), P(52), P(53), P(54), P(55),
	P(56), P(57), P(58), P(59), P(60), P(61), P(62), P(63),
	P(64), P(65), P(66), P(67), P(68), P(69), P(70), P(71),
	P(72), P(73), P(74), P(75), P(76), P(77), P(78), P(79)
};
const size_t sppctlpins_allSZ = ARRAY_SIZE(sppctlpins_all);

// pmux groups: some pins are muxable. group = pin
const char * const sppctlpmux_list_s[] = {
	D_PIS(0)
};
// gpio: is defined in gpio_inf_q645.c
const size_t PMUX_listSZ = sizeof(sppctlpmux_list_s)/sizeof(*(sppctlpmux_list_s));

static const unsigned pins_spif1[] = {  6,  7,  8 , 9, 10, 11 };
static const unsigned pins_spif2[] = { 22, 23, 24, 25, 26, 27 };
static const sppctlgrp_t q645grps_spif[] = {
	EGRP("SPI_FLASH1", 1, pins_spif1),
	EGRP("SPI_FLASH2", 2, pins_spif2)
};

static const unsigned pins_emmc[] = { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };
static const sppctlgrp_t q645grps_emmc[] = {
	EGRP("CARD0_EMMC", 1, pins_emmc)
};

static const unsigned pins_snand1[] = { 22, 23, 24, 25, 26, 27 };
static const unsigned pins_snand2[] = { 6,   7,  8,  9, 10, 11 };
static const sppctlgrp_t q645grps_snand[] = {
	EGRP("SPI_NAND1", 1, pins_snand1),
	EGRP("SPI_NAND2", 2, pins_snand2)
};

static const unsigned pins_sdc30[] = { 28, 29, 30, 31, 32, 33 };
static const sppctlgrp_t q645grps_sdc30[] = {
	EGRP("SD_CARD", 1, pins_sdc30)
};

static const unsigned pins_sdio30[] = { 34, 35, 36, 37, 38, 39 };
static const sppctlgrp_t q645grps_sdio30[] = {
	EGRP("SDIO", 1, pins_sdio30)
};

static const unsigned pins_uart0[] = { 40, 41 };
static const sppctlgrp_t q645grps_uart0[] = {
	EGRP("UART0", 1, pins_uart0)
};

static const unsigned pins_uart1[] = { 42, 43, 44, 45 };
static const sppctlgrp_t q645grps_uart1[] = {
	EGRP("UART1", 1, pins_uart1)
};

static const unsigned pins_uart2[] = { 46, 47, 48, 49 };
static const sppctlgrp_t q645grps_uart2[] = {
	EGRP("UART2", 1, pins_uart2)
};

static const unsigned pins_uart3[] = { 50, 51, 52, 53 };
static const sppctlgrp_t q645grps_uart3[] = {
	EGRP("UART3", 1, pins_uart3)
};

static const unsigned pins_uart4[] = { 54, 55 };
static const sppctlgrp_t q645grps_uart4[] = {
	EGRP("UART4", 1, pins_uart4)
};

static const unsigned pins_uadbg[] = { 74, 73 };
static const sppctlgrp_t q645grps_uadbg[] = {
	EGRP("UADBG", 1, pins_uadbg)
};

static const unsigned pins_spicombo0[] = { 12, 13, 14, 15, 18, 19, 20, 21 };
static const sppctlgrp_t q645grps_spicombo0[] = {
	EGRP("SPI_COMBO0", 1, pins_spicombo0)
};

static const unsigned pins_u2otg[] = { 61 };
static const sppctlgrp_t q645grps_u2otg[] = {
	EGRP("USB2_OTG", 1, pins_u2otg)
};

static const unsigned pins_i2cm0[] = { 59, 60 };
static const sppctlgrp_t q645grps_i2cm0[] = {
	EGRP("I2C_MASTER0", 1, pins_i2cm0)
};

static const unsigned pins_wakeup0[] = { 72 };
static const sppctlgrp_t q645grps_wakeup0[] = {
	EGRP("WAKEUP0", 1, pins_wakeup0)
};

static const unsigned pins_wakeup1[] = { 73 };
static const sppctlgrp_t q645grps_wakeup1[] = {
	EGRP("WAKEUP1", 1, pins_wakeup1)
};

static const unsigned pins_int_x1[] = { 78 };
static const unsigned pins_int_x2[] = { 79 };
static const unsigned pins_int_x3[] = { 52 };
static const unsigned pins_int_x4[] = { 53 };
static const unsigned pins_int_x5[] = { 49 };
static const unsigned pins_int_x6[] = { 59 };
static const unsigned pins_int_x7[] = { 60 };
static const sppctlgrp_t q645grps_int0[] = {
	EGRP("INT0_X1", 1, pins_int_x1),
	EGRP("INT0_X2", 1, pins_int_x2),
	EGRP("INT0_X3", 1, pins_int_x3),
	EGRP("INT0_X4", 1, pins_int_x4),
	EGRP("INT0_X5", 1, pins_int_x5),
	EGRP("INT0_X6", 1, pins_int_x6),
	EGRP("INT0_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int1[] = {
	EGRP("INT1_X1", 1, pins_int_x1),
	EGRP("INT1_X2", 1, pins_int_x2),
	EGRP("INT1_X3", 1, pins_int_x3),
	EGRP("INT1_X4", 1, pins_int_x4),
	EGRP("INT1_X5", 1, pins_int_x5),
	EGRP("INT1_X6", 1, pins_int_x6),
	EGRP("INT1_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int2[] = {
	EGRP("INT2_X1", 1, pins_int_x1),
	EGRP("INT2_X2", 1, pins_int_x2),
	EGRP("INT2_X3", 1, pins_int_x3),
	EGRP("INT2_X4", 1, pins_int_x4),
	EGRP("INT2_X5", 1, pins_int_x5),
	EGRP("INT2_X6", 1, pins_int_x6),
	EGRP("INT2_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int3[] = {
	EGRP("INT3_X1", 1, pins_int_x1),
	EGRP("INT3_X2", 1, pins_int_x2),
	EGRP("INT3_X3", 1, pins_int_x3),
	EGRP("INT3_X4", 1, pins_int_x4),
	EGRP("INT3_X5", 1, pins_int_x5),
	EGRP("INT3_X6", 1, pins_int_x6),
	EGRP("INT3_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int4[] = {
	EGRP("INT4_X1", 1, pins_int_x1),
	EGRP("INT4_X2", 1, pins_int_x2),
	EGRP("INT4_X3", 1, pins_int_x3),
	EGRP("INT4_X4", 1, pins_int_x4),
	EGRP("INT4_X5", 1, pins_int_x5),
	EGRP("INT4_X6", 1, pins_int_x6),
	EGRP("INT4_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int5[] = {
	EGRP("INT5_X1", 1, pins_int_x1),
	EGRP("INT5_X2", 1, pins_int_x2),
	EGRP("INT5_X3", 1, pins_int_x3),
	EGRP("INT5_X4", 1, pins_int_x4),
	EGRP("INT5_X5", 1, pins_int_x5),
	EGRP("INT5_X6", 1, pins_int_x6),
	EGRP("INT5_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int6[] = {
	EGRP("INT6_X1", 1, pins_int_x1),
	EGRP("INT6_X2", 1, pins_int_x2),
	EGRP("INT6_X3", 1, pins_int_x3),
	EGRP("INT6_X4", 1, pins_int_x4),
	EGRP("INT6_X5", 1, pins_int_x5),
	EGRP("INT6_X6", 1, pins_int_x6),
	EGRP("INT6_X7", 1, pins_int_x7)
};
static const sppctlgrp_t q645grps_int7[] = {
	EGRP("INT7_X1", 1, pins_int_x1),
	EGRP("INT7_X2", 1, pins_int_x2),
	EGRP("INT7_X3", 1, pins_int_x3),
	EGRP("INT7_X4", 1, pins_int_x4),
	EGRP("INT7_X5", 1, pins_int_x5),
	EGRP("INT7_X6", 1, pins_int_x6),
	EGRP("INT7_X7", 1, pins_int_x7)
};

func_t list_funcs[] = {
	FNCN("GPIO",            fOFF_0, 0, 0, 0),
	FNCN("IOP",             fOFF_0, 0, 0, 0),

	FNCE("SPI_FLASH",       fOFF_G, 1, 0, 2, q645grps_spif),
	FNCE("CARD0_EMMC",      fOFF_G, 1, 2, 1, q645grps_emmc),
	FNCE("SPI_NAND",        fOFF_G, 1, 3, 2, q645grps_snand),
	FNCE("SD_CARD",         fOFF_G, 1, 5, 1, q645grps_sdc30),
	FNCE("SDIO",            fOFF_G, 1, 6, 1, q645grps_sdio30),
	FNCE("UART0",           fOFF_G, 1, 7, 1, q645grps_uart0),
	FNCE("UART1",           fOFF_G, 1, 8, 1, q645grps_uart1),
	FNCE("UART2",           fOFF_G, 1, 9, 1, q645grps_uart2),
	FNCE("UART3",           fOFF_G, 1,10, 1, q645grps_uart3),
	FNCE("UART4",           fOFF_G, 32*3+23, 14, 1, q645grps_uart4),
	FNCE("UADBG",           fOFF_G, 1,11, 1, q645grps_uadbg),
	FNCE("SPI_COMBO0",      fOFF_G, 1,12, 1, q645grps_spicombo0),
	FNCE("I2C_MASTER0",     fOFF_G, 1,13, 1, q645grps_i2cm0),

	FNCE("INT0",            fOFF_G, 2, 2, 3, q645grps_int0),
	FNCE("INT1",            fOFF_G, 2, 5, 3, q645grps_int1),
	FNCE("INT2",            fOFF_G, 2, 8, 3, q645grps_int2),
	FNCE("INT3",            fOFF_G, 2,11, 3, q645grps_int3),
	FNCE("INT4",            fOFF_G, 3, 0, 3, q645grps_int4),
	FNCE("INT5",            fOFF_G, 3, 3, 3, q645grps_int5),
	FNCE("INT6",            fOFF_G, 3, 6, 3, q645grps_int6),
	FNCE("INT7",            fOFF_G, 3, 7, 3, q645grps_int7),

	FNCE("USB2_OTG",        fOFF_G, 2,15, 1, q645grps_u2otg),    // TO BE CONFIRMED
	FNCE("WAKEUP0",         fOFF_G, 2,15, 1, q645grps_wakeup0),  // TO BE CONFIRMED
	FNCE("WAKEUP1",         fOFF_G, 2,15, 1, q645grps_wakeup1)   // TO BE CONFIRMED
};

const size_t list_funcsSZ = ARRAY_SIZE(list_funcs);
