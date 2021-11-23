// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "spl2sw_register.h"
#include "spl2sw_mdio.h"

#define spl2sw_mdio_read_CMD   0x02
#define spl2sw_mdio_write_CMD  0x01

static int spl2sw_mdio_access(struct spl2sw_common *comm, u8 op_cd, u8 phy_addr,
			      u8 reg_addr, u32 wdata)
{
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

int spl2sw_mdio_read(struct spl2sw_common *comm, int phy_id, int regnum)
{
	int ret;

	ret = spl2sw_mdio_access(comm, spl2sw_mdio_read_CMD, phy_id, regnum, 0);
	if (ret < 0)
		return -EIO;

	return ret;
}

int spl2sw_mdio_write(struct spl2sw_common *comm, int phy_id, int regnum, u16 val)
{
	int ret;

	ret = spl2sw_mdio_access(comm, spl2sw_mdio_write_CMD, phy_id, regnum, val);
	if (ret < 0)
		return -EIO;

	return 0;
}

static int spl2sw_mii_read(struct mii_bus *bus, int phy_id, int regnum)
{
	struct spl2sw_common *comm = bus->priv;

	return spl2sw_mdio_read(comm, phy_id, regnum);
}

static int spl2sw_mii_write(struct mii_bus *bus, int phy_id, int regnum, u16 val)
{
	struct spl2sw_common *comm = bus->priv;

	return spl2sw_mdio_write(comm, phy_id, regnum, val);
}

u32 spl2sw_mdio_init(struct spl2sw_common *comm)
{
	struct mii_bus *mii_bus;
	u32 ret;

	mii_bus = devm_mdiobus_alloc(&comm->pdev->dev);
	if (!mii_bus) {
		pr_err(" Failed to allocate mdio_bus memory!\n");
		return -ENOMEM;
	}

	mii_bus->name = "sunplus_mii_bus";
	mii_bus->parent = &comm->pdev->dev;
	mii_bus->priv = comm;
	mii_bus->read = spl2sw_mii_read;
	mii_bus->write = spl2sw_mii_write;
	snprintf(mii_bus->id, MII_BUS_ID_SIZE, "%s-mii", dev_name(&comm->pdev->dev));

	ret = of_mdiobus_register(mii_bus, comm->mdio_node);
	if (ret) {
		pr_err(" Failed to register mii bus (ret = %d)!\n", ret);
		return ret;
	}

	comm->mii_bus = mii_bus;
	return ret;
}

void spl2sw_mdio_remove(struct spl2sw_common *comm)
{
	if (comm->mii_bus) {
		mdiobus_unregister(comm->mii_bus);
		comm->mii_bus = NULL;
	}
}
