// SPDX-License-Identifier: GPL-2.0
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

#define P(x) PINCTRL_PIN(x, D_PIS(x))

// function: GPIO. list of groups (pins)
const unsigned int sppctlpins_G[] = {
	  0,   1,   2,   3,   4,   5,   6,   7,
	  8,   9,  10,  11,  12,  13,  14,  15,
	 16,  17,  18,  19,  20,  21,  22,  23,
	 24,  25,  26,  27,  28,  29,  30,  31,
	 32,  33,  34,  35,  36,  37,  38,  39,
	 40,  41,  42,  43,  44,  45,  46,  47,
	 48,  49,  50,  51,  52,  53,  54,  55,
	 56,  57,  58,  59,  60,  61,  62,  63,
	 64,  65,  66,  67,  68,  69,  70,  71,
	 72,  73,  74,  75,  76,  77,  78,  79,
	 80,  81,  82,  83,  84,  85,  86,  87,
	 88,  89,  90,  91,  92,  93,  94,  95,
	 96,  97,  98,  99, 100, 101, 102, 103,
	104, 105, 106, 107
};

const struct pinctrl_pin_desc sppctlpins_all[] = {
	P(0),   P(1),   P(2),   P(3),   P(4),   P(5),   P(6),   P(7),
	P(8),   P(9),   P(10),  P(11),  P(12),  P(13),  P(14),  P(15),
	P(16),  P(17),  P(18),  P(19),  P(20),  P(21),  P(22),  P(23),
	P(24),  P(25),  P(26),  P(27),  P(28),  P(29),  P(30),  P(31),
	P(32),  P(33),  P(34),  P(35),  P(36),  P(37),  P(38),  P(39),
	P(40),  P(41),  P(42),  P(43),  P(44),  P(45),  P(46),  P(47),
	P(48),  P(49),  P(50),  P(51),  P(52),  P(53),  P(54),  P(55),
	P(56),  P(57),  P(58),  P(59),  P(60),  P(61),  P(62),  P(63),
	P(64),  P(65),  P(66),  P(67),  P(68),  P(69),  P(70),  P(71),
	P(72),  P(73),  P(74),  P(75),  P(76),  P(77),  P(78),  P(79),
	P(80),  P(81),  P(82),  P(83),  P(84),  P(85),  P(86),  P(87),
	P(88),  P(89),  P(90),  P(91),  P(92),  P(93),  P(94),  P(95),
	P(96),  P(97),  P(98),  P(99),  P(100), P(101), P(102), P(103),
	P(104), P(105), P(106), P(107)
};
const size_t sppctlpins_allSZ = ARRAY_SIZE(sppctlpins_all);

// pmux groups: some pins are muxable. group = pin
const char * const sppctlpmux_list_s[] = {
	D_PIS(0)
};
// gpio: is defined in gpio_inf_q645.c
const size_t PMUX_listSZ = sizeof(sppctlpmux_list_s)/sizeof(*(sppctlpmux_list_s));

static const unsigned int pins_spif[] = { 6, 7, 8, 9, 10, 11 };
static const struct sppctlgrp_t q645grps_spif[] = {
	EGRP("SPI_FLASH", 1, pins_spif),
};

static const unsigned int pins_emmc[] = { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };
static const struct sppctlgrp_t q645grps_emmc[] = {
	EGRP("CARD0_EMMC", 1, pins_emmc)
};

static const unsigned int pins_snand1[] = { 16, 17, 18, 19, 20, 21 };
static const unsigned int pins_snand2[] = { 6,   7,  8,  9, 10, 11 };
static const struct sppctlgrp_t q645grps_snand[] = {
	EGRP("SPI_NAND1", 1, pins_snand1),
	EGRP("SPI_NAND2", 2, pins_snand2)
};

static const unsigned int pins_sdc30[] = { 28, 29, 30, 31, 32, 33 };
static const struct sppctlgrp_t q645grps_sdc30[] = {
	EGRP("SD_CARD", 1, pins_sdc30)
};

