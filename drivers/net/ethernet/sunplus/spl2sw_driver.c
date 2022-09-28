// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include <linux/nvmem-consumer.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include "spl2sw_register.h"
#include "spl2sw_driver.h"
#include "spl2sw_phy.h"

/* OUI of Sunplus Technology Co., Ltd. */
static const char def_mac_addr[ETHERNET_MAC_ADDR_LEN] = {
	0xfc, 0x4b, 0xbc, 0x00, 0x00, 0x00
};

__attribute__((unused))
void print_packet(struct sk_buff *skb)
{
	u8 *p = skb->data;
	int len = skb->len;
	char buf[120], *packet_t;
	u32 LenType;
	int i;

	i = snprintf(buf, sizeof(buf), " MAC: DA=%pM, SA=%pM, ", &p[0], &p[6]);
	p += 12;		// point to LenType

	LenType = (((u32) p[0]) << 8) + p[1];
	if (LenType == 0x8100) {
		u32 tag = (((u32) p[2]) << 8) + p[3];
		u32 type = (((u32) p[4]) << 8) + p[5];

		snprintf(buf + i, sizeof(buf) - i,
			 "TPID=%04x, Tag=%04x, LenType=%04x, len=%d (VLAN tagged packet)",
			 LenType, tag, type, len);
		LenType = type;
		p += 4;		// point to LenType
	} else if (LenType > 1500) {
		switch (LenType) {
		case 0x0800:
			packet_t = "IPv4";
			break;
		case 0x0806:
			packet_t = "ARP";
			break;
		case 0x8035:
			packet_t = "RARP";
			break;
		case 0x86DD:
			packet_t = "IPv6";
			break;
		default:
			packet_t = "unknown";
		}

		snprintf(buf + i, sizeof(buf) - i, "Type=%04x, len=%d (%s packet)",
			 LenType, (int)len, packet_t);
	} else {
		snprintf(buf + i, sizeof(buf) - i, "Len=%04x, len=%d (802.3 packet)",
			 LenType, (int)len);
	}
	pr_info("%s\n", buf);
}

/* net_device_ops */
static int spl2sw_ethernet_open(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);

	pr_debug(" Open port = %x\n", mac->lan_port);

	/* Start phy */
	//netif_carrier_off(ndev);
	//spl2sw_phy_start(ndev);

	mac->comm->enable |= mac->lan_port;

	spl2sw_mac_hw_start(mac);

	netif_carrier_on(ndev);

	if (netif_carrier_ok(ndev)) {
		pr_debug(" Open netif_start_queue.\n");
		netif_start_queue(ndev);
	}

	return 0;
}

static int spl2sw_ethernet_stop(struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);
	unsigned long flags;

	//pr_info("[%s] IN\n", __func__);

	spin_lock_irqsave(&mac->comm->lock, flags);
	netif_stop_queue(ndev);
	netif_carrier_off(ndev);

	mac->comm->enable &= ~mac->lan_port;

	spin_unlock_irqrestore(&mac->comm->lock, flags);

	//spl2sw_phy_stop(ndev);
	spl2sw_mac_hw_stop(mac);

	return 0;
}

