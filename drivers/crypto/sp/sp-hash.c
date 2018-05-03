#include <crypto/algapi.h>
#include <linux/cryptohash.h>
#include <crypto/internal/hash.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/kfifo.h>
#include <linux/delay.h>

#include "sp-crypto.h"
#include "sp-hash.h"

#define SHA3_224_DIGEST_SIZE	(224 / 8)
#define SHA3_224_BLOCK_SIZE		(200 - 2 * SHA3_224_DIGEST_SIZE)
#define SHA3_256_DIGEST_SIZE	(256 / 8)
#define SHA3_256_BLOCK_SIZE		(200 - 2 * SHA3_256_DIGEST_SIZE)
#define SHA3_384_DIGEST_SIZE	(384 / 8)
#define SHA3_384_BLOCK_SIZE		(200 - 2 * SHA3_384_DIGEST_SIZE)
#define SHA3_512_DIGEST_SIZE	(512 / 8)
#define SHA3_512_BLOCK_SIZE		(200 - 2 * SHA3_512_DIGEST_SIZE)

#define SHA3_BUF_SIZE			(200)

#define GHASH_BLOCK_SIZE		(16)
#define GHASH_DIGEST_SIZE		(16)
#define GHASH_KEY_SIZE			(16)

struct sp_hash_ctx {
	crypto_ctx_t base;

	u8 *buf_org;
	u8 *buf;		// 1600bits 32B-aligned buf: digest + block + key(ghash)
	u8 *block;
	u32 block_size;
	u64 byte_count;
	u32 partial;
	trb_t *last; // save last trb

	/*  only use in ghash */
	u8 *key;

	u32 alg_type;
	u32 digest_len;
};

static int sp_shash_transform(struct sp_hash_ctx *ctx, u8 *src, u32 srclen)
{
	int ret = 0;
	crypto_ctx_t *ctx0 = &ctx->base;
	trb_t *trb;
	bool ioc = (ctx0->mode & M_FINAL) || (src == ctx->block);

	SP_CRYPTO_TRACE();
	//dump_buf(src, srclen);

	if (srclen) {
		DCACHE_CLEAN(src, srclen);

		trb = crypto_ctx_queue(ctx0,
			__pa(src), __pa(ctx->buf),
			__pa(ctx->buf), __pa(ctx->key),
			srclen, ctx0->mode, ioc);
		if (!trb) return -EINTR;
		ctx->last = trb;
	} else {
		trb_ring_t *ring = HASH_RING(ctx0->dd);
		if (ring->head == ring->tail) { // queue empty
			return 0;
		}
		SP_CRYPTO_TRACE();
#ifdef TRACE_WAIT_ORDER
		kfifo_put(&ring->f, &ctx0->wait);
#endif
		// modify last trb
		trb = ctx->last;
		trb->ioc = ioc;
		trb->mode = ctx0->mode;
		//dump_trb(trb);
	}

	if (ioc) {
		SP_CRYPTO_TRACE();
		ret = crypto_ctx_exec(ctx0);
		//if (ret == -ERESTARTSYS) ret = 0;
	}

	return ret;
}

static int sp_cra_init(struct crypto_tfm *tfm, u32 mode)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(tfm);
	u32 l;

	SP_CRYPTO_TRACE();

	ctx->base.mode = mode;
	ctx->alg_type = crypto_tfm_alg_type(tfm);
	ctx->block_size = crypto_tfm_alg_blocksize(tfm);
	switch (mode) {
	case M_MD5:
		l = CACHE_LINE_MASK + CACHE_ALIGN(MD5_DIGEST_SIZE) + (MD5_HMAC_BLOCK_SIZE * 2);
		break;
	case M_GHASH:
		l = CACHE_LINE_MASK + CACHE_ALIGN(GHASH_DIGEST_SIZE) + GHASH_BLOCK_SIZE + GHASH_KEY_SIZE;
		break;
	default: // M_SHA3
		l = CACHE_LINE_MASK + CACHE_ALIGN(SHA3_BUF_SIZE) + ctx->block_size;
		break;
	}
	ctx->buf_org = kzalloc(l, GFP_KERNEL);
	if (IS_ERR(ctx->buf_org))
		return -ENOMEM;

	ctx->buf = PTR_ALIGN(ctx->buf_org, CACHE_LINE_SIZE);

	return crypto_ctx_init(&ctx->base, SP_CRYPTO_HASH);
}

static int sp_cra_md5_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_MD5);
}

static int sp_cra_ghash_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_GHASH);
}

static int sp_cra_sha3_224_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_SHA3_224);
}

static int sp_cra_sha3_256_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_SHA3_256);
}

