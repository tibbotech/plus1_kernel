/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef _DT_BINDINGS_RST_SUNPLUS_SP7350_H
#define _DT_BINDINGS_RST_SUNPLUS_SP7350_H

/* mo_reset0 ~ mo_reset11 */
#define RST_SYSTEM              0x00
#define RST_CA55CORE0           0x01
#define RST_CA55CORE1           0x02
#define RST_CA55CORE2           0x03
#define RST_CA55CORE3           0x04
#define RST_CA55CUL3            0x05
#define RST_CA55                0x06
#define RST_GMAC                0x07
#define RST_RBUS                0x08
#define RST_RBUS_BLOCKB         0x09
#define RST_SYSTOP              0x0a
#define RST_THERMAL             0x0b
#define RST_BR0                 0x0c
#define RST_CARD_CTL0           0x0d
#define RST_CARD_CTL1           0x0e
#define RST_CARD_CTL2           0x0f

#define RST_CBDMA0              0x10
#define RST_CPIOR               0x11
#define RST_DDRPHY              0x12
#define RST_TZC                 0x13
#define RST_DDRCTL              0x14
#define RST_DRAM                0x15
#define RST_VCL                 0x16
#define RST_VCL0                0x17
#define RST_VCL1                0x18
#define RST_VCL2                0x19
#define RST_VCL3                0x1a
#define RST_VCL4                0x1b
#define RST_VCL5                0x1c
#define RST_DUMMY_MASTER0       0x1d
#define RST_DUMMY_MASTER1       0x1e
#define RST_DUMMY_MASTER2       0x1f

#define RST_DISPSYS             0x20
#define RST_DMIX                0x21
#define RST_GPOST0              0x22
#define RST_GPOST1              0x23
#define RST_GPOST2              0x24
#define RST_GPOST3              0x25
#define RST_IMGREAD0            0x26
#define RST_MIPITX              0x27
#define RST_OSD0                0x28
#define RST_OSD1                0x29
#define RST_OSD2                0x2a
#define RST_OSD3                0x2b
#define RST_TCON                0x2c
#define RST_TGEN                0x2d
#define RST_VPOST0              0x2e
#define RST_VSCL0               0x2f

#define RST_MIPICSI0            0x30
#define RST_MIPICSI1            0x31
#define RST_MIPICSI2            0x32
#define RST_MIPICSI3            0x33
#define RST_MIPICSI4            0x34
#define RST_MIPICSI5            0x35
#define RST_VI0_CSIIW0          0x36
#define RST_VI0_CSIIW1          0x37
#define RST_VI1_CSIIW0          0x38
#define RST_VI1_CSIIW1          0x39
#define RST_VI2_CSIIW0          0x3a
#define RST_VI2_CSIIW1          0x3b
#define RST_VI3_CSIIW0          0x3c
#define RST_VI3_CSIIW1          0x3d
#define RST_VI3_CSIIW2          0x3e
#define RST_VI3_CSIIW3          0x3f

#define RST_VI4_CSIIW0          0x40
#define RST_VI4_CSIIW1          0x41
#define RST_VI4_CSIIW2          0x42
#define RST_VI4_CSIIW3          0x43
#define RST_VI5_CSIIW0          0x44
#define RST_VI5_CSIIW1          0x45
#define RST_VI5_CSIIW2          0x46
#define RST_VI5_CSIIW3          0x47
#define RST_MIPICSI23_SEL       0x48
#define RST_VI23_CSIIW1         0x49
#define RST_VI23_CSIIW2         0x4a
#define RST_VI23_CSIIW3         0x4b
#define RST_VI23_CSIIW0         0x4c
#define RST_OTPRX               0x4d
#define RST_PRNG                0x4e
#define RST_SEMAPHORE           0x4f

#define RST_INTERRUPT           0x50
#define RST_SPIFL               0x51
#define RST_BCH                 0x52
#define RST_SPIND               0x53
#define RST_UADMA01             0x54
#define RST_UADMA23             0x55
#define RST_UA0                 0x56
#define RST_UA1                 0x57
#define RST_UA2                 0x58
#define RST_UA3                 0x59
#define RST_UADBG               0x5a
#define RST_UART2AXI            0x5b
#define RST_UPHY0               0x5c
#define RST_USB30C0             0x5d
#define RST_U3PHY0              0x5e
#define RST_USBC0               0x5f

