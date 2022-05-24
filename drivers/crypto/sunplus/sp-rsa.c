// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#include <linux/module.h>
#include <linux/mpi.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "sp-crypto.h"
#include "sp-rsa.h"

/*	preprocessing p^2 mode mod	*/
#define MAX_RSA_BITS	(2048)
#define MAX_RSA_BYTES	(MAX_RSA_BITS / BITS_PER_BYTE)

#define RSA_BYTES_MASK	(7)

//#define TEST_RSA_SPEED

struct rsa_priv_data {
	struct rsa_para p2;
	struct rsa_para mode;

	struct mutex lock;
	wait_queue_head_t wait;
	u32 wait_flag;

	struct sp_crypto_dev *dev;
};

static struct rsa_priv_data rsa_priv = {};

/*	preprocessing p^2 MODE mod  */
int mont_p2(struct rsa_para *mod,  struct rsa_para *p2)
{
	int ret = -ENOMEM;
	MPI base, exp, mod1, p21;
	u8 base_buf[1] = {2};
	u16 exp_buf = mod->crp_bytes * BITS_PER_BYTE * 2; /* 2N.  2 *2048 = 4096 */
	u32 nlimbs = BITS2LONGS(mod->crp_bytes * BITS_PER_BYTE);
	int sign = 0, nbytes = 0;
	char *result;

	mod1 = mpi_read_raw_data(mod->crp_p, mod->crp_bytes);
	if (IS_ERR_OR_NULL(mod1))
		goto out;

	base = mpi_read_raw_data(base_buf, sizeof(base_buf));
	if (IS_ERR_OR_NULL(base))
		goto out_base;

	p21 = mpi_alloc(nlimbs);
	if (IS_ERR_OR_NULL(p21))
		goto out_p21;

	exp_buf = cpu_to_be16(exp_buf);
	exp = mpi_read_raw_data(&exp_buf, sizeof(exp_buf));
	if (IS_ERR_OR_NULL(exp))
		goto out_exp;

	ret = mpi_powm(p21, base, exp, mod1);
	if (unlikely(ret)) {
		ret = -EIO;
		goto out_powm;
	}
	result = mpi_get_buffer(p21, &nbytes, &sign);
	if (IS_ERR_OR_NULL(result))
		goto out_powm;

	ret = 0;
	memcpy(p2->crp_p, result,  nbytes);
	p2->crp_bytes = nbytes;
	kfree(result);

out_powm:
	mpi_free(exp);
out_exp:
	mpi_free(p21);
out_p21:
	mpi_free(base);
out_base:
	mpi_free(mod1);
out:
	return ret;
}
EXPORT_SYMBOL(mont_p2);

rsabase_t mont_w(struct rsa_para *mod)
{
	rsabase_t t = 1;
	rsabase_t mode;
	int i;
	int lb = RSA_BASE;

#ifdef RSA_DATA_BIGENDBIAN
	u32 pos = mod->crp_bytes - sizeof(mode);

	mode = *(rsabase_t *) (mod->crp_p + pos);
	mode = sp_rsabase_be_to_cpu(mode);
#else
	memcpy(&mode, mod->crp_p, sizeof(mode));
	//mode = *(rsabase_t *) mod->crp_p;
	mode = sp_rsabase_le_to_cpu(mode);
#endif
	for (i = 1 ; i < lb - 1; i++)
		t = (t * t * mode);

	return -t;
}
EXPORT_SYMBOL(mont_w);

int sp_rsa_init(void)
{
	memset(&rsa_priv, 0, sizeof(rsa_priv));
	rsa_priv.p2.crp_p = kmalloc(MAX_RSA_BYTES, GFP_KERNEL);
	if (IS_ERR_OR_NULL(rsa_priv.p2.crp_p))
		return -ENOMEM;
	rsa_priv.mode.crp_p = kzalloc(MAX_RSA_BYTES, GFP_KERNEL);
	if (IS_ERR_OR_NULL(rsa_priv.mode.crp_p))
		return -ENOMEM;
	init_waitqueue_head(&rsa_priv.wait);
	mutex_init(&rsa_priv.lock);

	rsa_priv.dev = sp_crypto_alloc_dev(SP_CRYPTO_RSA);
	((struct sp_crypto_reg *)rsa_priv.dev->reg)->RSAP2PTR = __pa(rsa_priv.p2.crp_p);

	return 0;
}
EXPORT_SYMBOL(sp_rsa_init);

void sp_rsa_finit(void)
{
	kfree(rsa_priv.p2.crp_p);
	kfree(rsa_priv.mode.crp_p);

	sp_crypto_free_dev(rsa_priv.dev, SP_CRYPTO_RSA);
}
EXPORT_SYMBOL(sp_rsa_finit);