static const unsigned int pins_sdio30[] = { 34, 35, 36, 37, 38, 39 };
static const struct sppctlgrp_t q645grps_sdio30[] = {
	EGRP("SDIO", 1, pins_sdio30)
};

static const unsigned int pins_uart0[] = { 22, 23 };
static const struct sppctlgrp_t q645grps_uart0[] = {
	EGRP("UART0", 1, pins_uart0)
};

static const unsigned int pins_uart1[] = { 24, 25, 26, 27 };
static const struct sppctlgrp_t q645grps_uart1[] = {
	EGRP("UART1", 1, pins_uart1)
};

static const unsigned int pins_uart2[] = { 40, 41, 42, 43 };
static const struct sppctlgrp_t q645grps_uart2[] = {
	EGRP("UART2", 1, pins_uart2)
};

static const unsigned int pins_uart3[] = { 44, 45 };
static const struct sppctlgrp_t q645grps_uart3[] = {
	EGRP("UART3", 1, pins_uart3)
};

static const unsigned int pins_uart4[] = { 101, 102 };
static const struct sppctlgrp_t q645grps_uart4[] = {
	EGRP("UART4", 1, pins_uart4)
};

static const unsigned int pins_uadbg[] = { 46, 47 };
static const struct sppctlgrp_t q645grps_uadbg[] = {
	EGRP("UADBG", 1, pins_uadbg)
};

static const unsigned int pins_uart6[] = { 48, 49 };
static const struct sppctlgrp_t q645grps_uart6[] = {
	EGRP("UART6", 1, pins_uart6)
};

static const unsigned int pins_uart7[] = { 50, 51 };
static const struct sppctlgrp_t q645grps_uart7[] = {
	EGRP("UART7", 1, pins_uart7)
};

static const unsigned int pins_uart8[] = { 52, 53 };
static const struct sppctlgrp_t q645grps_uart8[] = {
	EGRP("UART8", 1, pins_uart8)
};

static const unsigned int pins_spicombo0[] = { 54, 55, 56, 57 };
static const struct sppctlgrp_t q645grps_spimaster0[] = {
	EGRP("SPI_MASTER0", 1, pins_spicombo0)
};
static const struct sppctlgrp_t q645grps_spislave0[] = {
	EGRP("SPI_SLAVE0", 1, pins_spicombo0)
};

static const unsigned int pins_spicombo1[] = { 58, 59, 60, 61 };
static const struct sppctlgrp_t q645grps_spimaster1[] = {
	EGRP("SPI_MASTER1", 1, pins_spicombo1)
};
static const struct sppctlgrp_t q645grps_spislave1[] = {
	EGRP("SPI_SLAVE1", 1, pins_spicombo1)
};

static const unsigned int pins_spicombo2[] = { 63, 64, 65, 66 };
static const struct sppctlgrp_t q645grps_spimaster2[] = {
	EGRP("SPI_MASTER2", 1, pins_spicombo2)
};
static const struct sppctlgrp_t q645grps_spislave2[] = {
	EGRP("SPI_SLAVE2", 1, pins_spicombo2)
};

static const unsigned int pins_spicombo3[] = { 67, 68, 69, 70 };
static const struct sppctlgrp_t q645grps_spimaster3[] = {
	EGRP("SPI_MASTER3", 1, pins_spicombo3)
};
static const struct sppctlgrp_t q645grps_spislave3[] = {
	EGRP("SPI_SLAVE3", 1, pins_spicombo3)
};

static const unsigned int pins_spicombo4[] = { 71, 72, 73, 74 };
static const struct sppctlgrp_t q645grps_spimaster4[] = {
	EGRP("SPI_MASTER4", 1, pins_spicombo4)
};
static const struct sppctlgrp_t q645grps_spislave4[] = {
	EGRP("SPI_SLAVE4", 1, pins_spicombo4)
};

