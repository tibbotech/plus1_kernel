// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 Pin Controller Driver
 * Copyright (C) Sunplus Technology
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
	104, 105,
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
	P(104), P(105),
};
const size_t sppctlpins_allSZ = ARRAY_SIZE(sppctlpins_all);

// pmux groups: some pins are muxable. group = pin
const char * const sppctlpmux_list_s[] = {
	D_PIS(0),
};
// gpio: is defined in gpio_inf_sp7350.c
const size_t PMUX_listSZ = sizeof(sppctlpmux_list_s)/sizeof(*(sppctlpmux_list_s));

static const unsigned int pins_spif[] = { 21, 22, 23, 24, 25, 26 };
static const struct sppctlgrp_t sp7350grps_spif[] = {
	EGRP("SPI_FLASH", 1, pins_spif),
};

static const unsigned int pins_emmc[] = { 20, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37 };
static const struct sppctlgrp_t sp7350grps_emmc[] = {
	EGRP("EMMC", 1, pins_emmc),
};

static const unsigned int pins_spi_nand_x1[] = { 30, 31, 32, 33, 34, 35 };
static const unsigned int pins_spi_nand_x2[] = { 21, 22, 23, 24, 25, 26 };
static const struct sppctlgrp_t sp7350grps_spi_nand[] = {
	EGRP("SPI_NAND_X1", 1, pins_spi_nand_x1),
	EGRP("SPI_NAND_X2", 2, pins_spi_nand_x2),
};

static const unsigned int pins_sdc30[] = { 38, 39, 40, 41, 42, 43 };
static const struct sppctlgrp_t sp7350grps_sdc30[] = {
	EGRP("SD_CARD", 1, pins_sdc30),
};

static const unsigned int pins_sdio30[] = { 44, 45, 46, 47, 48, 49 };
static const struct sppctlgrp_t sp7350grps_sdio30[] = {
	EGRP("SDIO", 1, pins_sdio30),
};

static const unsigned int pins_para_nand[] = { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36 };
static const struct sppctlgrp_t sp7350grps_para_nand[] = {
	EGRP("PARA_NAND", 1, pins_para_nand),
};

static const unsigned int pins_usb_otg[] = { 18, 19 };
static const struct sppctlgrp_t sp7350grps_usb_otg[] = {
	EGRP("USB_OTG", 1, pins_usb_otg),
};

