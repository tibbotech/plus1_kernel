/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef __SP_CRYPTO_H__
#define __SP_CRYPTO_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/kfifo.h>
#include <asm/cacheflush.h>
#include <crypto/scatterwalk.h>

//#define SP_CRYPTO_LOCAL_TEST
//#define USE_REF
//#define USE_ERF				// use event_ring_full intr
//#define TRACE_WAIT_ORDER		// trace wait/wake_up order

#define CACHE_LINE_SIZE			(32)
#define CACHE_LINE_MASK			(CACHE_LINE_SIZE - 1)
#define CACHE_ALIGN(l)			ALIGN(l, CACHE_LINE_SIZE)

#define TRB_SIZE			sizeof(struct trb_s)

#define AES_RING(p)			((p)->rings[0])
#define HASH_RING(p)			((p)->rings[1])
#define TRB_RING(p)			((p)->dd->rings[(p)->type - SP_CRYPTO_AES])

#define HASH_CMD_RING_SIZE		(PAGE_SIZE / TRB_SIZE)
#define HASH_CMD_RING_SG		(1)
#define AES_CMD_RING_SIZE		(PAGE_SIZE / TRB_SIZE)
#define AES_CMD_RING_SG			(1)
#define HASH_EVENT_RING_SIZE		(HASH_CMD_RING_SIZE * HASH_CMD_RING_SG)
#define AES_EVENT_RING_SIZE		(AES_CMD_RING_SIZE * AES_CMD_RING_SG)

#define TRB_RING_ALIGN_MASK		(32 - 1)

#define BUS_SMALL_ENDIAN
#define dev_reg				u32
#define BITS_PER_REG			(32)
#define SP_TRB_PARA_LEN			(12)

#define AUTODMA_CRCR_ADDR_MASK		(~TRB_RING_ALIGN_MASK)
#define AUTODMA_CRCR_CP			(1 << 4)
#define AUTODMA_CRCR_CRR		(1 << 3)
#define AUTODMA_CRCR_CS			(1 << 1)
#define AUTODMA_CRCR_RCS		(1 << 0)

#define AUTODMA_RCSR_EN			(1 << 31)
#define AUTODMA_RCSR_ERF		(1 << 30)
#define AUTODMA_RCSR_SIZE_MASK		(0xffff)

#define AUTODMA_ERBAR_ERBA_MASK	(~TRB_RING_ALIGN_MASK)
#define AUTODMA_TRIGGER			(1)

#ifdef BUS_SMALL_ENDIAN
#define AUTODMA_CRCR_FLAGS		(AUTODMA_CRCR_RCS | AUTODMA_CRCR_CP)
#else
#define AUTODMA_CRCR_FLAGS		(AUTODMA_CRCR_RCS)
#endif

#define SP_CRYPTO_TRUE			(1)
#define SP_CRYPTO_FALSE			(0)

#define TRB_NORMAL			(1)
#define TRB_LINK			(2)

#define R(r)		readl_relaxed(&reg->r)
#define W(r, v)		writel_relaxed(v, &reg->r)
#define WR(r, op)	W(r, R(r) op)

enum {
	SP_CRYPTO_AES = 0x20180428,
	SP_CRYPTO_HASH,
	SP_CRYPTO_RSA,
};

extern void *base_va;
/*A2P: Addr to Pointer */
#define A2P(a, b)	((void *)((uintptr_t)(a) + ((uintptr_t)(b) >> 16 >> 16 << 16 << 16)))
#define P2A(p)		((u32)(uintptr_t)(p))

struct trb_s {

	/* Cycle bits. indicates the current cycle of the ring */
#ifdef BUS_SMALL_ENDIAN
	u32 priv;
#else				/*  */
	u32 c;
	//u32 rsv0:31;
#endif				/*  */
	/* Completion Code. only use in event trb
	 *   0 Invalid Indicates this field has not been updated
	 *   1 Success indicates the transfer is successfully completed
	 */
	u32 cc:1;

	/* Toggle Cycle bit. Used in link TRB only.
	 *   indicates the cycle bits will be toggle in trb_next segment.
	 */
	u32 tc:1;

	/* Interrupt On Complete.
	 *   when this bit is set, controller will set an interrupt
	 *   after this TRB is transmitted.
	 */
	u32 ioc:1;

	u32 rsv1:1;

