/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef _DT_BINDINGS_CLOCK_SUNPLUS_SP7350_H
#define _DT_BINDINGS_CLOCK_SUNPLUS_SP7350_H

#define XTAL            25000000

/* clks G2.1 ~ 2.12 */
#define SYSTEM              0x00
#define CA55CORE0           0x01
#define CA55CORE1           0x02
#define CA55CORE2           0x03
#define CA55CORE3           0x04
#define CA55CUL3            0x05
#define CA55                0x06
#define GMAC                0x07
#define RBUS                0x08
#define RBUS_BLOCKB         0x09
#define SYSTOP              0x0a
#define THERMAL             0x0b
#define BR0                 0x0c
#define CARD_CTL0           0x0d
#define CARD_CTL1           0x0e
#define CARD_CTL2           0x0f

#define CBDMA0              0x10
#define CPIOR               0x11
#define DDRPHY              0x12
#define TZC                 0x13
#define DDRCTL              0x14
#define DRAM                0x15
#define VCL                 0x16
#define VCL0                0x17
#define VCL1                0x18
#define VCL2                0x19
#define VCL3                0x1a
#define VCL4                0x1b
#define VCL5                0x1c
#define DUMMY_MASTER0       0x1d
#define DUMMY_MASTER1       0x1e
#define DUMMY_MASTER2       0x1f

#define DISPSYS             0x20
#define DMIX                0x21
#define GPOST0              0x22
#define GPOST1              0x23
#define GPOST2              0x24
#define GPOST3              0x25
#define IMGREAD0            0x26
#define MIPITX              0x27
#define OSD0                0x28
#define OSD1                0x29
#define OSD2                0x2a
#define OSD3                0x2b
#define TCON                0x2c
#define TGEN                0x2d
#define VPOST0              0x2e
#define VSCL0               0x2f

#define MIPICSI0            0x30
#define MIPICSI1            0x31
#define MIPICSI2            0x32
#define MIPICSI3            0x33
#define MIPICSI4            0x34
#define MIPICSI5            0x35
#define VI0_CSIIW0          0x36
#define VI0_CSIIW1          0x37
#define VI1_CSIIW0          0x38
#define VI1_CSIIW1          0x39
#define VI2_CSIIW0          0x3a
#define VI2_CSIIW1          0x3b
#define VI3_CSIIW0          0x3c
#define VI3_CSIIW1          0x3d
#define VI3_CSIIW2          0x3e
#define VI3_CSIIW3          0x3f

#define VI4_CSIIW0          0x40
#define VI4_CSIIW1          0x41
#define VI4_CSIIW2          0x42
#define VI4_CSIIW3          0x43
#define VI5_CSIIW0          0x44
#define VI5_CSIIW1          0x45
#define VI5_CSIIW2          0x46
#define VI5_CSIIW3          0x47
#define MIPICSI23_SEL       0x48
#define VI23_CSIIW1         0x49
#define VI23_CSIIW2         0x4a
#define VI23_CSIIW3         0x4b
#define VI23_CSIIW0         0x4c
#define OTPRX               0x4d
#define PRNG                0x4e
#define SEMAPHORE           0x4f

#define INTERRUPT           0x50
#define SPIFL               0x51
#define BCH                 0x52
#define SPIND               0x53
#define UADMA01             0x54
#define UADMA23             0x55
#define UA0                 0x56
#define UA1                 0x57
#define UA2                 0x58
#define UA3                 0x59
#define UADBG               0x5a
#define UART2AXI            0x5b
#define UPHY0               0x5c
#define USB30C0             0x5d
#define U3PHY0              0x5e
#define USBC0               0x5f

#define VCD                 0x60
#define VCE                 0x61
#define VIDEO_CODEC         0x62
#define MAILBOX             0x63
#define AXI_DMA             0x64
#define PNAND               0x65
#define SEC                 0x66
#define rsv1                0x67
#define STC_AV3             0x68
#define STC_TIMESTAMP       0x69
#define STC_AV4             0x6a
#define NICTOP              0x6b
#define NICPAII             0x6c
#define NICPAI              0x6d
#define NPU                 0x6e
#define SECGRP_PAI          0x6f

#define SECGRP_PAII         0x70
#define SECGRP_MAIN         0x71
#define DPLL                0x72
#define HBUS                0x73
#define AUD                 0x74
#define AXIM_TOP            0x75
#define AXIM_PAI            0x76
#define AXIM_PAII           0x77
#define SYSAO               0x78
#define PMC                 0x79
#define RTC                 0x7a
#define INTERRUPT_AO        0x7b
#define UA6                 0x7c
#define UA7                 0x7d
#define GDMAUA              0x7e
#define CM4                 0x7f

#define STC0                0x80
#define STC_AV0             0x81
#define STC_AV1             0x82
#define STC_AV2             0x83
#define AHB_DMA             0x84
#define SAR12B              0x85
#define DISP_PWM            0x86
#define NICPIII             0x87
#define GPIO_AO_INT         0x88
#define I2CM0               0x89
#define I2CM1               0x8a
#define I2CM2               0x8b
#define I2CM3               0x8c
#define I2CM4               0x8d
#define I2CM5               0x8e
#define I2CM6               0x8f

#define I2CM7               0x90
#define I2CM8               0x91
#define I2CM9               0x92
#define SPICB0              0x93
#define SPICB1              0x94
#define SPICB2              0x95
#define SPICB3              0x96
#define SPICB4              0x97
#define SPICB5              0x98
#define PD_AXI_DMA          0x99
#define PD_CA55             0x9a
#define PD_CARD0            0x9b
#define PD_CARD1            0x9c
#define PD_CARD2            0x9d
#define PD_CBDMA0           0x9e
#define PD_CPIOR0           0x9f

#define PD_CPIOR1           0xa0
#define PD_CSDBG            0xa1
#define PD_CSETR            0xa2
#define PD_DUMMY0           0xa3
#define PD_DUMMY1           0xa4
#define PD_GMAC             0xa5
#define PD_IMGREAD0         0xa6
#define PD_NBS              0xa7
#define PD_NPU              0xa8
#define PD_OSD0             0xa9
#define PD_OSD1             0xaa
#define PD_OSD2             0xab
#define PD_OSD3             0xac
#define PD_SEC              0xad
#define PD_SPI_ND           0xae
#define PD_SPI_NOR          0xaf

#define PD_UART2AXI         0xb0
#define PD_USB30C0          0xb1
#define PD_USBC0            0xb2
#define PD_VCD              0xb3
#define PD_VCE              0xb4
#define PD_VCL              0xb5
#define PD_VI0_CSII         0xb6
#define PD_VI1_CSII         0xb7
#define PD_VI23_CSII        0xb8
#define PD_VI4_CSII         0xb9
#define PD_VI5_CSII         0xba
#define PD_AHB_DMA          0xbb
#define PD_AUD              0xbc
#define PD_CM4              0xbd
#define PD_HWUA_TX_GDMA     0xbe
#define QCTRL               0xbf

#define CLK_MAX             0xc0

/* plls */
#define PLL(i)          (CLK_MAX + i)

#define PLLA            PLL(0)
#define PLLC            PLL(1)
#define PLLL3           PLL(2)
#define PLLD            PLL(3)
#define PLLH            PLL(4)
#define PLLN            PLL(5)
#define PLLS            PLL(6)

#define PLL_MAX         7

#endif
