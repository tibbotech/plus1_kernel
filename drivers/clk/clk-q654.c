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
#include <dt-bindings/clock/sp-q654.h>
#include <linux/nvmem-consumer.h>

//#define TRACE	pr_info("### %s:%d (%d)\n", __FUNCTION__, __LINE__, (clk->reg - REG(4, 0)) / 4)
#define TRACE

#define EXT_CLK "extclk"

#define MASK_SET(shift, width, value) \
({ \
	u32 m = ((1 << (width)) - 1) << (shift); \
	(m << 16) | (((value) << (shift)) & m); \
})
#define MASK_GET(shift, width, value)	(((value) >> (shift)) & ((1 << (width)) - 1))

#define REG(i)	(pll_regs + (i) * 4)

#define PLLH_CTL	REG(1)
#define PLLN_CTL	REG(4)
#define PLLS_CTL	REG(7)
#define PLLC_CTL	REG(10)
#define PLLD_CTL	REG(13)
#define PLLA_CTL	REG(24)

#define IS_PLLH()	(clk->reg == PLLH_CTL)
#define IS_PLLA()	(clk->reg == PLLA_CTL)

#define PD_N	0
#define PREDIV	1
#define PRESCL	3
#define FBKDIV	4
#define PSTDIV	12

#define FBKDIV_WIDTH	8
#define FBKDIV_MIN	64
#define FBKDIV_MAX	(FBKDIV_MIN + BIT(FBKDIV_WIDTH) - 1)

struct sp_pll {
	struct clk_hw	hw;
	void __iomem	*reg;
	spinlock_t	lock;

	long brate;
	u32 idiv; // struct divs[] index
	u32 fbkdiv; // 64~(64+255)
	u32 bp; // bypass bit_idx
};
#define to_sp_pll(_hw)	container_of(_hw, struct sp_pll, hw)

static u32 mux_table[] = { 0x00, 0x01, 0x03, 0x07, 0x0f };
#define MAX_PARENTS ARRAY_SIZE(mux_table)

static struct clk *clks[CLK_MAX + AC_MAX + PLL_MAX];
static struct clk_onecell_data clk_data;
static void __iomem *moon_regs;

#define clk_regs	(moon_regs + 0x004) /* G0.1 ~ CLKEN */
#define mux_regs	(moon_regs + 0x100)	/* G2.0 ~ CLK_SEL */
#define pll_regs	(moon_regs + 0x200) /* G4.0 ~ PLL */

struct sp_clk {
	const char *name;
	u32 id;		/* defined in sp-q654.h, also for gate (reg_idx<<4)|(shift) */
	u32 mux;	/* mux reg_idx: MOON2.xx */
	u32 shift;	/* mux shift */
	u32 width;	/* mux width */
	const char *parent_names[MAX_PARENTS];
};

static const char * const default_parents[] = { EXT_CLK };

#define _(id, ...)	{ #id, id, ##__VA_ARGS__ }

static struct sp_clk sp_clks[] = {
	_(SYSTEM,		0,	0,	2, {"f_600m", "f_750m", "f_500m"}),
	_(CA55CORE0,	0,	6,	1, {"PLLC", "SYSTEM"}),
	_(CA55CORE1,	0,	11,	1, {"PLLC", "SYSTEM"}),
	_(CA55CORE2,	1,	0,	1, {"PLLC", "SYSTEM"}),
	_(CA55CORE3,	1,	5,	1, {"PLLC", "SYSTEM"}),
	_(CA55CUL3,		1,	10,	1, {"f_1200m", "SYSTEM"}),
	_(CA55),
	_(IOP),
	_(PBUS0),
	_(PBUS1),
	_(PBUS2),
	_(PBUS3),
	_(BR0),
	_(CARD_CTL0,	3,	7,	1, {"f_360m", "f_800m"}),
	_(CARD_CTL1,	3,	8,	1, {"f_360m", "f_800m"}),
	_(CARD_CTL2,	3,	9,	1, {"f_360m", "f_800m"}),