/* Transmit a packet (called by the kernel) */
static int spl2sw_ethernet_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct spl2sw_mac *mac = netdev_priv(ndev);
	struct spl2sw_common *comm = mac->comm;
	struct spl2sw_skb_info *skbinfo;
	struct spl2sw_mac_desc *txdesc;
	unsigned long flags;
	u32 tx_pos;
	u32 cmd1;
	u32 cmd2;

	//pr_info("[%s] IN\n", __func__);
	//print_packet(skb);

	if (unlikely(comm->tx_desc_full == 1)) {
		/* No TX descriptors left. Wait for tx interrupt. */
		pr_err(" TX descriptor queue full when xmit!\n");
		return NETDEV_TX_BUSY;
	}
	//pr_info(" skb->len = %d\n", skb->len);

	/* if skb size shorter than 60, fill it with '\0' */
	if (unlikely(skb->len < ETH_ZLEN)) {
		if (skb_tailroom(skb) >= (ETH_ZLEN - skb->len)) {
			memset(__skb_put(skb, ETH_ZLEN - skb->len), '\0',
			       ETH_ZLEN - skb->len);
		} else {
			struct sk_buff *old_skb = skb;

			skb = dev_alloc_skb(ETH_ZLEN + TX_OFFSET);
			if (skb) {
				memset(skb->data + old_skb->len, '\0',
				       ETH_ZLEN - old_skb->len);
				memcpy(skb->data, old_skb->data, old_skb->len);
				skb_put(skb, ETH_ZLEN);	/* add data to an sk_buff */
				dev_kfree_skb_irq(old_skb);
			} else {
				skb = old_skb;
			}
		}
	}

	spin_lock_irqsave(&comm->lock, flags);
	tx_pos = comm->tx_pos;
	txdesc = &comm->tx_desc[tx_pos];
	skbinfo = &comm->tx_temp_skb_info[tx_pos];
	skbinfo->len = skb->len;
	skbinfo->skb = skb;
	skbinfo->mapping = dma_map_single(&comm->pdev->dev, skb->data,
					  skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&comm->pdev->dev, skbinfo->mapping)) {
		ndev->stats.tx_errors++;
		skbinfo->mapping = 0;
		dev_kfree_skb_irq(skb);
		skbinfo->skb = NULL;
		goto xmit_drop;
	}

	cmd1 = (OWN_BIT | FS_BIT | LS_BIT | (mac->to_vlan << 12) | (skb->len & LEN_MASK));
	cmd2 = skb->len & LEN_MASK;

	if (tx_pos == (TX_DESC_NUM - 1))
		cmd2 |= EOR_BIT;
	//pr_info(" TX1: cmd1 = %08x, cmd2 = %08x\n", cmd1, cmd2);

	txdesc->addr1 = skbinfo->mapping;
	txdesc->cmd2 = cmd2;
	wmb();	/* Set OWN_BIT after other fields of descriptor are effective. */
	txdesc->cmd1 = cmd1;

	NEXT_TX(tx_pos);

	if (unlikely(tx_pos == comm->tx_done_pos)) {
		netif_stop_queue(ndev);
		comm->tx_desc_full = 1;
		//pr_info(" TX Descriptor Queue Full!\n");
	}
	comm->tx_pos = tx_pos;
	wmb();	/* make sure settings are effective. */

	/* trigger gmac to transmit */
	writel((0x1 << 1), comm->l2sw_reg_base + L2SW_CPU_TX_TRIG);

xmit_drop:
	spin_unlock_irqrestore(&comm->lock, flags);
	return NETDEV_TX_OK;
}

static void spl2sw_ethernet_set_rx_mode(struct net_device *ndev)
{
	if (ndev) {
		struct spl2sw_mac *mac = netdev_priv(ndev);
		struct spl2sw_common *comm = mac->comm;
		unsigned long flags;

		spin_lock_irqsave(&comm->ioctl_lock, flags);
		spl2sw_mac_rx_mode_set(mac);
		spin_unlock_irqrestore(&comm->ioctl_lock, flags);
	}
}

static int spl2sw_ethernet_set_mac_address(struct net_device *ndev, void *addr)
{
	struct sockaddr *hwaddr = (struct sockaddr *)addr;
	struct spl2sw_mac *mac = netdev_priv(ndev);

	//pr_info("[%s] IN\n", __func__);

	if (netif_running(ndev)) {
		pr_err(" Device %s is busy!\n", ndev->name);
		return -EBUSY;
	}

	memcpy(ndev->dev_addr, hwaddr->sa_data, ndev->addr_len);

	/* Delete the old Ethernet MAC address */
	pr_debug(" HW Addr = %pM\n", mac->mac_addr);
	if (is_valid_ether_addr(mac->mac_addr))
		spl2sw_mac_addr_del(mac);

	/* Set the Ethernet MAC address */
	memcpy(mac->mac_addr, hwaddr->sa_data, ndev->addr_len);
	spl2sw_mac_addr_set(mac);

	return 0;
}