	/* TRB type:
	 *   0x1: Normal. Normal TRB used in trb_ring.
	 *   0x2: Link. Link TRB to link to trb_ring segments
	 */
	u32 type:4;

	u32 rsv2:8;

	/*  Plain text size in bytes.
	 * indicates the read/write data bytes of this TRB. 0 means 0 length data.
	 */
	u32 size:16;

	/*For link TRB indicates the trb_next segment address.
	 * or  Source data pointer(depend on ENDC)
	 */
	u32 sptr;

	/* Destination data pointer (depend on ENDC)
	 */
	u32 dptr;

	/* Parameter
	 */
	u32 mode;
	u32 iptr; /* Initial Vector/Counter (IV/ICB) pointer */
	u32 kptr; /* AES/GHASH: Key/Sub-Key pointer */

#ifdef BUS_SMALL_ENDIAN
	//u32 rsv3:31;

	/* Cycle bits. indicates the current cycle of the ring */
	u32 c;

#else				/*  */
	u32 priv;

#endif				/*  */
};

struct trb_ring_s {
	struct trb_s *trb;
	dma_addr_t pa;

	/* get trb at tail, put trb at head */
	struct trb_s *head;
	struct trb_s *tail;
	struct trb_s *link;

	struct mutex lock;
	struct semaphore trb_sem; /* sem to protect over use of trb */

	u32 trigger_count;
	u32 irq_count;

#ifdef TRACE_WAIT_ORDER
	DECLARE_KFIFO(f, wait_queue_head_t *, 32);
#endif
};

#define SP_CRYPTO_DEV (ctx->base.dd->device)

struct sp_crypto_dev {
	void __iomem *reg;
	struct trb_ring_s *rings[2]; // AES/HASH
	u32 irq;
	u32 version;
	u32 devid;
	struct clk *clk;
	struct reset_control *rstc;
	struct device *device;
#ifdef USE_REF
	atomic_t rsa_ref_cnt;	/*reference count */
	atomic_t aes_ref_cnt;
	atomic_t hash_ref_cnt;
	u32 state;

#define SEC_DEV_INAVTIVE	0
#define SEC_DEV_ACTIVE		1
#endif
};

struct crypto_ctx_s {
	struct sp_crypto_dev *dd;

	int type;
	u32 mode; // PAR0

	bool done;
	wait_queue_head_t wait;
	//struct semaphore sem;
};

/* trb.para.mode */
#define M_MMASK		0x7F		// mode mask
// AES
#define M_AES_ECB	0x00000000
#define M_AES_CBC	0x00000001
#define M_AES_CTR	0x00000002
#define M_CHACHA20	0x00000003
#define M_ENC		(0 << 7)	// 0: encrypt
#define M_DEC		(1 << 7)	// 1: decrypt
// HASH
#define M_MD5		0x00000000
#define M_SHA3		0x00000001
#define M_SHA3_224	0x00000001
#define M_SHA3_256	0x00010001
#define M_SHA3_384	0x00020001
#define M_SHA3_512	0x00030001
#define M_GHASH		0x00000002
#define M_UPDATE	(0 << 7)	// 0: update
#define M_FINAL		(1 << 7)	// 1: final

#define AES_CMD_RD_IF	(1 << 8)
#define HASH_CMD_RD_IF	(1 << 7)
#define AES_TRB_IF		(1 << 6)
#define HASH_TRB_IF		(1 << 5)
#define AES_ERF_IF		(1 << 4)
#define HASH_ERF_IF		(1 << 3)
#define AES_DMA_IF		(1 << 2)
#define HASH_DMA_IF		(1 << 1)
#define RSA_DMA_IF		(1 << 0)

#define AES_CMD_RD_IE	(1 << 8)
#define HASH_CMD_RD_IE	(1 << 7)
#define AES_TRB_IE		(1 << 6)
#define HASH_TRB_IE		(1 << 5)
#define AES_ERF_IE		(1 << 4)
#define HASH_ERF_IE		(1 << 3)
#define AES_DMA_IE		(1 << 2)
#define HASH_DMA_IE		(1 << 1)
#define RSA_DMA_IE		(1 << 0)

struct sp_crypto_reg {