static const unsigned int pins_spicombo5[] = { 77, 78, 79, 80 };
static const struct sppctlgrp_t q645grps_spimaster5[] = {
	EGRP("SPI_MASTER5", 1, pins_spicombo5)
};
static const struct sppctlgrp_t q645grps_spislave5[] = {
	EGRP("SPI_SLAVE5", 1, pins_spicombo5)
};

static const unsigned int pins_i2cm0[] = { 75, 76 };
static const struct sppctlgrp_t q645grps_i2cm0[] = {
	EGRP("I2C_MASTER0", 1, pins_i2cm0)
};

static const unsigned int pins_i2cm1[] = { 81, 82 };
static const struct sppctlgrp_t q645grps_i2cm1[] = {
	EGRP("I2C_MASTER1", 1, pins_i2cm1)
};

static const unsigned int pins_i2cm2[] = { 83, 84 };
static const struct sppctlgrp_t q645grps_i2cm2[] = {
	EGRP("I2C_MASTER2", 1, pins_i2cm2)
};

static const unsigned int pins_i2cm3[] = { 85, 86 };
static const struct sppctlgrp_t q645grps_i2cm3[] = {
	EGRP("I2C_MASTER3", 1, pins_i2cm3)
};

static const unsigned int pins_i2cm4[] = { 87, 88 };
static const struct sppctlgrp_t q645grps_i2cm4[] = {
	EGRP("I2C_MASTER4", 1, pins_i2cm4)
};

static const unsigned int pins_i2cm5[] = { 89, 90 };
static const struct sppctlgrp_t q645grps_i2cm5[] = {
	EGRP("I2C_MASTER5", 1, pins_i2cm5)
};

static const unsigned int pins_pwm[] = { 58, 59, 60, 61 };
static const struct sppctlgrp_t q645grps_pwm[] = {
	EGRP("PWM", 1, pins_pwm)
};

static const unsigned int pins_aud_dac_clk[] = { 62, 63 };
static const struct sppctlgrp_t q645grps_aud_dac_clk[] = {
	EGRP("AUD_DAC_CLK", 1, pins_aud_dac_clk)
};

static const unsigned int pins_aud_tdmtx_xck[] = { 62 };
static const struct sppctlgrp_t q645grps_aud_tdmtx_xck[] = {
	EGRP("AUD_TDMTX_XCK", 1, pins_aud_tdmtx_xck)
};

static const unsigned int pins_aud_au2_data0[] = { 5 };
static const struct sppctlgrp_t q645grps_aud_au2_data0[] = {
	EGRP("AUD_AU2_DATA0", 1, pins_aud_au2_data0)
};

static const unsigned int pins_aud_au1_data0[] = { 4 };
static const struct sppctlgrp_t q645grps_aud_au1_data0[] = {
	EGRP("AUD_AU1_DATA0", 1, pins_aud_au1_data0)
};

static const unsigned int pins_aud_au2_ck[] = { 96, 97 };
static const struct sppctlgrp_t q645grps_aud_au2_ck[] = {
	EGRP("AUD_AU2_CK", 1, pins_aud_au2_ck)
};

static const unsigned int pins_aud_au1_ck[] = { 94, 95 };
static const struct sppctlgrp_t q645grps_aud_au1_ck[] = {
	EGRP("AUD_AU1_CK", 1, pins_aud_au1_ck)
};

static const unsigned int pins_aud_au_adc_data0[] = { 3, 64, 92, 93 };
static const struct sppctlgrp_t q645grps_aud_au_adc_data0[] = {
	EGRP("AUD_AU_ADC_DATA0", 1, pins_aud_au_adc_data0)
};

static const unsigned int pins_aud_adc2_data0[] = { 5 };
static const struct sppctlgrp_t q645grps_aud_adc2_data0[] = {
	EGRP("AUD_ADC2_DATA0", 1, pins_aud_adc2_data0)
};