static int spl2sw_ethernet_do_ioctl(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	struct mii_ioctl_data *data = if_mii(ifr);
	struct spl2sw_mac *mac = netdev_priv(ndev);
	struct spl2sw_common *comm = mac->comm;

	pr_debug(" if = %s, cmd = %04x\n", ifr->ifr_ifrn.ifrn_name, cmd);
	pr_debug(" phy_id = %d, reg_num = %d, val_in = %04x\n", data->phy_id,
		 data->reg_num, data->val_in);

	switch (cmd) {
	case SIOCGMIIPHY:
		if ((comm->dual_nic) && (strcmp(ifr->ifr_ifrn.ifrn_name, "eth1") == 0))
			data->phy_id = comm->phy2_addr;
		else
			data->phy_id = comm->phy1_addr;
		return 0;

	case SIOCGMIIREG:
		data->val_out = spl2sw_mdio_read(comm, data->phy_id, data->reg_num);
		pr_debug(" val_out = %04x\n", data->val_out);
		break;

	case SIOCSMIIREG:
		spl2sw_mdio_write(comm, data->phy_id, data->reg_num, data->val_in);
		break;

	default:
		pr_err(" ioctl #%d has not implemented yet!\n", cmd);
		return -EOPNOTSUPP;
	}
	return 0;
}

static void spl2sw_ethernet_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
}

static struct net_device_stats *spl2sw_ethernet_get_stats(struct net_device *ndev)
{
	struct spl2sw_mac *mac;

	//pr_info("[%s] IN\n", __func__);
	mac = netdev_priv(ndev);
	return &mac->dev_stats;
}

static const struct net_device_ops netdev_ops = {
	.ndo_open = spl2sw_ethernet_open,
	.ndo_stop = spl2sw_ethernet_stop,
	.ndo_start_xmit = spl2sw_ethernet_start_xmit,
	.ndo_set_rx_mode = spl2sw_ethernet_set_rx_mode,
	.ndo_set_mac_address = spl2sw_ethernet_set_mac_address,
	.ndo_do_ioctl = spl2sw_ethernet_do_ioctl,
	.ndo_tx_timeout = spl2sw_ethernet_tx_timeout,
	.ndo_get_stats = spl2sw_ethernet_get_stats,
};

static char *spl2sw_otp_read_mac(struct device *dev, ssize_t *len, char *name)
{
	struct nvmem_cell *cell = nvmem_cell_get(dev, name);
	char *ret;

	if (IS_ERR_OR_NULL(cell)) {
		pr_err(" OTP %s read failure: %ld", name, PTR_ERR(cell));
		return NULL;
	}

	ret = nvmem_cell_read(cell, len);
	nvmem_cell_put(cell);
	pr_debug(" %zd bytes are read from OTP %s.", *len, name);

	return ret;
}

static void spl2sw_check_mac_vendor_id_and_convert(char *mac_addr)
{
	/* Byte order of MAC address of some samples are reversed.
	 * Check vendor id and convert byte order if it is wrong.
	 */
	if ((mac_addr[5] == 0xFC) && (mac_addr[4] == 0x4B) && (mac_addr[3] == 0xBC) &&
	    ((mac_addr[0] != 0xFC) || (mac_addr[1] != 0x4B) || (mac_addr[2] != 0xBC))) {
		swap(mac_addr[0], mac_addr[5]);
		swap(mac_addr[1], mac_addr[4]);
		swap(mac_addr[2], mac_addr[3]);
	}
}

/*********************************************************************
 *
 * platform_driver
 *
 **********************************************************************/
