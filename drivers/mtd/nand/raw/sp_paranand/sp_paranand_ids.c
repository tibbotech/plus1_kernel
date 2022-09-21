// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#include <linux/mtd/rawnand.h>
#include <linux/sizes.h>

#ifdef NAND_ID
#undef NAND_ID
#endif
#define NAND_ID(nm, mid, did, id2, id3, id4, pgsz, oobsz, bksz, chipsz, idlen, ecc_strength, ecc_step) \
	{.name=(nm),{.id={mid, did, id2, id3, id4}}, .pagesize=pgsz, .chipsize=chipsz,              \
	 .erasesize=bksz, .options=0, .id_len=idlen, .oobsize=oobsz,                        \
	 NAND_ECC_INFO(ecc_strength, ecc_step)}                                                   \

/*
 * Chip ID list
 *
 * nm     - nand chip name.
 * mid    - manufacture id.
 * did    - device id.
 * pgsz   - page size. (unit: byte)
 * oobsz  - oob size. (unit: byte)
 * bksz   - block/erase size. (unit: byte)
 * chipsz - chip size. (unit: mega byte)
 * id_len - length of read id return. (<=8)
 * ecc_strength - ECC correctability.
 * ecc_step     - ECC step.
 * options- stores various chip bit options.
 */
const struct nand_flash_dev sp_pnand_ids[] = {
	/*
		name,                      mid,  did,                    pgsz, oobsz, bksz, chipsz, id_len, ecc_strength, ecc_step
	*/
	NAND_ID("Samsung K9F2G08XXX ZEBU", 0xec, 0xaa, 0x80, 0x15, 0x50, SZ_2K, SZ_64, SZ_128K, SZ_256, 5, 2, 512),

	{NULL},
};


