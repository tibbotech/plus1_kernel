// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "spl2sw_mac.h"

void spl2sw_mac_hw_stop(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg, disable;

	if (comm->enable == 0) {
		writel(0xffffffff, comm->l2sw_reg_base + L2SW_SW_INT_MASK_0);
		writel(0xffffffff & (~MAC_INT_PORT_ST_CHG),
		       comm->l2sw_reg_base + L2SW_SW_INT_STATUS_0);

		// Disable cpu 0 and cpu 1.
		reg = readl(comm->l2sw_reg_base + L2SW_CPU_CNTL);
		writel((0x3 << 6) | reg, comm->l2sw_reg_base + L2SW_CPU_CNTL);
	}

	if (comm->dual_nic) {
		// Disable lan 0 and lan 1.
		disable = ((~comm->enable) & 0x3) << 24;
		reg = readl(comm->l2sw_reg_base + L2SW_PORT_CNTL0);
		writel(disable | reg, comm->l2sw_reg_base + L2SW_PORT_CNTL0);
	}
}

void spl2sw_mac_hw_start(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Enable cpu port 0 (6) & port 0 crc padding (8) */
	reg = readl(comm->l2sw_reg_base + L2SW_CPU_CNTL);
	writel((reg & (~(0x1 << 6))) | (0x1 << 8), comm->l2sw_reg_base + L2SW_CPU_CNTL);

	/* Enable lan 0 & lan 1 */
	reg = readl(comm->l2sw_reg_base + L2SW_PORT_CNTL0);
	writel(reg & (~(comm->enable << 24)), comm->l2sw_reg_base + L2SW_PORT_CNTL0);

	writel(MAC_INT_MASK_DEF, comm->l2sw_reg_base + L2SW_SW_INT_MASK_0);

	//spl2sw_mac_regs_print();
}

void spl2sw_mac_addr_set(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Write MAC address. */
	writel(mac->mac_addr[0] + (mac->mac_addr[1] << 8),
	       comm->l2sw_reg_base + L2SW_W_MAC_15_0);
	writel(mac->mac_addr[2] + (mac->mac_addr[3] << 8) + (mac->mac_addr[4] << 16) +
	       (mac->mac_addr[5] << 24), comm->l2sw_reg_base + L2SW_W_MAC_47_16);

	/* Set aging=1 */
	writel((mac->cpu_port << 10) + (mac->vlan_id << 7) + (1 << 4) + 0x1,
	       comm->l2sw_reg_base + L2SW_WT_MAC_AD0);

	/* Wait for completing. */
	do {
		reg = readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0);
		ndelay(10);
		pr_debug(" wt_mac_ad0 = %08x\n", reg);
	} while ((reg & (0x1 << 1)) == 0x0);
	pr_debug(" mac_ad0 = %08x, mac_ad = %08x%04x\n",
		 readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0),
		 readl(comm->l2sw_reg_base + L2SW_W_MAC_47_16),
		 readl(comm->l2sw_reg_base + L2SW_W_MAC_15_0) & 0xffff);

	//spl2sw_mac_addr_print(mac);
}

void spl2sw_mac_addr_del(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Write MAC address. */
	writel(mac->mac_addr[0] + (mac->mac_addr[1] << 8),
	       comm->l2sw_reg_base + L2SW_W_MAC_15_0);
	writel(mac->mac_addr[2] + (mac->mac_addr[3] << 8) + (mac->mac_addr[4] << 16) +
	       (mac->mac_addr[5] << 24), comm->l2sw_reg_base + L2SW_W_MAC_47_16);

	/* Wait for completing. */
	writel((0x1 << 12) + (mac->vlan_id << 7) + 0x1,
	       comm->l2sw_reg_base + L2SW_WT_MAC_AD0);
	do {
		reg = readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0);
		ndelay(10);
		pr_debug(" wt_mac_ad0 = %08x\n", reg);
	} while ((reg & (0x1 << 1)) == 0x0);
	pr_debug(" mac_ad0 = %08x, mac_ad = %08x%04x\n",
		 readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0),
		 readl(comm->l2sw_reg_base + L2SW_W_MAC_47_16),
		 readl(comm->l2sw_reg_base + L2SW_W_MAC_15_0) & 0xffff);

	//spl2sw_mac_addr_print(mac);
}