static u32 spl2sw_init_netdev(struct platform_device *pdev, int eth_no, struct net_device **r_ndev)
{
	u32 ret = -ENODEV;
	struct spl2sw_mac *mac;
	struct net_device *ndev;
	char *m_addr_name = (eth_no == 0) ? "mac_addr0" : "mac_addr1";
	ssize_t otp_l = 0;
	char *otp_v;

	//pr_info("[%s] IN\n", __func__);

	/* Allocate the devices, and also allocate spl2sw_mac,
	 * we can get it by netdev_priv().
	 */
	ndev = alloc_etherdev(sizeof(struct spl2sw_mac));
	if (!ndev) {
		*r_ndev = NULL;
		return -ENOMEM;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->netdev_ops = &netdev_ops;

	mac = netdev_priv(ndev);
	mac->ndev = ndev;
	mac->pdev = pdev;
	mac->next_ndev = NULL;

	/* Get property 'mac-addr0' or 'mac-addr1' from dts. */
	otp_v = spl2sw_otp_read_mac(&pdev->dev, &otp_l, m_addr_name);
	if ((otp_l < 6) || IS_ERR_OR_NULL(otp_v)) {
		pr_info(" OTP mac %s (len = %zd) is invalid, using default!\n",
			m_addr_name, otp_l);
		otp_l = 0;
	} else {
		/* Check if mac-address is valid or not. If not, copy from default. */
		memcpy(mac->mac_addr, otp_v, 6);

		/* Order of MAC address stored in OTP are reverse.
		 * Convert them to correct order.
		 */
		swap(mac->mac_addr[0], mac->mac_addr[5]);
		swap(mac->mac_addr[1], mac->mac_addr[4]);
		swap(mac->mac_addr[2], mac->mac_addr[3]);

		/* Byte order of Some samples are reversed. Convert byte order here. */
		spl2sw_check_mac_vendor_id_and_convert(mac->mac_addr);

		if (!is_valid_ether_addr(mac->mac_addr)) {
			pr_info(" Invalid mac in OTP[%s] = %pM, use default!\n",
				m_addr_name, mac->mac_addr);
			otp_l = 0;
		}
	}

	if (!IS_ERR_OR_NULL(otp_v))
		kfree(otp_v);

	if (otp_l != 6) {
		memcpy(mac->mac_addr, def_mac_addr, ETHERNET_MAC_ADDR_LEN);
		mac->mac_addr[3] = get_random_int() % 256;
		mac->mac_addr[4] = get_random_int() % 256;
		mac->mac_addr[5] = get_random_int() % 256;
	}

	pr_info(" HW Addr = %pM\n", mac->mac_addr);

	memcpy(ndev->dev_addr, mac->mac_addr, ETHERNET_MAC_ADDR_LEN);

	ret = register_netdev(ndev);
	if (ret) {
		pr_err(" Failed to register net device \"%s\" (ret = %d)!\n", ndev->name, ret);
		free_netdev(ndev);
		*r_ndev = NULL;
		return ret;
	}
	pr_info(" Registered net device \"%s\" successfully.\n", ndev->name);

	*r_ndev = ndev;
	return 0;
}

#ifdef CONFIG_DYNAMIC_MODE_SWITCHING_BY_SYSFS
static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct spl2sw_mac *mac = netdev_priv(ndev);

	return sprintf(buf, "%d\n", (mac->comm->dual_nic) ? 1 : (mac->comm->sa_learning) ? 0 : 2);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct spl2sw_mac *mac = netdev_priv(ndev);
	struct spl2sw_common *comm = mac->comm;
	struct net_device *ndev2;
	struct spl2sw_mac *mac2;

	if (buf[0] == '1') {
		// Switch to dual NIC mode.
		mutex_lock(&comm->store_mode);
		if (!comm->dual_nic) {
			spl2sw_mac_hw_stop(mac);

			comm->dual_nic = 1;
			comm->sa_learning = 0;
			//spl2sw_mac_addr_print(mac);
			spl2sw_mac_disable_port_sa_learning(mac);
			spl2sw_mac_addr_table_del_all(mac);
			spl2sw_mac_switch_mode(mac);
			spl2sw_mac_rx_mode_set(mac);
			//spl2sw_mac_addr_print(mac);

			spl2sw_init_netdev(mac->pdev, 1, &ndev2);	// Initialize the 2nd net device (eth1)
			if (ndev2) {
				mac->next_ndev = ndev2;		// Pointed by previous net device.
				mac2 = netdev_priv(ndev2);
				mac2->comm = comm;
				ndev2->irq = comm->irq;

				spl2sw_mac_switch_mode(mac);
				spl2sw_mac_rx_mode_set(mac2);
				spl2sw_mac_addr_set(mac2);
				//spl2sw_mac_addr_print();
			}

			comm->enable &= 0x1;	// Keep lan 0, but always turn off lan 1.
			spl2sw_mac_hw_start(mac);
		}
		mutex_unlock(&comm->store_mode);
	} else if ((buf[0] == '0') || (buf[0] == '2')) {
		// Switch to one NIC with daisy-chain mode.
		mutex_lock(&comm->store_mode);

		if (buf[0] == '2') {
			if (comm->sa_learning == 1) {
				comm->sa_learning = 0;
				//spl2sw_mac_addr_print(mac);
				spl2sw_mac_disable_port_sa_learning(mac);
				spl2sw_mac_addr_table_del_all(mac);
				//spl2sw_mac_addr_print(mac);
			}
		} else {
			if (comm->sa_learning == 0) {
				comm->sa_learning = 1;
				spl2sw_spl2sw_mac_enable_port_sa_learning(mac);
				//spl2sw_mac_addr_print(mac);
			}
		}

		if (comm->dual_nic) {
			struct net_device *ndev2 = mac->next_ndev;

			if (!netif_running(ndev2)) {
				spl2sw_mac_hw_stop(mac);

				mac2 = netdev_priv(ndev2);
				spl2sw_mac_addr_del(mac2);
				//spl2sw_mac_addr_print(mac);

				// unregister and free net device.
				unregister_netdev(ndev2);
				free_netdev(ndev2);
				mac->next_ndev = NULL;
				pr_info(" Unregistered and freed net device \"eth1\"!\n");

				comm->dual_nic = 0;
				spl2sw_mac_switch_mode(mac);
				spl2sw_mac_rx_mode_set(mac);

				// If eth0 is up, turn on lan 0 and 1 when switching to daisy-chain mode.
				if (comm->enable & 0x1)
					comm->enable = 0x3;
				else
					comm->enable = 0;

				spl2sw_mac_hw_start(mac);
			} else {
				pr_err(" Error: Net device \"%s\" is running!\n", ndev2->name);
			}
		}
		mutex_unlock(&comm->store_mode);
	} else {
		pr_err(" Error: Unknown mode \"%c\"!\n", buf[0]);
	}

	return count;
}