	_(CBDMA0),
	_(CPIOL),
	_(CPIOR),
	_(DDR_PHY0),
	_(SDCTRL0),
	_(DUMMY_MASTER0),
	_(DUMMY_MASTER1),
	_(DUMMY_MASTER2),
	_(EVDN,			3,	4,	1, {"f_800m", "f_1000m"}),
	_(SDPROT0),
	_(UMCTL2),
	_(GPU,			2,	9,	3, {"f_800m", "f_1000m", "f_1080m", "f_400m"}),
	_(HSM,			2,	5,	1, {"f_500m", "SYSTEM"}),
	_(RBUS_TOP),
	_(SPACC),
	_(INTERRUPT),

	_(N78,			3,	2,	2, {"f_1000m", "f_1200m", "f_1080m"}),
	_(SYSTOP),
	_(OTPRX),
	_(PMC),
	_(RBUS_BLOCKA),
	_(RBUS_BLOCKB),
	_(RBUS_rsv1),
	_(RBUS_rsv2),
	_(RTC,			0,	0,	0, {"f_32k"}),
	_(MIPZ),
	_(SPIFL,		3,	11,	1, {"f_360m", "f_216m"}),
	_(BCH),
	_(SPIND,		3,	10,	1, {"f_600m", "f_800m"}),
	_(UADMA01),
	_(UADMA23),
	_(UA0,			2,	13,	1, {EXT_CLK, "f_200m"}),

	_(UA1,			2,	14,	1, {EXT_CLK, "f_200m"}),
	_(UA2,			2,	15,	1, {EXT_CLK, "f_200m"}),
	_(UA3,			3,	0,	1, {EXT_CLK, "f_200m"}),
	_(UA4),
	_(UA5),
	_(UADBG,		3,	1,	1, {EXT_CLK, "f_200m"}),
	_(UART2AXI),
	_(GDMAUA),
	_(UPHY0),
	_(USB30C0,		2,	2,	1, {"f_125m", "f_125m"}), /* CLKPIPE0_SRC also 125m */
	_(USB30C1,		2,	3,	1, {"f_125m", "f_125m"}), /* CLKPIPE1_SRC also 125m */
	_(U3PHY0,		2,	0,	2, {"f_100m", "f_50m", EXT_CLK}),
	_(U3PHY1,		2,	0,	2, {"f_100m", "f_50m", EXT_CLK}),
	_(USBC0),
	_(VCD,			0,	0,	0, {"f_360m"}),
	_(VCE,			24,	9,	2, {"f_540m", "f_600m", "f_750m"}),

	_(CM4,			3,	5,	2, {"SYSTEM", "SYSTEM_D2", "SYSTEM_D4"}), /* SYS_CLK, SYS_CLK/2, SYS_CLK/4 */
	_(STC0),
	_(STC_AV0),
	_(STC_AV1),
	_(STC_AV2),
	_(MAILBOX),
	_(PAI),
	_(PAII),
	_(DDRPHY,		0,	0,	0, {"f_800m"}),
	_(DDRCTL),
	_(I2CM0),
	_(SPI_COMBO_0),
	_(SPI_COMBO_1),
	_(SPI_COMBO_2),
	_(SPI_COMBO_3),
	_(SPI_COMBO_4),

	_(SPI_COMBO_5),
	_(CSIIW0,		0,	0,	0, {"f_320m"}),
	_(MIPICSI0,		0,	0,	0, {"f_320m"}),
	_(CSIIW1,		0,	0,	0, {"f_320m"}),
	_(MIPICSI1,		0,	0,	0, {"f_320m"}),
	_(CSIIW2,		0,	0,	0, {"f_320m"}),
	_(MIPICSI2,		0,	0,	0, {"f_320m"}),
	_(CSIIW3,		0,	0,	0, {"f_320m"}),
	_(MIPICSI3,		0,	0,	0, {"f_320m"}),
	_(VCL),
	_(DISP_PWM,		0,	0,	0, {"f_200m"}),
	_(I2CM1),
	_(I2CM2),
	_(I2CM3),
	_(I2CM4),
	_(I2CM5),

	_(UA6),
	_(UA7),
	_(UA8),
	_(AUD),
	_(VIDEO_CODEC),

	_(VCLCORE0,		25,	0,	4, {"f_500m", "f_600m", "f_400m", "f_300m", "f_200m"}),
	_(VCLCORE1,		25,	4,	3, {"f_400m", "f_500m", "f_300m", "f_200m"}),
	_(VCLCORE2,		25,	7,	3, {"f_300m", "f_400m", "f_100m", "f_200m"}),
};

