// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
/**
 * Cryptographic API.
 * Support for OMAP AES HW acceleration.
 * Author:jz.xiang
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
//#define DEBUG
#include <crypto/internal/aead.h>
#include <crypto/internal/skcipher.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <crypto/scatterwalk.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <crypto/aes.h>
#ifdef CONFIG_SOC_SP7350
#include <crypto/chacha.h>
#endif
#include "sp-crypto.h"

// max supported keysize == AES_KEYSIZE_256 (32 bytes)
#define WORKBUF_SIZE(ctx) (ctx->bsize + ctx->ivlen + AES_KEYSIZE_256)	// block + iv + key

/* structs */
struct sp_aes_ctx {
	struct crypto_ctx_s base;

	u8 *va, *iv;
	dma_addr_t pa; // workbuf phy addr
	u32 ivlen, keylen;
	u32 bsize; // block_size
};

static int sp_cra_aes_init(struct crypto_tfm *tfm, u32 mode)
{
	struct sp_aes_ctx *ctx = crypto_tfm_ctx(tfm);

	SP_CRYPTO_TRACE();

	ctx->base.mode = mode;
	ctx->ivlen = 16; // CHACHA_IV_SIZE == AES_BLOC_SIZE == 16
	ctx->bsize = (mode == M_CHACHA20) ? CHACHA_BLOCK_SIZE : AES_BLOCK_SIZE;

	crypto_ctx_init(&ctx->base, SP_CRYPTO_AES);
	ctx->va = dma_alloc_coherent(SP_CRYPTO_DEV, WORKBUF_SIZE(ctx), &ctx->pa, GFP_KERNEL);
	ctx->iv = NULL; // delay inited @ 1st time call sp_blk_aes_crypt()

	return 0;
}

static int sp_cra_aes_ecb_init(struct crypto_tfm *tfm)
{
	return sp_cra_aes_init(tfm, M_AES_ECB);
}

static int sp_cra_aes_cbc_init(struct crypto_tfm *tfm)
{
	return sp_cra_aes_init(tfm, M_AES_CBC);
}

static int sp_cra_aes_ctr_init(struct crypto_tfm *tfm)
{
	return sp_cra_aes_init(tfm, M_AES_CTR | (128 << 24)); // CTR M = 128
}

#ifdef CONFIG_SOC_SP7350
static int sp_cra_chacha20_init(struct crypto_tfm *tfm)
{
	return sp_cra_aes_init(tfm, M_CHACHA20);
}
#endif

static void sp_cra_aes_exit(struct crypto_tfm *tfm)
{
	struct sp_aes_ctx *ctx = crypto_tfm_ctx(tfm);

	dma_free_coherent(SP_CRYPTO_DEV, WORKBUF_SIZE(ctx), ctx->va, ctx->pa);
	crypto_ctx_exit(crypto_tfm_ctx(tfm));
}

#ifndef CONFIG_SOC_SP7350
/*
 * counter = counter + 1
 * counter: small endian
 */
static void ctr_inc1(u8 *counter, u32 len)
{
	u32 n = 0;
	u8 c;

	do {
		c = counter[n];
		++c;
		counter[n] = c;
		if (c)
			return;
		++n;
	} while (n < len);
}


/*
 * counter = counter + inc
 * counter: small endian
 */
static void ctr_inc(u8 *counter, u32 len, u32 inc)
{
	u32 c, c1;
	u32 *p = (u32 *)counter;

	c =  __le32_to_cpu(*p);
	c1 = c;
	c += inc;
	*p = __cpu_to_le32(c);

	if ((c >= inc) && (c >= c1))
		return;

	ctr_inc1(counter + sizeof(u32), len - sizeof(u32));
}

static void reverse_iv(u8 *dst, u8 *src)
{
	int i = AES_BLOCK_SIZE;

	while (i--)
		dst[i] = *(src++);
}
#endif