static DEVICE_ATTR_RW(mode);
static struct attribute *spl2sw_sysfs_entries[] = {
	&dev_attr_mode.attr,
	NULL,
};

static struct attribute_group l2sw_attribute_group = {
	.attrs = spl2sw_sysfs_entries,
};
#endif

static int spl2sw_soc0_open(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	u32 rc;

	//pr_info("[%s] IN\n", __func__);

	spl2sw_mac_hw_stop(mac);

#ifndef INTERRUPT_IMMEDIATELY
	//tasklet_enable(&comm->rx_tasklet);
	//tasklet_enable(&comm->tx_tasklet);
#endif

#ifdef RX_POLLING
	napi_enable(&comm->napi);
#endif

	rc = spl2sw_descs_init(comm);
	if (rc) {
		pr_err(" Fail to initialize mac descriptors!\n");
		goto INIT_DESC_FAIL;
	}

	/*start hardware port, open interrupt, start system tx queue */
	spl2sw_mac_init(mac);
	return 0;

INIT_DESC_FAIL:
	spl2sw_descs_free(comm);
	return rc;
}

static int spl2sw_soc0_stop(struct spl2sw_mac *mac)
{
#ifdef RX_POLLING
	napi_disable(&mac->comm->napi);
#endif

	spl2sw_mac_hw_stop(mac);

	spl2sw_descs_free(mac->comm);
	return 0;
}