static int sp_cra_sha3_384_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_SHA3_384);
}

static int sp_cra_sha3_512_init(struct crypto_tfm *tfm)
{
	return sp_cra_init(tfm, M_SHA3_512);
}

static void sp_cra_exit(struct crypto_tfm *tfm)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(tfm);

	SP_CRYPTO_TRACE();
	kfree(ctx->buf_org);
	crypto_ctx_exit(&ctx->base);
}

static int sp_shash_export(struct shash_desc *desc, void *out)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&desc->tfm->base);

	DCACHE_INVALIDATE(ctx->buf, ctx->digest_len);
	memcpy(out, ctx->buf, ctx->digest_len);
	//printk("%s: ", __FUNCTION__); dump_buf(out, ctx->digest_len);

	return 0;
}

static int sp_shash_import(struct shash_desc *desc, const void *in)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&desc->tfm->base);

	memcpy(ctx->buf, in, ctx->digest_len);
	DCACHE_CLEAN(ctx->buf, ctx->digest_len);

    return 0;
}

static int sp_shash_update(struct shash_desc *desc, const u8 *data,
		  u32 len)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&desc->tfm->base);
	const u32 bsize = ctx->block_size;
	const u32 left = ctx->partial; // left bytes in block
	const u32 avail = bsize - left; // free bytes in block
	int ret = 0;

	//SP_CRYPTO_TRACE();

	ctx->byte_count += len;
	//printk("%s: %p (%08x/%08x)\n", __FUNCTION__, data, len, (u32)ctx->byte_count);
	//printk("%s: ", __FUNCTION__); dump_buf(data, len);
	//DCACHE_FLUSH(data, len);

	if (avail > len) { // no enough data to process (left + len < BLOCK_SIZE)
		memcpy(ctx->block + left, data, len); // append to block
	} else {
		if (left != 0) { // have data in block
			memcpy(ctx->block + left, data, avail); // fill block

			ret = sp_shash_transform(ctx, ctx->block, bsize); // process block
			if (ret) goto out;

			data += avail;
			len -= avail;
			ctx->partial = 0;
		}

		if (len >= bsize) { // process BLOCK_SIZE*N bytes (N >= 1)
			u32 l = len / bsize * bsize;
			ret = sp_shash_transform(ctx, (u8 *)data, l);
			if (ret) goto out;

			data += l;
			len -= l;
		}
		memcpy(ctx->block, data, len); // append to block
	}
	ctx->partial += len;

out:
	return ret;
}

static int sp_shash_final(struct shash_desc *desc, u8 *out)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&desc->tfm->base);
	const u32 bsize = ctx->block_size;
	const u32 left = ctx->partial; // left bytes in block
	u8 *p = ctx->block + left;
	int padding; // padding zero bytes
	int ret = 0;

	SP_CRYPTO_TRACE();

	// padding
	switch (ctx->base.mode) {
	case M_MD5:
		padding = (bsize * 2 - left - 1 - sizeof(u64)) % bsize;
		*p++ = 0x80;
		break;
	case M_GHASH:
		padding = left ? (bsize - left) : 0;
		break;
	default: // SHA3
		padding = bsize - left - 1;
		*p++ = 0x06;
		break;
	}

	memset(p, 0, padding); // padding zero
	p += padding;

	switch (ctx->base.mode) {
	case M_MD5:
		((u32 *)p)[0] = ctx->byte_count << 3;
		((u32 *)p)[1] = ctx->byte_count >> 29;
		p += sizeof(u64);
		break;
	case M_GHASH:
		break;
	default: // SHA3
		*(p - 1) |= 0x80;
		break;
	}

	// process block
	//printk("%s: ", __FUNCTION__); dump_buf(ctx->block, p - ctx->block);
	ctx->base.mode |= M_FINAL;
	ret = sp_shash_transform(ctx, ctx->block, p - ctx->block);
	ctx->base.mode &= ~M_FINAL;

	mutex_unlock(&HASH_RING(ctx->base.dd)->lock);
	if (!ret) {
		sp_shash_export(desc, out);
	}

	return ret;
}

static int sp_shash_ghash_setkey(struct crypto_shash *tfm,
			const u8 *key, unsigned int keylen)
{

	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&tfm->base);

	SP_CRYPTO_TRACE();
#if 1 // TODO: dirty hack code, fix me
	if (keylen & 1) {
		// called from crypto_gcm_setkey (gcm.c)
		keylen &= ~1;
		ctx->digest_len = GHASH_DIGEST_SIZE;
	}
