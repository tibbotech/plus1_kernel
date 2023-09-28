// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
//#define DEBUG
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <dt-bindings/clock/sp-sp7350.h>
#include <linux/nvmem-consumer.h>

//#define TRACE	pr_info("### %s (%s): %d\n", __FUNCTION__, clk_hw_get_name(hw), __LINE__)
#define TRACE

#define EXT_CLK "extclk"

#define MASK_SET(shift, width, value) \
({ \
	u32 m = ((1 << (width)) - 1) << (shift); \
	(m << 16) | (((value) << (shift)) & m); \
})
#define MASK_GET(shift, width, value)	(((value) >> (shift)) & ((1 << (width)) - 1))

#define REG(i)	(pll_regs + (i) * 4)

#define PLLA_CTL	REG(0)
#define PLLC_CTL	REG(5)
#define PLLL3_CTL	REG(8)
#define PLLD_CTL	REG(11)
#define PLLH_CTL	REG(14)
#define PLLN_CTL	REG(17)
#define PLLS_CTL	REG(20)

#define IS_PLLA()	(pll->reg == PLLA_CTL)
#define IS_PLLC()	(pll->reg == PLLC_CTL)
#define IS_PLLL3()	(pll->reg == PLLL3_CTL)
#define IS_PLLD()	(pll->reg == PLLD_CTL)
#define IS_PLLH()	(pll->reg == PLLH_CTL)
#define IS_PLLN()	(pll->reg == PLLN_CTL)
#define IS_PLLS()	(pll->reg == PLLS_CTL)
#define IS_PLLHS()	(IS_PLLH() || IS_PLLS())
#define NO_PSTDIV()	(IS_PLLHS())

#define BP	0	/* Reg 0 */
#define PREDIV	1
#define PSTDIV	3
#define FBKDIV	7
#define PRESCL	15
#define BNKSEL	0	/* Reg 1 */
#define PD_N	2

#define FBKDIV_WIDTH	6 /* bit[7:6] reserved */
#define FBKDIV_MIN	64
#define FBKDIV_MAX	(FBKDIV_MIN + BIT(FBKDIV_WIDTH) - 1)

struct sp_pll {
	struct clk_hw	hw;
	void __iomem	*reg;
	spinlock_t	lock;

	long	brate;
	u32	idiv; // struct divs[] index
	u32	fbkdiv; // 64~(64+63)
};
#define to_sp_pll(_hw)	container_of(_hw, struct sp_pll, hw)

static u32 mux_table[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f };
static u32 mux_table_sd[] = { 0x00, 0x01, 0x02, 0x03 };
static u32 mux_table_fl[] = { 0x00, 0x01, 0x02, 0x03, 0x06, 0x07 };
#define MAX_PARENTS ARRAY_SIZE(mux_table)

static struct clk *clks[CLK_MAX + PLL_MAX];
static struct clk_onecell_data clk_data;
static void __iomem *clk_regs;
static void __iomem *qctl_regs;

#define pll_regs	(clk_regs + 31 * 4)	/* G3.0  ~ PLL */

/************************************************* QCTL *************************************************/
// QCTL G30
#define Q_CA55		0x0100
#define Q_CSDBG		0x0200
#define Q_CSETR		0x0300
#define Q_NPU		0x0400
#define Q_RSV_2		0x0500
#define Q_AXI_DMA	0x0600
#define Q_RSV_3		0x0700
#define Q_CPIOR0	0x0800
#define Q_SEC		0x0900
#define Q_USB30C0	0x0a00
#define Q_USBC0		0x0b00
#define Q_CARD0		0x0c00
#define Q_CARD1		0x0d00
#define Q_CARD2		0x0e00
#define Q_SPI_NOR	0x0f00
#define Q_SPI_NOR_SL	0x1000
#define Q_NBS		0x1100
#define Q_SPI_ND	0x1200
#define Q_RSV_4		0x1300
#define Q_GMAC		0x1400
#define Q_RSV_5		0x1500
#define Q_VI23_CSII	0x1600
#define Q_VCL		0x1700
#define Q_VCE		0x1800
#define Q_VCD		0x1900
#define Q_IMGREAD0	0x1a00
#define Q_OSD0		0x1b00
#define Q_OSD1		0x1c00
#define Q_OSD2		0x1d00
#define Q_OSD3		0x1e00
#define Q_VI0_CSII	0x1f00
#define Q_VI1_CSII	0x2000
#define Q_VI4_CSII	0x2100
#define Q_VI5_CSII	0x2200
#define Q_RSV_6		0x2300
#define Q_CM4		0x2400
#define Q_AUD		0x2500
#define Q_AHB_DMA	0x2600
#define Q_RSV_7		0x2700
#define Q_CA55SCUL3	0x2800
#define Q_CA55PDBG	0x2900
#define Q_NPU_INTER	0x2a00
#define Q_RSV_8		0x2b00
#define Q_CPIOR1	0x2c00

