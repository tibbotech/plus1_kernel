/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SPL2SW_PHY_H__
#define __SPL2SW_PHY_H__

#include "spl2sw_define.h"

int  spl2sw_phy_probe(struct net_device *ndev);
void spl2sw_phy_start(struct net_device *ndev);
void spl2sw_phy_stop(struct net_device *ndev);
void spl2sw_phy_remove(struct net_device *ndev);
int  spl2sw_phy_cfg(struct spl2sw_mac *mac);

#endif