#endif
	//dump_stack();
	//dump_buf(key, keylen);
	if (keylen != GHASH_KEY_SIZE) {
		crypto_shash_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		SP_CRYPTO_ERR("unsupported GHASH key length: %d\n", keylen);
		return -EINVAL;
	}

	ctx->key = ctx->buf + CACHE_ALIGN(GHASH_DIGEST_SIZE) + GHASH_BLOCK_SIZE;
	memcpy(ctx->key, key, keylen);
	DCACHE_CLEAN(ctx->key, keylen);

	return 0;
}

static int sp_shash_init(struct shash_desc *desc)
{
	struct sp_hash_ctx *ctx = crypto_tfm_ctx(&desc->tfm->base);
	crypto_ctx_t *ctx0 = &ctx->base;

	SP_CRYPTO_TRACE();
	//printk("!!! %s: %d\n", __FUNCTION__, ctx->digest_len);
	//dump_stack();
	if (!ctx->digest_len) {
		// called from crypto_create_session (CIOCGSESSION)
		ctx->digest_len = crypto_shash_alg(desc->tfm)->digestsize;
	} else	{
		// called from crypto_run (CIOCCRYPT) or gcm_hash (gcm.c)
		u32 l = (ctx0->mode & M_SHA3) ? SHA3_BUF_SIZE : ctx->digest_len;

		ctx->block = ctx->buf + CACHE_ALIGN(l);
		ctx->byte_count = 0;
		ctx->partial = 0;

		if (ctx0->mode == M_MD5) {
			((u32 *)ctx->buf)[0] = 0x67452301;
			((u32 *)ctx->buf)[1] = 0xefcdab89;
			((u32 *)ctx->buf)[2] = 0x98badcfe;
			((u32 *)ctx->buf)[3] = 0x10325476;
		} else {
			memset(ctx->buf, 0, l);
		}
		DCACHE_CLEAN(ctx->buf, l);

		// can't do mutex_lock in hash_update, we must lock it @ here
		SP_CRYPTO_TRACE();
		mutex_lock(&HASH_RING(ctx0->dd)->lock);
	}

	//SP_CRYPTO_INF("sp_shash_init: %08x %d %d --- %p %p %p\n", ctx->base.mode, ctx->digest_len, ctx->block_size, ctx->buf, ctx->block, ctx->base.mode == M_GHASH ? ctx->key : NULL);

	return 0;
}

static struct shash_alg sp_shash_alg[] =
{
	{
		.digestsize	=	MD5_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.update		=	sp_shash_update,
		.final		=	sp_shash_final,
		.export		=	sp_shash_export,
		.import		=	sp_shash_import,
		.base		=	{
			.cra_name		=	"md5",
			.cra_driver_name = 	"sp-md5",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	MD5_HMAC_BLOCK_SIZE,
			.cra_module		=	THIS_MODULE,
			.cra_priority	= 	300,
			.cra_ctxsize	= 	sizeof(struct sp_hash_ctx),
			.cra_alignmask	= 	0,
			.cra_init		= 	sp_cra_md5_init,
			.cra_exit		= 	sp_cra_exit,
		},
	},
	{
		.digestsize =	GHASH_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.setkey 	=	sp_shash_ghash_setkey,
		.update 	=	sp_shash_update,
		.final		=	sp_shash_final,
		.base		=	{
			.cra_name		=	"ghash",
			.cra_driver_name =  "sp-ghash",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	GHASH_BLOCK_SIZE,
			.cra_module 	=	THIS_MODULE,
			.cra_priority	= 	300,
			.cra_ctxsize	=	sizeof(struct sp_hash_ctx),
			.cra_alignmask	=	0,
			.cra_init		=	sp_cra_ghash_init,
			.cra_exit		=	sp_cra_exit,
		},
	},
	{
		.digestsize =	SHA3_224_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.update 	=	sp_shash_update,
		.final		=	sp_shash_final,
		.export 	=	sp_shash_export,
		.import 	=	sp_shash_import,
		.base		=	{
			.cra_name		=	"sha3-224",
			.cra_driver_name =  "sp-sha3-224",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	SHA3_224_BLOCK_SIZE,
			.cra_module 	=	THIS_MODULE,
			.cra_priority	=	300,
			.cra_ctxsize	=	sizeof(struct sp_hash_ctx),
			.cra_alignmask	=	0,
			.cra_init		=	sp_cra_sha3_224_init,
			.cra_exit		=	sp_cra_exit,
		},
	},
	{
		.digestsize =	SHA3_256_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.update 	=	sp_shash_update,
		.final		=	sp_shash_final,
		.export		=	sp_shash_export,
		.import		=	sp_shash_import,
		.base		=	{
			.cra_name		=	"sha3-256",
			.cra_driver_name =  "sp-sha3-256",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	SHA3_256_BLOCK_SIZE,
			.cra_module 	=	THIS_MODULE,
			.cra_priority	=	300,
			.cra_ctxsize	=	sizeof(struct sp_hash_ctx),
			.cra_alignmask	=	0,
			.cra_init		=	sp_cra_sha3_256_init,
			.cra_exit		=	sp_cra_exit,
		},
	},
	{
		.digestsize =	SHA3_384_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.update 	=	sp_shash_update,
		.final		=	sp_shash_final,
		.export		=	sp_shash_export,
		.import		=	sp_shash_import,
		.base		=	{
			.cra_name		=	"sha3-384",
			.cra_driver_name =  "sp-sha3-384",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	SHA3_384_BLOCK_SIZE,
			.cra_module 	=	THIS_MODULE,
			.cra_priority	=	300,
			.cra_ctxsize	=	sizeof(struct sp_hash_ctx),
			.cra_alignmask	=	0,
			.cra_init		=	sp_cra_sha3_384_init,
			.cra_exit		=	sp_cra_exit,
		},
	},
	{
		.digestsize =	SHA3_512_DIGEST_SIZE,
		.init		=	sp_shash_init,
		.update 	=	sp_shash_update,
		.final		=	sp_shash_final,
		.export		=	sp_shash_export,
		.import		=	sp_shash_import,
		.base		=	{
			.cra_name		=	"sha3-512",
			.cra_driver_name =  "sp-sha3-512",
			.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize	=	SHA3_512_BLOCK_SIZE,
			.cra_module 	=	THIS_MODULE,
			.cra_priority	=	300,
			.cra_ctxsize	=	sizeof(struct sp_hash_ctx),
			.cra_alignmask	=	0,
			.cra_init		=	sp_cra_sha3_512_init,
			.cra_exit		=	sp_cra_exit,
		},
	},
};