static void dump_sglist(struct scatterlist *sglist, int count)
{
#ifdef DEBUG
	int i;
	struct scatterlist *sg;

	pr_info("sglist: %px\n", sglist);
	for_each_sg(sglist, sg, count, i) {
		pr_info("\t%08x (%08x)\n", sg_dma_address(sg), sg_dma_len(sg));
	}
#endif
}

static int sp_blk_aes_set_key(struct crypto_skcipher *tfm,
	const u8 *in_key, unsigned int key_len)
{
	struct sp_aes_ctx *ctx = crypto_tfm_ctx(crypto_skcipher_tfm(tfm));

	SP_CRYPTO_TRACE();
	// TODO: dirty hack code, fix me
	if (key_len & 1) {
		key_len &= ~1;
		ctx->base.mode = M_AES_CTR | (32 << 24); // GCTR M = 32;
	}

	if (key_len != AES_KEYSIZE_128 && key_len != AES_KEYSIZE_192 && key_len != AES_KEYSIZE_256) {
		SP_CRYPTO_ERR("unsupported AES key length: %d\n", key_len);
		return -EINVAL;
	}

	ctx->keylen = key_len;
	ctx->base.mode |= (key_len / 4) << 16; // AESPAR0_NK
	memcpy(ctx->va + ctx->bsize + ctx->ivlen, in_key, key_len); // key: iv + ivlen

	return 0;
}