#define QACCEPT(s)	(s & 1)
#define QACTIVE(s)	(s & 2)
#define QDENY(s)	(s & 4)

static void QREQ(u32 n, u32 s)
{
	u32 shift = (n & 1) ? 0 : 8;
	void __iomem *r = qctl_regs + (n >> 1) * 4;
	writel(((0x10000 | s) << 3) << shift, r);
}

static u32 QSTA(u32 n)
{
	u32 shift = (n & 1) ? 0 : 8;
	void __iomem *r = qctl_regs + (n >> 1) * 4;

	return (readl(r) >> shift) & 7;
}

static void sp_qctrl_power_up(u32 qctl)
{
	QREQ(qctl, 1);
}

static void sp_qctrl_power_down(u32 qctl)
{
	u32 s; // QCTL[2:0] status

	while (1) {
		QREQ(qctl, 0);
		while (1) {
			s = QSTA(qctl);
			if (!QACCEPT(s) && !QDENY(s)) { // Q_STOPPED
				return;
			}
			else if (QACCEPT(s) && QDENY(s)) { // Q_DENIED
				break;
			}
		}

		// re-enter Q_RUN
		QREQ(qctl, 1);
		while (1) {
			s = QSTA(qctl);
			if (QACCEPT(s) && !QDENY(s)) // Q_RUN
				break;
		}
	}
}

/************************************************ SP_CLK ************************************************/

struct sp_clk {
	const char *name;
	u32 id;		/* defined in sp-sp7350.h, also for gate (reg_idx<<4)|(shift) */
	u32 mux;	/* mux reg_idx: based from G3.0 */
	u32 shift;	/* mux shift */
	u32 width;	/* mux width */
	const char *parent_names[MAX_PARENTS];
};

static const char * const default_parents[] = { EXT_CLK };

#define _(id, ...)	{ #id, id, ##__VA_ARGS__ }

static const char *gmac_parents_alt[] = {"PLLS_500", "PLLS_50", "PLLS_2P5"}; // for MO_GMAC_PHYSEL G3.23[12] == 1