void spl2sw_mac_addr_table_del_all(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Wait for address table being idle. */
	do {
		reg = readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
		ndelay(10);
	} while (!(reg & MAC_ADDR_LOOKUP_IDLE));

	/* Search address table from start. */
	writel(readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH) | MAC_BEGIN_SEARCH_ADDR,
	       comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
	while (1) {
		do {
			reg = readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_ST);
			ndelay(10);
			pr_debug(" addr_tbl_st = %08x\n", reg);
		} while (!(reg & (MAC_AT_TABLE_END | MAC_AT_DATA_READY)));

		if (reg & MAC_AT_TABLE_END)
			break;

		pr_debug(" addr_tbl_st = %08x\n", reg);
		pr_debug(" @AT #%u: port=%01x, cpu=%01x, vid=%u, aging=%u, proxy=%u, mc_ingress=%u\n",
			 (reg >> 22) & 0x3ff, (reg >> 12) & 0x3, (reg >> 10) & 0x3,
			 (reg >> 7) & 0x7, (reg >> 4) & 0x7, (reg >> 3) & 0x1, (reg >> 2) & 0x1);

		/* Delete all entries which are learnt from lan ports. */
		if ((reg >> 12) & 0x3) {
			writel(readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER0),
			       comm->l2sw_reg_base + L2SW_W_MAC_15_0);
			writel(readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER1),
			       comm->l2sw_reg_base + L2SW_W_MAC_47_16);

			writel((0x1 << 12) + (reg & (0x7 << 7)) + 0x1,
			       comm->l2sw_reg_base + L2SW_WT_MAC_AD0);
			do {
				reg = readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0);
				ndelay(10);
				pr_debug(" wt_mac_ad0 = %08x\n", reg);
			} while ((reg & (0x1 << 1)) == 0x0);
			pr_debug(" mac_ad0 = %08x, mac_ad = %08x%04x\n",
				 readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0),
				 readl(comm->l2sw_reg_base + L2SW_W_MAC_47_16),
				 readl(comm->l2sw_reg_base + L2SW_W_MAC_15_0) & 0xffff);
		}

		/* Search next. */
		writel(readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH) | MAC_SEARCH_NEXT_ADDR,
		       comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
	}
}

__attribute__((unused))
void spl2sw_mac_addr_print(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg, regl, regh;

	// Wait for address table being idle.
	do {
		reg = readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
		ndelay(10);
	} while (!(reg & MAC_ADDR_LOOKUP_IDLE));

	// Search address table from start.
	writel(readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH) | MAC_BEGIN_SEARCH_ADDR,
	       comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
	while (1) {
		do {
			reg = readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_ST);
			ndelay(10);
			//pr_info(" addr_tbl_st = %08x\n", reg);
		} while (!(reg & (MAC_AT_TABLE_END | MAC_AT_DATA_READY)));

		if (reg & MAC_AT_TABLE_END)
			break;

		regl = readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER0);
		ndelay(10);	// delay here is necessary or you will get all 0 of MAC_ad_ser1.
		regh = readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER1);

		//pr_info(" addr_tbl_st = %08x\n", reg);
		pr_info(" AT #%u: port=%01x, cpu=%01x, vid=%u, aging=%u, proxy=%u, mc_ingress=%u, HWaddr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			(reg >> 22) & 0x3ff, (reg >> 12) & 0x3, (reg >> 10) & 0x3,
			(reg >> 7) & 0x7, (reg >> 4) & 0x7, (reg >> 3) & 0x1, (reg >> 2) & 0x1,
			regl & 0xff, (regl >> 8) & 0xff, regh & 0xff, (regh >> 8) & 0xff,
			(regh >> 16) & 0xff, (regh >> 24) & 0xff);

		/* Search next. */
		writel(readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH) | MAC_SEARCH_NEXT_ADDR,
		       comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH);
	}
}