/************************************************* PLL_A *************************************************/

/* from Q654_PLLA_REG_setting.xlsx */
struct {
	u32 rate;
	u32 regs[6];
} pa[] = {
	{
		.rate = 135475200,
		.regs = {
			0x5473, // G4.24
			0x0a11, // G4.25
			0x0014, // G4.26
			0x00c2, // G4.27
			0x0bfd, // G4.28
		}
	},
	{
		.rate = 147456000,
		.regs = {
			0x5473,
			0x0a11,
			0x0028,
			0x01f5,
			0x0bfd,
		}
	},
};

static void PLLA_set_rate(struct sp_pll *clk)
{
	const u32 *pp = pa[clk->idiv].regs;
	int i;

	for (i = 0; i < ARRAY_SIZE(pa->regs); i++) {
		writel(0xffff0000 | pp[i], clk->reg + (i * 4));
		pr_debug("%04x\n", pp[i]);
	}
}

static long PLLA_round_rate(struct sp_pll *clk, unsigned long rate)
{
	int i = ARRAY_SIZE(pa);

	while (--i) {
		if (rate >= pa[i].rate)
			break;
	}
	clk->idiv = i;
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

#define DIV2_BITS	BITS(3, 1, 3)

static struct sp_div divs[] = {
	DIV(4, 1, 4), // 32
	DIV(3, 1, 4), // 24
	DIV(3, 1, 3), // 18
	DIV(2, 1, 4), // 16
	DIV(2, 1, 3), // 12
	DIV(3, 2, 3), // 9
	DIV(4, 1, 1), // 8
	DIV(3, 1, 1), // 6
	DIV(2, 1, 1), // 4
	DIV(3, 2, 1), // 3
	DIV(1, 1, 1), // 2
	DIV(1, 2, 1), // 1
};

static long sp_pll_calc_div(struct sp_pll *clk, unsigned long rate)
{
	long ret = 0, mr = 0;
	int mi = 0, md = 0x7fffffff, d;
	int i, j = IS_PLLH() ? (ARRAY_SIZE(divs) - 6) : 0;

	for (i = ARRAY_SIZE(divs) - 1; i >= j; i--) {
		long br = clk->brate * 2 / divs[i].div2;

		ret = DIV_ROUND_CLOSEST(rate, br);
		if (ret >= FBKDIV_MIN && ret <= FBKDIV_MAX) {
			//pr_info(">>>%u>>> %ld * %ld = %ld - %lu\n", divs[i].div2, br, ret, br * ret, rate);
			br *= ret;
			if (br < rate)
				d = rate - br;
			else if (br > rate)
				d = br - rate;
			else { // br == rate
				clk->idiv = i;
				return ret;
			}
			if (d < md) {
				md = d;
				mi = i;
				mr = ret;
			}
		}
	}

	clk->idiv = mi;
	return mr;
}

static long sp_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct sp_pll *clk = to_sp_pll(hw);
	long ret;

	TRACE;
	//pr_info("round_rate: %lu %lu\n", rate, *prate);

	if (rate == *prate)
		ret = *prate; /* bypass */
	else if (IS_PLLA())
		ret = PLLA_round_rate(clk, rate);
	else {
		ret = sp_pll_calc_div(clk, rate);
		ret = clk->brate * 2 / divs[clk->idiv].div2 * ret;
	}

	return ret;
}

static unsigned long sp_pll_recalc_rate(struct clk_hw *hw,
		unsigned long prate)
{
	struct sp_pll *clk = to_sp_pll(hw);
	u32 reg = readl(clk->reg);
	u32 bp = clk->bp;
	unsigned long ret;

	//TRACE;
	//pr_info("%08x\n", reg);
	if (readl(clk->reg + (bp / 16) * 4) & BIT(bp % 16)) // bypass ?
		ret = prate;
	else if (IS_PLLA())
		ret = pa[clk->idiv].rate;
	else {
		u32 fbkdiv = MASK_GET(FBKDIV, FBKDIV_WIDTH, reg) + 64;
		u32 prediv = MASK_GET(PREDIV, 2, reg) + 1;
		u32 prescl = MASK_GET(PRESCL, 1, reg) + 1;
		u32 pstdiv = MASK_GET(PSTDIV, 2, reg) + 1;

		ret = clk->brate / prediv * fbkdiv * prescl / (IS_PLLH() ? 1 : pstdiv);
	}
	//pr_info("recalc_rate: %lu -> %lu\n", prate, ret);

	return ret;
}