static struct sp_clk sp_clks[] = {
	_(SYSTEM,	24,	0,	2, {"PLLS_500", "PLLS_333", "PLLS_400"}),
	_(CA55CORE0,	29,	0,	3, {"PLLC", "PLLC_D2", "PLLC_D3", "PLLC_D4"}),
	_(CA55CORE1,	29,	3,	3, {"PLLC", "PLLC_D2", "PLLC_D3", "PLLC_D4"}),
	_(CA55CORE2,	29,	6,	3, {"PLLC", "PLLC_D2", "PLLC_D3", "PLLC_D4"}),
	_(CA55CORE3,	29,	9,	3, {"PLLC", "PLLC_D2", "PLLC_D3", "PLLC_D4"}),
	_(CA55CUL3,	29,	12,	3, {"PLLL3", "PLLL3_D2", "PLLL3_D3", "PLLL3_D4"}),
	_(CA55),
	_(GMAC,		Q_GMAC|23,	10,	2, {"PLLS_125", "PLLS_25", "PLLS_2P5"}),
	_(RBUS,		23,	6,	2, {"PLLS_400", "PLLS_100", "PLLS_200"}),
	_(RBUS_BLOCKB),
	_(SYSTOP),
	_(THERMAL),
	_(BR0),
	_(CARD_CTL0,	Q_CARD0|23,	0,	2, {"PLLS_400", "PLLH_358", "PLLS_800", "PLLH_716"}),
	_(CARD_CTL1,	Q_CARD1|23,	4,	2, {"PLLS_400", "PLLH_358", "PLLS_800", "PLLH_716"}),
	_(CARD_CTL2,	Q_CARD2|23,	2,	2, {"PLLS_400", "PLLH_358", "PLLS_800", "PLLH_716"}),
	_(CBDMA0),
	_(CPIOR,	Q_CPIOR0),
	_(DDRPHY,	0,	0,	0, {"PLLD"}),
	_(TZC),
	_(DDRCTL,	0,	0,	0, {"SYSAO"}),
	_(DRAM,		0,	0,	0, {"PLLD"}),
	_(VCL,		Q_VCL),
	_(VCL0,		Q_VCL|24,	2,	4, {"PLLH_614", "PLLS_400", "PLLH_307", "PLLS_200", "PLLS_500"}),
	_(VCL1,		Q_VCL|24,	6,	3, {"PLLS_500", "PLLH_307", "PLLS_200", "PLLS_400"}),
	_(VCL2,		Q_VCL|24,	9,	3, {"PLLS_400", "PLLS_200", "PLLH_307", "PLLS_100"}),
	_(VCL3,		Q_VCL|24,	12,	4, {"PLLH_614", "PLLS_400", "PLLH_307", "PLLS_200", "PLLS_500"}),
	_(VCL4,		Q_VCL),
	_(VCL5,		Q_VCL|25,	0,	1, {"PLLS_400", "PLLS_200"}),
	_(DUMMY_MASTER0),
	_(DUMMY_MASTER1),
	_(DUMMY_MASTER2),
	_(DISPSYS),
	_(DMIX),
	_(GPOST0),
	_(GPOST1),
	_(GPOST2),
	_(GPOST3),
	_(IMGREAD0,	Q_IMGREAD0),
	_(MIPITX,	25,	7,	5, {"PLLH_MIPI", "PLLH_MIPI_D2", "PLLH_MIPI_D4", "PLLH_MIPI_D8", "PLLH_MIPI_D16", "PLLH"}),
	_(OSD0,		Q_OSD0),
	_(OSD1,		Q_OSD1),
	_(OSD2,		Q_OSD2),
	_(OSD3,		Q_OSD3),
	_(TCON,		25,	7,	5, {"PLLH_MIPI", "PLLH_MIPI_D2", "PLLH_MIPI_D4", "PLLH_MIPI_D8", "PLLH_MIPI_D16", "PLLH"}),
	_(TGEN),
	_(VPOST0),
	_(VSCL0),
	_(MIPICSI0),
	_(MIPICSI1),
	_(MIPICSI2),
	_(MIPICSI3),
	_(MIPICSI4),
	_(MIPICSI5),
	_(VI0_CSIIW0,	Q_VI0_CSII),
	_(VI0_CSIIW1,	Q_VI0_CSII),
	_(VI1_CSIIW0,	Q_VI1_CSII),
	_(VI1_CSIIW1,	Q_VI1_CSII),
	_(VI2_CSIIW0,	Q_VI23_CSII),
	_(VI2_CSIIW1,	Q_VI23_CSII),
	_(VI3_CSIIW0,	Q_VI23_CSII),
	_(VI3_CSIIW1,	Q_VI23_CSII),
	_(VI3_CSIIW2,	Q_VI23_CSII),
	_(VI3_CSIIW3,	Q_VI23_CSII),
	_(VI4_CSIIW0,	Q_VI4_CSII),
	_(VI4_CSIIW1,	Q_VI4_CSII),
	_(VI4_CSIIW2,	Q_VI4_CSII),
	_(VI4_CSIIW3,	Q_VI4_CSII),
	_(VI5_CSIIW0,	Q_VI5_CSII),
	_(VI5_CSIIW1,	Q_VI5_CSII),
	_(VI5_CSIIW2,	Q_VI5_CSII),
	_(VI5_CSIIW3,	Q_VI5_CSII),
	_(MIPICSI23_SEL),
	_(VI23_CSIIW1,	Q_VI23_CSII),
	_(VI23_CSIIW2,	Q_VI23_CSII),
	_(VI23_CSIIW3,	Q_VI23_CSII),
	_(VI23_CSIIW0,	Q_VI23_CSII),
	_(OTPRX),
	_(PRNG),
	_(SEMAPHORE),
	_(INTERRUPT),
	_(SPIFL,	Q_SPI_NOR|25,	1,	3, {"PLLH_716", "PLLH_358", "PLLH_537", "PLLH_268", "PLLH_614", "PLLH_307"}),
	_(BCH),
	_(SPIND,	Q_SPI_ND|25,	4,	3, {"PLLH_716", "PLLH_358", "PLLH_537", "PLLH_268", "PLLH_614", "PLLH_307"}),
	_(UADMA01),
	_(UADMA23),
	_(UA0,		27,	10,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UA1,		27,	12,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UA2,		27,	14,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UA3,		28,	0,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UADBG,	23,	8,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	//_(UART2AXI,	23,	8,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UPHY0),
	_(USB30C0,	Q_USB30C0),
	_(U3PHY0,	25,	14,	2, {EXT_CLK, "PLLS_50", "PLLS_100"}),
	_(USBC0,	Q_USBC0),
	_(VCD,		Q_VCD,	0,	0, {"PLLH_358"}),
	_(VCE,		Q_VCE,	0,	0, {"PLLH_537"}),
	_(VIDEO_CODEC),
	_(MAILBOX),
	_(AXI_DMA,	Q_AXI_DMA),
	_(PNAND,	23,	13,	2, {"PLLS_400", "PLLS_100", "PLLS_200"}),
	_(SEC,		Q_SEC),
	//_(rsv1),
	_(STC_AV3),
	_(STC_TIMESTAMP),
	_(STC_AV4),
	_(NICTOP),
	_(NICPAII),
	_(NICPAI),
	_(NPU,		Q_NPU|23,	15,	1, {"PLLN", "PLLN_D2"}),
	_(SECGRP_PAI),
	_(SECGRP_PAII),
	_(SECGRP_MAIN),
	_(DPLL),
	_(HBUS),
	_(AUD,		Q_AUD,	0,	0, {"PLLA"}),
	_(AXIM_TOP),
	_(AXIM_PAI),
	_(AXIM_PAII),
	_(SYSAO,	28,	6,	3, {"PLLS_200", "PLLS_50", EXT_CLK, "PLLS_100"}),
	_(PMC),
	_(RTC),
	_(INTERRUPT_AO),
	_(UA6,		28,	2,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(UA7,		28,	4,	2, {"PLLS_200", "PLLS_100", EXT_CLK}),
	_(GDMAUA),
	_(CM4,		Q_CM4|27,	2,	3, {"PLLS_400", "PLLS_100", "PLLS_200", EXT_CLK}),
	_(STC0),
	_(STC_AV0),
	_(STC_AV1),
	_(STC_AV2),
	_(AHB_DMA,	Q_AHB_DMA),
	_(SAR12B,	0,	0,	0, {"PLLS_100"}),
	_(DISP_PWM,	27,	5,	1, {EXT_CLK, "PLLS_200"}),
	_(NICPIII),
	_(GPIO_AO_INT),
	_(I2CM0,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM1,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM2,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM3,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM4,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM5,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM6,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM7,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM8,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(I2CM9,	27,	6,	2, {"PLLS_100", "PLLS_50", EXT_CLK}),
	_(SPICB0,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
	_(SPICB1,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
	_(SPICB2,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
	_(SPICB3,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
	_(SPICB4,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
	_(SPICB5,	27,	8,	2, {"PLLS_400", "PLLS_200", "PLLS_100"}),
#if 0 // these clocks belong QCTRL, always enabled, not used by user
	_(PD_AXI_DMA),
	_(PD_CA55),
	_(PD_CARD0),
	_(PD_CARD1),
	_(PD_CARD2),
	_(PD_CBDMA0),
	_(PD_CPIOR0),
	_(PD_CPIOR1),
	_(PD_CSDBG),
	_(PD_CSETR),
	_(PD_DUMMY0),
	_(PD_DUMMY1),
	_(PD_GMAC),
	_(PD_IMGREAD0),
	_(PD_NBS),
	_(PD_NPU),
	_(PD_OSD0),
	_(PD_OSD1),
	_(PD_OSD2),
	_(PD_OSD3),
	_(PD_SEC),
	_(PD_SPI_ND),
	_(PD_SPI_NOR),
	_(PD_UART2AXI),
	_(PD_USB30C0),
	_(PD_USBC0),
	_(PD_VCD),
	_(PD_VCE),
	_(PD_VCL),
	_(PD_VI0_CSII),
	_(PD_VI1_CSII),
	_(PD_VI23_CSII),
	_(PD_VI4_CSII),
	_(PD_VI5_CSII),
	_(PD_AHB_DMA),
	_(PD_AUD),
	_(PD_CM4),
	_(PD_HWUA_TX_GDMA),
	_(QCTRL),
#endif
};

#define MEMCTL_HWM	0x03ff0000
#define ca55_memctl	(clk_regs + (31 + 32 + 14) * 4)	/* G4.14 */

void sp_clkc_ca55_memctl(u32 val)
{
	val |= MEMCTL_HWM;
	//pr_debug(">>> write ca55_memctl(MOON4.14) to %08x", val);
	writel(val, ca55_memctl);
}
EXPORT_SYMBOL(sp_clkc_ca55_memctl);

/************************************************* PLL_A *************************************************/

/* from PLLA_Q654_REG_setting_220701.xlsx with LKDTOUT & TEST bits set to 0 */
struct {
	u32 rate;
	u32 regs[5];
} pa[] = {
	{
		.rate = 135475200,
		.regs = {
			0x5200,
			0xf02c,
			0x0c21,
			0x3fd0,
			0x0154,
		}
	},
	{
		.rate = 147456000,
		.regs = {
			0x5200,
			0xf02c,
			0x1f51,
			0x3fd0,
			0x0168,
		}
	},
};

static ulong plla_get_rate(struct sp_pll *pll)
{
	pll->idiv = (readl(pll->reg + 8) == pa[1].regs[2]);
	return pa[pll->idiv].rate;
}

static void plla_set_rate(struct sp_pll *pll)
{
	const u32 *pp = pa[pll->idiv].regs;
	int i;

	for (i = 0; i < ARRAY_SIZE(pa->regs); i++) {
		writel(0xffff0000 | pp[i], pll->reg + (i * 4));
	}
}

static ulong plla_round_rate(struct sp_pll *pll, ulong rate)
{
	int i = ARRAY_SIZE(pa);

	while (--i) {
		if (rate >= pa[i].rate)
			break;
	}
	pll->idiv = i;
	return pa[i].rate;
}

/************************************************* SP_PLL *************************************************/

struct sp_div {
	u32 div2;
	u32 bits;
};

#define BITS(prediv, prescl, pstdiv) ((prediv << PREDIV)|(prescl << PRESCL)|(pstdiv << PSTDIV))
#define DIV(prediv, prescl, pstdiv) \
{ \
	prediv * 2 / prescl * pstdiv, \
	BITS((prediv - 1), (prescl - 1), (pstdiv - 1)) \
}

static const struct sp_div divs_0[] = {
	DIV(1, 2, 1), // 1
	DIV(1, 1, 1), // 2
	DIV(2, 1, 1), // 4
	DIV(2, 1, 2), // 8
};

#define DIVD(prediv, prescl, pstdiv) \
{ \
	prediv * 2 / prescl * pstdiv_d[pstdiv], \
	BITS((prediv - 1), (prescl - 1), pstdiv) \
}

static const int pstdiv_d[] = {1, 3, 6, 12};
static const struct sp_div divs_d[] = {
	DIVD(1, 2, 0),	// 1
	DIVD(1, 1, 0),	// 2
	DIVD(1, 2, 1),	// 3
	DIVD(2, 1, 0),	// 4
	DIVD(1, 1, 1),	// 6
	DIVD(2, 1, 1),	// 12
	DIVD(2, 1, 2),	// 24
	DIVD(2, 1, 3),	// 48
};

#define divs		(IS_PLLD() ? divs_d : divs_0)
#define divs_size	(IS_PLLD() ? ARRAY_SIZE(divs_d) : ARRAY_SIZE(divs_0))

static ulong sp_pll_calc_div(struct sp_pll *pll, ulong rate)
{
	ulong ret = 0, mr = 0;
	int mi = 0, md = 0x7fffffff;
	int i = NO_PSTDIV() ? 3 : divs_size;

	//pr_info("[%s] calc_rate: %lu\n", clk_hw_get_name(&pll->hw), rate);

	while (i--) {
		long br = pll->brate * 2 / divs[i].div2;
		long d, rr;

		ret = DIV_ROUND_CLOSEST(rate, br);
		if (ret < FBKDIV_MIN)
			ret = FBKDIV_MIN;
		else if (ret > FBKDIV_MAX)
			ret = FBKDIV_MAX;

		rr = br * ret;
		if (rr <= rate)
			d = rate - rr;
		else if (rr > rate)
			d = rr - rate;
		//pr_info(">>>%u>>> %ld * %ld = %ld - %lu = %ld\n", div->div2, br, ret, br * ret, rate, d);

		if (d < md) {
			mi = i;
			mr = ret;
			if (!d) break;
			md = d;
		}
	}

	pll->idiv = mi;
	return mr;
}

static long sp_pll_round_rate(struct clk_hw *hw, ulong rate,
		ulong *prate)
{
	struct sp_pll *pll = to_sp_pll(hw);
	long ret;

	//TRACE;
	//pr_info("round_rate: %lu %lu\n", rate, *prate);

	if (rate == *prate)
		ret = *prate; /* bypass */
	else if (IS_PLLA())
		ret = plla_round_rate(pll, rate);
	else {
		ret = sp_pll_calc_div(pll, rate);
		ret = pll->brate * 2 / divs[pll->idiv].div2 * ret;
	}

	return ret;
}

static ulong sp_pll_recalc_rate(struct clk_hw *hw,
		ulong prate)
{
	struct sp_pll *pll = to_sp_pll(hw);
	u32 reg = readl(pll->reg);
	ulong ret;

	//TRACE;
	//pr_info("%08x\n", reg);
	if (readl(pll->reg) & BIT(BP)) // bypass ?
		ret = prate;
	else if (IS_PLLA())
		ret = plla_get_rate(pll);
	else {
		u32 fbkdiv = MASK_GET(FBKDIV, FBKDIV_WIDTH, reg) + 64;
		u32 prediv = MASK_GET(PREDIV, 2, reg) + 1;
		u32 prescl = MASK_GET(PRESCL, 1, reg) + 1;
		u32 pstdiv = MASK_GET(PSTDIV, 2, reg) + 1;

		ret = pll->brate / prediv * fbkdiv * prescl;
		if (!NO_PSTDIV())
			ret /= IS_PLLD() ? pstdiv_d[pstdiv - 1] : pstdiv;
	}
	//pr_info("[%s] recalc_rate: %lu %lu -> %lu\n", clk_hw_get_name(&pll->hw), pll->brate, prate, ret);

	return ret;
}

static void sp_pll_set_bnksel(struct sp_pll *pll, u32 reg)
{
	/* bnksel */
	u32 fbkdiv = MASK_GET(FBKDIV, FBKDIV_WIDTH, reg) + 64;
	u32 prediv = MASK_GET(PREDIV, 2, reg) + 1;
	u32 prescl = MASK_GET(PRESCL, 1, reg) + 1;
	u32 bnksel;
	long fvco = pll->brate / prediv * fbkdiv * prescl * (IS_PLLN() ? 2 : 1);
	if (fvco < 1500000000)		// 1.5G
		bnksel = 0;
	else if (fvco < 2000000000)	// 2G
		bnksel = 1;
	else if (fvco < 2500000000)	// 2.5G
		bnksel = 2;
	else
		bnksel = 3;
	//pr_info("write: fvco=%ld bnksel=%d\n", fvco, bnksel);
	writel(bnksel | 0x00030000, pll->reg + 4);
}

static int sp_pll_set_rate(struct clk_hw *hw, ulong rate,
		ulong prate)
{
	struct sp_pll *pll = to_sp_pll(hw);
	ulong flags;
	u32 reg;

	//TRACE;
	//pr_info("set_rate: %lu -> %lu (%d)\n", prate, rate, readl(pll->reg + 8) & 0x100);

	spin_lock_irqsave(&pll->lock, flags);

	reg = BIT(BP + 16); // BP_HIWORD_MASK

	if (rate == prate) {
		reg |= BIT(BP);
		writel(reg, pll->reg); // set bp
	} else if (IS_PLLA())
		plla_set_rate(pll);
	else {
		u32 fbkdiv = sp_pll_calc_div(pll, rate) - FBKDIV_MIN;

		reg |= IS_PLLHS() ? 0xffc60000 : 0xfffe0000; // HIWORD_MASK
		reg |= MASK_SET(FBKDIV, FBKDIV_WIDTH, fbkdiv) | divs[pll->idiv].bits;

		if (IS_PLLC())
			writel(0x00010001, pll_regs + 30 * 4);  // G3.30[0] = 1
		else if (IS_PLLL3())
			writel(0x80008000, pll_regs + 29 * 4);  // G3.29[15] = 1

		writel(reg, pll->reg);
		sp_pll_set_bnksel(pll, reg);

		if (IS_PLLC() || IS_PLLL3()) {
#if 1 // FIXME: clock ready signal always 0 @ ZEBU
			do {
				reg = readl(pll->reg + 8) & 0x100;
				//pr_debug("%u", reg);
			} while (!reg); // wait clock ready
#endif
			if (IS_PLLC())
				writel(0x00010000, pll_regs + 30 * 4);  // G3.30[0] = 0
			else
				writel(0x80000000, pll_regs + 29 * 4);  // G3.29[15] = 0
		}
	}

	spin_unlock_irqrestore(&pll->lock, flags);

	return 0;
}
#if 0
static int sp_pll_enable(struct clk_hw *hw)
{
	struct sp_pll *pll = to_sp_pll(hw);
	ulong flags;

	TRACE;
	spin_lock_irqsave(&pll->lock, flags);
	writel(BIT(PD_N + 16) | BIT(PD_N), pll->reg + 4); /* power up */
	spin_unlock_irqrestore(&pll->lock, flags);

	return 0;
}

static void sp_pll_disable(struct clk_hw *hw)
{
	struct sp_pll *pll = to_sp_pll(hw);
	ulong flags;

	TRACE;
	spin_lock_irqsave(&pll->lock, flags);
	writel(BIT(PD_N + 16), pll->reg + 4); /* power down */
	spin_unlock_irqrestore(&pll->lock, flags);
}

static int sp_pll_is_enabled(struct clk_hw *hw)
{
	struct sp_pll *pll = to_sp_pll(hw);

	return readl(pll->reg + 4) & BIT(PD_N);
}
#endif
static const struct clk_ops sp_pll_ops = {
#if 0
	.enable = sp_pll_enable,
	.disable = sp_pll_disable,
	.is_enabled = sp_pll_is_enabled,
#endif
	.round_rate = sp_pll_round_rate,
	.recalc_rate = sp_pll_recalc_rate,
	.set_rate = sp_pll_set_rate,
};

void pr_clk(struct clk *clk)
{
	pr_info("%-20s%lu\n", __clk_get_name(clk), clk_get_rate(clk));
}

struct clk *clk_register_sp_pll(const char *name, void __iomem *reg)
{
	struct sp_pll *pll;
	struct clk *clk;
	//ulong flags = 0;
	const char *parent = EXT_CLK;
	struct clk_init_data initd = {
		.name = name,
		.parent_names = &parent,
		.ops = &sp_pll_ops,
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED
	};

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->hw.init = &initd;
	pll->reg = reg;
	pll->brate = IS_PLLN() ? (XTAL / 2) : XTAL;
	spin_lock_init(&pll->lock);

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk))) {
		kfree(pll);
	} else {
		pr_clk(clk);
		clk_register_clkdev(clk, NULL, name);
	}

	return clk;
}

/********************************************** SP_CLK_GATE **********************************************/

static int sp_clk_gate_enable(struct clk_hw *hw)
{
	struct clk_gate *gate = to_clk_gate(hw);
	u32 qctl_flag = (&gate->flags)[1];

	//pr_info("%02x %02x\n", gate->flags, qctl_flag);
	if (qctl_flag)
		sp_qctrl_power_up(qctl_flag - 1);

	return clk_gate_ops.enable(hw);
}

static void sp_clk_gate_disable(struct clk_hw *hw)
{
	struct clk_gate *gate = to_clk_gate(hw);
	u32 qctl_flag = (&gate->flags)[1];

	//pr_info("%02x %02x\n", gate->flags, qctl_flag);
	if (qctl_flag)
		sp_qctrl_power_down(qctl_flag - 1);

	clk_gate_ops.disable(hw);
}

static const struct clk_ops sp_clk_gate_ops = {
	.enable = sp_clk_gate_enable,
	.disable = sp_clk_gate_disable,
	.is_enabled = clk_gate_is_enabled,
};

static struct clk *clk_register_sp_clk(struct sp_clk *sp_clk)
{
	const char * const *parent_names = sp_clk->parent_names[0] ? sp_clk->parent_names : default_parents;
	struct clk_mux *mux = NULL;
	struct clk_gate *gate;
	struct clk *clk;
	int num_parents = sp_clk->width + 1;
	unsigned long flags = CLK_IGNORE_UNUSED;

	if (sp_clk->id == CA55CUL3 || sp_clk->id == AUD || sp_clk->id == NPU)
		flags |= CLK_SET_RATE_PARENT;

	if (sp_clk->width) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			return ERR_PTR(-ENOMEM);
		mux->reg = pll_regs + (sp_clk->mux & 0xff) * 4;
		mux->shift = sp_clk->shift;
		mux->mask = BIT(sp_clk->width) - 1;
		mux->table = mux_table;
		switch (sp_clk->id) {
			case CARD_CTL0:
			case CARD_CTL1:
			case CARD_CTL2:
				mux->table = mux_table_sd;
				num_parents = ARRAY_SIZE(mux_table_sd);
				break;
			case SPIFL:
			case SPIND:
				mux->table = mux_table_fl;
				num_parents = ARRAY_SIZE(mux_table_fl);
				break;
			case GMAC:
				if (readl(pll_regs + 23 * 4) & BIT(12)) // MO_GMAC_PHYSEL G3.23[12]
					parent_names = gmac_parents_alt;
				break;
		}
		mux->flags = CLK_MUX_HIWORD_MASK | CLK_MUX_ROUND_CLOSEST;
	}

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate) {
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}
	gate->reg = clk_regs + (sp_clk->id >> 4 << 2);
	gate->bit_idx = sp_clk->id & 0x0f;
	gate->flags = CLK_GATE_HIWORD_MASK;
	(&gate->flags)[1] = sp_clk->mux >> 8; // pass Q_XXXX

	clk = clk_register_composite(NULL, sp_clk->name,
				     parent_names, num_parents,
				     mux ? &mux->hw : NULL, &clk_mux_ops,
				     NULL, NULL,
				     &gate->hw, &sp_clk_gate_ops,
				     flags);
	if (IS_ERR(clk)) {
		kfree(mux);
		kfree(gate);
	}

	return clk;
}

#define SP_REGISTER(pll)	\
	clks[pll] = clk_register_sp_pll(#pll, pll##_CTL)

static void __init sp_clkc_init(struct device_node *np)
{
	int i;

	pr_info("sp-clkc init\n");

	clk_regs = of_iomap(np, 0);
	if (WARN_ON(!clk_regs)) {
		pr_warn("sp-clkc regs missing.\n");
		return; // -EIO
	}
	pr_debug("sp-clkc: clk_regs = %px", clk_regs);

	qctl_regs = of_iomap(np, 1);
	if (WARN_ON(!qctl_regs)) {
		pr_warn("sp-clkc qctl regs missing.\n");
		return; // -EIO
	}
	pr_debug("sp-clkc: qctl_regs = %px", qctl_regs);

	/* enable all clks */
	for (i = 0; i < 12; i++)
		writel(0xffffffff, clk_regs + i * 4);

	/* PLLs */
	SP_REGISTER(PLLA);
	SP_REGISTER(PLLC);
	SP_REGISTER(PLLL3);
	SP_REGISTER(PLLD);
	SP_REGISTER(PLLH);
	SP_REGISTER(PLLN);
	SP_REGISTER(PLLS);

	pr_debug("sp-clkc: register fixed_rate/factor");
	/* fixed frequency & fixed factor */
	clk_register_fixed_factor(NULL, "PLLC_D2", "PLLC", 0, 1, 2);
	clk_register_fixed_factor(NULL, "PLLC_D3", "PLLC", 0, 1, 3);
	clk_register_fixed_factor(NULL, "PLLC_D4", "PLLC", 0, 1, 4);

	clk_register_fixed_factor(NULL, "PLLL3_D2", "PLLL3", 0, 1, 2);
	clk_register_fixed_factor(NULL, "PLLL3_D3", "PLLL3", 0, 1, 3);
	clk_register_fixed_factor(NULL, "PLLL3_D4", "PLLL3", 0, 1, 4);

	clk_register_fixed_factor(NULL, "PLLN_D2", "PLLN", 0, 1, 2);

	clk_register_fixed_factor(NULL, "PLLH_716", "PLLH", 0, 1, 3);
	clk_register_fixed_factor(NULL, "PLLH_614", "PLLH", 0, 2, 7);
	clk_register_fixed_factor(NULL, "PLLH_537", "PLLH", 0, 1, 4);
	clk_register_fixed_factor(NULL, "PLLH_358", "PLLH", 0, 1, 6);
	clk_register_fixed_factor(NULL, "PLLH_307", "PLLH", 0, 1, 7);
	clk_register_fixed_factor(NULL, "PLLH_268", "PLLH", 0, 1, 8);
	clk_register_fixed_factor(NULL, "PLLH_MIPI", "PLLH", 0, 1, 9);
	clk_register_fixed_factor(NULL, "PLLH_MIPI_D2",  "PLLH_MIPI", 0, 1, 2);
	clk_register_fixed_factor(NULL, "PLLH_MIPI_D4",  "PLLH_MIPI", 0, 1, 4);
	clk_register_fixed_factor(NULL, "PLLH_MIPI_D8",  "PLLH_MIPI", 0, 1, 8);
	clk_register_fixed_factor(NULL, "PLLH_MIPI_D16", "PLLH_MIPI", 0, 1, 16);

	clk_register_fixed_factor(NULL, "PLLS_800", "PLLS", 0, 2, 5);
	clk_register_fixed_factor(NULL, "PLLS_500", "PLLS", 0, 1, 4);
	clk_register_fixed_factor(NULL, "PLLS_400", "PLLS", 0, 1, 5);
	clk_register_fixed_factor(NULL, "PLLS_333", "PLLS", 0, 1, 6);
	clk_register_fixed_factor(NULL, "PLLS_200", "PLLS", 0, 1, 10);
	clk_register_fixed_factor(NULL, "PLLS_125", "PLLS", 0, 1, 16);
	clk_register_fixed_factor(NULL, "PLLS_100", "PLLS", 0, 1, 20);
	clk_register_fixed_factor(NULL, "PLLS_50",  "PLLS", 0, 1, 40);
	clk_register_fixed_factor(NULL, "PLLS_25",  "PLLS", 0, 1, 80);
	clk_register_fixed_factor(NULL, "PLLS_2P5", "PLLS", 0, 1, 800);

	pr_debug("sp-clkc: register sp_clks");
	/* sp_clks */
	for (i = 0; i < ARRAY_SIZE(sp_clks); i++) {
		struct sp_clk *sp_clk = &sp_clks[i];
		int j = sp_clk->id;

		clks[j] = clk_register_sp_clk(sp_clk);
	}

#if 0 // test
	printk("TEST:\n");
	clk_set_rate(clks[NPU], 900000000);
	pr_clk(clks[NPU]);
	pr_clk(clks[PLLN]);
	pr_info("%04x %04x %04x\n", readl(PLLN_CTL), readl(PLLN_CTL+4), readl(PLLN_CTL+8));
	clk_set_rate(clks[NPU], 500000000);
	pr_clk(clks[NPU]);
	pr_clk(clks[PLLN]);
	pr_info("%04x %04x %04x\n", readl(PLLN_CTL), readl(PLLN_CTL+4), readl(PLLN_CTL+8));
#endif

	pr_debug("sp-clkc: of_clk_add_provider");
	clk_data.clks = clks;
	clk_data.clk_num = ARRAY_SIZE(clks);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);
}

CLK_OF_DECLARE(sp_clkc, "sunplus,sp7350-clkc", sp_clkc_init);
