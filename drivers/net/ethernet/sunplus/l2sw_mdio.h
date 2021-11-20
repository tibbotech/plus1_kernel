/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __L2SW_MDIO_H__
#define __L2SW_MDIO_H__

#include "l2sw_define.h"

#define PHY_RUN_STATEMACHINE

int  mdio_read(struct l2sw_mac *mac, int phy_id, int regnum);
int  mdio_write(struct l2sw_mac *mac, int phy_id, int regnum, u16 val);
u32  mdio_init(struct platform_device *pdev, struct net_device *ndev);
void mdio_remove(struct net_device *ndev);

int  mac_phy_probe(struct net_device *ndev);
void mac_phy_start(struct net_device *ndev);
void mac_phy_stop(struct net_device *ndev);
void mac_phy_remove(struct net_device *ndev);
int  phy_cfg(struct l2sw_mac *mac);

#endif