	/*  Field Name  Bit     Access Description  */
	/*  0.0 AES DMA Control Status register (AESDMACS)
	 *  SIZE        31:16   RW DMA Transfer Length
	 *  Reserved    15:4    RO Reserved
	 *  ED          3       RO Endian 0:little endian 1:big endian
	 *  Reserved    2:1     RO Reserved
	 *  EN          0       RW DMA enable, it will be auto-clear to 0
	 *  when DMA finishes
	 */
	dev_reg AESDMACS;

	/* 0.1 AES Source Data pointer (AESSPTR) */
	/* SPTR        31:0    RW Source address must 32B alignment */
	dev_reg AESSPTR;

	/* 0.2 AES Destination Data pointer (AESDPTR)
	 * DPTR        31:0    RW Destination address must 32B alignment
	 */
	dev_reg AESDPTR;

	/* 0.3 0.4 0.5: AES Parameters */
	dev_reg AESPAR0; // mode
	dev_reg AESPAR1; // iptr
	dev_reg AESPAR2; // kptr

	/* 0.6 HASH DMA Control Status register (HASHDMACS)
	 * SIZE        31:16   RW DMA Transfer Length
	 * Reserved    15:4    RO Reserved
	 * ED          3       RO Endian 0:little endian 1:big endian
	 * Reserved    2:1     RO Reserved
	 * EN          0       RW DMA enable, it will be auto-clear to 0
	 * when DMA finishes
	 */
	dev_reg HASHDMACS;

	/* 0.7 HASH Source Data pointer (HASHSPTR)
	 * SPTR 31:0 RW Source address must 32B alignment
	 */
	dev_reg HASHSPTR;

	/* 0.8 HASH Destination Data pointer (HASHDPTR)
	 * DPTR        31:0    RW Source address must 32B alignment
	 */
	dev_reg HASHDPTR;

	/* 0.9 0.10 0.11: HASH Parameters */
	dev_reg HASHPAR0; // mode
	dev_reg HASHPAR1; // iptr
	dev_reg HASHPAR2; // kptr(hptr)

	/* 0.12 RSA DMA Control Status register (RSADMACS)
	 * SIZE        31:16   RW DMA Transfer Length
	 * Reserved    15:4    RO Reserved
	 * ED          3       RO Endian 0:little endian 1:big endian
	 * Reserved    2:1     RO Reserved
	 * EN          0       RW DMA enable, it will be auto-clear to 0
	 * when DMA finishes
	 */
	dev_reg RSADMACS;
#define RSA_DMA_ENABLE  (1 << 0)
#define RSA_DATA_BE     (1 << 3)
#define RSA_DATA_LE     (0 << 3)
#define RSA_DMA_SIZE(x) (x << 16)

	/* 0.13 RSA Source Data pointer (RSASPTR)
	 * SPTR        31:0    RW Source(X) address Z=X**Y (mod N),
	 * must 32B alignment
	 */
	dev_reg RSASPTR;

	/* 0.14 RSA Destination Data pointer (RSADPTR)
	 * DPTR        31:0    RW Destination(Z) address Z=X**Y (mod N),
	 * must 32B alignment
	 */
	dev_reg RSADPTR;

	/* 0.15 RSA Dma Parameter 0 (RSAPAR0)
	 * D           31:16   RW N length, Only support 64*n(3<=n<=32)
	 * Reserved    15:8    RO Reserved
	 * PRECALC     7       RW Precalculate P2
	 *						  0: Precalculate and write back to pointer from P2PTR
	 *						  1: Fetch from P2PTR
	 * Reserved    6:0     RO Reserved
	 */
	dev_reg RSAPAR0;
#define RSA_SET_PARA_D(x)   ((x) << 16)
#define RSA_PARA_PRECAL_P2  (0 << 7)
#define RSA_PARA_FETCH_P2   (1 << 7)

	/* 0.16 RSA Dma Parameter 1 (RSAPAR1)
	 * YPTR 31:0 RW Y pointer Z=X**Y (mod N)
	 */
	dev_reg RSAYPTR;

	/* 0.17 RSA Dma Parameter 2 (RSAPAR2)
	 * NPTR 31:0 RW N pointer Z=X**Y (mod N)
	 */
	dev_reg RSANPTR;

	/* 0.18 RSA Dma Parameter 3 (RSAPAR3)
	 * P2PTR 31:0 RW P2 pointer P2 = P**2(mod N)
	 */
	dev_reg RSAP2PTR;

	/* 0.19 0.20 RSA Dma Parameter 4 (RSAPAR4)
	 * WPTR 31:0 RW W pointer W= -N**-1(mod N)
	 */
	dev_reg RSAWPTRL;
	dev_reg RSAWPTRH;