static int sp_blk_aes_crypt(struct skcipher_request *req, u32 enc)
{
#define NO_WALK		0
#define SRC_WALK	1
#define DST_WALK	2
#define BOTH_WALK	3

	u32 flag = BOTH_WALK;
	int ret = 0;
	int src_cnt, dst_cnt;
	dma_addr_t src_phy, dst_phy;
	bool ioc;
	u32 processed = 0, process = 0;
	struct scatterlist *sp;
	struct scatterlist *dp;
	struct scatterlist *src = req->src;
	struct scatterlist *dst = req->dst;
	u32 nbytes = req->cryptlen;
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct sp_aes_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct crypto_ctx_s *ctx0 = &ctx->base;
	struct trb_ring_s *ring = AES_RING(ctx0->dd);
	dma_addr_t iv_phy, key_phy, tmp_phy = 0;
	u32 mode = ctx0->mode & M_MMASK;
	u32 mm = ctx0->mode | enc; // AESPAR0 (trb.para.mode)

	//pr_info(">>>>> %08x <<<<<\n", mm);
	if (mode != M_CHACHA20 || !ctx->iv) {
		ctx->iv = ctx->va + ctx->bsize;
		memcpy(ctx->iv, req->iv, ctx->ivlen);
	}
#ifdef CONFIG_SOC_SP7021
	if (mode == M_AES_CTR) {
		// reverse iv byte-order for HW
		reverse_iv(ctx->iv, req->iv);
	} else
#endif
	if (mode == M_AES_CBC && enc == M_DEC) {
		scatterwalk_map_and_copy(req->iv, src,
			nbytes - ctx->ivlen, ctx->ivlen, 0);
	}

	iv_phy = ctx->pa + ctx->bsize;
	key_phy = iv_phy + ctx->ivlen;
	sp = src;
	dp = dst;

	src_cnt = sg_nents(src);
	dst_cnt = sg_nents(dst);

	//dump_sglist(src, src_cnt);
	if (unlikely(dma_map_sg(SP_CRYPTO_DEV, src, src_cnt, DMA_TO_DEVICE) <= 0)) {
		SP_CRYPTO_ERR("sp aes map src sg  fail\n");
		return -EINVAL;
	}
	dump_sglist(src, src_cnt);

	//dump_sglist(dst, dst_cnt);
	if (unlikely(dma_map_sg(SP_CRYPTO_DEV, dst, dst_cnt, DMA_FROM_DEVICE) <= 0)) {
		SP_CRYPTO_ERR("sp aes map dst sg  fail\n");
		dma_unmap_sg(SP_CRYPTO_DEV, src,  src_cnt, DMA_TO_DEVICE);
		return -EINVAL;
	}
	dump_sglist(dst, dst_cnt);

	if (mutex_lock_interruptible(&ring->lock)) {
		dma_unmap_sg(SP_CRYPTO_DEV, src, src_cnt, DMA_TO_DEVICE);
		dma_unmap_sg(SP_CRYPTO_DEV, dst, dst_cnt, DMA_FROM_DEVICE);
		return -EINTR;
	}
	//mutex_lock(&ring->lock);

	while (processed < nbytes) {
		u32 reast;
		struct trb_s *trb;

		if (flag == BOTH_WALK) {
			src_phy = sg_dma_address(sp);
			dst_phy = sg_dma_address(dp);
			if (sg_dma_len(sp) > sg_dma_len(dp)) {
				process = sg_dma_len(dp);
				dp = sg_next(dp);
				flag = DST_WALK;
				SP_CRYPTO_TRACE();
			} else if (sg_dma_len(sp) == sg_dma_len(dp)) {
				process = sg_dma_len(sp);
				sp = sg_next(sp);
				dp = sg_next(dp);
				flag = BOTH_WALK;
				SP_CRYPTO_TRACE();
			} else {
				process = sg_dma_len(sp);
				sp = sg_next(sp);
				flag = SRC_WALK;
				SP_CRYPTO_TRACE();
			}
		} else if (flag == DST_WALK) {
			src_phy += process;
			dst_phy = sg_dma_address(dp);
			reast = sg_dma_len(sp) - (src_phy - sg_dma_address(sp));
			if (reast > sg_dma_len(dp)) {
				process = sg_dma_len(dp);
				dp = sg_next(dp);
				flag = DST_WALK;
			} else if (reast == sg_dma_len(dp)) {
				process = sg_dma_len(dp);
				dp = sg_next(dp);
				sp = sg_next(sp);
				flag = BOTH_WALK;
			} else {
				process = reast;
				sp = sg_next(sp);
				flag = SRC_WALK;
			}
		} else if (flag == SRC_WALK) {
			src_phy = sg_dma_address(sp);
			dst_phy += process;
			reast = sg_dma_len(dp) - (dst_phy - sg_dma_address(dp));
			if (reast > sg_dma_len(sp)) {
				process = sg_dma_len(sp);
				sp = sg_next(sp);
				flag = SRC_WALK;
			} else if (reast == sg_dma_len(sp)) {
				process = sg_dma_len(sp);
				dp = sg_next(dp);
				sp = sg_next(sp);
				flag = BOTH_WALK;
			} else {
				process = reast;
				dp = sg_next(dp);
				flag = DST_WALK;
			}
		} else {
			src_phy += process;
			dst_phy += process;
		}

		process = min_t(u32, process, nbytes - processed);
		if (process < ctx->bsize) {
			tmp_phy = ctx->pa;
		} else if (process % ctx->bsize) {
			process &= ~(ctx->bsize - 1);
			flag = NO_WALK;
		}

		processed += process;
		ioc = (processed == nbytes) || (ring->trb_sem.count == 1);

		SP_CRYPTO_TRACE();
		if (tmp_phy)
			trb = crypto_ctx_queue(ctx0, src_phy, tmp_phy, iv_phy, key_phy, ctx->bsize, mm, ioc);
		else
			trb = crypto_ctx_queue(ctx0, src_phy, dst_phy, iv_phy, key_phy, process, mm, ioc);
		if (!trb) {
			ret = -EINTR;
			goto out;
		}
		iv_phy = 0; // after iv inital value, using HW auto-iv

		if (ioc) {
			SP_CRYPTO_TRACE();
			ret = crypto_ctx_exec(ctx0);
			if (ret) {
				//if (ret == -ERESTARTSYS) ret = 0;
				goto out;
			}
		}
	};
	SP_CRYPTO_TRACE();
	BUG_ON(processed != nbytes);

out:
	mutex_unlock(&ring->lock);
	dma_unmap_sg(SP_CRYPTO_DEV, src, src_cnt, DMA_TO_DEVICE);
	dma_unmap_sg(SP_CRYPTO_DEV, dst, dst_cnt, DMA_FROM_DEVICE);

	if (tmp_phy) {
		//dump_buf(ctx->va, ctx->bsize);
		scatterwalk_map_and_copy(ctx->va, dst, nbytes - process, process, 1);
	}

	// update iv for return
#ifdef CONFIG_SOC_SP7021
	if (mode == M_AES_CTR) {
		ctr_inc(ctx->iv, ctx->ivlen, nbytes / ctx->bsize);
		reverse_iv(req->iv, ctx->iv);
	} else
#endif
	if (mode == M_AES_CBC && enc == M_ENC) {
		scatterwalk_map_and_copy(req->iv, dst,
			nbytes - ctx->ivlen, ctx->ivlen, 0);
	} else
	if (mode != M_CHACHA20) {
		memcpy(req->iv, ctx->iv, ctx->ivlen);
	}

	return ret;
}

