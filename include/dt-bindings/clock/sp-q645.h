/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef _DT_BINDINGS_CLOCK_SUNPLUS_Q645_H
#define _DT_BINDINGS_CLOCK_SUNPLUS_Q645_H

#define XTAL            25000000

/* clks: mo_scfg_0 ~ mo_scfg_5 */
#define SYSTEM          0x00
#define CA55CORE0       0x01
#define CA55CORE1       0x02
#define CA55CORE2       0x03
#define CA55CORE3       0x04
#define CA55CUL3        0x05
#define CA55            0x06
#define IOP             0x07
#define PBUS0           0x08
#define PBUS1           0x09
#define PBUS2           0x0a
#define PBUS3           0x0b
#define BR0             0x0c
#define CARD_CTL0       0x0d
#define CARD_CTL1       0x0e
#define CARD_CTL2       0x0f

#define CBDMA0          0x10
#define CPIOL           0x11
#define CPIOR           0x12
#define DDR_PHY0        0x13
#define SDCTRL0         0x14
#define DUMMY_MASTER0   0x15
#define DUMMY_MASTER1   0x16
#define DUMMY_MASTER2   0x17
#define EVDN            0x18
#define SDPROT0         0x19
#define UMCTL2          0x1a
#define GPU             0x1b
#define HSM             0x1c
#define RBUS_TOP        0x1d
#define SPACC           0x1e
#define INTERRUPT       0x1f

#define N78             0x20
#define SYSTOP          0x21
#define OTPRX           0x22
#define PMC             0x23
#define RBUS_BLOCKA     0x24
#define RBUS_BLOCKB     0x25
#define RBUS_rsv1       0x26
#define RBUS_rsv2       0x27
#define RTC             0x28
#define MIPZ            0x29
#define SPIFL           0x2a
#define BCH             0x2b
#define SPIND           0x2c
#define UADMA01         0x2d
#define UADMA23         0x2e
#define UA0             0x2f

#define UA1             0x30
#define UA2             0x31
#define UA3             0x32
#define UA4             0x33
#define UA5             0x34
#define UADBG           0x35
#define UART2AXI        0x36
#define GDMAUA          0x37
#define UPHY0           0x38
#define USB30C0         0x39
#define USB30C1         0x3a
#define U3PHY0          0x3b
#define U3PHY1          0x3c
#define USBC0           0x3d
#define VCD             0x3e
#define VCE             0x3f

#define CM4             0x40
#define STC0            0x41
#define STC_AV0         0x42
#define STC_AV1         0x43
#define STC_AV2         0x44
#define MAILBOX         0x45
#define PAI             0x46
#define PAII            0x47
#define DDRPHY          0x48
#define DDRCTL          0x49
#define I2CM0           0x4a
#define SPI_COMBO_0     0x4b
#define SPI_COMBO_1     0x4c
#define SPI_COMBO_2     0x4d
#define SPI_COMBO_3     0x4e
#define SPI_COMBO_4     0x4f

#define SPI_COMBO_5     0x50
#define CSIIW0          0x51
#define MIPICSI0        0x52
#define CSIIW1          0x53
#define MIPICSI1        0x54
#define CSIIW2          0x55
#define MIPICSI2        0x56
#define CSIIW3          0x57
#define MIPICSI3        0x58
#define VCL             0x59
#define DISP_PWM        0x5a
#define I2CM1           0x5b
#define I2CM2           0x5c
#define I2CM3           0x5d
#define I2CM4           0x5e
#define I2CM5           0x5f

#define UA6             0x60
#define UA7             0x61
#define UA8             0x62
#define AUD             0x63
#define VIDEO_CODEC     0x64

#define CLK_MAX         0x65

/* additional clks: not listed @ above */
#define AC(i)           (CLK_MAX + i)

#define VCLCORE0        AC(0)
#define VCLCORE1        AC(1)
#define VCLCORE2        AC(2)

#define AC_MAX          3

/* plls */
#define PLL(i)          (CLK_MAX + AC_MAX + i)

#define PLLS            PLL(0)
#define PLLC            PLL(1)
#define PLLN            PLL(2)
#define PLLH            PLL(3)
#define PLLD            PLL(4)
#define PLLA            PLL(5)

#define PLL_MAX         6

#endif
