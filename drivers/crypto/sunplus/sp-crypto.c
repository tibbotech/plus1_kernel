// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <mach/io_map.h>

#include "sp-crypto.h"
#include "sp-aes.h"
#include "sp-hash.h"
#include "sp-rsa.h"

#ifdef CONFIG_CRYPTO_DEV_SP_TEST
#include "sp-crypto-test.c"
#endif

static struct sp_crypto_dev sp_dd_tb[1];

void dump_buf(u8 *buf, u32 len)
{
	static const char s[] = {
		' ', ' ', ' ', ' ', ' ', ' ', ' ', '|',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', '\n'
	};
	char ss[52] = "";
	u32 i = 0, j;

	pr_info("buf:%p, len:%d\n", buf, len);
	while (i < len) {
		j = i & 0x0F;
		sprintf(ss + j * 3, "%02x%c", buf[i], s[j]);
		i++;
		if ((i & 0x0F) == 0) {
			pr_info("%s", ss);
			ss[0] = 0;
		}
	}
	if (i & 0x0F) {
		//strcat(ss, "\n");
		pr_info("%s", ss);
	}
}
EXPORT_SYMBOL(dump_buf);

#define OUT(fmt, args...) { p += sprintf(p, fmt, ##args); }

static char *print_trb(char *p, struct trb_s *trb)
{
	OUT("trb ========== %p\n", trb);
	OUT(" c:0x%x\n", trb->c);
	OUT("cc:0x%x\n", trb->cc);
	OUT("tc:0x%x\n", trb->tc);
	OUT("ioc:0x%x\n", trb->ioc);
	OUT("type:0x%x\n", trb->type);
	OUT("size:0x%x\n", trb->size);
	OUT("sptr:0x%0x\n", trb->sptr);
	OUT("dptr:0x%0x\n", trb->dptr);
	OUT("mode:0x%x\n", trb->mode);
	OUT("iptr:0x%x\n", trb->iptr);
	OUT("kptr:0x%x\n", trb->kptr);
	OUT("priv:%p\n", trb->priv);

	return p;
}

static char *print_hw(char *p, struct trb_s *trb)
{
	struct sp_crypto_reg *reg = sp_dd_tb->reg;
	struct trb_ring_s *ring;
	int i, j;

	// AES
	ring = AES_RING(sp_dd_tb);
	OUT("AES_CR   : %08x\n", R(AES_CR));
	OUT("AES_ER   : %08x\n", R(AES_ER));
	OUT("AES_CRCR : %08x\n", R(AESDMA_CRCR));
	OUT("sem.count: %d\n", ring->trb_sem.count);
	OUT("head     : %p\n", ring->head);
	OUT("tail     : %p\n", ring->tail);
	OUT("triggers : %d\n", ring->trigger_count);
	OUT("irqs     : %d\n", ring->irq_count);
#ifdef TRACE_WAIT_ORDER
	OUT("wait.len : %d\n", kfifo_len(&ring->f));
#endif
	//if (trb || (R(AESDMA_CRCR) & AUTODMA_CRCR_CRR)) // dump_trb || running
	{
		//i = (trb ? trb : (struct trb_s *)__va(R(AES_CR))) - ring->trb;
		i = ring->head - ring->trb;
		for (j = -2; j < 3; j++)
			p = print_trb(p, ring->trb + ((i + j + 127) % 127));
	}

	OUT("\n");
	// HASH
	ring = HASH_RING(sp_dd_tb);
	OUT("HASH_CR  : %08x\n", R(HASH_CR));
	OUT("HASH_ER  : %08x\n", R(HASH_ER));
	OUT("HAHS_CRCR: %08x\n", R(HASHDMA_CRCR));
	OUT("sem.count: %d\n", ring->trb_sem.count);
	OUT("head     : %p\n", ring->head);
	OUT("tail     : %p\n", ring->tail);
	OUT("triggers : %d\n", ring->trigger_count);
	OUT("irqs     : %d\n", ring->irq_count);
#ifdef TRACE_WAIT_ORDER
	OUT("wait.len : %d\n", kfifo_len(&ring->f));
#endif
	//if (trb || (R(HASHDMA_CRCR) & AUTODMA_CRCR_CRR)) // dump_trb || running
	{
		//i = (trb ? trb : (struct trb_s *)__va(R(HASH_CR))) - ring->trb;
		i = ring->head - ring->trb;
		for (j = -2; j < 3; j++)
			p = print_trb(p, ring->trb + ((i + j + 127) % 127));
	}

	return p;
}

