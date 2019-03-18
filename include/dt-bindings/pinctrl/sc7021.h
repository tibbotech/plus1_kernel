/*
 * SC7021 pinmux pinctrl bindings.
 * Copyright (C) SunPlus Tech/Tibbo Tech. 2019
 * Author: Dvorkin Dmitry <dvorkin@tibbo.com>
 */

#ifndef _DT_BINDINGS_PINCTRL_SC7021_H
#define _DT_BINDINGS_PINCTRL_SC7021_H

#define IOP_G_MASTE 0x01<<0
#define IOP_G_FIRST 0x01<<1

#define SC7021_PCTL_G_PMUX (       0x00|IOP_G_MASTE)
#define SC7021_PCTL_G_GPIO (IOP_G_FIRST|IOP_G_MASTE)
#define SC7021_PCTL_G_IOPP (IOP_G_FIRST|0x00)

#define SC7021_PCTL_L_OUT 0x01<<0
#define SC7021_PCTL_L_OU1 0x01<<1
#define SC7021_PCTL_L_INV 0x01<<2
#define SC7021_PCTL_L_ODR 0x01<<3

#define SC7021_PCTLE_P(v) ((v)<<24)
#define SC7021_PCTLE_G(v) ((v)<<16)
#define SC7021_PCTLE_F(v) ((v)<<8)
#define SC7021_PCTLE_L(v) ((v)<<0)

#define SC7021_PCTLD_P(v) (((v)>>24) & 0xFF)
#define SC7021_PCTLD_G(v) (((v)>>16) & 0xFF)
#define SC7021_PCTLD_F(v) (((v) >>8) & 0xFF)
#define SC7021_PCTLD_L(v) (((v) >>0) & 0xFF)

/* pack into 32-bit value :
  pin#{8bit} . iop{8bit} . function{8bit} . flags{8bit}
 */
#define SC7021_IOPAD(pin,iop,fun,fls) (((pin)<<24)|((iop)<<16)|((fun)<<8)|(fls))