static const unsigned int pins_aud_adc1_data0[] = { 4 };
static const struct sppctlgrp_t q645grps_aud_adc1_data0[] = {
	EGRP("AUD_ADC1_DATA0", 1, pins_aud_adc1_data0)
};

static const unsigned int pins_aud_aud_tdm[] = { 3, 64, 92, 93 };
static const struct sppctlgrp_t q645grps_aud_aud_tdm[] = {
	EGRP("AUD_AUD_TDM", 1, pins_aud_aud_tdm)
};

static const unsigned int pins_spdif1[] = { 91 };
static const unsigned int pins_spdif2[] = { 3  };
static const unsigned int pins_spdif3[] = { 4  };
static const unsigned int pins_spdif4[] = { 5  };
static const unsigned int pins_spdif5[] = { 62 };
static const unsigned int pins_spdif6[] = { 2  };
static const struct sppctlgrp_t q645grps_spdif_in[] = {
	EGRP("SPDIF_IN_X1", 1, pins_spdif1),
	EGRP("SPDIF_IN_X2", 2, pins_spdif2),
	EGRP("SPDIF_IN_X3", 3, pins_spdif3),
	EGRP("SPDIF_IN_X4", 4, pins_spdif4),
	EGRP("SPDIF_IN_X5", 5, pins_spdif5),
	EGRP("SPDIF_IN_X6", 6, pins_spdif6)
};
static const struct sppctlgrp_t q645grps_spdif_out[] = {
	EGRP("SPDIF_OUT_X1", 1, pins_spdif1),
	EGRP("SPDIF_OUT_X2", 2, pins_spdif2),
	EGRP("SPDIF_OUT_X3", 3, pins_spdif3),
	EGRP("SPDIF_OUT_X4", 4, pins_spdif4),
	EGRP("SPDIF_OUT_X5", 5, pins_spdif5),
	EGRP("SPDIF_OUT_X6", 6, pins_spdif6)
};