void dump_trb(struct trb_s *trb)
{
	char s[300];

	print_trb(s, trb);
	pr_info("%s", s);
}

/* hwcfg: enable/disable aes/hash hw
 * echo <aes:-1~2> [hash:-1~2] [print] > /sys/module/sp_crypto/parameters/hwcfg
 * -1: no change
 *  0: disable
 *  1: enable
 *  2: toggle
 */
static int hwcfg_set(const char *val, const struct kernel_param *kp)
{
	static int f_aes = 1, f_hash = 1; // initial status: aes & hash enabled
	int en_aes = -1, en_hash = -1, pr = 1;
	int ret;

	ret = sscanf(val, "%d %d %d", &en_aes, &en_hash, &pr);
	if (ret)
		return ret;

	if (en_aes >= 0) {
		if (en_aes > 1)
			en_aes = 1 - f_aes; // toggle
		ret = en_aes ? sp_aes_init() : sp_aes_finit();
		f_aes = en_aes;
		if (pr)
			pr_info("sp_aes %s: %d\n", en_aes ? "enable" : "disable", ret);
	}
	if (en_hash >= 0) {
		if (en_hash > 1)
			en_hash = 1 - f_hash; // toggle
		ret = en_hash ? sp_hash_init() : sp_hash_finit();
		f_hash = en_hash;
		if (pr)
			pr_info("sp_hash %s: %d\n", en_hash ? "enable" : "disable", ret);
	}

	return 0;
}

static int hwcfg_get(char *buffer, const struct kernel_param *kp)
{
	return print_hw(buffer, NULL) - buffer;
}

static const struct kernel_param_ops hwcfg_ops = {
	.set = hwcfg_set,
	.get = hwcfg_get,
};
module_param_cb(hwcfg, &hwcfg_ops, NULL, 0600);

int crypto_ctx_init(struct crypto_ctx_s *ctx, int type)
{
	init_waitqueue_head(&ctx->wait);
	ctx->dd = sp_crypto_alloc_dev(SP_CRYPTO_AES);
	//sema_init(&ctx->sem, 1);
	ctx->type = type;

	return 0;
}

void crypto_ctx_exit(struct crypto_ctx_s *ctx)
{
	sp_crypto_free_dev(ctx->dd, SP_CRYPTO_AES);
}

int crypto_ctx_exec(struct crypto_ctx_s *ctx)
{
	struct trb_ring_s *ring = TRB_RING(ctx);
	struct sp_crypto_reg *reg = ctx->dd->reg;
	int ret;

	ring->tail->c = 0; // clear tail_trb.c for HW stop
	ctx->done = false;
	smp_wmb(); /* memory barrier */
	ring->trigger_count++;
	if (ctx->type == SP_CRYPTO_HASH) {
		SP_CRYPTO_INF("HASH_CR  : %08x\n", R(HASH_CR));
		SP_CRYPTO_INF("HASH_ER  : %08x\n", R(HASH_ER));
		SP_CRYPTO_INF("HASH_CRCR: %08x\n", R(HASHDMA_CRCR));
		if (!(R(HASHDMA_CRCR) & AUTODMA_CRCR_CRR)) {	// autodma not running
			WR(HASHDMA_RTR, | AUTODMA_TRIGGER);		// trigger autodma run
		}

		//if (!(ctx->mode & M_FINAL))
		{ // busy-waiting, can't sleep in hash //update
			while (!ctx->done)
				cpu_relax(); // TODO: handle timeout
			return 0;
		}

	} else {
		SP_CRYPTO_INF("AES_CR  : %08x\n", R(AES_CR));
		SP_CRYPTO_INF("AES_ER  : %08x\n", R(AES_ER));
		SP_CRYPTO_INF("AES_CRCR: %08x\n", R(AESDMA_CRCR));
		if (!(R(AESDMA_CRCR) & AUTODMA_CRCR_CRR))
			WR(AESDMA_RTR, | AUTODMA_TRIGGER);
	}

	ret = wait_event_interruptible_timeout(ctx->wait, ctx->done, 60*HZ);
	if (!ret)
		ret = -ETIMEDOUT;
	return (ret < 0) ? ret : 0;
}

