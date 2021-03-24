#ifndef _DT_BINDINGS_CLOCK_SUNPLUS_Q645_H
#define _DT_BINDINGS_CLOCK_SUNPLUS_Q645_H

/* plls */
#define PLL_A       0
#define PLL_E       1
#define PLL_E_2P5   2
#define PLL_E_25    3
#define PLL_E_112P5 4
#define PLL_F       5
#define PLL_TV      6
#define PLL_TV_A    7
#define PLL_SYS     8
#define PLL_FLA     9
#define PLL_GPU     10
#define PLL_CPU     11

/* gates: mo_scfg_0 ~ mo_scfg_5 */
#define SYSTEM      0x10
#define CA55CORE0   0x11
#define CA55CORE1   0x12
#define CA55CORE2   0x13
#define CA55CORE3   0x14
#define CA55SCUL3   0x15
#define CA55        0x16
#define IOP         0x17
#define PBUS0       0x18
#define PBUS1       0x19
#define PBUS2       0x1a
#define PBUS3       0x1b
#define BR0         0x1c
#define CARD_CTL0   0x1d
#define CARD_CTL1   0x1e
#define CARD_CTL2   0x1f

#define CBDMA0      0x20
#define CPIOL       0x21
#define CPIOR       0x22
#define DDR_PHY0    0x23
#define SDCTRL0     0x24
#define DUMMY_MASTER0   0x25
#define DUMMY_MASTER1   0x26
#define DUMMY_MASTER2   0x27
#define EVDN        0x28
#define SDPROT0     0x29
#define UMCTL2      0x2a
#define GPU         0x2b
#define HSM         0x2c
#define RBUS_TOP    0x2d
#define SPACC       0x2e
#define INTERRUPT   0x2f

#define N78         0x30
#define NIC400      0x31
#define OTPRX       0x32
#define PMC         0x33
#define RBUS_L00    0x34
#define RBUS_L01    0x35
#define RBUS_L02    0x36
#define RBUS_L03    0x37
#define RTC         0x38
#define MIPZ        0x39
#define SPIFL       0x3a
#define BCH         0x3b
#define SPIND       0x3c
#define UADMA01     0x3d
#define UADMA23     0x3e
#define UA0         0x3f

#define UA1         0x40
#define UA2         0x41
#define UA3         0x42
#define UA4         0x43
#define UA5         0x44
#define UADBG       0x45
#define UART2AXI    0x46
#define GDMAUA      0x47
#define UPHY0       0x48
#define USB30C0     0x49
#define USB30C1     0x4a
#define U3PHY0      0x4b
#define U3PHY1      0x4c
#define USBC0       0x4d
#define VCD         0x4e
#define VCE         0x4f

#define CM4         0x50
#define STC0        0x51
#define STC_AV0     0x52
#define STC_AV1     0x53
#define STC_AV2     0x54
#define MAILBOX     0x55

#define CLK_MAX     0x60

#endif