void spl2sw_mac_hw_init(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Disable cpu0 and cpu 1. */
	reg = readl(comm->l2sw_reg_base + L2SW_CPU_CNTL);
	writel((0x3 << 6) | reg, comm->l2sw_reg_base + L2SW_CPU_CNTL);

	/* Descriptor base address */
	writel(mac->comm->desc_dma, comm->l2sw_reg_base + L2SW_TX_LBASE_ADDR_0);
	writel(mac->comm->desc_dma + sizeof(struct spl2sw_mac_desc) * TX_DESC_NUM,
	       comm->l2sw_reg_base + L2SW_TX_HBASE_ADDR_0);
	writel(mac->comm->desc_dma + sizeof(struct spl2sw_mac_desc) * (TX_DESC_NUM +
	       MAC_GUARD_DESC_NUM), comm->l2sw_reg_base + L2SW_RX_HBASE_ADDR_0);
	writel(mac->comm->desc_dma + sizeof(struct spl2sw_mac_desc) * (TX_DESC_NUM +
	       MAC_GUARD_DESC_NUM + RX_QUEUE0_DESC_NUM),
	       comm->l2sw_reg_base + L2SW_RX_LBASE_ADDR_0);

	/* Fc_rls_th=0x4a, Fc_set_th=0x3a, Drop_rls_th=0x2d, Drop_set_th=0x1d */
	writel(0x4a3a2d1d, comm->l2sw_reg_base + L2SW_FL_CNTL_TH);

	/* Cpu_rls_th=0x4a, Cpu_set_th=0x3a, Cpu_th=0x12, Port_th=0x12 */
	writel(0x4a3a1212, comm->l2sw_reg_base + L2SW_CPU_FL_CNTL_TH);

	/* mtcc_lmt=0xf, Pri_th_l=6, Pri_th_h=6, weigh_8x_en=1 */
	writel(0xf6680000, comm->l2sw_reg_base + L2SW_PRI_FL_CNTL);

	/* High-active LED */
	reg = readl(comm->l2sw_reg_base + L2SW_LED_PORT0);
	writel(reg | (1 << 28), comm->l2sw_reg_base + L2SW_LED_PORT0);

	/* phy address */
	reg = readl(comm->l2sw_reg_base + L2SW_MAC_FORCE_MODE);
	reg = (reg & (~(0x1f << 16))) | ((mac->comm->phy1_addr & 0x1f) << 16);
	reg = (reg & (~(0x1f << 24))) | ((mac->comm->phy2_addr & 0x1f) << 24);
	writel(reg, comm->l2sw_reg_base + L2SW_MAC_FORCE_MODE);

	/* Disable cpu port0 aging (12)
	 * Disable cpu port0 learning (14)
	 * Enable UC and MC packets
	 */
	reg = readl(comm->l2sw_reg_base + L2SW_CPU_CNTL);
	writel((reg & (~((0x1 << 14) | (0x3c << 0)))) | (0x1 << 12),
	       comm->l2sw_reg_base + L2SW_CPU_CNTL);

	spl2sw_mac_switch_mode(mac);

	if (!comm->dual_nic) {
		/* enable lan 0 & lan 1 */
		reg = readl(comm->l2sw_reg_base + L2SW_PORT_CNTL0);
		writel(reg & (~(0x3 << 24)), comm->l2sw_reg_base + L2SW_PORT_CNTL0);
	}

	writel(MAC_INT_MASK_DEF, comm->l2sw_reg_base + L2SW_SW_INT_MASK_0);
}