struct trb_s *crypto_ctx_queue(struct crypto_ctx_s *ctx,
	dma_addr_t src, dma_addr_t dst,
	dma_addr_t iv, dma_addr_t key,
	u32 len, u32 mode, bool ioc)
{
	struct trb_ring_s *ring = TRB_RING(ctx);
	struct trb_s *trb = trb_get(ring);

	if (trb) {
#ifdef TRACE_WAIT_ORDER
		if (ioc)
			kfifo_put(&ring->f, &ctx->wait);
#endif
		trb->priv = ctx;
		trb->ioc  = ioc;
		trb->size = len;
		trb->sptr = src;
		trb->dptr = dst;
		trb->mode = mode;
		trb->iptr = iv;
		trb->kptr = key;
		smp_wmb(); /* memory barrier */
		trb->c    = ~0;	// this field must WRite at last
		//dump_trb(trb);
	}

	return trb;
}

inline struct trb_s *trb_next(struct trb_s *trb)
{
	trb++;
	return (trb->type == TRB_LINK) ? (struct trb_s *)(trb->dptr) : trb;
}

inline struct trb_s *trb_get(struct trb_ring_s *ring)
{
	struct trb_s *trb = NULL;

	if (!down_interruptible(&ring->trb_sem)) {
		trb = ring->tail;
		ring->tail = trb_next(ring->tail);
		BUG_ON(!trb);
	}
	return trb;
}

/* USE_IN_IRQ, move ring head to next */
inline struct trb_s *trb_put(struct trb_ring_s *ring)
{
	up(&ring->trb_sem);
	ring->head->cc = 0;
	ring->head = trb_next(ring->head);
	return ring->head;
}

/* USE_IN_IRQ, reset ring head & tail to base addr */
inline void trb_ring_reset(struct trb_ring_s *ring)
{
	ring->head = ring->tail = ring->trb;
}

static struct trb_ring_s *trb_ring_new(struct device *dev, u32 size)
{
	struct trb_ring_s *ring;

	ring = kzalloc(sizeof(struct trb_ring_s), GFP_KERNEL);
	if (IS_ERR(ring))
		return ring;

	ring->trb = dma_alloc_coherent(dev, PAGE_SIZE, &ring->pa, GFP_KERNEL);
	if (IS_ERR(ring->trb)) {
		kfree(ring);
		return ERR_CAST(ring->trb);
	}

	ring->head = ring->tail = ring->trb;
	sema_init(&ring->trb_sem, size - 2); // not include link trb & tail trb
	mutex_init(&ring->lock);
#ifdef TRACE_WAIT_ORDER
	INIT_KFIFO(ring->f);
#endif

	size--;
	ring->link = ring->trb + size;
	ring->link->type = TRB_LINK;
	ring->link->sptr = ring->pa;				// PA
	ring->link->dptr = (dma_addr_t)ring->trb;	// VA
	while (size--)
		ring->trb[size].type = TRB_NORMAL;

	return ring;
}

static void trb_ring_free(struct trb_ring_s *ring)
{
	dma_free_coherent(NULL, PAGE_SIZE, ring->trb, ring->pa);
	kfree(ring);
}

irqreturn_t sp_crypto_irq(int irq, void *dev_id)
{
	struct sp_crypto_dev *dev = dev_id;
	struct sp_crypto_reg *reg = dev->reg;
	u32 secif = R(SECIF);
	u32 flag;

	W(SECIF, secif); // clear int
	//pr_info("<%04x>", secif);

	/* aes hash rsa may come at one irq */
	flag = secif & (AES_TRB_IF | AES_ERF_IF | AES_DMA_IF | AES_CMD_RD_IF);
	if (flag)
		sp_aes_irq(dev_id, flag);

	flag = secif & (HASH_TRB_IF | HASH_ERF_IF | HASH_DMA_IF | HASH_CMD_RD_IF);
	if (flag)
		sp_hash_irq(dev_id, flag);

	flag = secif & RSA_DMA_IF;
	if (flag)
		sp_rsa_irq(dev_id, flag);

	return IRQ_HANDLED;
}