#define MUXF_GPIO 0
#define MUXF_L2SW_CLK_OUT 1
#define MUXF_L2SW_MAC_SMI_MDC 2
#define MUXF_L2SW_LED_FLASH0 3
#define MUXF_L2SW_LED_FLASH1 4
#define MUXF_L2SW_LED_ON0 5
#define MUXF_L2SW_LED_ON1 6
#define MUXF_L2SW_MAC_SMI_MDIO 7
#define MUXF_L2SW_P0_MAC_RMII_TXEN 8
#define MUXF_L2SW_P0_MAC_RMII_TXD0 9
#define MUXF_L2SW_P0_MAC_RMII_TXD1 10
#define MUXF_L2SW_P0_MAC_RMII_CRSDV 11
#define MUXF_L2SW_P0_MAC_RMII_RXD0 12
#define MUXF_L2SW_P0_MAC_RMII_RXD1 13
#define MUXF_L2SW_P0_MAC_RMII_RXER 14
#define MUXF_L2SW_P1_MAC_RMII_TXEN 15
#define MUXF_L2SW_P1_MAC_RMII_TXD0 16
#define MUXF_L2SW_P1_MAC_RMII_TXD1 17
#define MUXF_L2SW_P1_MAC_RMII_CRSDV 18
#define MUXF_L2SW_P1_MAC_RMII_RXD0 19
#define MUXF_L2SW_P1_MAC_RMII_RXD1 20
#define MUXF_L2SW_P1_MAC_RMII_RXER 21
#define MUXF_DAISY_MODE 22
#define MUXF_SDIO_CLK 23
#define MUXF_SDIO_CMD 24
#define MUXF_SDIO_D0 25
#define MUXF_SDIO_D1 26
#define MUXF_SDIO_D2 27
#define MUXF_SDIO_D3 28
#define MUXF_PWM0 29
#define MUXF_PWM1 30
#define MUXF_PWM2 31
#define MUXF_PWM3 32
#define MUXF_PWM4 33
#define MUXF_PWM5 34
#define MUXF_PWM6 35
#define MUXF_PWM7 36
#define MUXF_ICM0_D 37
#define MUXF_ICM1_D 38
#define MUXF_ICM2_D 39
#define MUXF_ICM3_D 40
#define MUXF_ICM0_CLK 41
#define MUXF_ICM1_CLK 42
#define MUXF_ICM2_CLK 43
#define MUXF_ICM3_CLK 44
#define MUXF_SPIM0_INT 45
#define MUXF_SPIM0_CLK 46
#define MUXF_SPIM0_EN 47
#define MUXF_SPIM0_DO 48
#define MUXF_SPIM0_DI 49
#define MUXF_SPIM1_INT 50
#define MUXF_SPIM1_CLK 51
#define MUXF_SPIM1_CEN 52
#define MUXF_SPIM1_DO 53
#define MUXF_SPIM1_DI 54
#define MUXF_SPIM2_INT 55
#define MUXF_SPIM2_CLK 56
#define MUXF_SPIM2_CEN 57
#define MUXF_SPIM2_DO 58
#define MUXF_SPIM2_DI 59
#define MUXF_SPIM3_INT 60
#define MUXF_SPIM3_CLK 61
#define MUXF_SPIM3_CEN 62
#define MUXF_SPIM3_DO 63
#define MUXF_SPIM3_DI 64
#define MUXF_SPI0S_INT 65
#define MUXF_SPI0S_CLK 66
#define MUXF_SPI0S_EN 67
#define MUXF_SPI0S_DO 68
#define MUXF_SPI0S_DI 69
#define MUXF_SPI1S_INT 70
#define MUXF_SPI1S_CLK 71
#define MUXF_SPI1S_EN 72
#define MUXF_SPI1S_DO 73
#define MUXF_SPI1S_DI 74
#define MUXF_SPI2S_INT 75
#define MUXF_SPI2S_CLK 76
#define MUXF_SPI2S_EN 77
#define MUXF_SPI2S_DO 78
#define MUXF_SPI2S_DI 79
#define MUXF_SPI3S_INT 80
#define MUXF_SPI3S_CLK 81
#define MUXF_SPI3S_EN 82
#define MUXF_SPI3S_DO 83
#define MUXF_SPI3S_DI 84
#define MUXF_I2CM0_CK 85
#define MUXF_I2CM0_DAT 86
#define MUXF_I2CM1_CK 87
#define MUXF_I2CM1_DAT 88
#define MUXF_I2CM2_CK 89
#define MUXF_I2CM2_D 90
#define MUXF_I2CM3_CK 91
#define MUXF_I2CM3_D 92
#define MUXF_UA1_TX 93
#define MUXF_UA1_RX 94
#define MUXF_UA1_CTS 95
#define MUXF_UA1_RTS 96
#define MUXF_UA2_TX 97
#define MUXF_UA2_RX 98
#define MUXF_UA2_CTS 99
#define MUXF_UA2_RTS 100
#define MUXF_UA3_TX 101
#define MUXF_UA3_RX 102
#define MUXF_UA3_CTS 103
#define MUXF_UA3_RTS 104
#define MUXF_UA4_TX 105
#define MUXF_UA4_RX 106
#define MUXF_UA4_CTS 107
#define MUXF_UA4_RTS 108
#define MUXF_TIMER0_INT 109
#define MUXF_TIMER1_INT 110
#define MUXF_TIMER2_INT 111
#define MUXF_TIMER3_INT 112
#define MUXF_GPIO_INT0 113
#define MUXF_GPIO_INT1 114
#define MUXF_GPIO_INT2 115
#define MUXF_GPIO_INT3 116
#define MUXF_GPIO_INT4 117
#define MUXF_GPIO_INT5 118
#define MUXF_GPIO_INT6 119
#define MUXF_GPIO_INT7 120

#endif