#define RST_VCD                 0x60
#define RST_VCE                 0x61
#define RST_VIDEO_CODEC         0x62
#define RST_MAILBOX             0x63
#define RST_AXI_DMA             0x64
#define RST_PNAND               0x65
#define RST_SEC                 0x66
#define RST_rsv1                0x67
#define RST_STC_AV3             0x68
#define RST_STC_TIMESTAMP       0x69
#define RST_STC_AV4             0x6a
#define RST_NICTOP              0x6b
#define RST_NICPAII             0x6c
#define RST_NICPAI              0x6d
#define RST_NPU                 0x6e
#define RST_SECGRP_PAI          0x6f

#define RST_SECGRP_PAII         0x70
#define RST_SECGRP_MAIN         0x71
#define RST_DPLL                0x72
#define RST_HBUS                0x73
#define RST_AUD                 0x74
#define RST_AXIM_TOP            0x75
#define RST_AXIM_PAI            0x76
#define RST_AXIM_PAII           0x77
#define RST_SYSAO               0x78
#define RST_PMC                 0x79
#define RST_RTC                 0x7a
#define RST_INTERRUPT_AO        0x7b
#define RST_UA6                 0x7c
#define RST_UA7                 0x7d
#define RST_GDMAUA              0x7e
#define RST_CM4                 0x7f

#define RST_STC0                0x80
#define RST_STC_AV0             0x81
#define RST_STC_AV1             0x82
#define RST_STC_AV2             0x83
#define RST_AHB_DMA             0x84
#define RST_SAR12B              0x85
#define RST_DISP_PWM            0x86
#define RST_NICPIII             0x87
#define RST_GPIO_AO_INT         0x88
#define RST_I2CM0               0x89
#define RST_I2CM1               0x8a
#define RST_I2CM2               0x8b
#define RST_I2CM3               0x8c
#define RST_I2CM4               0x8d
#define RST_I2CM5               0x8e
#define RST_I2CM6               0x8f

#define RST_I2CM7               0x90
#define RST_I2CM8               0x91
#define RST_I2CM9               0x92
#define RST_SPICB0              0x93
#define RST_SPICB1              0x94
#define RST_SPICB2              0x95
#define RST_SPICB3              0x96
#define RST_SPICB4              0x97
#define RST_SPICB5              0x98
#define RST_PD_AXI_DMA          0x99
#define RST_PD_CA55             0x9a
#define RST_PD_CARD0            0x9b
#define RST_PD_CARD1            0x9c
#define RST_PD_CARD2            0x9d
#define RST_PD_CBDMA0           0x9e
#define RST_PD_CPIOR0           0x9f

#define RST_PD_CPIOR1           0xa0
#define RST_PD_CSDBG            0xa1
#define RST_PD_CSETR            0xa2
#define RST_PD_DUMMY0           0xa3
#define RST_PD_DUMMY1           0xa4
#define RST_PD_GMAC             0xa5
#define RST_PD_IMGREAD0         0xa6
#define RST_PD_NBS              0xa7
#define RST_PD_NPU              0xa8
#define RST_PD_OSD0             0xa9
#define RST_PD_OSD1             0xaa
#define RST_PD_OSD2             0xab
#define RST_PD_OSD3             0xac
#define RST_PD_SEC              0xad
#define RST_PD_SPI_ND           0xae
#define RST_PD_SPI_NOR          0xaf

#define RST_PD_UART2AXI         0xb0
#define RST_PD_USB30C0          0xb1
#define RST_PD_USBC0            0xb2
#define RST_PD_VCD              0xb3
#define RST_PD_VCE              0xb4
#define RST_PD_VCL              0xb5
#define RST_PD_VI0_CSII         0xb6
#define RST_PD_VI1_CSII         0xb7
#define RST_PD_VI23_CSII        0xb8
#define RST_PD_VI4_CSII         0xb9
#define RST_PD_VI5_CSII         0xba
#define RST_PD_AHB_DMA          0xbb
#define RST_PD_AUD              0xbc
#define RST_PD_CM4              0xbd
#define RST_PD_HWUA_TX_GDMA     0xbe
#define RST_QCTRL               0xbf

#define RST_MAX                 0xc0

#endif