/* alloc hw dev */
struct sp_crypto_dev *sp_crypto_alloc_dev(int type)
{
#ifdef USE_REF
	int i;
	struct sp_crypto_dev *dev = sp_dd_tb;

	for (i = 0; i < ARRAY_SIZE(sp_dd_tb); i++) {
		switch (type) {
		case SP_CRYPTO_RSA:
			if（atomic_read(&dev->rsa_ref_cnt) >
				atomic_read(&sp_dd_tb[i].rsa_ref_cnt))
				dev = &sp_dd_tb[i];

			break;
		case SP_CRYPTO_AES:
			if（atomic_read(&dev->aes_ref_cnt) >
				atomic_read(&sp_dd_tb[i].aes_ref_cnt))
				dev = &sp_dd_tb[i];
			break;
		case SP_CRYPTO_HASH:
			if（atomic_read(&dev->hash_ref_cnt) >
				atomic_read(&sp_dd_tb[i].hash_ref_cnt))
				dev = &sp_dd_tb[i];
			break;
		}
	}

	switch (type) {
	case SP_CRYPTO_RSA:
		atomic_inc(&dev->rsa_ref_cnt);
		break;
	case SP_CRYPTO_AES:
		atomic_inc(&dev->aes_ref_cnt);
		break;
	case SP_CRYPTO_HASH:
		atomic_inc(&dev->hash_ref_cnt);
		break;
	}
	//spcrypto_dump_dev(dev);
	return dev;
#else
	return sp_dd_tb;
#endif
}

/* free hw dev */
void sp_crypto_free_dev(struct sp_crypto_dev *dev, u32 type)
{
#ifdef USE_REF
	switch (type) {
	case SP_CRYPTO_RSA:
		if (atomic_read(&dev->rsa_ref_cnt) == 0) {
			SP_CRYPTO_ERR("rsa_ref_cnt underflow\n");
			return;
		}
		atomic_dec(&dev->rsa_ref_cnt);
		return;
	case SP_CRYPTO_AES:
		if (atomic_read(&dev->aes_ref_cnt) == 0) {
			SP_CRYPTO_ERR("aes_ref_cnt underflow\n");
			return;
		}
		atomic_dec(&dev->aes_ref_cnt);
		return;
	case SP_CRYPTO_HASH:
		if (atomic_read(&dev->hash_ref_cnt) == 0) {
			SP_CRYPTO_ERR("hash_ref_cnt underflow\n");
			return;
		}
		atomic_dec(&dev->hash_ref_cnt);
		return;
	}
#endif
}

static int sp_crypto_probe(struct platform_device *pdev)
{
	struct sp_crypto_reg *reg;
	struct resource *res_irq;
	struct trb_ring_s *ring;
	u32 phy_addr;
	struct sp_crypto_dev *dev = sp_dd_tb;//platform_get_drvdata(pdev);
	struct resource *res_mem;
	void __iomem *membase;
	int ret = 0;

	SP_CRYPTO_TRACE();

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	membase = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(membase))
		return PTR_ERR(membase);

	dev->reg = membase;
	dev->device = &pdev->dev;

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq)
		return -ENODEV;

	dev->irq = res_irq->start;

	dev->clk = devm_clk_get(&pdev->dev, NULL);
	ERR_OUT(dev->clk, goto out0, "get clk");
	ret = clk_prepare_enable(dev->clk);
	ERR_OUT(ret, goto out0, "enable clk");

	/* reset */
	dev->rstc = devm_reset_control_get(&pdev->dev, NULL);
	ERR_OUT(dev->clk, goto out1, "get reset_control");
	ret = reset_control_deassert(dev->rstc);
	ERR_OUT(dev->clk, goto out1, "deassert reset_control");

	platform_set_drvdata(pdev, dev);
	reg = dev->reg;
	SP_CRYPTO_INF("SP_CRYPTO_ENGINE @ %p =================\n", reg);

	/////////////////////////////////////////////////////////////////////////////////

	dev->version = R(VERSION);
	SP_CRYPTO_INF("devid %d version %0x\n", dev->devid, dev->version);

