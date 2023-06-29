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

#define LONG_DEV_ID
#ifdef LONG_DEV_ID
#define NAND_ID(nm, mid, did, id2, id3, id4, pgsz, oobsz, bksz, chipsz, idlen, ecc_strength, ecc_step) \
	{.name=(nm),{.id={mid, did, id2, id3, id4}}, .pagesize=pgsz, .chipsize=chipsz,              \
	 .erasesize=bksz, .options=0, .id_len=idlen, .oobsize=oobsz,                        \
	 NAND_ECC_INFO(ecc_strength, ecc_step)}
#else
#define NAND_ID(nm, mid, did, pgsz, oobsz, bksz, chipsz, ecc_strength, ecc_step) \
	{.name=(nm),{.id={mid, did}}, .pagesize=pgsz, .chipsize=chipsz,                 \
	 .erasesize=bksz, .options=0, .oobsize=oobsz, NAND_ECC_INFO(ecc_strength, ecc_step)}
#endif
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
#ifdef LONG_DEV_ID
const struct nand_flash_dev sp_pnand_ids[] = {
	/*
		name,                      mid,  did,                    pgsz, oobsz, bksz, chipsz, id_len, ecc_strength, ecc_step
	*/
	NAND_ID("Samsung K9F2G08XXX ZEBU", 0xec, 0xaa, 0x80, 0x15, 0x50, SZ_2K, SZ_64, SZ_128K, SZ_256, 5, 2, 512),
	NAND_ID("Samsung K9GBG08U0B", 0xec, 0xd7, 0x94, 0x7e, 0x64, SZ_8K, SZ_1K, SZ_1M, SZ_512, 5, 2, 512),
	NAND_ID("GigaDevice 9AU4G8F3AMGI", 0xc8, 0xdc, 0x90, 0x95, 0xd6, SZ_2K, SZ_64, SZ_128K, SZ_512, 5, 2, 512),
	NAND_ID("GigaDevice 9FU4G8F4BMGI", 0xc8, 0xdc, 0x90, 0xa6, 0x57, SZ_2K, SZ_64, SZ_128K, SZ_256, 5, 2, 512),
	{NULL},
};
#else
const struct nand_flash_dev sp_pnand_ids[] = {
	/*
		name,                      mid,  did,     pgsz, oobsz, bksz, chipsz, ecc_strength, ecc_step
	*/
	NAND_ID("Samsung K9GBG08U0B", 0xec, 0xd7, SZ_2K, SZ_64, SZ_128K, SZ_256, 2, 512),
	NAND_ID("GD9FU4G8F4BMGI", 0xc8, 0xdc, SZ_2K, SZ_64, SZ_128K, SZ_256, 2, 512),
	//NAND_ID("GD9AU4G8F3AMG1", 0xc8, 0xdc, SZ_2K, SZ_64, SZ_128K, SZ_256, 2, 512),
	{NULL},
};
#endif