int sp_hash_finit(void)
{
	SP_CRYPTO_TRACE();
	return crypto_unregister_shashes(sp_shash_alg, ARRAY_SIZE(sp_shash_alg));
}
EXPORT_SYMBOL(sp_hash_finit);


int sp_hash_init(void)
{
	int i;
	SP_CRYPTO_TRACE();
	for (i = 0; i < ARRAY_SIZE(sp_shash_alg); i++) {
		sp_shash_alg[i].base.cra_flags &= ~CRYPTO_ALG_DEAD;
	}
	return crypto_register_shashes(sp_shash_alg, ARRAY_SIZE(sp_shash_alg));
}
EXPORT_SYMBOL(sp_hash_init);

void sp_hash_irq(void *devid, u32 flag)
{
	struct sp_crypto_dev *dev = devid;
	trb_ring_t *ring = HASH_RING(dev);

#ifdef TRACE_WAIT_ORDER
	SP_CRYPTO_INF(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s:%08x %08x %08x %d\n", __FUNCTION__, flag, dev->reg->HASH_ER, dev->reg->HASHDMA_CRCR, kfifo_len(&ring->f));
#else
	SP_CRYPTO_INF(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s:%08x %08x %08x\n", __FUNCTION__, flag, dev->reg->HASH_ER, dev->reg->HASHDMA_CRCR);
#endif
	if (flag & HASH_TRB_IF) { // autodma/trb done
		trb_t *trb = ring->head;
		ring->irq_count++;
		if (!trb->cc) dump_trb(trb);
		while (trb->cc) {
			//dump_trb(trb);
			if (trb->ioc) {
				crypto_ctx_t *ctx = trb->priv;
				if (ctx->type == SP_CRYPTO_HASH) {
#ifdef TRACE_WAIT_ORDER
					wait_queue_head_t *w;
					BUG_ON(!kfifo_get(&ring->f, &w));
					BUG_ON(w != &ctx->wait);
#endif
					ctx->done = true;
					if (ctx->mode & M_FINAL) wake_up(&ctx->wait);
				}
				else printk("HASH_SKIP\n");
			}
			trb = trb_put(ring);
		}
	}
#ifdef USE_ERF
	if (flag & HASH_ERF_IF) { // event ring full
		SP_CRYPTO_ERR("\n!!! %08x %08x\n", dev->reg->HASHDMA_ERBAR, dev->reg->HASHDMA_ERDPR);
		dev->reg->HASHDMA_RCSR |= AUTODMA_RCSR_ERF; // clear event ring full
	}
#endif
	SP_CRYPTO_INF("\n");
}
EXPORT_SYMBOL(sp_hash_irq);