static const unsigned int pins_gmac_rgmii[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
static const unsigned int pins_gmac_rmii[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };
static const struct sppctlgrp_t sp7350grps_gmac[] = {
	EGRP("GMAC_RGMII", 1, pins_gmac_rgmii),
	EGRP("GMAC_RMII", 2, pins_gmac_rmii),
};

static const unsigned int pins_pwm0_x1[] = { 78 };
static const unsigned int pins_pwm0_x2[] = { 58 };
static const struct sppctlgrp_t sp7350grps_pwm0[] = {
	EGRP("PWM0_X1", 1, pins_pwm0_x1),
	EGRP("PWM0_X2", 2, pins_pwm0_x2),
};

static const unsigned int pins_pwm1_x1[] = { 79 };
static const unsigned int pins_pwm1_x2[] = { 59 };
static const struct sppctlgrp_t sp7350grps_pwm1[] = {
	EGRP("PWM1_X1", 1, pins_pwm1_x1),
	EGRP("PWM1_X2", 2, pins_pwm1_x2),
};

static const unsigned int pins_pwm2_x1[] = { 60 };
static const unsigned int pins_pwm2_x2[] = { 92 };
static const struct sppctlgrp_t sp7350grps_pwm2[] = {
	EGRP("PWM2_X1", 1, pins_pwm2_x1),
	EGRP("PWM2_X2", 2, pins_pwm2_x2),
};

static const unsigned int pins_pwm3_x1[] = { 61 };
static const unsigned int pins_pwm3_x2[] = { 93 };
static const struct sppctlgrp_t sp7350grps_pwm3[] = {
	EGRP("PWM3_X1", 1, pins_pwm3_x1),
	EGRP("PWM3_X2", 2, pins_pwm3_x2),
};

static const unsigned int pins_uart0_x1[] = { 50, 51 };
static const unsigned int pins_uart0_x2[] = { 68, 69 };
static const struct sppctlgrp_t sp7350grps_uart0[] = {
	EGRP("UART0_X1", 1, pins_uart0_x1),
	EGRP("UART0_X2", 2, pins_uart0_x2),
};

static const unsigned int pins_uart1_x1[] = { 52, 53 };
static const unsigned int pins_uart1_x2[] = { 64, 65 };
static const struct sppctlgrp_t sp7350grps_uart1[] = {
	EGRP("UART1_X1", 1, pins_uart1_x1),
	EGRP("UART1_X2", 2, pins_uart1_x2),
};

static const unsigned int pins_uart1_fc_x1[] = { 54, 55 };
static const unsigned int pins_uart1_fc_x2[] = { 66, 67 };
static const struct sppctlgrp_t sp7350grps_uart1_fc[] = {
	EGRP("UART1_FC_X1", 1, pins_uart1_fc_x1),
	EGRP("UART1_FC_X2", 2, pins_uart1_fc_x2),
};

static const unsigned int pins_uart2_x1[] = { 56, 57 };
static const unsigned int pins_uart2_x2[] = { 76, 77 };
static const struct sppctlgrp_t sp7350grps_uart2[] = {
	EGRP("UART2_X1", 1, pins_uart2_x1),
	EGRP("UART2_X2", 2, pins_uart2_x2),
};

static const unsigned int pins_uart2_fc_x1[] = { 58, 59 };
static const unsigned int pins_uart2_fc_x2[] = { 78, 79 };
static const struct sppctlgrp_t sp7350grps_uart2_fc[] = {
	EGRP("UART2_FC_X1", 1, pins_uart2_fc_x1),
	EGRP("UART2_FC_X2", 2, pins_uart2_fc_x2),
};

static const unsigned int pins_uart3_x1[] = { 62, 63 };
static const unsigned int pins_uart3_x2[] = { 7, 8 };
static const struct sppctlgrp_t sp7350grps_uart3[] = {
	EGRP("UART3_X1", 1, pins_uart3_x1),
	EGRP("UART3_X2", 2, pins_uart3_x2),
};

static const unsigned int pins_uadbg[] = { 13, 14 };
static const struct sppctlgrp_t sp7350grps_uadbg[] = {
	EGRP("UADBG", 1, pins_uadbg),
};

static const unsigned int pins_uart6_x1[] = { 80, 81 };
static const unsigned int pins_uart6_x2[] = { 48, 49 };
static const struct sppctlgrp_t sp7350grps_uart6[] = {
	EGRP("UART6_X1", 1, pins_uart6_x1),
	EGRP("UART6_X2", 2, pins_uart6_x2),
};

static const unsigned int pins_uart7[] = { 82, 83 };
static const struct sppctlgrp_t sp7350grps_uart7[] = {
	EGRP("UART7", 1, pins_uart7),
};

static const unsigned int pins_i2c_combo0_x1[] = { 68, 69 };
static const unsigned int pins_i2c_combo0_x2[] = { 54, 55 };
static const struct sppctlgrp_t sp7350grps_i2c_combo0[] = {
	EGRP("I2C_COMBO0_X1", 1, pins_i2c_combo0_x1),
	EGRP("I2C_COMBO0_X2", 2, pins_i2c_combo0_x2),
};

static const unsigned int pins_i2c_combo1[] = { 70, 71 };
static const struct sppctlgrp_t sp7350grps_i2c_combo1[] = {
	EGRP("I2C_COMBO1", 1, pins_i2c_combo1),
};

static const unsigned int pins_i2c_combo2_x1[] = { 76, 77 };
static const unsigned int pins_i2c_combo2_x2[] = { 56, 57 };
static const struct sppctlgrp_t sp7350grps_i2c_combo2[] = {
	EGRP("I2C_COMBO2_X1", 1, pins_i2c_combo2_x1),
	EGRP("I2C_COMBO2_X2", 2, pins_i2c_combo2_x2),
};

static const unsigned int pins_i2c_combo3[] = { 88, 89 };
static const struct sppctlgrp_t sp7350grps_i2c_combo3[] = {
	EGRP("I2C_COMBO3", 1, pins_i2c_combo3),
};

static const unsigned int pins_i2c_combo4[] = { 90, 91 };
static const struct sppctlgrp_t sp7350grps_i2c_combo4[] = {
	EGRP("I2C_COMBO4", 1, pins_i2c_combo4),
};

static const unsigned int pins_i2c_combo5[] = { 92, 93 };
static const struct sppctlgrp_t sp7350grps_i2c_combo5[] = {
	EGRP("I2C_COMBO5", 1, pins_i2c_combo5),
};

static const unsigned int pins_i2c_combo6_x1[] = { 84, 85 };
static const unsigned int pins_i2c_combo6_x2[] = { 1, 2 };
static const struct sppctlgrp_t sp7350grps_i2c_combo6[] = {
	EGRP("I2C_COMBO6_X1", 1, pins_i2c_combo6_x1),
	EGRP("I2C_COMBO6_X2", 2, pins_i2c_combo6_x2),
};

static const unsigned int pins_i2c_combo7_x1[] = { 86, 87 };
static const unsigned int pins_i2c_combo7_x2[] = { 3, 4 };
static const struct sppctlgrp_t sp7350grps_i2c_combo7[] = {
	EGRP("I2C_COMBO7_X1", 1, pins_i2c_combo7_x1),
	EGRP("I2C_COMBO7_X2", 2, pins_i2c_combo7_x2),
};

static const unsigned int pins_i2c_combo8_x1[] = { 95, 96 };
static const unsigned int pins_i2c_combo8_x2[] = { 9, 10 };
static const struct sppctlgrp_t sp7350grps_i2c_combo8[] = {
	EGRP("I2C_COMBO8_X1", 1, pins_i2c_combo8_x1),
	EGRP("I2C_COMBO8_X2", 2, pins_i2c_combo8_x2),
};

static const unsigned int pins_i2c_combo9_x1[] = { 97, 98 };
static const unsigned int pins_i2c_combo9_x2[] = { 11, 12 };
static const struct sppctlgrp_t sp7350grps_i2c_combo9[] = {
	EGRP("I2C_COMBO9_X1", 1, pins_i2c_combo9_x1),
	EGRP("I2C_COMBO9_X2", 2, pins_i2c_combo9_x2),
};

static const unsigned int pins_spi_master0_x1[] = { 64, 65, 66, 67 };
static const unsigned int pins_spi_master0_x2[] = { 9, 10, 11, 12 };
static const struct sppctlgrp_t sp7350grps_spi_master0[] = {
	EGRP("SPI_MASTER0_X1", 1, pins_spi_master0_x1),
	EGRP("SPI_MASTER0_X2", 2, pins_spi_master0_x2),
};

static const unsigned int pins_spi_master1_x1[] = { 80, 81, 82, 83 };
static const unsigned int pins_spi_master1_x2[] = { 14, 15, 16, 17 };
static const struct sppctlgrp_t sp7350grps_spi_master1[] = {
	EGRP("SPI_MASTER1_X1", 1, pins_spi_master1_x1),
	EGRP("SPI_MASTER1_X2", 2, pins_spi_master1_x2),
};

static const unsigned int pins_spi_master2[] = { 88, 89, 90, 91 };
static const struct sppctlgrp_t sp7350grps_spi_master2[] = {
	EGRP("SPI_MASTER2", 1, pins_spi_master2),
};

static const unsigned int pins_spi_master3_x1[] = { 44, 45, 46, 47 };
static const unsigned int pins_spi_master3_x2[] = { 52, 53, 54, 55 };
static const struct sppctlgrp_t sp7350grps_spi_master3[] = {
	EGRP("SPI_MASTER3_X1", 1, pins_spi_master3_x1),
	EGRP("SPI_MASTER3_X2", 2, pins_spi_master3_x2),
};

static const unsigned int pins_spi_master4[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t sp7350grps_spi_master4[] = {
	EGRP("SPI_MASTER4", 1, pins_spi_master4),
};

static const unsigned int pins_spi_slave0_x1[] = { 94, 95, 96, 97 };
static const unsigned int pins_spi_slave0_x2[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t sp7350grps_spi_slave0[] = {
	EGRP("SPI_SLAVE0_X1", 1, pins_spi_slave0_x1),
	EGRP("SPI_SLAVE0_X2", 2, pins_spi_slave0_x2),
};

static const unsigned int pins_aud_tdmtx_xck[] = { 93 };
static const struct sppctlgrp_t sp7350grps_aud_tdmtx_xck[] = {
	EGRP("AUD_TDMTX_XCK", 1, pins_aud_tdmtx_xck),
};

static const unsigned int pins_aud_dac_xck1[] = { 71 };
static const struct sppctlgrp_t sp7350grps_aud_dac_xck1[] = {
	EGRP("AUD_DAC_XCK1", 1, pins_aud_dac_xck1),
};

static const unsigned int pins_aud_dac_xck[] = { 83 };
static const struct sppctlgrp_t sp7350grps_aud_dac_xck[] = {
	EGRP("AUD_DAC_XCK", 1, pins_aud_dac_xck),
};

static const unsigned int pins_aud_au2_data0[] = { 82 };
static const struct sppctlgrp_t sp7350grps_aud_au2_data0[] = {
	EGRP("AUD_AU2_DATA0", 1, pins_aud_au2_data0),
};

static const unsigned int pins_aud_au1_data0[] = { 58 };
static const struct sppctlgrp_t sp7350grps_aud_au1_data0[] = {
	EGRP("AUD_AU1_DATA0", 1, pins_aud_au1_data0),
};

static const unsigned int pins_aud_au2_ck[] = { 80, 81 };
static const struct sppctlgrp_t sp7350grps_aud_au2_ck[] = {
	EGRP("AUD_AU2_CK", 1, pins_aud_au2_ck),
};

static const unsigned int pins_aud_au1_ck[] = { 56, 57 };
static const struct sppctlgrp_t sp7350grps_aud_au1_ck[] = {
	EGRP("AUD_AU1_CK", 1, pins_aud_au1_ck),
};

static const unsigned int pins_aud_au_adc_data0_x1[] = { 94, 95, 96, 97 };
static const unsigned int pins_aud_au_adc_data0_x2[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t sp7350grps_aud_au_adc_data0[] = {
	EGRP("AUD_AU_ADC_DATA0_X1", 1, pins_aud_au_adc_data0_x1),
	EGRP("AUD_AU_ADC_DATA0_X2", 2, pins_aud_au_adc_data0_x2),
};

static const unsigned int pins_aud_adc2_data0[] = { 82 };
static const struct sppctlgrp_t sp7350grps_aud_adc2_data0[] = {
	EGRP("AUD_ADC2_DATA0", 1, pins_aud_adc2_data0),
};

static const unsigned int pins_aud_adc1_data0[] = { 58 };
static const struct sppctlgrp_t sp7350grps_aud_adc1_data0[] = {
	EGRP("AUD_ADC1_DATA0", 1, pins_aud_adc1_data0),
};

static const unsigned int pins_aud_tdm[] = { 94, 95, 96, 97 };
static const struct sppctlgrp_t sp7350grps_aud_tdm[] = {
	EGRP("AUD_TDM", 1, pins_aud_tdm),
};

static const unsigned int pins_spdif_x1[] = { 91 };
static const unsigned int pins_spdif_x2[] = { 53 };
static const unsigned int pins_spdif_x3[] = { 54 };
static const unsigned int pins_spdif_x4[] = { 55 };
static const unsigned int pins_spdif_x5[] = { 62 };
static const unsigned int pins_spdif_x6[] = { 52 };
static const struct sppctlgrp_t sp7350grps_spdif_in[] = {
	EGRP("SPDIF_IN_X1", 1, pins_spdif_x1),
	EGRP("SPDIF_IN_X2", 2, pins_spdif_x2),
	EGRP("SPDIF_IN_X3", 3, pins_spdif_x3),
	EGRP("SPDIF_IN_X4", 4, pins_spdif_x4),
	EGRP("SPDIF_IN_X5", 5, pins_spdif_x5),
	EGRP("SPDIF_IN_X6", 6, pins_spdif_x6),
};
static const struct sppctlgrp_t sp7350grps_spdif_out[] = {
	EGRP("SPDIF_OUT_X1", 1, pins_spdif_x1),
	EGRP("SPDIF_OUT_X2", 2, pins_spdif_x2),
	EGRP("SPDIF_OUT_X3", 3, pins_spdif_x3),
	EGRP("SPDIF_OUT_X4", 4, pins_spdif_x4),
	EGRP("SPDIF_OUT_X5", 5, pins_spdif_x5),
	EGRP("SPDIF_OUT_X6", 6, pins_spdif_x6),
};

static const unsigned int pins_int0_x1[] = { 1 };
static const unsigned int pins_int0_x2[] = { 2 };
static const unsigned int pins_int0_x3[] = { 3 };
static const unsigned int pins_int0_x4[] = { 4 };
static const unsigned int pins_int0_x5[] = { 5 };
static const unsigned int pins_int0_x6[] = { 6 };
static const unsigned int pins_int0_x7[] = { 13 };
static const unsigned int pins_int0_x8[] = { 14 };
static const unsigned int pins_int0_x9[] = { 15 };
static const struct sppctlgrp_t sp7350grps_int0[] = {
	EGRP("INT0_X1", 1, pins_int0_x1),
	EGRP("INT0_X2", 2, pins_int0_x2),
	EGRP("INT0_X3", 3, pins_int0_x3),
	EGRP("INT0_X4", 4, pins_int0_x4),
	EGRP("INT0_X5", 5, pins_int0_x5),
	EGRP("INT0_X6", 6, pins_int0_x6),
	EGRP("INT0_X7", 7, pins_int0_x7),
	EGRP("INT0_X8", 8, pins_int0_x8),
	EGRP("INT0_X9", 9, pins_int0_x9),
};
static const struct sppctlgrp_t sp7350grps_int1[] = {
	EGRP("INT1_X1", 1, pins_int0_x1),
	EGRP("INT1_X2", 2, pins_int0_x2),
	EGRP("INT1_X3", 3, pins_int0_x3),
	EGRP("INT1_X4", 4, pins_int0_x4),
	EGRP("INT1_X5", 5, pins_int0_x5),
	EGRP("INT1_X6", 6, pins_int0_x6),
	EGRP("INT1_X7", 7, pins_int0_x7),
	EGRP("INT1_X8", 8, pins_int0_x8),
	EGRP("INT1_X9", 9, pins_int0_x9),
};

static const unsigned int pins_int2_x1[] = { 5 };
static const unsigned int pins_int2_x2[] = { 6 };
static const unsigned int pins_int2_x3[] = { 7 };
static const unsigned int pins_int2_x4[] = { 8 };
static const unsigned int pins_int2_x5[] = { 9 };
static const unsigned int pins_int2_x6[] = { 10 };
static const unsigned int pins_int2_x7[] = { 11 };
static const unsigned int pins_int2_x8[] = { 16 };
static const unsigned int pins_int2_x9[] = { 17 };
static const struct sppctlgrp_t sp7350grps_int2[] = {
	EGRP("INT2_X1", 1, pins_int2_x1),
	EGRP("INT2_X2", 2, pins_int2_x2),
	EGRP("INT2_X3", 3, pins_int2_x3),
	EGRP("INT2_X4", 4, pins_int2_x4),
	EGRP("INT2_X5", 5, pins_int2_x5),
	EGRP("INT2_X6", 6, pins_int2_x6),
	EGRP("INT2_X7", 7, pins_int2_x7),
	EGRP("INT2_X8", 8, pins_int2_x8),
	EGRP("INT2_X9", 9, pins_int2_x9),
};
static const struct sppctlgrp_t sp7350grps_int3[] = {
	EGRP("INT3_X1", 1, pins_int2_x1),
	EGRP("INT3_X2", 2, pins_int2_x2),
	EGRP("INT3_X3", 3, pins_int2_x3),
	EGRP("INT3_X4", 4, pins_int2_x4),
	EGRP("INT3_X5", 5, pins_int2_x5),
	EGRP("INT3_X6", 6, pins_int2_x6),
	EGRP("INT3_X7", 7, pins_int2_x7),
	EGRP("INT3_X8", 8, pins_int2_x8),
	EGRP("INT3_X9", 9, pins_int2_x9),
};

static const unsigned int pins_int4_x1[] = { 7 };
static const unsigned int pins_int4_x2[] = { 8 };
static const unsigned int pins_int4_x3[] = { 9 };
static const unsigned int pins_int4_x4[] = { 10 };
static const unsigned int pins_int4_x5[] = { 11 };
static const unsigned int pins_int4_x6[] = { 12 };
static const unsigned int pins_int4_x7[] = { 13 };
static const unsigned int pins_int4_x8[] = { 18 };
static const unsigned int pins_int4_x9[] = { 19 };
static const struct sppctlgrp_t sp7350grps_int4[] = {
	EGRP("INT4_X1", 1, pins_int4_x1),
	EGRP("INT4_X2", 2, pins_int4_x2),
	EGRP("INT4_X3", 3, pins_int4_x3),
	EGRP("INT4_X4", 4, pins_int4_x4),
	EGRP("INT4_X5", 5, pins_int4_x5),
	EGRP("INT4_X6", 6, pins_int4_x6),
	EGRP("INT4_X7", 7, pins_int4_x7),
	EGRP("INT4_X8", 8, pins_int4_x8),
	EGRP("INT4_X9", 9, pins_int4_x9),
};
static const struct sppctlgrp_t sp7350grps_int5[] = {
	EGRP("INT5_X1", 1, pins_int4_x1),
	EGRP("INT5_X2", 2, pins_int4_x2),
	EGRP("INT5_X3", 3, pins_int4_x3),
	EGRP("INT5_X4", 4, pins_int4_x4),
	EGRP("INT5_X5", 5, pins_int4_x5),
	EGRP("INT5_X6", 6, pins_int4_x6),
	EGRP("INT5_X7", 7, pins_int4_x7),
	EGRP("INT5_X8", 8, pins_int4_x8),
	EGRP("INT5_X9", 9, pins_int4_x9),
};

static const unsigned int pins_int6_x1[] = { 9 };
static const unsigned int pins_int6_x2[] = { 10 };
static const unsigned int pins_int6_x3[] = { 11 };
static const unsigned int pins_int6_x4[] = { 12 };
static const unsigned int pins_int6_x5[] = { 13 };
static const unsigned int pins_int6_x6[] = { 14 };
static const unsigned int pins_int6_x7[] = { 15 };
static const unsigned int pins_int6_x8[] = { 16 };
static const unsigned int pins_int6_x9[] = { 17 };
static const unsigned int pins_int6_x10[] = { 18 };
static const unsigned int pins_int6_x11[] = { 19 };
static const struct sppctlgrp_t sp7350grps_int6[] = {
	EGRP("INT6_X1", 1, pins_int6_x1),
	EGRP("INT6_X2", 2, pins_int6_x2),
	EGRP("INT6_X3", 3, pins_int6_x3),
	EGRP("INT6_X4", 4, pins_int6_x4),
	EGRP("INT6_X5", 5, pins_int6_x5),
	EGRP("INT6_X6", 6, pins_int6_x6),
	EGRP("INT6_X7", 7, pins_int6_x7),
	EGRP("INT6_X8", 8, pins_int6_x8),
	EGRP("INT6_X9", 9, pins_int6_x9),
	EGRP("INT6_X10", 10, pins_int6_x10),
	EGRP("INT6_X11", 11, pins_int6_x11),
};
static const struct sppctlgrp_t sp7350grps_int7[] = {
	EGRP("INT7_X1", 1, pins_int6_x1),
	EGRP("INT7_X2", 2, pins_int6_x2),
	EGRP("INT7_X3", 3, pins_int6_x3),
	EGRP("INT7_X4", 4, pins_int6_x4),
	EGRP("INT7_X5", 5, pins_int6_x5),
	EGRP("INT7_X6", 6, pins_int6_x6),
	EGRP("INT7_X7", 7, pins_int6_x7),
	EGRP("INT7_X8", 8, pins_int6_x8),
	EGRP("INT7_X9", 9, pins_int6_x9),
	EGRP("INT7_X10", 10, pins_int6_x10),
	EGRP("INT7_X11", 11, pins_int6_x11),
};

static const unsigned int pins_gpio_ao_int0_x1[] = { 52, 53, 54, 55, 56, 57, 58, 59 };
static const unsigned int pins_gpio_ao_int0_x2[] = { 68, 69, 70, 71, 72, 73, 74, 75 };
static const struct sppctlgrp_t sp7350grps_gpio_ao_int0[] = {
	EGRP("GPIO_AO_INT0_X1", 1, pins_gpio_ao_int0_x1),
	EGRP("GPIO_AO_INT0_X2", 1, pins_gpio_ao_int0_x2),
};

static const unsigned int pins_gpio_ao_int1_x1[] = { 60, 61, 62, 63, 64, 65, 66, 67 };
static const unsigned int pins_gpio_ao_int1_x2[] = { 76, 77, 78, 79, 80, 81, 82, 83 };
static const struct sppctlgrp_t sp7350grps_gpio_ao_int1[] = {
	EGRP("GPIO_AO_INT1_X1", 1, pins_gpio_ao_int1_x1),
	EGRP("GPIO_AO_INT1_X2", 1, pins_gpio_ao_int1_x2),
};

static const unsigned int pins_gpio_ao_int2_x1[] = { 68, 69, 70, 71, 72, 73, 74, 75 };
static const unsigned int pins_gpio_ao_int2_x2[] = { 84, 85, 86, 87, 88, 89, 90, 91 };
static const struct sppctlgrp_t sp7350grps_gpio_ao_int2[] = {
	EGRP("GPIO_AO_INT2_X1", 1, pins_gpio_ao_int2_x1),
	EGRP("GPIO_AO_INT2_X2", 1, pins_gpio_ao_int2_x2),
};

static const unsigned int pins_gpio_ao_int3_x1[] = { 76, 77, 78, 79, 80, 81, 82, 83 };
static const unsigned int pins_gpio_ao_int3_x2[] = { 91, 92, 93, 94, 95, 96, 97, 98 };
static const struct sppctlgrp_t sp7350grps_gpio_ao_int3[] = {
	EGRP("GPIO_AO_INT3_X1", 1, pins_gpio_ao_int3_x1),
	EGRP("GPIO_AO_INT3_X2", 1, pins_gpio_ao_int3_x2),
};

struct func_t list_funcs[] = {
	FNCN("GPIO",            fOFF_0, 0, 0, 0),
	FNCN("IOP",             fOFF_0, 0, 0, 0),

	FNCE("SPI_FLASH",       fOFF_G, 1, 0,  1, sp7350grps_spif),
	FNCE("EMMC",            fOFF_G, 1, 1,  1, sp7350grps_emmc),
	FNCE("SPI_NAND",        fOFF_G, 1, 2,  2, sp7350grps_spi_nand),
	FNCE("SD_CARD",         fOFF_G, 1, 4,  1, sp7350grps_sdc30),
	FNCE("SDIO",            fOFF_G, 1, 5,  1, sp7350grps_sdio30),
	FNCE("PARA_NAND",       fOFF_G, 1, 6,  1, sp7350grps_para_nand),
	FNCE("USB_OTG",         fOFF_G, 1, 7,  1, sp7350grps_usb_otg),
	FNCE("GMAC",            fOFF_G, 1, 9,  1, sp7350grps_gmac),
	FNCE("PWM0",            fOFF_G, 1, 10, 2, sp7350grps_pwm0),
	FNCE("PWM1",            fOFF_G, 1, 12, 2, sp7350grps_pwm1),
	FNCE("PWM2",            fOFF_G, 1, 14, 2, sp7350grps_pwm2),

	FNCE("PWM3",            fOFF_G, 2, 0,  2, sp7350grps_pwm3),
	FNCE("UART0",           fOFF_G, 2, 2,  2, sp7350grps_uart0),
	FNCE("UART1",           fOFF_G, 2, 4,  2, sp7350grps_uart1),
	FNCE("UART1_FC",        fOFF_G, 2, 6,  2, sp7350grps_uart1_fc),
	FNCE("UART2",           fOFF_G, 2, 8,  2, sp7350grps_uart2),
	FNCE("UART2_FC",        fOFF_G, 2, 10, 2, sp7350grps_uart2_fc),
	FNCE("UART3",           fOFF_G, 2, 12, 2, sp7350grps_uart3),
	FNCE("UADBG",           fOFF_G, 2, 14, 1, sp7350grps_uadbg),

	FNCE("UART6",           fOFF_G, 3, 0,  2, sp7350grps_uart6),
	FNCE("UART7",           fOFF_G, 3, 2,  1, sp7350grps_uart7),
	FNCE("I2C_COMBO0",      fOFF_G, 3, 3,  2, sp7350grps_i2c_combo0),
	FNCE("I2C_COMBO1",      fOFF_G, 3, 5,  1, sp7350grps_i2c_combo1),
	FNCE("I2C_COMBO2",      fOFF_G, 3, 6,  2, sp7350grps_i2c_combo2),
	FNCE("I2C_COMBO3",      fOFF_G, 3, 8,  1, sp7350grps_i2c_combo3),
	FNCE("I2C_COMBO4",      fOFF_G, 3, 9,  1, sp7350grps_i2c_combo4),
	FNCE("I2C_COMBO5",      fOFF_G, 3, 10, 1, sp7350grps_i2c_combo5),
	FNCE("I2C_COMBO6",      fOFF_G, 3, 11, 2, sp7350grps_i2c_combo6),
	FNCE("I2C_COMBO7",      fOFF_G, 3, 13, 2, sp7350grps_i2c_combo7),

	FNCE("I2C_COMBO8",      fOFF_G, 4, 0,  2, sp7350grps_i2c_combo8),
	FNCE("I2C_COMBO9",      fOFF_G, 4, 2,  2, sp7350grps_i2c_combo9),
	FNCE("SPI_MASTER0",     fOFF_G, 4, 14, 2, sp7350grps_spi_master0),

	FNCE("SPI_MASTER1",     fOFF_G, 5, 1,  2, sp7350grps_spi_master1),
	FNCE("SPI_MASTER2",     fOFF_G, 5, 3,  1, sp7350grps_spi_master2),
	FNCE("SPI_MASTER3",     fOFF_G, 5, 4,  2, sp7350grps_spi_master3),
	FNCE("SPI_MASTER4",     fOFF_G, 5, 6,  1, sp7350grps_spi_master4),
	FNCE("SPI_SLAVE0",      fOFF_G, 5, 7,  2, sp7350grps_spi_slave0),
	FNCE("AUD_TDMTX_XCK",   fOFF_G, 5, 9,  1, sp7350grps_aud_tdmtx_xck),
	FNCE("AUD_DAC_XCK1",    fOFF_G, 5, 10, 1, sp7350grps_aud_dac_xck1),
	FNCE("AUD_DAC_XCK",     fOFF_G, 5, 11, 1, sp7350grps_aud_dac_xck),
	FNCE("AUD_AU2_DATA0",   fOFF_G, 5, 12, 1, sp7350grps_aud_au2_data0),
	FNCE("AUD_AU1_DATA0",   fOFF_G, 5, 13, 1, sp7350grps_aud_au1_data0),
	FNCE("AUD_AU2_CK",      fOFF_G, 5, 14, 1, sp7350grps_aud_au2_ck),
	FNCE("AUD_AU1_CK",      fOFF_G, 5, 15, 1, sp7350grps_aud_au1_ck),

	FNCE("AUD_AU_ADC_DATA0",fOFF_G, 6, 0,  2, sp7350grps_aud_au_adc_data0),
	FNCE("AUD_ADC2_DATA0",  fOFF_G, 6, 2,  1, sp7350grps_aud_adc2_data0),
	FNCE("AUD_ADC1_DATA0",  fOFF_G, 6, 3,  1, sp7350grps_aud_adc1_data0),
	FNCE("AUD_TDM",         fOFF_G, 6, 4,  1, sp7350grps_aud_tdm),
	FNCE("SPDIF_IN",        fOFF_G, 6, 5,  3, sp7350grps_spdif_in),
	FNCE("SPDIF_OUT",       fOFF_G, 6, 8,  3, sp7350grps_spdif_out),

	FNCE("INT0",            fOFF_G, 7, 5,  4, sp7350grps_int0),
	FNCE("INT1",            fOFF_G, 7, 9,  4, sp7350grps_int1),

	FNCE("INT2",            fOFF_G, 8, 0,  4, sp7350grps_int2),
	FNCE("INT3",            fOFF_G, 8, 4,  4, sp7350grps_int3),
	FNCE("INT4",            fOFF_G, 8, 8,  4, sp7350grps_int4),
	FNCE("INT5",            fOFF_G, 8, 12, 4, sp7350grps_int5),

	FNCE("INT6",            fOFF_G, 9, 0,  4, sp7350grps_int6),
	FNCE("INT7",            fOFF_G, 9, 4,  4, sp7350grps_int7),
	FNCE("GPIO_AO_INT0",    fOFF_G, 9, 8,  2, sp7350grps_gpio_ao_int0),
	FNCE("GPIO_AO_INT1",    fOFF_G, 9, 10, 2, sp7350grps_gpio_ao_int1),
	FNCE("GPIO_AO_INT2",    fOFF_G, 9, 12, 2, sp7350grps_gpio_ao_int2),
	FNCE("GPIO_AO_INT3",    fOFF_G, 9, 14, 2, sp7350grps_gpio_ao_int3),
};

const size_t list_funcsSZ = ARRAY_SIZE(list_funcs);