void spl2sw_mac_switch_mode(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	if (comm->dual_nic) {
		mac->cpu_port = 0x1;	// soc0
		mac->lan_port = 0x1;	// forward to port 0
		mac->to_vlan = 0x1;	// vlan group: 0
		mac->vlan_id = 0x0;	// vlan group: 0

		if (mac->next_ndev) {
			struct spl2sw_mac *mac2 = netdev_priv(mac->next_ndev);

			mac2->cpu_port = 0x1;	// soc0
			mac2->lan_port = 0x2;	// forward to port 1
			mac2->to_vlan = 0x2;	// vlan group: 1
			mac2->vlan_id = 0x1;	// vlan group: 1
		}

		/* Port 0: VLAN group 0
		 * Port 1: VLAN group 1
		 */
		writel((1 << 4) + 0, comm->l2sw_reg_base + L2SW_PVID_CONFIG0);

		/* VLAN group 0: cpu0+port0
		 * VLAN group 1: cpu0+port1
		 */
		writel((0xa << 8) + 0x9, comm->l2sw_reg_base + L2SW_VLAN_MEMSET_CONFIG0);

		/* RMC forward: to cpu
		 * LED: 60mS
		 * BC storm prev: 31 BC
		 */
		reg = readl(comm->l2sw_reg_base + L2SW_SW_GLB_CNTL);
		writel((reg & (~((0x3 << 25) | (0x3 << 23) | (0x3 << 4)))) |
		       (0x1 << 25) | (0x1 << 23) | (0x1 << 4),
		       comm->l2sw_reg_base + L2SW_SW_GLB_CNTL);
	} else {
		mac->cpu_port = 0x1;	// soc0
		mac->lan_port = 0x3;	// forward to port 0 and 1
		mac->to_vlan = 0x1;	// vlan group: 0
		mac->vlan_id = 0x0;	// vlan group: 0
		comm->enable = 0x3;	// enable lan 0 and 1

		/* Port 0: VLAN group 0
		 * port 1: VLAN group 0
		 */
		writel((0 << 4) + 0, comm->l2sw_reg_base + L2SW_PVID_CONFIG0);

		/* VLAN group 0: cpu0+port1+port0 */
		writel((0x0 << 8) + 0xb, comm->l2sw_reg_base + L2SW_VLAN_MEMSET_CONFIG0);

		/* RMC forward: broadcast
		 * LED: 60mS
		 * BC storm prev: 31 BC
		 */
		reg = readl(comm->l2sw_reg_base + L2SW_SW_GLB_CNTL);
		writel((reg & (~((0x3 << 25) | (0x3 << 23) | (0x3 << 4)))) |
		       (0x0 << 25) | (0x1 << 23) | (0x1 << 4),
		       comm->l2sw_reg_base + L2SW_SW_GLB_CNTL);
	}
}

void spl2sw_mac_disable_port_sa_learning(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Disable lan port SA learning. */
	reg = readl(comm->l2sw_reg_base + L2SW_PORT_CNTL1);
	writel(reg | (0x3 << 8), comm->l2sw_reg_base + L2SW_PORT_CNTL1);
}

void spl2sw_spl2sw_mac_enable_port_sa_learning(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	/* Disable lan port SA learning. */
	reg = readl(comm->l2sw_reg_base + L2SW_PORT_CNTL1);
	writel(reg & (~(0x3 << 8)), comm->l2sw_reg_base + L2SW_PORT_CNTL1);
}