static const unsigned int pins_int_x1[] = { 0 };
static const unsigned int pins_int_x2[] = { 1 };
static const unsigned int pins_int_x3[] = { 2 };
static const unsigned int pins_int_x4[] = { 3 };
static const unsigned int pins_int_x5[] = { 46 };
static const unsigned int pins_int_x6[] = { 106 };
static const unsigned int pins_int_x7[] = { 107 };
static const struct sppctlgrp_t q645grps_int0[] = {
	EGRP("INT0_X1", 1, pins_int_x1),
	EGRP("INT0_X2", 1, pins_int_x2),
	EGRP("INT0_X3", 1, pins_int_x3),
	EGRP("INT0_X4", 1, pins_int_x4),
	EGRP("INT0_X5", 1, pins_int_x5),
	EGRP("INT0_X6", 1, pins_int_x6),
	EGRP("INT0_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int1[] = {
	EGRP("INT1_X1", 1, pins_int_x1),
	EGRP("INT1_X2", 1, pins_int_x2),
	EGRP("INT1_X3", 1, pins_int_x3),
	EGRP("INT1_X4", 1, pins_int_x4),
	EGRP("INT1_X5", 1, pins_int_x5),
	EGRP("INT1_X6", 1, pins_int_x6),
	EGRP("INT1_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int2[] = {
	EGRP("INT2_X1", 1, pins_int_x1),
	EGRP("INT2_X2", 1, pins_int_x2),
	EGRP("INT2_X3", 1, pins_int_x3),
	EGRP("INT2_X4", 1, pins_int_x4),
	EGRP("INT2_X5", 1, pins_int_x5),
	EGRP("INT2_X6", 1, pins_int_x6),
	EGRP("INT2_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int3[] = {
	EGRP("INT3_X1", 1, pins_int_x1),
	EGRP("INT3_X2", 1, pins_int_x2),
	EGRP("INT3_X3", 1, pins_int_x3),
	EGRP("INT3_X4", 1, pins_int_x4),
	EGRP("INT3_X5", 1, pins_int_x5),
	EGRP("INT3_X6", 1, pins_int_x6),
	EGRP("INT3_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int4[] = {
	EGRP("INT4_X1", 1, pins_int_x1),
	EGRP("INT4_X2", 1, pins_int_x2),
	EGRP("INT4_X3", 1, pins_int_x3),
	EGRP("INT4_X4", 1, pins_int_x4),
	EGRP("INT4_X5", 1, pins_int_x5),
	EGRP("INT4_X6", 1, pins_int_x6),
	EGRP("INT4_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int5[] = {
	EGRP("INT5_X1", 1, pins_int_x1),
	EGRP("INT5_X2", 1, pins_int_x2),
	EGRP("INT5_X3", 1, pins_int_x3),
	EGRP("INT5_X4", 1, pins_int_x4),
	EGRP("INT5_X5", 1, pins_int_x5),
	EGRP("INT5_X6", 1, pins_int_x6),
	EGRP("INT5_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int6[] = {
	EGRP("INT6_X1", 1, pins_int_x1),
	EGRP("INT6_X2", 1, pins_int_x2),
	EGRP("INT6_X3", 1, pins_int_x3),
	EGRP("INT6_X4", 1, pins_int_x4),
	EGRP("INT6_X5", 1, pins_int_x5),
	EGRP("INT6_X6", 1, pins_int_x6),
	EGRP("INT6_X7", 1, pins_int_x7)
};
static const struct sppctlgrp_t q645grps_int7[] = {
	EGRP("INT7_X1", 1, pins_int_x1),
	EGRP("INT7_X2", 1, pins_int_x2),
	EGRP("INT7_X3", 1, pins_int_x3),
	EGRP("INT7_X4", 1, pins_int_x4),
	EGRP("INT7_X5", 1, pins_int_x5),
	EGRP("INT7_X6", 1, pins_int_x6),
	EGRP("INT7_X7", 1, pins_int_x7)
};

struct func_t list_funcs[] = {
	FNCN("GPIO",            fOFF_0, 0,  0, 0),
	FNCN("IOP",             fOFF_0, 0,  0, 0),

	FNCE("SPI_FLASH",       fOFF_G, 1,  0, 1, q645grps_spif),
	FNCE("UART4",           fOFF_G, 1,  1, 1, q645grps_uart4),
	FNCE("PWM",             fOFF_G, 1,  2, 1, q645grps_pwm),
	FNCE("CARD0_EMMC",      fOFF_G, 1,  3, 1, q645grps_emmc),
	FNCE("SPI_NAND",        fOFF_G, 1,  4, 2, q645grps_snand),
	FNCE("SD_CARD",         fOFF_G, 1,  6, 1, q645grps_sdc30),
	FNCE("SDIO",            fOFF_G, 1,  7, 1, q645grps_sdio30),
	FNCE("UART0",           fOFF_G, 1,  8, 1, q645grps_uart0),
	FNCE("UART1",           fOFF_G, 1,  9, 1, q645grps_uart1),
	FNCE("UART2",           fOFF_G, 1, 10, 1, q645grps_uart2),
	FNCE("UART3",           fOFF_G, 1, 11, 1, q645grps_uart3),
	FNCE("UADBG",           fOFF_G, 1, 12, 1, q645grps_uadbg),
	FNCE("UART6",           fOFF_G, 1, 13, 1, q645grps_uart6),
	FNCE("UART7",           fOFF_G, 1, 14, 1, q645grps_uart7),
	FNCE("UART8",           fOFF_G, 1, 15, 1, q645grps_uart8),

	FNCE("SPI_MASTER0",     fOFF_G, 2,  0, 1, q645grps_spimaster0),
	FNCE("SPI_SLAVE0",      fOFF_G, 2,  1, 1, q645grps_spislave0),
	FNCE("SPI_MASTER1",     fOFF_G, 2,  6, 1, q645grps_spimaster1),
	FNCE("SPI_SLAVE1",      fOFF_G, 2,  7, 1, q645grps_spislave1),
	FNCE("SPI_MASTER2",     fOFF_G, 2,  8, 1, q645grps_spimaster2),
	FNCE("SPI_SLAVE2",      fOFF_G, 2,  9, 1, q645grps_spislave2),
	FNCE("SPI_MASTER3",     fOFF_G, 2, 10, 1, q645grps_spimaster3),
	FNCE("SPI_SLAVE3",      fOFF_G, 2, 11, 1, q645grps_spislave3),
	FNCE("SPI_MASTER4",     fOFF_G, 2, 12, 1, q645grps_spimaster4),
	FNCE("SPI_SLAVE4",      fOFF_G, 2, 13, 1, q645grps_spislave4),
	FNCE("SPI_MASTER5",     fOFF_G, 2, 14, 1, q645grps_spimaster5),
	FNCE("SPI_SLAVE5",      fOFF_G, 2, 15, 1, q645grps_spislave5),

	FNCE("I2C_MASTER0",     fOFF_G, 3,  0, 1, q645grps_i2cm0),
	FNCE("I2C_MASTER1",     fOFF_G, 3,  1, 1, q645grps_i2cm1),
	FNCE("I2C_MASTER2",     fOFF_G, 3,  2, 1, q645grps_i2cm2),
	FNCE("I2C_MASTER3",     fOFF_G, 3,  3, 1, q645grps_i2cm3),
	FNCE("I2C_MASTER4",     fOFF_G, 3,  4, 1, q645grps_i2cm4),
	FNCE("I2C_MASTER5",     fOFF_G, 3,  5, 1, q645grps_i2cm5),
	FNCE("AUD_TDMTX_XCK",   fOFF_G, 3,  6, 1, q645grps_aud_tdmtx_xck),
	FNCE("AUD_DAC_CLK",     fOFF_G, 3,  7, 1, q645grps_aud_dac_clk),
	FNCE("AUD_AU2_DATA0",   fOFF_G, 3,  8, 1, q645grps_aud_au2_data0),
	FNCE("AUD_AU1_DATA0",   fOFF_G, 3,  9, 1, q645grps_aud_au1_data0),
	FNCE("AUD_AU2_CK",      fOFF_G, 3, 10, 1, q645grps_aud_au2_ck),
	FNCE("AUD_AU1_CK",      fOFF_G, 3, 11, 1, q645grps_aud_au1_ck),
	FNCE("AUD_AU_ADC_DATA0", fOFF_G, 3, 12, 1, q645grps_aud_au_adc_data0),
	FNCE("AUD_ADC2_DATA0",  fOFF_G, 3, 13, 1, q645grps_aud_adc2_data0),
	FNCE("AUD_ADC1_DATA0",  fOFF_G, 3, 14, 1, q645grps_aud_adc1_data0),
	FNCE("AUD_AUD_TDM",     fOFF_G, 3, 15, 1, q645grps_aud_aud_tdm),

	FNCE("SPDIF_IN",        fOFF_G, 4,  0, 3, q645grps_spdif_in),
	FNCE("SPDIF_OUT",       fOFF_G, 4,  3, 3, q645grps_spdif_out),
	FNCE("INT0",            fOFF_G, 4, 10, 3, q645grps_int0),
	FNCE("INT1",            fOFF_G, 4, 13, 3, q645grps_int1),

	FNCE("INT2",            fOFF_G, 5,  0, 3, q645grps_int2),
	FNCE("INT3",            fOFF_G, 5,  3, 3, q645grps_int3),
	FNCE("INT4",            fOFF_G, 5,  6, 3, q645grps_int4),
	FNCE("INT5",            fOFF_G, 5,  9, 3, q645grps_int5),
	FNCE("INT6",            fOFF_G, 5, 12, 3, q645grps_int6),

	FNCE("INT7",            fOFF_G, 6,  0, 3, q645grps_int7)
};

const size_t list_funcsSZ = ARRAY_SIZE(list_funcs);