#ifdef USE_REF
	atomic_set(&dev->rsa_ref_cnt, 0);
	atomic_set(&dev->aes_ref_cnt, 0);
	atomic_set(&dev->hash_ref_cnt, 0);
	dev->state = SEC_DEV_INAVTIVE;
#endif

	SP_CRYPTO_TRACE();
	HASH_RING(dev) = ring = trb_ring_new(&pdev->dev, HASH_CMD_RING_SIZE);
	ERR_OUT(ring, goto out2, "new hash_cmd_ring");

	phy_addr = ring->pa;
	W(HASHDMA_CRCR, phy_addr | AUTODMA_CRCR_FLAGS);
	W(HASHDMA_ERBAR, phy_addr);
#ifdef USE_ERF
	W(HASHDMA_ERDPR, phy_addr + (HASH_EVENT_RING_SIZE - 1) * TRB_SIZE);
#else
	W(HASHDMA_ERDPR, phy_addr + HASH_EVENT_RING_SIZE * TRB_SIZE);
#endif
	W(HASHDMA_RCSR, HASH_EVENT_RING_SIZE - 1);
	WR(HASHDMA_RCSR, | AUTODMA_RCSR_ERF);
	WR(HASHDMA_RCSR, | AUTODMA_RCSR_EN); // TODO: HW issue, autodma enable must be alone
	SP_CRYPTO_INF("HASH_RING === VA:%p PA:%08x\n", ring->trb, phy_addr);
	SP_CRYPTO_INF("HASH_RCSR: %08x\n", R(HASHDMA_RCSR));
	SP_CRYPTO_INF("HASH_RING: %08x %d\n", phy_addr, ring->trb_sem.count);
	SP_CRYPTO_INF("HASH_CR  : %08x\n", R(HASH_CR));
	SP_CRYPTO_INF("HASH_ER  : %08x\n", R(HASH_ER));

	SP_CRYPTO_TRACE();
	AES_RING(dev) = ring = trb_ring_new(&pdev->dev, AES_CMD_RING_SIZE);
	ERR_OUT(ring, goto out3, "new hash_cmd_ring");

	phy_addr = ring->pa;
	W(AESDMA_CRCR, phy_addr | AUTODMA_CRCR_FLAGS);
	W(AESDMA_ERBAR, phy_addr);
#ifdef USE_ERF
	W(AESDMA_ERDPR, phy_addr + (AES_EVENT_RING_SIZE - 1) * TRB_SIZE);
#else
	W(AESDMA_ERDPR, phy_addr + AES_EVENT_RING_SIZE * TRB_SIZE);
#endif
	W(AESDMA_RCSR, AES_EVENT_RING_SIZE - 1);
	WR(AESDMA_RCSR, | AUTODMA_RCSR_ERF);
	WR(AESDMA_RCSR, | AUTODMA_RCSR_EN); // TODO: same as above
	SP_CRYPTO_INF("AES_RING  === VA:%p PA:%08x\n", ring->trb, phy_addr);
	SP_CRYPTO_INF("AES_RCSR : %08x\n", R(AESDMA_RCSR));
	SP_CRYPTO_INF("AES_RING : %08x %d\n", phy_addr, ring->trb_sem.count);
	SP_CRYPTO_INF("AES_CR   : %08x\n", R(AES_CR));
	SP_CRYPTO_INF("AES_ER   : %08x\n", R(AES_ER));

	ret = devm_request_irq(&pdev->dev, dev->irq, sp_crypto_irq, IRQF_TRIGGER_HIGH, "sp_crypto", dev);
	ERR_OUT(ret, goto out4, "request_irq(%d)", dev->irq);

	SP_CRYPTO_TRACE();
#ifdef USE_ERF
	W(SECIE, RSA_DMA_IE | AES_TRB_IE | HASH_TRB_IE | AES_ERF_IE | HASH_ERF_IE);
#else
	W(SECIE, RSA_DMA_IE | AES_TRB_IE | HASH_TRB_IE);