static int spl2sw_probe(struct platform_device *pdev)
{
	struct spl2sw_common *comm;
	struct resource *r_mem;
	struct net_device *ndev, *ndev2;
	struct spl2sw_mac *mac, *mac2;
	u32 mode;
	int ret = 0;
	int rc;

	//pr_info("[%s] IN\n", __func__);

	if (platform_get_drvdata(pdev) != NULL)
		return -ENODEV;

	/* Allocate memory for 'spl2sw_common' area. */
	comm = devm_kzalloc(&pdev->dev, sizeof(*comm), GFP_KERNEL);
	if (!comm)
		return -ENOMEM;
	comm->pdev = pdev;

	/*
	 * spin_lock:         return if it obtain spin lock, or it will wait (not sleep)
	 * spin_lock_irqsave: save flags, disable interrupt, obtain spin lock
	 * spin_lock_irq:     disable interrupt, obtain spin lock, if in a interrupt, don't need to use spin_lock_irqsave
	 */
	spin_lock_init(&comm->lock);
	spin_lock_init(&comm->ioctl_lock);
	mutex_init(&comm->store_mode);

	// Get memory resoruce 0 from dts.
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		pr_err(" No MEM resource 0 found!\n");
		return -ENXIO;
	}
	pr_debug(" res->name = \"%s\", r_mem->start = %pa\n", r_mem->name, &r_mem->start);

	comm->l2sw_reg_base = devm_ioremap(&pdev->dev, r_mem->start,
					   (r_mem->end - r_mem->start + 1));
	if (!comm->l2sw_reg_base) {
		pr_err(" ioremap failed!\n");
		return -ENOMEM;
	}

	// Get irq resource from dts.
	if (spl2sw_get_irq(pdev, comm) != 0)
		return -ENXIO;

	// Get L2-switch mode.
	ret = of_property_read_u32(pdev->dev.of_node, "mode", &mode);
	if (ret)
		mode = 0;
	pr_info(" L2 switch mode = %u\n", mode);
	if (mode == 2) {
		comm->dual_nic = 0;	// daisy-chain mode 2
		comm->sa_learning = 0;
	} else if (mode == 1) {
		comm->dual_nic = 1;	// dual NIC mode
		comm->sa_learning = 0;
	} else {
		comm->dual_nic = 0;	// daisy-chain mode
		comm->sa_learning = 1;
	}

	// Get resource of clock controller
	comm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(comm->clk)) {
		dev_err(&pdev->dev, "Failed to retrieve clock controller!\n");
		return PTR_ERR(comm->clk);
	}

	comm->rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(comm->rstc)) {
		dev_err(&pdev->dev, "Failed to retrieve reset controller!\n");
		return PTR_ERR(comm->rstc);
	}

	// Enable clock.
	clk_prepare_enable(comm->clk);
	udelay(1);

	ret = reset_control_assert(comm->rstc);
	udelay(1);
	ret = reset_control_deassert(comm->rstc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to deassert reset line (err = %d)!\n", ret);
		return -ENODEV;
	}
	udelay(1);

	ret = spl2sw_init_netdev(pdev, 0, &ndev);	// Initialize the 1st net device (eth0)
	if (ndev == NULL)
		return ret;

	platform_set_drvdata(pdev, ndev);	// Pointed by drvdata net device.

	ndev->irq = comm->irq;

	mac = netdev_priv(ndev);
	mac->comm = comm;
	comm->ndev = ndev;
	pr_debug(" ndev = %p, mac = %p, comm = %p\n", ndev, mac, mac->comm);

	comm->phy1_node = of_parse_phandle(pdev->dev.of_node, "phy-handle1", 0);
	comm->phy2_node = of_parse_phandle(pdev->dev.of_node, "phy-handle2", 0);

	// Get address of phy of ethernet from dts.
	if (of_property_read_u32(comm->phy1_node, "reg", &rc) == 0) {
		comm->phy1_addr = rc;
	} else {
		comm->phy1_addr = 0;
		pr_info(" Cannot get address of phy of ethernet 1! Set to 0 by default.\n");
	}

	if (of_property_read_u32(comm->phy2_node, "reg", &rc) == 0) {
		comm->phy2_addr = rc;
	} else {
		comm->phy2_addr = 1;
		pr_info(" Cannot get address of phy of ethernet 2! Set to 1 by default.\n");
	}

	spl2sw_mac_enable_port(mac);

#ifndef ZEBU_XTOR
	if (comm->phy1_node) {
		comm->mdio_node = of_get_parent(mac->comm->phy1_node);
		if (!comm->mdio_node) {
			pr_err(" Failed to get mdio node!\n");
			goto out_unregister_dev;
		}

		ret = spl2sw_mdio_init(comm);
		if (ret) {
			pr_err(" Failed to initialize mdio!\n");
			goto out_unregister_dev;
		}

		ret = spl2sw_phy_probe(ndev);
		if (ret) {
			pr_err(" Failed to probe phy!\n");
			goto out_freemdio;
		}
	} else {
		pr_err(" Failed to get phy-handle!\n");
	}

	spl2sw_phy_cfg(mac);
#endif

#ifdef RX_POLLING
	netif_napi_add(ndev, &comm->napi, rx_poll, RX_NAPI_WEIGHT);
#endif

	// Register irq to system.
	if (spl2sw_request_irq(pdev, comm, ndev) != 0) {
		ret = -ENODEV;
		goto out_freemdio;
	}

#ifndef INTERRUPT_IMMEDIATELY
	tasklet_init(&comm->rx_tasklet, rx_do_tasklet, (unsigned long)mac);
	//tasklet_disable(&comm->rx_tasklet);
	tasklet_init(&comm->tx_tasklet, tx_do_tasklet, (unsigned long)mac);
	//tasklet_disable(&comm->tx_tasklet);
