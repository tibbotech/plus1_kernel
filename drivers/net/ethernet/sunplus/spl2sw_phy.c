// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "spl2sw_register.h"
#include "spl2sw_mdio.h"
#include "spl2sw_phy.h"

static void spl2sw_mii_linkchange(struct net_device *ndev)
{
}

int spl2sw_phy_probe(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);
	struct phy_device *phydev;
	int i;

	phydev = of_phy_connect(ndev, mac->comm->phy1_node, spl2sw_mii_linkchange,
				0, PHY_INTERFACE_MODE_RMII);
	if (!phydev) {
		pr_err(" \"%s\" has no phy found\n", ndev->name);
		return -1;
	}

	if (mac->comm->phy2_node) {
		of_phy_connect(ndev, mac->comm->phy2_node, spl2sw_mii_linkchange,
			       0, PHY_INTERFACE_MODE_RMII);
	}

	for (i = 0; i < sizeof(phydev->supported) / sizeof(long); i++)
		phydev->advertising[i] = phydev->supported[i];

	phydev->irq = PHY_IGNORE_INTERRUPT;
	mac->comm->phy_dev = phydev;

	return 0;
}

void spl2sw_phy_start(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);

	phy_start(mac->comm->phy_dev);
}

void spl2sw_phy_stop(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);

	if (mac->comm->phy_dev != NULL) {
		phy_stop(mac->comm->phy_dev);
	}
}

void spl2sw_phy_remove(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);

	if (mac->comm->phy_dev != NULL) {
		phy_disconnect(mac->comm->phy_dev);
		mac->comm->phy_dev = NULL;
	}
}

int spl2sw_phy_cfg(struct spl2sw_mac *mac)
{
	// Bug workaround:
	// Flow-control of phy should be enabled. L2SW IP flow-control will refer
	// to the bit to decide to enable or disable flow-control.
	spl2sw_mdio_write(mac->comm, mac->comm->phy1_addr, 4,
		   spl2sw_mdio_read(mac->comm, mac->comm->phy1_addr, 4) | (1 << 10));
	spl2sw_mdio_write(mac->comm, mac->comm->phy2_addr, 4,
		   spl2sw_mdio_read(mac->comm, mac->comm->phy2_addr, 4) | (1 << 10));

	return 0;
}