void spl2sw_mac_rx_mode_set(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	struct net_device *ndev = mac->ndev;
	u32 mask, reg, rx_mode;

	pr_debug(" ndev->flags = %08x\n", ndev->flags);

	mask = (mac->lan_port << 2) | (mac->lan_port << 0);
	reg = readl(comm->l2sw_reg_base + L2SW_CPU_CNTL);

	if (ndev->flags & IFF_PROMISC) {	/* Set promiscuous mode */
		/* Allow MC and unknown UC packets */
		rx_mode = (mac->lan_port << 2) | (mac->lan_port << 0);
	} else if ((!netdev_mc_empty(ndev) && (ndev->flags & IFF_MULTICAST)) ||
		   (ndev->flags & IFF_ALLMULTI)) {
		/* Allow MC packets */
		rx_mode = (mac->lan_port << 2);
	} else {
		/* Disable MC and unknown UC packets */
		rx_mode = 0;
	}

	writel((reg & (~mask)) | ((~rx_mode) & mask), comm->l2sw_reg_base + L2SW_CPU_CNTL);
	pr_debug(" cpu_cntl = %08x\n", readl(comm->l2sw_reg_base + L2SW_CPU_CNTL));
}

void spl2sw_mac_enable_port(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	//phy address
	reg = readl(comm->l2sw_reg_base + L2SW_MAC_FORCE_MODE);
	reg = (reg & (~(0x1f << 16))) | ((mac->comm->phy1_addr & 0x1f) << 16);
	reg = (reg & (~(0x1f << 24))) | ((mac->comm->phy2_addr & 0x1f) << 24);
	writel(reg, comm->l2sw_reg_base + L2SW_MAC_FORCE_MODE);
}

bool spl2sw_mac_init(struct spl2sw_mac *mac)
{
	u32 i;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++)
		mac->comm->rx_pos[i] = 0;
	mb();	/* make sure settings are effective. */

	spl2sw_mac_hw_init(mac);

	return 1;
}

void spl2sw_mac_soft_reset(struct spl2sw_mac *mac)
{
	struct net_device *ndev2;
	u32 i;

	if (netif_carrier_ok(mac->ndev)) {
		netif_carrier_off(mac->ndev);
		netif_stop_queue(mac->ndev);
	}

	ndev2 = mac->next_ndev;
	if (ndev2) {
		if (netif_carrier_ok(ndev2)) {
			netif_carrier_off(ndev2);
			netif_stop_queue(ndev2);
		}
	}

	spl2sw_mac_hw_stop(mac);

	//spl2sw_descs_clean(mac->comm);
	spl2sw_rx_descs_flush(mac->comm);
	mac->comm->tx_pos = 0;
	mac->comm->tx_done_pos = 0;
	mac->comm->tx_desc_full = 0;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++)
		mac->comm->rx_pos[i] = 0;
	mb();	/* make sure settings are effective. */

	spl2sw_mac_hw_init(mac);
	spl2sw_mac_hw_start(mac);

	if (!netif_carrier_ok(mac->ndev)) {
		netif_carrier_on(mac->ndev);
		netif_start_queue(mac->ndev);
	}

	if (ndev2) {
		if (!netif_carrier_ok(ndev2)) {
			netif_carrier_on(ndev2);
			netif_start_queue(ndev2);
		}
	}
}