	/* 0.21 AES DMA Command Ring Control Register (AESDMA_CRCR)
	 * CRPTR       31:5    RW Command Ring Pointer The command ring
	 * should be 32bytes aligned
	 * CP          4       RW Cycle bit Position 0:Word 0[0] 1:Word7[31]
	 * CRR         3       RO Command Ring Running Indicates the command
	 * ring is running, SW can only change the pointer
	 * when this bit is cleared
	 * Reserved    2       RO Reserved
	 * CS          1       RW Command Ring Stop
	 * Write 1 to stop the command ring
	 * RCS         0       RW Ring Cycle State
	 * Indicates the initial state of ring cycle bit
	 */
	dev_reg AESDMA_CRCR;


	/* 0.22 AES DMA Event Ring Base Address Register (AESDMA_ERBAR)
	 * ERBA        31:4    RW Event Ring Base Address
	 * the first TRB of the status will be write to this address
	 * Reserved 3:0 RO Reserved
	 */
	dev_reg AESDMA_ERBAR;

	/* 0.23 AES DMA Event Ring De-queue Pointer Register (AESDMA_ERDPR)
	 * ERDP        31:4    RW Event Ring De-queue Pointer
	 * Indicates the TRB address of which the CPU is
	 * processing now
	 * Reserved    3:0     RO Reserved
	 */
	dev_reg AESDMA_ERDPR;

	/* 0.24 AES DMA Ring Control and Status Register (AESDMA_RCSR)
	 * EN          31      RW Auto DMA enable
	 * To enable the auto DMA feature
	 * ERF         30      RW1C Event ring Full
	 * Indicates the Event Ring has been writing full
	 * Reserved    29:16   RO  Reserved
	 * Size        15:0    RW Event Ring Size
	 * HWwill write to ERBA if the size reaches this value
	 * and ERDP != ERBA (number of trbs)
	 */
	dev_reg AESDMA_RCSR;

	/* 0.25 AES DMA Ring Trig Register (AESDMA_RTR)
	 * Reserved    31:1    RO Reserved
	 * CRT         0       RW Command Ring Trig
	 * After SW write 1 to this bit, HW will start transfer
	 * TRBs until the ring is empty or stopped
	 */
	dev_reg AESDMA_RTR;

	/* 0.26 HASH DMA Command Ring Control Register (HASHDMA_CRCR)
	 * CRPTR       31:5    RW  Command Ring Pointer
	 * The command ring should be 32bytes aligned
	 * CP          4       RW Cycle bit Position
	 * 0:Word 0[0] 1:Word7[31]
	 * CRR         3       RO Command Ring Running
	 * Indicates the command ring is running, SW can only
	 * change the pointer when this bit is cleared
	 * Reserved    2       RO Reserved
	 * CS          1       RW Command Ring Stop
	 * Write 1 to stop the command ring
	 * RCS         0       RW Ring Cycle State
	 * Indicates the initial state of ring cycle bit
	 */
	dev_reg HASHDMA_CRCR;


	/* 0.27 HASH DMA Event Ring Base Address Register (HASHDMA_ERBAR)
	 * ERBA        31:4    RW Event Ring Base Address
	 * The first TRB of the status will be write to this address
	 * Reserved    3:0     RO Reserved
	 */
	dev_reg HASHDMA_ERBAR;

	/* 0.28 HASH DMA Event Ring De-queue Pointer Register (HASHDMA_ERDPR)
	 * ERDP        31:4    RW Event Ring De-queue Pointer
	 * Indicates the TRB address of which the CPU is processing now
	 * Reserved    3:0     RO Reserved
	 */
	dev_reg HASHDMA_ERDPR;

	/* 0.29 HASH DMA Ring Control and Status Register (HASHDMA_RCSR)
	 * EN          31      RW Auto DMA enable
	 * To enable the auto DMA feature
	 * ERF         30      RW1C Event ring Full
	 * Indicates the Event Ring has been writing full
	 * Reserved    29:16   RO Reserved
	 * Size 15:0 RW Event Ring Size
	 * HWwill write to ERBA if the size reaches this value and  ERDP != ERBA
	 */
	dev_reg HASHDMA_RCSR;

