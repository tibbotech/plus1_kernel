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
#include <dt-bindings/clock/sp-q645.h>

//#define TRACE	pr_info("### %s:%d (%d)\n", __FUNCTION__, __LINE__, (clk->reg - REG(4, 0)) / 4)
#define TRACE

#define MASK_SET(shift, width, value) \
({ \
	u32 m = ((1 << (width)) - 1) << (shift); \
	(m << 16) | (((value) << (shift)) & m); \
})
#define MASK_GET(shift, width, value)	(((value) >> (shift)) & ((1 << (width)) - 1))

#define REG(i)	(pll_regs + (i) * 4)

#define PLLHSM_CTL	REG(1)
#define PLLNPU_CTL	REG(4)
#define PLLSYS_CTL	REG(7)
#define PLLCPU_CTL	REG(10)
#define PLLDRAM_CTL	REG(13)

#define EXTCLK		"extclk"

#define PD_N	0
#define PREDIV	1
#define PRESCL	3
#define FBKDIV	4
#define PSTDIV	12
#define BP		14

#define FBKDIV_WIDTH	8
#define FBKDIV_MIN		64
#define FBKDIV_MAX		(FBKDIV_MIN + BIT(FBKDIV_WIDTH) - 1)

struct sp_pll {
	struct clk_hw	hw;
	void __iomem	*reg;
	spinlock_t		lock;

	long brate;
	u32 idiv; // struct divs[] index
	u32 fbkdiv; // 64~(64+255)
};
#define to_sp_pll(_hw)	container_of(_hw, struct sp_pll, hw)

#define P_EXTCLK	(1 << 16)
static char *parents[] = {
	"pllsys",
	"extclk",
};

/* FIXME: parent clk incorrect cause clk_get_rate got error value */
static u32 gates[] = {
	SYSTEM,
	CA55CORE0,
	CA55CORE1,
	CA55CORE2,
	CA55CORE3,
	CA55CUL3,
	CA55,
	IOP,
	PBUS0,
	PBUS1,
	PBUS2,
	PBUS3,
	BR0,
	CARD_CTL0,
	CARD_CTL1,
	CARD_CTL2,

	CBDMA0,
	CPIOL,
	CPIOR,
	DDR_PHY0,
	SDCTRL0,
	DUMMY_MASTER0,
	DUMMY_MASTER1,
	DUMMY_MASTER2,
	EVDN,
	SDPROT0,
	UMCTL2,
	GPU,
	HSM,
	RBUS_TOP,
	SPACC,
	INTERRUPT,

	N78,
	SYSTOP,
	OTPRX,
	PMC,
	RBUS_BLOCKA,
	RBUS_BLOCKB,
	RBUS_rsv1,
	RBUS_rsv2,
	RTC,
	MIPZ,
	SPIFL,
	BCH,
	SPIND,
	UADMA01 | P_EXTCLK,
	UADMA23 | P_EXTCLK,
	UA0 | P_EXTCLK,

	UA1 | P_EXTCLK,
	UA2 | P_EXTCLK,
	UA3 | P_EXTCLK,
	UA4 | P_EXTCLK,
	UA5 | P_EXTCLK,
	UADBG | P_EXTCLK,
	UART2AXI | P_EXTCLK,
	GDMAUA,
	UPHY0,
	USB30C0,
	USB30C1,
	U3PHY0,
	U3PHY1,
	USBC0,
	VCD,
	VCE,

	CM4,
	STC0,
	STC_AV0,
	STC_AV1,
	STC_AV2,
	MAILBOX,
	PAI,
	PAII,
	DDRPHY,
	DDRCTL,
	I2CM0,
	SPI_COMBO_0,
	SPI_COMBO_1,
	SPI_COMBO_2,
	SPI_COMBO_3,
	SPI_COMBO_4,

	SPI_COMBO_5,
	CSIIW0,
	MIPICSI0,
	CSIIW1,
	MIPICSI1,
	CSIIW2,
	MIPICSI2,
	CSIIW3,
	MIPICSI3,
	VCL,
	DISP_PWM,
	I2CM1,
	I2CM2,
	I2CM3,
	I2CM4,
	I2CM5,

	UA6,
	UA7,
	UA8,
	AUD,
	VIDEO_CODEC
};
static struct clk *clks[PLL_MAX + CLK_MAX];
static struct clk_onecell_data clk_data;
static void __iomem *clk_regs;
static void __iomem *pll_regs;

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
	int i, j = (clk->reg == PLLHSM_CTL) ? (ARRAY_SIZE(divs) - 6) : 0;

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
	unsigned long ret;
	bool bHSM = clk->reg == PLLHSM_CTL;

	//TRACE;
	//pr_info("%08x\n", reg);
	if (bHSM ? (readl(clk->reg + 4) & BIT(8)) : (reg & BIT(BP)))
		ret = prate; /* bypass */
	else {
		u32 fbkdiv = MASK_GET(FBKDIV, FBKDIV_WIDTH, reg) + 64;
		u32 prediv = MASK_GET(PREDIV, 2, reg) + 1;
		u32 prescl = MASK_GET(PRESCL, 1, reg) + 1;
		u32 pstdiv = MASK_GET(PSTDIV, 2, reg) + 1;
		ret = clk->brate / prediv * fbkdiv * prescl / (bHSM ? 1 : pstdiv);
	}
	//pr_info("recalc_rate: %lu -> %lu\n", prate, ret);

	return ret;
}