#endif

	spl2sw_soc0_open(mac);

	// Set MAC address
	spl2sw_mac_addr_set(mac);
	//spl2sw_mac_addr_print(mac);

#ifdef CONFIG_DYNAMIC_MODE_SWITCHING_BY_SYSFS
	/* Add the device attributes */
	rc = sysfs_create_group(&pdev->dev.kobj, &l2sw_attribute_group);
	if (rc)
		dev_err(&pdev->dev, "Error creating sysfs files!\n");
#endif

	spl2sw_mac_addr_table_del_all(mac);
	if (comm->sa_learning)
		spl2sw_spl2sw_mac_enable_port_sa_learning(mac);
	else
		spl2sw_mac_disable_port_sa_learning(mac);
	spl2sw_mac_rx_mode_set(mac);
	//spl2sw_mac_addr_print(mac);

	if (comm->dual_nic) {
		spl2sw_init_netdev(pdev, 1, &ndev2);
		if (ndev2 == NULL)
			goto fail_to_init_2nd_port;
		mac->next_ndev = ndev2;	// Pointed by previous net device.

		ndev2->irq = comm->irq;
		mac2 = netdev_priv(ndev2);
		mac2->comm = comm;
		pr_debug(" ndev = %p, mac = %p, comm = %p\n", ndev2, mac2, mac2->comm);

		spl2sw_mac_switch_mode(mac);
		spl2sw_mac_rx_mode_set(mac2);
		spl2sw_mac_addr_set(mac2);	// Set MAC address for the second net device.
		//spl2sw_mac_addr_print(mac);
	}

fail_to_init_2nd_port:
	return 0;

out_freemdio:
	spl2sw_mdio_remove(comm);

#ifndef ZEBU_XTOR
out_unregister_dev:
#endif
	unregister_netdev(ndev);
	return ret;
}

static int spl2sw_remove(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct net_device *ndev2;
	struct spl2sw_mac *mac;

	//pr_info("[%s] IN\n", __func__);

	ndev = platform_get_drvdata(pdev);
	if (ndev == NULL)
		return 0;
	mac = netdev_priv(ndev);

	// Unregister and free 2nd net device.
	ndev2 = mac->next_ndev;
	if (ndev2) {
		unregister_netdev(ndev2);
		free_netdev(ndev2);
	}

#ifdef CONFIG_DYNAMIC_MODE_SWITCHING_BY_SYSFS
	sysfs_remove_group(&pdev->dev.kobj, &l2sw_attribute_group);
#endif

#ifndef INTERRUPT_IMMEDIATELY
	tasklet_kill(&mac->comm->rx_tasklet);
	tasklet_kill(&mac->comm->tx_tasklet);
#endif

	mac->comm->enable = 0;
	spl2sw_soc0_stop(mac);

	//spl2sw_phy_remove(ndev);
	spl2sw_mdio_remove(mac->comm);

	// Unregister and free 1st net device.
	unregister_netdev(ndev);
	free_netdev(ndev);

	clk_disable(mac->comm->clk);

	return 0;
}

#ifdef CONFIG_PM
static int spl2sw_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	netif_device_detach(ndev);
	return 0;
}

static int spl2sw_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	netif_device_attach(ndev);
	return 0;
}

static const struct dev_pm_ops spl2sw_pm_ops = {
	.suspend = spl2sw_suspend,
	.resume = spl2sw_resume,
};
#endif

static const struct of_device_id spl2sw_of_match[] = {
	{.compatible = "sunplus,sp7021-l2sw"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, spl2sw_of_match);

static struct platform_driver spl2sw_driver = {
	.probe = spl2sw_probe,
	.remove = spl2sw_remove,
	.driver = {
		   .name = "spl2sw",
		   .owner = THIS_MODULE,
		   .of_match_table = spl2sw_of_match,
#ifdef CONFIG_PM
		   .pm = &spl2sw_pm_ops,	// not sure
#endif
		   },
};

module_platform_driver(spl2sw_driver);

MODULE_AUTHOR("Wells Lu <wells.lu@sunplus.com>");
MODULE_DESCRIPTION("Sunplus 10M/100M Ethernet (with L2 switch) driver");
MODULE_LICENSE("GPL v2");