__attribute__((unused))
void spl2sw_mac_regs_print(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;

	pr_info(" sw_int_status_0     = %08x\n", readl(comm->l2sw_reg_base + L2SW_SW_INT_STATUS_0));
	pr_info(" sw_int_mask_0       = %08x\n", readl(comm->l2sw_reg_base + L2SW_SW_INT_MASK_0));
	pr_info(" fl_cntl_th          = %08x\n", readl(comm->l2sw_reg_base + L2SW_FL_CNTL_TH));
	pr_info(" cpu_fl_cntl_th      = %08x\n", readl(comm->l2sw_reg_base + L2SW_CPU_FL_CNTL_TH));
	pr_info(" pri_fl_cntl         = %08x\n", readl(comm->l2sw_reg_base + L2SW_PRI_FL_CNTL));
	pr_info(" vlan_pri_th         = %08x\n", readl(comm->l2sw_reg_base + L2SW_VLAN_PRI_TH));
	pr_info(" En_tos_bus          = %08x\n", readl(comm->l2sw_reg_base + L2SW_EN_TOS_BUS));
	pr_info(" Tos_map0            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP0));
	pr_info(" Tos_map1            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP1));
	pr_info(" Tos_map2            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP2));
	pr_info(" Tos_map3            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP3));
	pr_info(" Tos_map4            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP4));
	pr_info(" Tos_map5            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP5));
	pr_info(" Tos_map6            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP6));
	pr_info(" Tos_map7            = %08x\n", readl(comm->l2sw_reg_base + L2SW_TOS_MAP7));
	pr_info(" global_que_status   = %08x\n", readl(comm->l2sw_reg_base + L2SW_GLOBAL_QUE_STATUS));
	pr_info(" addr_tbl_srch       = %08x\n", readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_SRCH));
	pr_info(" addr_tbl_st         = %08x\n", readl(comm->l2sw_reg_base + L2SW_ADDR_TBL_ST));
	pr_info(" MAC_ad_ser0         = %08x\n", readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER0));
	pr_info(" MAC_ad_ser1         = %08x\n", readl(comm->l2sw_reg_base + L2SW_MAC_AD_SER1));
	pr_info(" wt_mac_ad0          = %08x\n", readl(comm->l2sw_reg_base + L2SW_WT_MAC_AD0));
	pr_info(" w_mac_15_0          = %08x\n", readl(comm->l2sw_reg_base + L2SW_W_MAC_15_0));
	pr_info(" w_mac_47_16         = %08x\n", readl(comm->l2sw_reg_base + L2SW_W_MAC_47_16));
	pr_info(" PVID_config0        = %08x\n", readl(comm->l2sw_reg_base + L2SW_PVID_CONFIG0));
	pr_info(" PVID_config1        = %08x\n", readl(comm->l2sw_reg_base + L2SW_PVID_CONFIG1));
	pr_info(" VLAN_memset_config0 = %08x\n", readl(comm->l2sw_reg_base + L2SW_VLAN_MEMSET_CONFIG0));
	pr_info(" VLAN_memset_config1 = %08x\n", readl(comm->l2sw_reg_base + L2SW_VLAN_MEMSET_CONFIG1));
	pr_info(" port_ability        = %08x\n", readl(comm->l2sw_reg_base + L2SW_PORT_ABILITY));
	pr_info(" port_st             = %08x\n", readl(comm->l2sw_reg_base + L2SW_PORT_ST));
	pr_info(" cpu_cntl            = %08x\n", readl(comm->l2sw_reg_base + L2SW_CPU_CNTL));
	pr_info(" port_cntl0          = %08x\n", readl(comm->l2sw_reg_base + L2SW_PORT_CNTL0));
	pr_info(" port_cntl1          = %08x\n", readl(comm->l2sw_reg_base + L2SW_PORT_CNTL1));
	pr_info(" port_cntl2          = %08x\n", readl(comm->l2sw_reg_base + L2SW_PORT_CNTL2));
	pr_info(" sw_glb_cntl         = %08x\n", readl(comm->l2sw_reg_base + L2SW_SW_GLB_CNTL));
	pr_info(" l2sw_sw_reset       = %08x\n", readl(comm->l2sw_reg_base + L2SW_L2SW_SW_RESET));
	pr_info(" led_port0           = %08x\n", readl(comm->l2sw_reg_base + L2SW_LED_PORT0));
	pr_info(" led_port1           = %08x\n", readl(comm->l2sw_reg_base + L2SW_LED_PORT1));
	pr_info(" led_port2           = %08x\n", readl(comm->l2sw_reg_base + L2SW_LED_PORT2));
	pr_info(" led_port3           = %08x\n", readl(comm->l2sw_reg_base + L2SW_LED_PORT3));
	pr_info(" led_port4           = %08x\n", readl(comm->l2sw_reg_base + L2SW_LED_PORT4));
	pr_info(" watch_dog_trig_rst  = %08x\n", readl(comm->l2sw_reg_base + L2SW_WATCH_DOG_TRIG_RST));
	pr_info(" watch_dog_stop_cpu  = %08x\n", readl(comm->l2sw_reg_base + L2SW_WATCH_DOG_STOP_CPU));
	pr_info(" phy_cntl_reg0       = %08x\n", readl(comm->l2sw_reg_base + L2SW_PHY_CNTL_REG0));
	pr_info(" phy_cntl_reg1       = %08x\n", readl(comm->l2sw_reg_base + L2SW_PHY_CNTL_REG1));
	pr_info(" mac_force_mode      = %08x\n", readl(comm->l2sw_reg_base + L2SW_MAC_FORCE_MODE));
	pr_info(" VLAN_group_config0  = %08x\n", readl(comm->l2sw_reg_base + L2SW_VLAN_GROUP_CONFIG0));
	pr_info(" VLAN_group_config1  = %08x\n", readl(comm->l2sw_reg_base + L2SW_VLAN_GROUP_CONFIG1));
	pr_info(" flow_ctrl_th3       = %08x\n", readl(comm->l2sw_reg_base + L2SW_FLOW_CTRL_TH3));
	pr_info(" queue_status_0      = %08x\n", readl(comm->l2sw_reg_base + L2SW_QUEUE_STATUS_0));
	pr_info(" debug_cntl          = %08x\n", readl(comm->l2sw_reg_base + L2SW_DEBUG_CNTL));
	pr_info(" queue_status_0      = %08x\n", readl(comm->l2sw_reg_base + L2SW_QUEUE_STATUS_0));
	pr_info(" debug_cntl          = %08x\n", readl(comm->l2sw_reg_base + L2SW_DEBUG_CNTL));
	pr_info(" l2sw_rsv2           = %08x\n", readl(comm->l2sw_reg_base + L2SW_RESERVED_1));
	pr_info(" mem_test_info       = %08x\n", readl(comm->l2sw_reg_base + L2SW_MEM_TEST_INFO));
	pr_info(" sw_int_status_1     = %08x\n", readl(comm->l2sw_reg_base + L2SW_SW_INT_STATUS_1));
	pr_info(" sw_int_mask_1       = %08x\n", readl(comm->l2sw_reg_base + L2SW_SW_INT_MASK_1));
	pr_info(" cpu_tx_trig         = %08x\n", readl(comm->l2sw_reg_base + L2SW_CPU_TX_TRIG));
	pr_info(" tx_hbase_addr_0     = %08x\n", readl(comm->l2sw_reg_base + L2SW_TX_HBASE_ADDR_0));
	pr_info(" tx_lbase_addr_0     = %08x\n", readl(comm->l2sw_reg_base + L2SW_TX_LBASE_ADDR_0));
	pr_info(" rx_hbase_addr_0     = %08x\n", readl(comm->l2sw_reg_base + L2SW_RX_HBASE_ADDR_0));
	pr_info(" rx_lbase_addr_0     = %08x\n", readl(comm->l2sw_reg_base + L2SW_RX_LBASE_ADDR_0));
	pr_info(" tx_hw_addr_0        = %08x\n", readl(comm->l2sw_reg_base + L2SW_TX_HW_ADDR_0));
	pr_info(" tx_lw_addr_0        = %08x\n", readl(comm->l2sw_reg_base + L2SW_TX_LW_ADDR_0));
	pr_info(" rx_hw_addr_0        = %08x\n", readl(comm->l2sw_reg_base + L2SW_RX_HW_ADDR_0));
	pr_info(" rx_lw_addr_0        = %08x\n", readl(comm->l2sw_reg_base + L2SW_RX_LW_ADDR_0));
	pr_info(" cpu_port_cntl_reg_0 = %08x\n", readl(comm->l2sw_reg_base + L2SW_CPU_PORT_CNTL_REG_0));
}