static int sp_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long prate)
{
	struct sp_pll *clk = to_sp_pll(hw);
	unsigned long flags;
	u32 bp = clk->bp;
	u32 reg;

	//TRACE;
	//pr_info("set_rate: %lu -> %lu\n", prate, rate);

	spin_lock_irqsave(&clk->lock, flags);

	reg = BIT((bp % 16) + 16); // BP_HIWORD_MASK

	if (rate == prate) {
		reg |= BIT(bp % 16);
		writel(reg, clk->reg + (bp / 16) * 4); // set bp
	} else if (IS_PLLA())
		PLLA_set_rate(clk);
	else {
		u32 fbkdiv = sp_pll_calc_div(clk, rate) - FBKDIV_MIN;

		if (bp > 16)
			writel(reg, clk->reg + (bp / 16) * 4); // clear bp @ another reg
		reg |= 0x3ffe0000; // BIT[13:1] HIWORD_MASK
		reg |= MASK_SET(FBKDIV, FBKDIV_WIDTH, fbkdiv) | divs[clk->idiv].bits;
		writel(reg, clk->reg);
	}

	spin_unlock_irqrestore(&clk->lock, flags);

	return 0;
}

static int sp_pll_enable(struct clk_hw *hw)
{
	struct sp_pll *clk = to_sp_pll(hw);
	unsigned long flags;

	TRACE;
	spin_lock_irqsave(&clk->lock, flags);
	writel(BIT(PD_N + 16) | BIT(PD_N), clk->reg); /* power up */
	spin_unlock_irqrestore(&clk->lock, flags);

	return 0;
}

static void sp_pll_disable(struct clk_hw *hw)
{
	struct sp_pll *clk = to_sp_pll(hw);
	unsigned long flags;

	TRACE;
	spin_lock_irqsave(&clk->lock, flags);
	writel(BIT(PD_N + 16), clk->reg); /* power down */
	spin_unlock_irqrestore(&clk->lock, flags);
}

static int sp_pll_is_enabled(struct clk_hw *hw)
{
	struct sp_pll *clk = to_sp_pll(hw);

	return readl(clk->reg) & BIT(PD_N);
}

static const struct clk_ops sp_pll_ops = {
	.enable = sp_pll_enable,
	.disable = sp_pll_disable,
	.is_enabled = sp_pll_is_enabled,
	.round_rate = sp_pll_round_rate,
	.recalc_rate = sp_pll_recalc_rate,
	.set_rate = sp_pll_set_rate
};

struct clk *clk_register_sp_pll(const char *name, void __iomem *reg, u32 bp)
{
	struct sp_pll *pll;
	struct clk *clk;
	//unsigned long flags = 0;
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

	if (reg == PLLS_CTL)
		initd.flags |= CLK_IS_CRITICAL;

	pll->hw.init = &initd;
	pll->reg = reg;
	pll->brate = (reg == PLLN_CTL) ? (XTAL / 2) : XTAL;
	pll->bp = bp;
	spin_lock_init(&pll->lock);

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk))) {
		kfree(pll);
	} else {
		pr_info("%-20s%lu\n", name, clk_get_rate(clk));
		clk_register_clkdev(clk, NULL, name);
	}

	return clk;
}

static struct clk *
clk_register_sp_clk(struct sp_clk *sp_clk)
{
	const char * const *parent_names = sp_clk->parent_names[0] ? sp_clk->parent_names : default_parents;
	struct clk_mux *mux = NULL;
	struct clk_gate *gate;
	struct clk *clk;

	if (sp_clk->width) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			return ERR_PTR(-ENOMEM);
		mux->reg = mux_regs + (sp_clk->mux << 2);
		mux->shift = sp_clk->shift;
		mux->mask = BIT(sp_clk->width) - 1;
		mux->table = mux_table;
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

	clk = clk_register_composite(NULL, sp_clk->name, parent_names,
					mux ? sp_clk->width + 1 : 1,
					mux ? &mux->hw : NULL, &clk_mux_ops,
					NULL, NULL,
					&gate->hw, &clk_gate_ops,
					CLK_IGNORE_UNUSED);
	if (IS_ERR(clk)) {
		kfree(mux);
		kfree(gate);
	}

	return clk;
}