	/* 0.30 HASH DMA Ring Trig Register (HASHDMA_RTR)
	 * Reserved    31:1    RO Reserved
	 * CRT         0       RW Command Ring Trig
	 * After SW write 1 to this bit, HW will start transfer
	 * TRBs until the ring is empty or stopped
	 */
	dev_reg HASHDMA_RTR;

	/* 0.31 */
	dev_reg reserved;



	/* 1.0 SEC IP Version (VERSION)
	 * VERSION 31:0 RO the date of version
	 */
	dev_reg VERSION;

	/* 1.1 Interrupt Enable (SECIE)
	 * Reserve     31:7    RO Reserve
	 * AES_TRB_IE  6       RW AES TRB done interrupt enable
	 * HASH TRB IE 5       RW HASH TRB done interrupt enable
	 * AES_ERF_IE  4       RW AES Event Ring Full interrupt enable
	 * HASH_ERF_IE 3       RW HASH Event Ring Full interrupt enable
	 * AES_DMA_IE  2       RW AES DMA finish interrupt enable
	 * HASH_DMA_IE 1       RW HASH DMA finish interrupt enable
	 * RSA_DMA_IE  0       RW RSA DMA finish interrupt enable
	 */
	dev_reg SECIE;


	/* 1.2 Interrupt Flag (SECIF)
	 * Reserve     31:7    RO Reserve
	 * AES_TRB_IF  6       RW AES TRB done interrupt flag
	 * HASH_TRB_IF 5       RW HASH TRB done interrupt flag
	 * AES_ERF_IF  4       RW AES Event Ring Full interrupt flag
	 * HASH_ERF_IF 3       RW HASH Event Ring Full interrupt flag
	 * AES_DMA_IF  2       RW AES DMA finish interrupt flag
	 * HASH_DMA_IF 1       RW HASH DMA finish interrupt flag
	 * RSA_DMA_IF  0       RW RSA DMA finish interrupt flag
	 */
	dev_reg SECIF;

	// for debug
	dev_reg AES_CR;		// AES  cmd ring pointer
	dev_reg AES_ER;		// AES  evt ring pointer
	dev_reg HASH_CR;	// HASH cmd ring pointer
	dev_reg HASH_ER;	// HASH evt ring pointer
};

/* funciton  */
#define ERR_OUT(err, goto_label, info, ...) \
do { \
	void *_err = (void *)((unsigned long)(err)); \
	if (IS_ERR(_err)) { \
		ret = PTR_ERR(_err); \
		SP_CRYPTO_ERR("ERR(%d) @ %s(%d): "info"\n", ret, __func__, __LINE__, ##__VA_ARGS__); \
		goto_label; \
	} \
} while (0)

//#define SP_CRYPTO_TRACE()  pr_info("%s:%d\n", __func__, __LINE__)
#define SP_CRYPTO_TRACE()
#define SP_CRYPTO_ERR(fmt, ...)  pr_err(fmt, ##__VA_ARGS__)
#define SP_CRYPTO_WAR(fmt, ...)  pr_warn(fmt, ##__VA_ARGS__)
#define SP_CRYPTO_INF(fmt, ...)  //pr_info(fmt, ##__VA_ARGS__)
#define SP_CRYPTO_DBG(fmt, ...)  //pr_debug(fmt, ##__VA_ARGS__)

void sp_crypto_free_dev(struct sp_crypto_dev *dev, u32 type);
struct sp_crypto_dev *sp_crypto_alloc_dev(int type);

int crypto_ctx_init(struct crypto_ctx_s *ctx, int type);
void crypto_ctx_exit(struct crypto_ctx_s *ctx);
int crypto_ctx_exec(struct crypto_ctx_s *ctx);
struct trb_s *crypto_ctx_queue(struct crypto_ctx_s *ctx,
	dma_addr_t src, dma_addr_t dst,
	dma_addr_t iv, dma_addr_t key,
	u32 len, u32 mode, bool ioc);

struct trb_s *trb_next(struct trb_s *trb);
struct trb_s *trb_get(struct trb_ring_s *ring);
struct trb_s *trb_put(struct trb_ring_s *ring);	/* USE_IN_IRQ, move ring tail to next */
void trb_ring_reset(struct trb_ring_s *ring);	/* USE_IN_IRQ, reset ring head & tail to base addr */

void dump_trb(struct trb_s *trb);

extern void dump_buf(u8 *buf, u32 len);

#endif //__SP_CRYPTO_H__
