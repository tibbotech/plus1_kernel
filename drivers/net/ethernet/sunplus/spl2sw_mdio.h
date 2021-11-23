/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SPL2SW_MDIO_H__
#define __SPL2SW_MDIO_H__

#include "spl2sw_define.h"

int  spl2sw_mdio_read(struct spl2sw_common *comm, int phy_id, int regnum);
int  spl2sw_mdio_write(struct spl2sw_common *comm, int phy_id, int regnum, u16 val);
u32  spl2sw_mdio_init(struct spl2sw_common *comm);
void spl2sw_mdio_remove(struct spl2sw_common *comm);

#endif