static int sp_blk_aes_encrypt(struct skcipher_request *req)
{
	return sp_blk_aes_crypt(req, M_ENC);
}

static int sp_blk_aes_decrypt(struct skcipher_request *req)
{
	return sp_blk_aes_crypt(req, M_DEC);
}

/* tell the block cipher walk routines that this is a stream cipher by
 * setting cra_blocksize to 1. Even using blkcipher_walk_virt_block
 * during encrypt/decrypt doesn't solve this problem, because it calls
 * blkcipher_walk_done under the covers, which doesn't use walk->blocksize,
 * but instead uses this tfm->blocksize.
 */
struct skcipher_alg sp_aes_alg[] = {

	{
		.base.cra_name		= "ecb(aes)",
		.base.cra_driver_name	= "sp-aes-ecb",
		.base.cra_priority	= 300,
		.base.cra_blocksize	= AES_BLOCK_SIZE,
		.base.cra_alignmask	= AES_BLOCK_SIZE - 1,
		.base.cra_ctxsize	= sizeof(struct sp_aes_ctx),
		.base.cra_module	= THIS_MODULE,
		.base.cra_init		= sp_cra_aes_ecb_init,
		.base.cra_exit		= sp_cra_aes_exit,
		.min_keysize		= AES_MIN_KEY_SIZE,
		.max_keysize		= AES_MAX_KEY_SIZE,
		.ivsize			= AES_BLOCK_SIZE,
		.setkey			= sp_blk_aes_set_key,
		.encrypt		= sp_blk_aes_encrypt,
		.decrypt		= sp_blk_aes_decrypt,
	},
	{
		.base.cra_name		= "cbc(aes)",
		.base.cra_driver_name	= "sp-aes-cbc",
		.base.cra_priority	= 300,
		.base.cra_blocksize	= AES_BLOCK_SIZE,
		.base.cra_alignmask	= AES_BLOCK_SIZE - 1,
		.base.cra_ctxsize	= sizeof(struct sp_aes_ctx),
		.base.cra_module	= THIS_MODULE,
		.base.cra_init		= sp_cra_aes_cbc_init,
		.base.cra_exit		= sp_cra_aes_exit,
		.min_keysize		= AES_MIN_KEY_SIZE,
		.max_keysize		= AES_MAX_KEY_SIZE,
		.ivsize			= AES_BLOCK_SIZE,
		.setkey			= sp_blk_aes_set_key,
		.encrypt		= sp_blk_aes_encrypt,
		.decrypt		= sp_blk_aes_decrypt,
	},
	{
		.base.cra_name		= "ctr(aes)",
		.base.cra_driver_name	= "sp-aes-ctr",
		.base.cra_priority	= 300,
		.base.cra_blocksize	= 1,
		.base.cra_alignmask	= AES_BLOCK_SIZE - 1,
		.base.cra_ctxsize	= sizeof(struct sp_aes_ctx),
		.base.cra_module	= THIS_MODULE,
		.base.cra_init		= sp_cra_aes_ctr_init,
		.base.cra_exit		= sp_cra_aes_exit,
		.min_keysize		= AES_MIN_KEY_SIZE,
		.max_keysize		= AES_MAX_KEY_SIZE,
		.ivsize			= AES_BLOCK_SIZE,
		.setkey			= sp_blk_aes_set_key,
		.encrypt		= sp_blk_aes_encrypt,
		.decrypt		= sp_blk_aes_decrypt,
	},
#ifdef CONFIG_SOC_SP7350
	{
		.base.cra_name		= "chacha20",
		.base.cra_driver_name	= "sp-chacha20",
		.base.cra_priority	= 300,
		.base.cra_blocksize	= 1,
		.base.cra_alignmask	= CHACHA_BLOCK_SIZE - 1,
		.base.cra_ctxsize	= sizeof(struct sp_aes_ctx),
		.base.cra_module	= THIS_MODULE,
		.base.cra_init		= sp_cra_chacha20_init,
		.base.cra_exit		= sp_cra_aes_exit,
		.min_keysize		= CHACHA_KEY_SIZE,
		.max_keysize		= CHACHA_KEY_SIZE,
		.ivsize			= CHACHA_IV_SIZE,
		.setkey			= sp_blk_aes_set_key,
		.encrypt		= sp_blk_aes_encrypt,
		.decrypt		= sp_blk_aes_decrypt,
	},
#endif
};