static void __init sp_clkc_init(struct device_node *np)
{
	int i;

	pr_info("sp-clkc init\n");

	if (moon_regs) {
		pr_warn("sp-clkc already present.\n");
		return; // -ENXIO
	}

	moon_regs = of_iomap(np, 0);
	if (WARN_ON(!moon_regs)) {
		pr_warn("sp-clkc regs missing.\n");
		return; // -EIO
	}

	pr_debug("sp-clkc: moon_regs = %llx", (u64)moon_regs);

	/* PLLs */
	clks[PLLS] = clk_register_sp_pll("PLLS", PLLS_CTL, 14);
	clks[PLLC] = clk_register_sp_pll("PLLC", PLLC_CTL, 14);
	clks[PLLN] = clk_register_sp_pll("PLLN", PLLN_CTL, 14);
	clks[PLLH] = clk_register_sp_pll("PLLH", PLLH_CTL, 24); // BP: 16 + 8
	clks[PLLD] = clk_register_sp_pll("PLLD", PLLD_CTL, 14);
	clks[PLLA] = clk_register_sp_pll("PLLA", PLLA_CTL, 2);

	pr_debug("sp-clkc: register fixed_rate/factor");
	/* fixed frequency & fixed factor */
	clk_register_fixed_factor(NULL, "f_1200m", "PLLS", 0, 1, 2);
	clk_register_fixed_factor(NULL, "f_600m",  "PLLS", 0, 1, 4);
	clk_register_fixed_factor(NULL, "f_300m",  "PLLS", 0, 1, 8);
	clk_register_fixed_factor(NULL, "f_750m",  "PLLC", 0, 1, 2);
	clk_register_fixed_factor(NULL, "f_1000m", "PLLN", 0, 1, 1);
	clk_register_fixed_factor(NULL, "f_500m",  "PLLN", 0, 1, 2);
	clk_register_fixed_factor(NULL, "f_250m",  "PLLN", 0, 1, 4);
	clk_register_fixed_factor(NULL, "f_125m",  "PLLN", 0, 1, 8);
	clk_register_fixed_factor(NULL, "f_1080m", "PLLH", 0, 1, 2);
	clk_register_fixed_factor(NULL, "f_540m",  "PLLH", 0, 1, 4);
	clk_register_fixed_factor(NULL, "f_360m",  "PLLH", 0, 1, 6);
	clk_register_fixed_factor(NULL, "f_216m",  "PLLH", 0, 1, 10);
	clk_register_fixed_factor(NULL, "f_800m",  "PLLD", 0, 1, 2);
	clk_register_fixed_factor(NULL, "f_400m",  "PLLD", 0, 1, 4);
	clk_register_fixed_factor(NULL, "f_200m",  "PLLD", 0, 1, 8);
	clk_register_fixed_factor(NULL, "f_100m", EXT_CLK, 0, 4, 1);
	clk_register_fixed_factor(NULL, "f_50m",  EXT_CLK, 0, 2, 1);
	clk_register_fixed_factor(NULL, "SYSTEM_D2", "SYSTEM", 0, 1, 2); // SYS_CLK/2
	clk_register_fixed_factor(NULL, "SYSTEM_D4", "SYSTEM", 0, 1, 4); // SYS_CLK/4

	pr_debug("sp-clkc: register sp_clks");

	/* sp_clks */
	for (i = 0; i < ARRAY_SIZE(sp_clks); i++) {
		struct sp_clk *sp_clk = &sp_clks[i];
		int j = sp_clk->id;

		clks[j] = clk_register_sp_clk(sp_clk);
	}

	pr_debug("sp-clkc: of_clk_add_provider");
	clk_data.clks = clks;
	clk_data.clk_num = ARRAY_SIZE(clks);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);

	pr_debug("===================================================");
	for (i = 0; i < ARRAY_SIZE(clks); i++) {
		struct clk *clk = clks[i];

		if (clk)
			pr_debug("[%02x] %-14s: %lu", i, __clk_get_name(clk), clk_get_rate(clk));
	}
	pr_info("sp-clkc init done!\n");
}

CLK_OF_DECLARE(sp_clkc, "sunplus,q654-clkc", sp_clkc_init);