void sp_rsa_irq(void *devid, u32 flag)
{
	//SP_CRYPTO_INF(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s:%08x\n", __FUNCTION__, flag);
	rsa_priv.wait_flag = SP_CRYPTO_TRUE;
	wake_up(&rsa_priv.wait);
}
EXPORT_SYMBOL(sp_rsa_irq);

static int sp_rsa_cmp(struct rsa_para *a, struct rsa_para *b)
{
	if (a->crp_bytes != b->crp_bytes)
		return 1;
	return memcmp(a->crp_p, b->crp_p, a->crp_bytes);
}

/*
 * res.crp_p base.crp_p exp.crp_p mod.crp_p must physical continuity
 */
int sp_powm(struct rsa_para *res, struct rsa_para *base,
				 struct rsa_para *exp, struct rsa_para *mod)
{
	struct sp_crypto_reg *reg = rsa_priv.dev->reg;
	struct device *dev = rsa_priv.dev->device;
	dma_addr_t a1, a2, a3, a4;
	u32 rsa_bytes;
	int ret;

	if (unlikely((base->crp_bytes & RSA_BYTES_MASK) ||
		(exp->crp_bytes & RSA_BYTES_MASK) ||
		(mod->crp_bytes & RSA_BYTES_MASK))) {
		dev_err(dev, "invalid arg: base->crp_bytes = 0x%x, exp->crp_bytes = 0x%x, mod->crp_bytes = 0x%x",
				base->crp_bytes, exp->crp_bytes, mod->crp_bytes);
		return -EINVAL;
	}

	rsa_bytes = max(base->crp_bytes, exp->crp_bytes);
	rsa_bytes = max(rsa_bytes, mod->crp_bytes);
	res->crp_bytes = mod->crp_bytes;

	mutex_lock(&rsa_priv.lock);
	if (sp_rsa_cmp(mod, &rsa_priv.mode)) {
		rsabase_t w = mont_w(mod);

		W(RSAWPTRL, (u32)w);
		W(RSAWPTRH, (u32)(w >> BITS_PER_REG));
		rsa_priv.mode.crp_bytes = mod->crp_bytes;
		memcpy(rsa_priv.mode.crp_p, mod->crp_p, mod->crp_bytes);
		W(RSAPAR0, RSA_SET_PARA_D(rsa_bytes * BITS_PER_BYTE) | RSA_PARA_PRECAL_P2);
		//dev_info(dev, "!!!!!!!!!!!!!!!! %08x %08x\n", reg->RSAWPTRH, reg->RSAWPTRL);
	} else {
		W(RSAPAR0, RSA_SET_PARA_D(rsa_bytes * BITS_PER_BYTE) | RSA_PARA_FETCH_P2);
	}

	W(RSASPTR, a1 = dma_map_single(dev, base->crp_p, base->crp_bytes, DMA_TO_DEVICE));
	W(RSAYPTR, a2 = dma_map_single(dev, exp->crp_p, exp->crp_bytes, DMA_TO_DEVICE));
	W(RSANPTR, a3 = dma_map_single(dev, mod->crp_p, mod->crp_bytes, DMA_TO_DEVICE));
	W(RSADPTR, a4 = dma_map_single(dev, res->crp_p, res->crp_bytes, DMA_FROM_DEVICE));

	rsa_priv.wait_flag = SP_CRYPTO_FALSE;
	smp_wmb(); /* memory barrier */

#ifdef RSA_DATA_BIGENDBIAN
	W(RSADMACS, RSA_DMA_SIZE(rsa_bytes) | RSA_DATA_BE | RSA_DMA_ENABLE);
#else
	W(RSADMACS, RSA_DMA_SIZE(rsa_bytes) | RSA_DATA_LE | RSA_DMA_ENABLE);
#endif
	ret = wait_event_interruptible_timeout(rsa_priv.wait, rsa_priv.wait_flag, 30*HZ);
	mutex_unlock(&rsa_priv.lock);
	if (!ret) {
		dev_err(dev, "wait RSA timeout\n");
		ret = -ETIMEDOUT;
	} else if (ret > 0) {// TODO: find the break-fail root cause
		ret = 0;
	} else { // < 0, ERROR
		dev_err(dev, "wait RSA error: %d\n", ret);
		//rsa_priv.mode.crp_bytes = 0; // reset
	}

	dma_unmap_single(dev, a1, base->crp_bytes, DMA_TO_DEVICE);
	dma_unmap_single(dev, a2, exp->crp_bytes, DMA_TO_DEVICE);
	dma_unmap_single(dev, a3, mod->crp_bytes, DMA_TO_DEVICE);
	dma_unmap_single(dev, a4, res->crp_bytes, DMA_FROM_DEVICE);

	return ret;
}
EXPORT_SYMBOL(sp_powm);

MODULE_LICENSE("GPL v2");