int sp_aes_finit(void)
{
	SP_CRYPTO_TRACE();
	crypto_unregister_skciphers(sp_aes_alg, ARRAY_SIZE(sp_aes_alg));
	return 0;
}
EXPORT_SYMBOL(sp_aes_finit);

int sp_aes_init(void)
{
	SP_CRYPTO_TRACE();
	return  crypto_register_skciphers(sp_aes_alg, ARRAY_SIZE(sp_aes_alg));
}
EXPORT_SYMBOL(sp_aes_init);

void sp_aes_irq(void *devid, u32 flag)
{
	struct sp_crypto_dev *dev = devid;
	struct sp_crypto_reg *reg = dev->reg;
	struct trb_ring_s *ring = AES_RING(dev);

#ifdef TRACE_WAIT_ORDER
	SP_CRYPTO_INF(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s:%08x %08x %08x %d\n", __func__,
		flag, reg->AES_ER, reg->AESDMA_CRCR, ring->trb_sem.count/*kfifo_len(&ring->f)*/);
#else
	SP_CRYPTO_INF(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s:%08x %08x %08x\n", __func__,
		flag, reg->AES_ER, reg->AESDMA_CRCR);
#endif
	if (flag & AES_TRB_IF) { // autodma/trb done
		struct trb_s *trb = ring->head;

		ring->irq_count++;
		if (!trb->cc)
			dump_trb(trb);
		while (trb->cc) {
			//dump_trb(trb);
			if (trb->ioc) {
				struct crypto_ctx_s *ctx = A2P(trb->priv, base_va);

				if (ctx->type == SP_CRYPTO_AES) {
#ifdef TRACE_WAIT_ORDER
					wait_queue_head_t *w;

					BUG_ON(!kfifo_get(&ring->f, &w));
					BUG_ON(w != &ctx->wait);
#endif
					ctx->done = true;
					wake_up(&ctx->wait);
				} else {
					pr_info("\nAES_SKIP: %08x\n", ctx->type);
					dump_trb(trb);
				}
			}
			trb = trb_put(ring);
		}
	}
#ifdef USE_ERF
	if (flag & AES_ERF_IF) { // event ring full
		SP_CRYPTO_ERR("\n!!! %08x %08x\n", reg->AESDMA_ERBAR, reg->AESDMA_ERDPR);
		reg->AESDMA_RCSR |= AUTODMA_RCSR_ERF; // clear event ring full
	}
#endif
	SP_CRYPTO_INF("\n");
}
EXPORT_SYMBOL(sp_aes_irq);