#endif
	SP_CRYPTO_INF("SECIE: %08x\n", R(SECIE));
	//BUG_ON(1);

	return 0;

out4:
	trb_ring_free(AES_RING(dev));
out3:
	trb_ring_free(HASH_RING(dev));
out2:
	reset_control_assert(dev->rstc);
out1:
	clk_disable_unprepare(dev->clk);
out0:
	return ret;
}

static int sp_crypto_remove(struct platform_device *pdev)
{
	struct sp_crypto_dev *dev = platform_get_drvdata(pdev);
	struct sp_crypto_reg *reg = dev->reg;

	SP_CRYPTO_TRACE();

	/* hw stop */
	// HASH
	WR(HASHDMA_RCSR, | AUTODMA_RCSR_ERF);	// clear event ring
	WR(HASHDMA_CRCR, | AUTODMA_CRCR_CS);	// stop running
	WR(HASHDMA_RCSR, &~AUTODMA_RCSR_EN);	// disable autodma
	// AES
	WR(AESDMA_RCSR, | AUTODMA_RCSR_ERF);	// clear event ring
	WR(AESDMA_CRCR, | AUTODMA_CRCR_CS);		// stop running
	WR(AESDMA_RCSR, &~AUTODMA_RCSR_EN);		// disable autodma

	/* free resource */
	trb_ring_free(AES_RING(dev));
	trb_ring_free(HASH_RING(dev));
	reset_control_assert(dev->rstc);
	clk_disable_unprepare(dev->clk);

	return 0;
}

static const struct of_device_id sp_crypto_of_match[] = {
	{ .compatible = "sunplus,sp7021-crypto" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_crypto_of_match);

static struct platform_driver sp_crtpto_driver = {
	.probe		= sp_crypto_probe,
	.remove		= sp_crypto_remove,
	.driver		= {
		.name	= "sp_crypto",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sp_crypto_of_match),
	},
};

static int __init sp_crypto_module_init(void)
{
	int ret = 0;

	SP_CRYPTO_TRACE();

	ret = sp_hash_init();
	ERR_OUT(ret, goto out0, "sp_hash_init");
	ret = sp_aes_init();
	ERR_OUT(ret, goto out1, "sp_aes_init");

	platform_driver_register(&sp_crtpto_driver);
#ifdef USE_REF
	for (i = 0; i < ARRAY_SIZE(sp_dd_tb); ++i) {
		struct platform_device *pdev = platform_device_alloc("sp_crypto", i);

		sp_dd_tb[i].device = &pdev->dev;
		sp_dd_tb[i].reg = ioremap((u32)sp_dd_tb[i].reg, sizeof(struct sp_crypto_reg));
		platform_set_drvdata(pdev, &sp_dd_tb[i]);
		platform_device_add(pdev);

		SP_CRYPTO_INF("SP_CRYPTO_ENGINE_%d =================\n", i);
		SP_CRYPTO_INF("reg     : %p\n", sp_dd_tb[i].reg);
		SP_CRYPTO_INF("regsize : %d\n", sizeof(struct sp_crypto_reg));
	}

	// must after reg ioremap, it's used in sp_rsa_init()
#endif
	SP_CRYPTO_TRACE();
	ret = sp_rsa_init();
	ERR_OUT(ret, goto out2, "sp_rsa_init");

	return 0;

out2:
	sp_aes_finit();
out1:
	sp_hash_finit();
out0:
	return ret;
}

static void __exit sp_crypto_module_exit(void)
{
	SP_CRYPTO_TRACE();
#ifdef USE_REF
	for (i = 0; i < ARRAY_SIZE(sp_dd_tb); ++i) {
		platform_device_del(to_platform_device(sp_dd_tb[i].device));
		platform_device_put(to_platform_device(sp_dd_tb[i].device));
		iounmap((void *)sp_dd_tb[i].reg);
	}
#endif
	platform_driver_unregister(&sp_crtpto_driver);

	sp_rsa_finit();
	sp_aes_finit();
	sp_hash_finit();
}

module_init(sp_crypto_module_init);
module_exit(sp_crypto_module_exit);

MODULE_DESCRIPTION("sunplus aes sha3 rsa hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("sunplus ltd jz.xiang");