static int sp_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long prate)
{
	struct sp_pll *clk = to_sp_pll(hw);
	unsigned long flags;
	u32 reg;

	//TRACE;
	//pr_info("set_rate: %lu -> %lu\n", prate, rate);

	spin_lock_irqsave(&clk->lock, flags);

	reg = BIT(BP + 16); /* BP HIWORD_MASK */

	if (rate == prate)
		reg |= BIT(BP); /* bypass */
	else {
		u32 fbkdiv = sp_pll_calc_div(clk, rate) - FBKDIV_MIN;
		reg |= 0x3ffe0000; // BIT[13:1] HIWORD_MASK
		reg |= MASK_SET(FBKDIV, FBKDIV_WIDTH, fbkdiv) | divs[clk->idiv].bits;
	}

	writel(reg, clk->reg);

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

struct clk *clk_register_sp_pll(const char *name, void __iomem *reg)
{
	struct sp_pll *pll;
	struct clk *clk;
	//unsigned long flags = 0;
	const char *parent = EXTCLK;
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

	if (reg == PLLSYS_CTL)
		initd.flags |= CLK_IS_CRITICAL;

	pll->hw.init = &initd;
	pll->reg = reg;
	pll->brate = (reg == PLLNPU_CTL) ? (XTAL / 2) : XTAL;
	spin_lock_init(&pll->lock);

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk))) {
		kfree(pll);
	} else {
		pr_info("%-20s%lu\n", name, clk_get_rate(clk));
		clk_register_clkdev(clk, NULL, name);
	}

#if 0 // test set
	clk_set_rate(clk, clk_get_rate(clk) / 2);
	pr_info("%-20s%lu\n", name, clk_get_rate(clk));
	clk_set_rate(clk, clk_get_rate(clk) * 2);
	pr_info("%-20s%lu\n\n", name, clk_get_rate(clk));
#endif

	return clk;
}

static void __init sp_clkc_init(struct device_node *np)
{
	int i, j;
	pr_info("@@@ sp-clkc init\n");

	if (clk_regs) {
		pr_warn("sp-clkc already present.\n");
		return; // -ENXIO
	}

	clk_regs = of_iomap(np, 0);
	pll_regs = of_iomap(np, 1);
	if (WARN_ON(!clk_regs || !pll_regs)) {
		pr_warn("sp-clkc regs missing.\n");
		return; // -EIO
	}

	/* PLLs */
	clks[PLL_SYS] = clk_register_sp_pll("pllsys", PLLSYS_CTL);
	clks[PLL_CPU] = clk_register_sp_pll("pllcpu", PLLCPU_CTL);
	clks[PLL_NPU] = clk_register_sp_pll("pllnpu", PLLNPU_CTL);
	clks[PLL_HSM] = clk_register_sp_pll("pllhsm", PLLHSM_CTL);
	clks[PLL_DRAM] = clk_register_sp_pll("plldram", PLLDRAM_CTL);

	/* gates */
	for (i = 0; i < ARRAY_SIZE(gates); i++) {
		char s[10];
		j = gates[i] & 0xffff;
		sprintf(s, "clken%02x", j);
		clks[PLL_MAX + j] = clk_register_gate(NULL, s, parents[gates[i] >> 16], CLK_IGNORE_UNUSED,
			clk_regs + (j >> 4 << 2), j & 0x0f,
			CLK_GATE_HIWORD_MASK, NULL);
		//printk("%02x %p %p.%d\n", j, clks[PLL_MAX + j], REG(0, j >> 4), j & 0x0f);
	}

	clk_data.clks = clks;
	clk_data.clk_num = ARRAY_SIZE(clks);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);

	pr_info("@@@ sp-clkc init done!\n");
}

CLK_OF_DECLARE(sp_clkc, "sunplus,q645-clkc", sp_clkc_init);
