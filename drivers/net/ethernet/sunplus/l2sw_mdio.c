// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "l2sw_mdio.h"
#include "l2sw_register.h"

#define MDIO_READ_CMD   0x02
#define MDIO_WRITE_CMD  0x01

static int mdio_access(struct l2sw_mac *mac, u8 op_cd, u8 phy_addr, u8 reg_addr, u32 wdata)
{
	struct l2sw_common *comm = mac->comm;
	u32 val, ret;

	writel((wdata << 16) | (op_cd << 13) | (reg_addr << 8) | phy_addr,
	       comm->l2sw_reg_base + L2SW_PHY_CNTL_REG0);

	ret = read_poll_timeout(readl, val, val & op_cd, 10, 1000, 1,
				comm->l2sw_reg_base + L2SW_PHY_CNTL_REG1);
	if (ret == 0)
		return val >> 16;
	else
		return ret;
}

int mdio_read(struct l2sw_mac *mac, int phy_id, int regnum)
{
	int ret;

	ret = mdio_access(mac, MDIO_READ_CMD, phy_id, regnum, 0);
	if (ret < 0)
		return -EIO;

	return ret;
}

int mdio_write(struct l2sw_mac *mac, int phy_id, int regnum, u16 val)
{
	int ret;

	ret = mdio_access(mac, MDIO_WRITE_CMD, phy_id, regnum, val);
	if (ret < 0)
		return -EIO;

	return 0;
}

static int mii_read(struct mii_bus *bus, int phy_id, int regnum)
{
	struct l2sw_mac *mac = bus->priv;

	return mdio_read(mac, phy_id, regnum);
}

static int mii_write(struct mii_bus *bus, int phy_id, int regnum, u16 val)
{
	struct l2sw_mac *mac = bus->priv;

	return mdio_write(mac, phy_id, regnum, val);
}

u32 mdio_init(struct platform_device *pdev, struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);
	struct device_node *mdio_node;
	struct mii_bus *mii_bus;
	u32 ret;

	mii_bus = mdiobus_alloc();
	if (!mii_bus) {
		pr_err(" Failed to allocate mdio_bus memory!\n");
		return -ENOMEM;
	}

	mii_bus->name = "sunplus_mii_bus";
	mii_bus->parent = &pdev->dev;
	mii_bus->priv = mac;
	mii_bus->read = mii_read;
	mii_bus->write = mii_write;
	snprintf(mii_bus->id, MII_BUS_ID_SIZE, "%s-mii", dev_name(&pdev->dev));

	mdio_node = of_get_parent(mac->comm->phy1_node);
	ret = of_mdiobus_register(mii_bus, mdio_node);
	if (ret) {
		pr_err(" Failed to register mii bus (ret = %d)!\n", ret);
		mdiobus_free(mii_bus);
		return ret;
	}

	mac->comm->mii_bus = mii_bus;
	return ret;
}

void mdio_remove(struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);

	if (mac->comm->mii_bus != NULL) {
		mdiobus_unregister(mac->comm->mii_bus);
		mdiobus_free(mac->comm->mii_bus);
		mac->comm->mii_bus = NULL;
	}
}

static void mii_linkchange(struct net_device *ndev)
{
}

int mac_phy_probe(struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);
	struct phy_device *phydev;
	int i;

	phydev = of_phy_connect(mac->ndev, mac->comm->phy1_node, mii_linkchange,
				0, PHY_INTERFACE_MODE_RMII);
	if (!phydev) {
		pr_err(" \"%s\" has no phy found\n", ndev->name);
		return -1;
	}

	if (mac->comm->phy2_node) {
		of_phy_connect(mac->ndev, mac->comm->phy2_node, mii_linkchange,
			       0, PHY_INTERFACE_MODE_RMII);
	}

#ifdef PHY_RUN_STATEMACHINE
	//phydev->supported &= (PHY_GBIT_FEATURES | SUPPORTED_Pause |
	//                    SUPPORTED_Asym_Pause);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);
	//phydev->advertising = phydev->supported;
	for (i = 0; i < sizeof(phydev->supported) / sizeof(long); i++)
		phydev->advertising[i] = phydev->supported[i];

	phydev->irq = PHY_IGNORE_INTERRUPT;
	mac->comm->phy_dev = phydev;
#endif

	return 0;
}

void mac_phy_start(struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);

#ifdef PHY_RUN_STATEMACHINE
	phy_start(mac->comm->phy_dev);
#else
	u32 phyaddr;
	struct phy_device *phydev = NULL;

	/* find the first (lowest address) PHY on the current MAC's MII bus */
	for (phyaddr = 0; phyaddr < PHY_MAX_ADDR; phyaddr++) {
		if (mac->comm->mii_bus->mdio_map[phyaddr]) {
			phydev = mac->comm->mii_bus->mdio_map[phyaddr];
			break;	/* break out with first one found */
		}
	}

	phydev = phy_attach(ndev, dev_name(&phydev->dev), 0, PHY_INTERFACE_MODE_GMII);
	if (IS_ERR(phydev)) {
		pr_err(" Failed to attach phy!\n");
		return;
	}

	phydev->supported &= (PHY_GBIT_FEATURES | SUPPORTED_Pause | SUPPORTED_Asym_Pause);
	phydev->advertising = phydev->supported;
	phydev->irq = PHY_IGNORE_INTERRUPT;
	mac->comm->phy_dev = phydev;

	phy_start_aneg(phydev);
#endif
}

void mac_phy_stop(struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);

	if (mac->comm->phy_dev != NULL) {
#ifdef PHY_RUN_STATEMACHINE
		phy_stop(mac->comm->phy_dev);
#else
		phy_detach(mac->comm->phy_dev);
		mac->comm->phy_dev = NULL;
#endif
	}
}

void mac_phy_remove(struct net_device *ndev)
{
	struct l2sw_mac *mac = netdev_priv(ndev);

	if (mac->comm->phy_dev != NULL) {
#ifdef PHY_RUN_STATEMACHINE
		phy_disconnect(mac->comm->phy_dev);
#endif
		mac->comm->phy_dev = NULL;
	}
}

int phy_cfg(struct l2sw_mac *mac)
{
	// Bug workaround:
	// Flow-control of phy should be enabled. L2SW IP flow-control will refer
	// to the bit to decide to enable or disable flow-control.
	mdio_write(mac, mac->comm->phy1_addr, 4,
		   mdio_read(mac, mac->comm->phy1_addr, 4) | (1 << 10));
	mdio_write(mac, mac->comm->phy2_addr, 4,
		   mdio_read(mac, mac->comm->phy2_addr, 4) | (1 << 10));

	return 0;
}
