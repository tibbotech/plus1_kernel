#include "l2sw_driver.h"
#include <linux/clk.h>
#include <linux/reset.h>


extern int read_otp_data(int addr, char *value);
static const char def_mac_addr[ETHERNET_MAC_ADDR_LEN] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x80};


/*********************************************************************
*
* net_device_ops
*
**********************************************************************/
#if 0
static void print_packet(struct sk_buff *skb)
{
	u8 *p = skb->data;

	printk("MAC: DA=%02x:%02x:%02x:%02x:%02x:%02x, "
		    "SA=%02x:%02x:%02x:%02x:%02x:%02x, "
		    "Len/Type=%04x, len=%d\n",
		(u32)p[0],(u32)p[1],(u32)p[2],(u32)p[3],(u32)p[4],(u32)p[5],
		(u32)p[6],(u32)p[7],(u32)p[8],(u32)p[9],(u32)p[10],(u32)p[11],
		(u32)((((u32)p[12])<<8)+p[13]),
		(int)skb->len);
}
#endif

static inline void rx_skb(struct l2sw_mac *mac, struct sk_buff *skb)
{
	mac->dev_stats.rx_packets++;
	mac->dev_stats.rx_bytes += skb->len;

	netif_rx(skb);
}

static inline void interrupt_finish(struct l2sw_mac *mac)
{
	u32 reg;
	struct net_device *net_dev = (struct net_device *)mac->net_dev;

	u32 status = (u32)mac->mac_comm->int_status;

	if (status & MAC_INT_PORT_ST_CHG) { /* link status changed*/
		reg = read_port_ability();
#ifdef CONFIG_AN_NIC_WITH_DAISY_CHAIN
		if (!netif_carrier_ok(net_dev) && (reg & (PORT_ABILITY_LINK_ST_P1|PORT_ABILITY_LINK_ST_P0))) {
			netif_carrier_on(net_dev);
			netif_start_queue(net_dev);
		}
		else if (netif_carrier_ok(net_dev) && !(reg & (PORT_ABILITY_LINK_ST_P1|PORT_ABILITY_LINK_ST_P0))) {
			netif_carrier_off(net_dev);
			netif_stop_queue(net_dev);
		}
#endif
#ifdef CONFIG_DUAL_NIC
		if (!netif_carrier_ok(net_dev) && (reg & PORT_ABILITY_LINK_ST_P0)) {
			netif_carrier_on(net_dev);
			netif_start_queue(net_dev);
		}
		else if (netif_carrier_ok(net_dev) && !(reg & PORT_ABILITY_LINK_ST_P0)) {
			netif_carrier_off(net_dev);
			netif_stop_queue(net_dev);
		}

		if (mac->next_netdev) {
			struct net_device *ndev2 = mac->next_netdev;

			if (!netif_carrier_ok(ndev2) && (reg & PORT_ABILITY_LINK_ST_P1)) {
				netif_carrier_on(ndev2);
				netif_start_queue(ndev2);
			}
			else if (netif_carrier_ok(ndev2) && !(reg & PORT_ABILITY_LINK_ST_P1)) {
				netif_carrier_off(ndev2);
				netif_stop_queue(ndev2);
			}
		}
#endif
	}
}


static inline void  rx_interrupt(struct l2sw_mac *mac, u32 irq_status)
{
	struct sk_buff *skb, *new_skb;
	struct skb_info *sinfo;
	volatile struct mac_desc *desc;
	u32 rx_pos, pkg_len;
	u32 cmd;
	u32 num, rx_count;
	s32 queue;
	struct l2sw_mac_common *mac_comm = mac->mac_comm;

	// Process high-priority queue and then low-priority queue.
	for (queue = RX_DESC_QUEUE_NUM - 1; queue >= 0; queue--) {
		rx_pos = mac_comm->rx_pos[queue];
		rx_count =  mac_comm->rx_desc_num[queue];
		//ETH_INFO(" rx_pos = %d, rx_count = %d\n", rx_pos, rx_count);

		for (num = 0; num < rx_count; num++) {
			sinfo = mac_comm->rx_skb_info[queue] + rx_pos;
			desc = mac_comm->rx_desc[queue] + rx_pos;
			cmd = desc->cmd1;
			//ETH_INFO(" RX: cmd1 = %08x, cmd2 = %08x\n", cmd, desc->cmd2);

			if (cmd & OWN_BIT) {
				//ETH_INFO(" RX: is owned by NIC, rx_pos=%d, desc=%p", rx_pos, desc);
				break;
			}

			if (cmd & ERR_CODE) {
#ifdef CONFIG_DUAL_NIC
				if ((cmd & PKTSP_MASK) == PKTSP_PORT1) {
					if (mac->next_netdev) {
						struct l2sw_mac *mac2 = netdev_priv(mac->next_netdev);

						mac2->dev_stats.rx_length_errors++;
						mac2->dev_stats.rx_dropped++;
					}
				} else {
					mac->dev_stats.rx_length_errors++;
					mac->dev_stats.rx_dropped++;
				}
#else
				mac->dev_stats.rx_length_errors++;
				mac->dev_stats.rx_dropped++;
#endif
				goto NEXT;
			}

			if (cmd & RX_IP_CHKSUM_BIT) {
				ETH_ERR(" RX IP Checksum error!\n");
#ifdef CONFIG_DUAL_NIC
				if ((cmd & PKTSP_MASK) == PKTSP_PORT1) {
					if (mac->next_netdev) {
						struct l2sw_mac *mac2 = netdev_priv(mac->next_netdev);

						mac2->dev_stats.rx_crc_errors++;
					}
				} else {
					mac->dev_stats.rx_crc_errors++;
				}
#else
				mac->dev_stats.rx_crc_errors++;
#endif
				goto NEXT;
			}

			if (cmd & RX_TCP_UDP_CHKSUM_BIT) {
				goto NEXT;
			}

			pkg_len = cmd & LEN_MASK;

			if (pkg_len < 64) {
				goto NEXT;
			}

			/* allocate an skbuff for receiving, and it's an inline function */
			new_skb = __dev_alloc_skb(mac_comm->rx_desc_buff_size + RX_OFFSET, GFP_ATOMIC | GFP_DMA);
			if (new_skb == NULL) {
				mac->dev_stats.rx_dropped++;
				goto NEXT;
			}
			new_skb->dev = mac->net_dev;

			dma_unmap_single(&mac->pdev->dev, sinfo->mapping, mac_comm->rx_desc_buff_size, DMA_FROM_DEVICE);

			skb = sinfo->skb;
			skb->ip_summed = CHECKSUM_NONE;

			/*skb_put will judge if tail exceeds end, but __skb_put won't*/
			__skb_put(skb, (pkg_len - 4 > mac_comm->rx_desc_buff_size) ? mac_comm->rx_desc_buff_size : pkg_len - 4);

			sinfo->mapping = dma_map_single(&mac->pdev->dev, new_skb->data, mac_comm->rx_desc_buff_size, DMA_FROM_DEVICE);
			sinfo->skb = new_skb;
			//print_packet(skb);

#if 0// dump rx data
			ETH_INFO(" RX Dump pkg_len = %d\n", pkg_len);
			u8 * pdata = skb->data;
			int i;
			for (i = 0; i < pkg_len; i++)
			{
				printk("i = %d: data = %d\n", i, *(pdata+i));
			}

#endif

#ifdef CONFIG_DUAL_NIC
			if ((cmd & PKTSP_MASK) == PKTSP_PORT1) {
				struct net_device *netdev2 = mac->next_netdev;
				if (netdev2) {
					skb->protocol = eth_type_trans(skb, netdev2);
					rx_skb(netdev_priv(netdev2), skb);
				}
			} else {
				skb->protocol = eth_type_trans(skb, mac->net_dev);
				rx_skb(mac, skb);
			}
#else
			skb->protocol = eth_type_trans(skb, mac->net_dev);
			rx_skb(mac, skb);
#endif
			desc->addr1 = sinfo->mapping;

NEXT:
			desc->cmd2 = (rx_pos==mac_comm->rx_desc_num[queue]-1)? EOR_BIT|MAC_RX_LEN_MAX: MAC_RX_LEN_MAX;
			wmb();
			desc->cmd1 = (OWN_BIT | ( mac_comm->rx_desc_buff_size & LEN_MASK));

			NEXT_RX(queue, rx_pos);
		}

		/* notify gmac how many desc(rx_count) we can use again */
		//  rx_finished(queue, rx_count);

		mac_comm->rx_pos[queue] = rx_pos;
	}
}

#ifndef INTERRUPT_IMMEDIATELY
static void rx_do_tasklet(unsigned long data)
{
	struct l2sw_mac *mac = (struct l2sw_mac *) data;

	rx_interrupt(mac, mac->mac_comm->int_status);
	//write_sw_int_status0((MAC_INT_RX) & mac->mac_comm->int_status);
	interrupt_finish(mac);
}
#endif

#ifdef RX_POLLING
static int rx_poll(struct napi_struct *napi, int budget)
{
	struct l2sw_mac *mac = container_of(napi, struct l2sw_mac, napi);

	rx_interrupt(mac, mac->mac_comm->int_status);
	napi_complete(napi);
	interrupt_finish(mac);

	return 0;
}
#endif

static inline void tx_interrupt(struct l2sw_mac *mac)
{
	u32 tx_done_pos;
	u32 cmd;
	struct skb_info *skbinfo;
	struct l2sw_mac *smac;
	struct l2sw_mac_common *mac_comm = mac->mac_comm;

	tx_done_pos = mac_comm->tx_done_pos;
	//ETH_INFO(" tx_done_pos = %d\n", tx_done_pos);
	while (tx_done_pos != mac_comm->tx_pos || (mac_comm->tx_desc_full == 1)) {
		cmd = mac_comm->tx_desc[tx_done_pos].cmd1;
		//ETH_INFO(" tx_done_pos = %d\n", tx_done_pos);
		//ETH_INFO(" TX2: cmd1 = %08x, cmd2 = %08x\n", cmd, mac_comm->tx_desc[tx_done_pos].cmd2);
		skbinfo = &mac_comm->tx_temp_skb_info[tx_done_pos];
		if (skbinfo->skb == NULL) {
			ETH_ERR(" skb is null!\n");
		}

		smac = mac;
#ifdef CONFIG_DUAL_NIC
		if ((mac->next_netdev) && ((cmd & FORCE_DP_MASK) == FORCE_DP_P1)) {
			smac = netdev_priv(mac->next_netdev);
		}
#endif
		if (cmd & (ERR_CODE)) {
			ETH_ERR(" TX Error [%x]\n", cmd);

			smac->dev_stats.tx_errors++;
#if 0
			if (status & OWC_BIT) {
				smac->dev_stats.tx_window_errors++;
			}

			if (cmd & BUR_BIT) {
				ETH_ERR(" TX aborted error\n");
				smac->dev_stats.tx_aborted_errors++;
			}
			if (cmd & LNKF_BIT) {
				ETH_ERR(" TX link failure\n");
				smac->dev_stats.tx_carrier_errors++;
			}
			if (cmd & TWDE_BIT){
				ETH_ERR(" TX watchdog timer expired!\n");
			}
			if (cmd & TBE_MASK){
				ETH_ERR(" TX descriptor bit error!\n");
			}
#endif
		}
		else {
#if 0
			smac->dev_stats.collisions += (cmd & CC_MASK) >> 16;
#endif
			smac->dev_stats.tx_packets++;
			smac->dev_stats.tx_bytes += skbinfo->len;
		}

		dma_unmap_single(&mac->pdev->dev, skbinfo->mapping, skbinfo->len, DMA_TO_DEVICE);
		skbinfo->mapping = 0;
		dev_kfree_skb_irq(skbinfo->skb);
		skbinfo->skb = NULL;

		NEXT_TX(tx_done_pos);
		if (mac_comm->tx_desc_full == 1) {
			mac_comm->tx_desc_full = 0;
		}
	}

	mac_comm->tx_done_pos = tx_done_pos;
	if (TX_DESC_AVAIL(mac_comm) > 0) {
		if (netif_queue_stopped(mac->net_dev)) {
			netif_wake_queue(mac->net_dev);
		}
#ifdef CONFIG_DUAL_NIC
		if (mac->next_netdev) {
			if (netif_queue_stopped(mac->next_netdev)) {
				netif_wake_queue(mac->next_netdev);
			}
		}
#endif
	}
}

#ifndef INTERRUPT_IMMEDIATELY
static void tx_do_tasklet(unsigned long data)
{
	struct l2sw_mac *mac = (struct l2sw_mac *) data;

	tx_interrupt(mac);
	write_sw_int_status0(mac->mac_comm->int_status & MAC_INT_TX);
	wmb();
	interrupt_finish(mac);
}
#endif

static irqreturn_t ethernet_interrupt(int irq, void *dev_id)
{
	struct net_device *net_dev;
	struct l2sw_mac *mac;
	u32 status;

	//ETH_INFO("[%s] IN\n", __FUNCTION__);
	net_dev = (struct net_device*)dev_id;
	if (unlikely(net_dev == NULL)) {
		ETH_ERR(" net_dev is null!\n");
		return -1;
	}

	mac = netdev_priv(net_dev);
	spin_lock(&(mac->lock));

	/*
	if (unlikely(!netif_running(net_dev))) {
		spin_unlock(&mac->lock);
		ETH_ERR(" %s interrupt occurs when network device is not running!\n", net_dev->name);
		return -1;
	}
	*/

	write_sw_int_mask0(0xffffffff); /* mask interrupt */
	status =  read_sw_int_status0();
	//ETH_INFO(" Int Status = [%08x]\n", status);
	rmb();
	if (status == 0){
		write_sw_int_mask0(0x00000000);
		ETH_ERR(" Interrput status is null!\n");
		return -1;
	}
	write_sw_int_status0(status);
	wmb();
	mac->mac_comm->int_status = status;

#ifdef RX_POLLING
	if (napi_schedule_prep(&mac->mac_comm->napi)) {
		__napi_schedule(&mac->mac_comm->napi);
	}
#else /* RX_POLLING */
	if (status & MAC_INT_RX) {
		if (status & MAC_INT_RX_L_DESCF) {
			ETH_ERR(" RX Low-priority Descriptor full!\n");
		}

		if (status & MAC_INT_RX_H_DESCF) {
			ETH_ERR(" RX High-priority Descriptor full!\n");
		}

		if (status & MAC_INT_RX_DES_ER) {
			ETH_ERR(" Illegal RX Descriptor!\n");
			mac->dev_stats.rx_fifo_errors++;
		}

	#ifdef INTERRUPT_IMMEDIATELY
		rx_interrupt(mac, status);
		//write_sw_int_status0(mac->mac_comm->int_status & MAC_INT_RX);
		interrupt_finish(mac);
	#else
		tasklet_schedule(&mac->mac_comm->rx_tasklet);
	#endif
	}
#endif /* RX_POLLING */

	if (status & MAC_INT_TX) {
		if (status & MAC_INT_TX1_LAN_FULL) {
			ETH_ERR(" Lan Port 1 Queue Full!\n");
		}

		if (status & MAC_INT_TX0_LAN_FULL) {
			ETH_ERR(" Lan Port 0 Queue Full!\n");
		}

		if (status & MAC_INT_TX_DES_ER) {
			ETH_ERR(" Illegal TX Descriptor Error\n");
			mac->dev_stats.tx_fifo_errors++;
			mac_soft_reset(mac);
			goto OUT;
		}

#ifdef INTERRUPT_IMMEDIATELY
		tx_interrupt(mac);
		write_sw_int_status0(mac->mac_comm->int_status & MAC_INT_TX);
		wmb();
		interrupt_finish(mac);
#else
		tasklet_schedule(&mac->mac_comm->tx_tasklet);
#endif
	}

OUT:
	write_sw_int_mask0(0x00000000);
	spin_unlock(&(mac->lock));
	return IRQ_HANDLED;
}

static int ethernet_open(struct net_device *net_dev)
{
	struct l2sw_mac *mac = netdev_priv(net_dev);

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	/* Start phy */
	//netif_carrier_off(net_dev);
	//mac_phy_start(net_dev);

	mac_hw_start();

	netif_carrier_on(net_dev);

	if (netif_carrier_ok(net_dev)) {
		//ETH_INFO(" Open netif_start_queue.\n");
		netif_start_queue(net_dev);
	}

	mac->mac_comm->enable |= mac->lan_port;

	return 0;
}

static int ethernet_stop(struct net_device *net_dev)
{
	struct l2sw_mac *mac = netdev_priv(net_dev);
	unsigned long flags;

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	spin_lock_irqsave(&mac->lock, flags);
	netif_stop_queue(net_dev);
	netif_carrier_off(net_dev);

	spin_unlock_irqrestore(&mac->lock, flags);

	mac->mac_comm->enable &= ~mac->lan_port;

	if (mac->mac_comm->enable == 0) {
		//mac_phy_stop(net_dev);
		mac_hw_stop();
	}

	return 0;
}

/* Transmit a packet (called by the kernel) */
static int ethernet_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct l2sw_mac *mac;
	struct l2sw_mac_common *mac_comm;
	u32 tx_pos;
	u32 cmd1;
	u32 cmd2;
	struct mac_desc *txdesc;

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	cmd1 = 0;
	cmd2 = 0;
	mac = netdev_priv(net_dev);
	mac_comm = mac->mac_comm;
	//print_packet(skb);

	spin_lock_irq(&mac->lock); //or use spin_lock_irqsave ?
	if (mac_comm->tx_desc_full == 1) { /* no desc left, wait for tx interrupt*/
		spin_unlock_irq(&mac->lock);
		ETH_ERR("[%s] Cannot transmit (no tx descriptor)!\n", __FUNCTION__);
		return -1;
	}

	tx_pos = mac_comm->tx_pos;
	//ETH_INFO("[%s] skb->len = %d\n", __FUNCTION__, skb->len);

	/* if skb size shorter than 60, fill it with '\0' */
	if (skb->len < ETH_ZLEN) {
		if (skb_tailroom(skb) >= (ETH_ZLEN - skb->len)) {
			memset(__skb_put(skb, ETH_ZLEN - skb->len), '\0', ETH_ZLEN - skb->len);
		} else {
			struct sk_buff *old_skb = skb;
			skb = dev_alloc_skb(ETH_ZLEN + TX_OFFSET);
			if (skb) {
				memset(skb->data + old_skb->len, '\0', ETH_ZLEN - old_skb->len);
				memcpy(skb->data, old_skb->data, old_skb->len);
				skb_put(skb, ETH_ZLEN); /* add data to an sk_buff */
				dev_kfree_skb_irq(old_skb);
			} else {
				skb = old_skb;
			}
		}
	}

	txdesc = &mac_comm->tx_desc[tx_pos];
	mac_comm->tx_temp_skb_info[tx_pos].len = skb->len;
	mac_comm->tx_temp_skb_info[tx_pos].skb = skb;
	mac_comm->tx_temp_skb_info[tx_pos].mapping = dma_map_single(&mac->pdev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	cmd1 = (OWN_BIT | FS_BIT | LS_BIT | (mac->to_vlan<<12)| (skb->len& LEN_MASK));
	cmd2 = (mac_comm->tx_pos+1==TX_DESC_NUM)? EOR_BIT|(skb->len&LEN_MASK): (skb->len&LEN_MASK);
	//ETH_INFO(" TX1: cmd1 = %08x, cmd2 = %08x\n", cmd1, cmd2);

	txdesc->addr1 = mac_comm->tx_temp_skb_info[tx_pos].mapping;
	txdesc->cmd2 = cmd2;
	wmb();
	txdesc->cmd1 = cmd1;

	wmb();
	NEXT_TX(tx_pos);
	mac_comm->tx_pos = tx_pos;

	if (mac_comm->tx_pos == mac_comm->tx_done_pos) {
		netif_stop_queue(net_dev);
		mac_comm->tx_desc_full = 1;
	}
	wmb();

	/* trigger gmac to transmit*/
	tx_trigger(tx_pos);
	wmb();

	spin_unlock_irq(&mac->lock);
	return 0;
}

static void ethernet_set_rx_mode(struct net_device *net_dev)
{
	struct l2sw_mac *mac;

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	mac = netdev_priv(net_dev);
	rx_mode_set(mac);
}

static int ethernet_set_mac_address(struct net_device *net_dev, void *addr)
{
	struct sockaddr *hwaddr = (struct sockaddr *)addr;
	struct l2sw_mac *mac = netdev_priv(net_dev);

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	if (netif_running(net_dev)) {
		ETH_ERR("[%s] Ebusy\n", __FUNCTION__);
		return -EBUSY;
	}

	memcpy(net_dev->dev_addr, hwaddr->sa_data, net_dev->addr_len);

	/* Delete the old Ethernet MAC address */
	//ETH_INFO(" HW Addr = %02x:%02x:%02x:%02x:%02x:%02x\n", mac->mac_addr[0],mac->mac_addr[1],
	//       mac->mac_addr[2],mac->mac_addr[3],mac->mac_addr[4],mac->mac_addr[5]);
	if (is_valid_ether_addr(mac->mac_addr)) {
		mac_hw_addr_del(mac);
	}

	/* Set the Ethernet MAC address */
	memcpy(mac->mac_addr, hwaddr->sa_data, net_dev->addr_len);
	mac_hw_addr_set(mac);

	return 0;
}

static int ethernet_do_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd)
{
#if 0
	struct l2sw_mac *mac = netdev_priv(net_dev);
	struct phy_device *phydev = mac->mac_comm->phy_dev;
#endif
	struct mii_ioctl_data *data = if_mii(ifr);

	//ETH_INFO("[%s] IN\n", __FUNCTION__);
	//ETH_INFO("[%s] if=%s, cmd = 0x%x\n", __FUNCTION__, ifr->ifr_ifrn.ifrn_name, cmd);
#if 0
	if (!netif_running(dev))
	{
		ETH_ERR("[%s] Interface is running!\n", __FUNCTION__);
		return -EINVAL;
	}

	ETH_INFO("[%s] L:%d\n", __FUNCTION__, __LINE__);
	if (!phydev)
		return -ENODEV;
	ETH_INFO("[%s] L:%d\n", __FUNCTION__, __LINE__);
#endif

	switch (cmd) {
	case SIOCGMIIPHY:
		return 0;

	case SIOCGMIIREG:
		 /*tmp for SMII AC timing measure,only for port0 phy0*/
		data->val_out =  mdio_read(PHY0_ADDR, data->reg_num);
		break;

	case SIOCSMIIREG:
		//return phy_mii_ioctl(phydev, ifr, cmd);
		/*tmp for SMII AC timing measure,only for port0 phy0*/
		mdio_write(PHY0_ADDR, data->reg_num,data->val_in);
		break;

	default:
		ETH_ERR("[%s] ioctl #%d has not implemented yet!\n", __func__, cmd);
		return -EOPNOTSUPP;
	}
	return 0;
}

static int ethernet_change_mtu(struct net_device *dev, int new_mtu)
{
	return eth_change_mtu(dev, new_mtu);
}

static void ethernet_tx_timeout(struct net_device *net_dev)
{
}

static struct net_device_stats *ethernet_get_stats(struct net_device *net_dev)
{
	struct l2sw_mac *mac;

	//ETH_INFO("[%s] IN\n", __func__);
	mac = netdev_priv(net_dev);
	return &mac->dev_stats;
}


static struct net_device_ops netdev_ops = {
	.ndo_open               = ethernet_open,
	.ndo_stop               = ethernet_stop,
	.ndo_start_xmit         = ethernet_start_xmit,
	.ndo_set_rx_mode        = ethernet_set_rx_mode,
	.ndo_set_mac_address    = ethernet_set_mac_address,
	.ndo_do_ioctl           = ethernet_do_ioctl,
	.ndo_change_mtu         = ethernet_change_mtu,
	.ndo_tx_timeout         = ethernet_tx_timeout,
	.ndo_get_stats          = ethernet_get_stats,
};

/*********************************************************************
*
* platform_driver
*
**********************************************************************/
static u32 init_netdev(struct platform_device *pdev)
{
	u32 ret = -ENODEV;
	struct l2sw_mac *mac;
	struct net_device *net_dev;
	struct resource *res;
	struct resource *r_mem = NULL;
	u32 rc;
	int i;

	//ETH_INFO("[%s] IN\n", __func__);

	/* allocate the devices, and also allocate l2sw_mac, we can get it by netdev_priv() */
	if ((net_dev = alloc_etherdev(sizeof(struct l2sw_mac))) == NULL) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, net_dev);
	mac = netdev_priv(net_dev);
	mac->net_dev = net_dev;
	mac->pdev = pdev;
#ifdef CONFIG_DUAL_NIC
	mac->next_netdev = NULL;
#endif
	mac->mac_comm = (struct l2sw_mac_common*)kmalloc(sizeof(struct l2sw_mac_common), GFP_KERNEL);
	if (mac->mac_comm == NULL) {
		goto out_freedev;
	}
	memset(mac->mac_comm, '\0', sizeof(struct l2sw_mac_common));
	mac->mac_comm->pdev = pdev;
	mac->mac_comm->net_dev = net_dev;

	ETH_INFO("[%s] net_dev=0x%08x, mac=0x%08x, mac_comm=0x%08x\n", __func__, (int)net_dev, (int)mac, (int)mac->mac_comm);

	/*
	 * spin_lock:         return if i
	 t obtain spin lock, or it will wait (not sleep)
	 * spin_lock_irqsave: save flags, disable interrupt, obtain spin lock
	 * spin_lock_irq:     disable interrupt, obtain spin lock, if in a interrupt, don't need to use spin_lock_irqsave
	 */
	spin_lock_init(&mac->lock);

	// Get memory resoruce from dts.
	if ((r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)) != NULL) {
		ETH_INFO(" r_mem->start = 0x%08x\n", r_mem->start);
		if (l2sw_reg_base_set(devm_ioremap(&pdev->dev,r_mem->start, (r_mem->end - r_mem->start + 1))) != 0){
			ETH_ERR("[%s] ioremap failed!\n", __FUNCTION__);
			goto out_free_mac_comm;
		}
	} else {
		ETH_ERR("[%s] No MEM resource found!\n", __FUNCTION__);
		goto out_free_mac_comm;
	}

	// Get irq resource from dts.
	if ((res = platform_get_resource(pdev, IORESOURCE_IRQ, 0)) != NULL) {
		//ETH_INFO(" res->name = \"%s\", res->start = 0x%08x, res->end = 0x%08x, res->flags = 0x%08x\n",
		//      res->name, res->start, res->end, res->flags);
		net_dev->irq = res->start;
		mac->mac_comm->irq_type = res->flags & IORESOURCE_BITS;
	} else {
		ETH_ERR("[%s] No IRQ resource found!\n", __FUNCTION__);
		goto out_free_mac_comm;
	}

	l2sw_pinmux_set();
	l2sw_enable_port(pdev);

	phy_cfg();

#if 0 // phy_probe??
	#ifdef PHY_CONFIG
	mac->mac_comm->phy_node = of_parse_phandle(pdev->dev.of_node, "phy-handle", 0);
	#endif

	if ((ret = mdio_init(pdev, net_dev))) {
		ETH_ERR("[%s] Failed to initialize mdio!\n", __FUNCTION__);
		goto out_free_mac_comm;
	}

	if ((ret = mac_phy_probe(net_dev))) {
		ETH_ERR("[%s] Failed to probe phy!\n", __FUNCTION__);
		goto out_freemdio;
	}
#endif

	net_dev->netdev_ops = &netdev_ops;

#ifdef RX_POLLING
	netif_napi_add(net_dev, &mac->mac_comm->napi, rx_poll, RX_NAPI_WEIGHT);
#endif

	// Get property 'mac-addr1' from dts.
	if (of_property_read_u32(pdev->dev.of_node, "mac-addr1", &rc) == 0) {
		for (i = 0; i < 6; i++) {
			if (read_otp_data(rc+i, &mac->mac_addr[i]) != 0) {
				break;
			}
		}

		// Check if mac-address is valid or not. If not, copy from default.
		if ((i != 6) || !is_valid_ether_addr(mac->mac_addr)) {
			ETH_INFO(" Invalid mac addr from OTP[%d:%d]=%02x:%02x:%02x:%02x:%02x:%02x, use default!\n", rc, rc+5, mac->mac_addr[0],
				mac->mac_addr[1], mac->mac_addr[2], mac->mac_addr[3], mac->mac_addr[4], mac->mac_addr[5]);
			i = -1;
		}
	} else {
		i = -1;
		ETH_INFO(" Cannot get OTP address of 'mac-addr1', use default mac address!\n");
	}
	if (i != 6) {
		memcpy(mac->mac_addr, def_mac_addr, ETHERNET_MAC_ADDR_LEN);
	}

	ETH_INFO(" HW Addr = %02x:%02x:%02x:%02x:%02x:%02x\n", mac->mac_addr[0],mac->mac_addr[1],
		mac->mac_addr[2],mac->mac_addr[3],mac->mac_addr[4],mac->mac_addr[5]);

	memcpy(net_dev->dev_addr, mac->mac_addr, ETHERNET_MAC_ADDR_LEN);
	if ((ret = register_netdev(net_dev)) != 0) {
		ETH_ERR("[%s] Failed to register device \"%s\" (ret=%d)\n", __FUNCTION__, net_dev->name, ret);
		goto out_freemdio;
	}
	ETH_INFO("[%s] Registered device \"%s\"\n", __FUNCTION__, net_dev->name);

	/*register interrupt function to system*/
	rc = devm_request_irq(&pdev->dev, net_dev->irq, ethernet_interrupt, mac->mac_comm->irq_type, net_dev->name, net_dev);
	if (rc != 0) {
		ETH_ERR("[%s] Failed to request irq #%d (type=0x%01x) for \"%s\" (rc=%d)!\n", __FUNCTION__,
			net_dev->irq, mac->mac_comm->irq_type, net_dev->name, rc);
		goto out_unregister_dev;
	}

#ifndef INTERRUPT_IMMEDIATELY
	mac->mac_comm->int_status = 0;
	tasklet_init(&mac->mac_comm->rx_tasklet, rx_do_tasklet, (unsigned long)mac);
	//tasklet_disable(&mac->mac_comm->rx_tasklet);
	tasklet_init(&mac->mac_comm->tx_tasklet, tx_do_tasklet, (unsigned long)mac);
	//tasklet_disable(&mac->mac_comm->tx_tasklet);
#endif
	return 0;

out_unregister_dev:
	unregister_netdev(net_dev);

out_freemdio:
	if (mac->mac_comm->mii_bus != NULL) {
		mdio_remove(mac);
	}

out_free_mac_comm:
	if (mac->mac_comm != NULL) {
		kfree(mac->mac_comm);
	}

out_freedev:
	if (net_dev != NULL) {
		free_netdev(net_dev);
	}

	platform_set_drvdata(pdev, NULL);
	return ret;
}

#ifdef CONFIG_DUAL_NIC
static u32 init_2nd_netdev(struct platform_device *pdev, struct net_device *pre_ndev)
{
	u32 ret = -ENODEV;
	struct l2sw_mac *mac;
	struct net_device *net_dev;
	u32 rc;
	int i;

	//ETH_INFO("[%s] IN\n", __func__);

	/* allocate the devices, and also allocate l2sw_mac, we can get it by netdev_priv() */
	if ((net_dev = alloc_etherdev(sizeof(struct l2sw_mac))) == NULL) {
		return -ENOMEM;
	}

	((struct l2sw_mac*)netdev_priv(pre_ndev))->next_netdev = net_dev;
	mac = netdev_priv(net_dev);
	mac->net_dev = net_dev;
	mac->pdev = pdev;
	mac->next_netdev = NULL;
	mac->mac_comm = ((struct l2sw_mac*)netdev_priv(pre_ndev))->mac_comm;
	net_dev->irq = pre_ndev->irq;
	ETH_INFO("[%s] net_dev=0x%08x, mac=0x%08x, mac_comm=0x%08x\n", __func__, (int)net_dev, (int)mac, (int)mac->mac_comm);

	/*
	 * spin_lock:         return if i
	 t obtain spin lock, or it will wait (not sleep)
	 * spin_lock_irqsave: save flags, disable interrupt, obtain spin lock
	 * spin_lock_irq:     disable interrupt, obtain spin lock, if in a interrupt, don't need to use spin_lock_irqsave
	 */
	spin_lock_init(&mac->lock);

	net_dev->netdev_ops = &netdev_ops;

	// Get property 'mac-addr2' from dts.
	if (of_property_read_u32(pdev->dev.of_node, "mac-addr2", &rc) == 0) {
		for (i = 0; i < 6; i++) {
			if (read_otp_data(rc+i, &mac->mac_addr[i]) != 0) {
				break;
			}
		}

		// Check if mac-address is valid or not. If not, copy from default.
		if ((i != 6) || !is_valid_ether_addr(mac->mac_addr)) {
			ETH_INFO(" Invalid mac addr from OTP[%d:%d]=%02x:%02x:%02x:%02x:%02x:%02x, use default!\n", rc, rc+5, mac->mac_addr[0],
				mac->mac_addr[1], mac->mac_addr[2], mac->mac_addr[3], mac->mac_addr[4], mac->mac_addr[5]);
			i = -1;
		}
	} else {
		i = -1;
		ETH_INFO(" Cannot get OTP address of 'mac-addr2', use default mac address!\n");
	}
	if (i != 6) {
		memcpy(mac->mac_addr, def_mac_addr, ETHERNET_MAC_ADDR_LEN);
		mac->mac_addr[5] += 1;
	}

#if 0
	mac->mac_addr[0] = 0x00; mac->mac_addr[1] = 0x22; mac->mac_addr[2] = 0x60;
	mac->mac_addr[3] = 0x00; mac->mac_addr[4] = 0x81; mac->mac_addr[5] = 0xb7;
#endif
	ETH_INFO(" HW Addr2 = %02x:%02x:%02x:%02x:%02x:%02x\n", mac->mac_addr[0],mac->mac_addr[1],
		 mac->mac_addr[2],mac->mac_addr[3],mac->mac_addr[4],mac->mac_addr[5]);

	memcpy(net_dev->dev_addr, mac->mac_addr, ETHERNET_MAC_ADDR_LEN);
	if ((ret = register_netdev(net_dev)) != 0) {
		ETH_ERR("[%s] Failed to register device \"%s\" (ret=%d)\n", __FUNCTION__, net_dev->name, ret);
		goto out_freedev;
	}
	ETH_INFO("[%s] Registered device \"%s\"\n", __FUNCTION__, net_dev->name);

	return 0;

out_freedev:
	free_netdev(net_dev);

	((struct l2sw_mac*)netdev_priv(pre_ndev))->next_netdev = NULL;
	return ret;
}
#endif

static int soc0_open(struct l2sw_mac *mac)
{
	struct l2sw_mac_common *mac_comm;
	u32 rc, i;

	//ETH_INFO("[%s] IN\n", __FUNCTION__);

	mac_hw_stop();

#ifndef INTERRUPT_IMMEDIATELY
	//tasklet_enable(&mac->mac_comm->rx_tasklet);
	//tasklet_enable(&mac->mac_comm->tx_tasklet);
#endif

	mac_comm = mac->mac_comm;

#ifdef RX_POLLING
	napi_enable(&mac_comm->napi);
#endif

	mac_comm->rx_desc_num[0] = RX_QUEUE0_DESC_NUM;
#if RX_DESC_QUEUE_NUM > 1
	mac_comm->rx_desc_num[1] = RX_QUEUE1_DESC_NUM;
#endif

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		mac_comm->rx_desc[i] = NULL;
		mac_comm->rx_skb_info[i] = NULL;
		mac_comm->rx_pos[i] = 0;
	}

	mac_comm->rx_desc_buff_size = MAC_RX_LEN_MAX;
	mac_comm->tx_done_pos = 0;
	mac_comm->tx_desc = NULL;
	mac_comm->tx_pos = 0;
	mac_comm->tx_desc_full = 0;
	for (i = 0; i < TX_DESC_NUM; i++) {
		mac_comm->tx_temp_skb_info[i].skb = NULL;
	}

	rc = descs_alloc(mac_comm);
	if (rc) {
		ETH_ERR("[%s] Failed to allocate mac descriptors!\n", __FUNCTION__);
		return rc;
	}

	rc = descs_init(mac_comm);
	if (rc) {
	ETH_ERR("[%s] Fail to initialize mac descriptors!\n", __FUNCTION__);
		goto INIT_DESC_FAIL;
	}

	mac_comm->tx_desc_full = 0;
	wmb();

	/*start hardware port,open interrupt, start system tx queue*/
	mac_init(mac);
	return 0;

INIT_DESC_FAIL:
	descs_free(mac_comm);
	return rc;
}

static int soc0_stop(struct l2sw_mac *mac)
{
#ifdef RX_POLLING
	napi_disable(&mac->mac_comm->napi);
#endif

	mac_hw_stop();

	descs_free(mac->mac_comm);
	return 0;
}

static int l2sw_probe(struct platform_device *pdev)
{
	int ret;
	struct net_device *net_dev;
	struct l2sw_mac *mac;
	struct reset_control *rstc;

	//ETH_INFO("[%s] IN\n", __func__);

	if (platform_get_drvdata(pdev) != NULL) {
		return -ENODEV;
	}

	rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(rstc)) {
		dev_err(&pdev->dev, "Failed to retrieve reset controller!\n");
		return PTR_ERR(rstc);
	}
	ret = reset_control_deassert(rstc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to deassert reset line (err=%d)!\n", ret);
		return -ENODEV;
	}

	ret = init_netdev(pdev);
	if (ret != 0) {
		return ret;
	}

	if ((net_dev = platform_get_drvdata(pdev)) == NULL) {
		return -ENODEV;
	}

	mac = netdev_priv(net_dev);

	mac->cpu_port = 0x1;    // soc0
#ifdef CONFIG_DUAL_NIC
	mac->lan_port = 0x1;    // forward to port 0
#else
	mac->lan_port = 0x3;    // forward to port 0 and 1
#endif
	mac->to_vlan = 0x1;     // vlan group: 0
	mac->vlan_id = 0x0;     // vlan group: 0

	mac->mac_comm->rstc = rstc;
	mac->mac_comm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mac->mac_comm->clk)) {
		dev_err(&pdev->dev, "Failed to retrieve clock controller!\n");
		return PTR_ERR(mac->mac_comm->clk);
	}

	clk_prepare_enable(mac->mac_comm->clk);

	soc0_open(mac);

	// Set MAC address
	mac_hw_addr_set(mac);
	//mac_hw_addr_print();

#ifdef CONFIG_DUAL_NIC
	init_2nd_netdev(pdev, net_dev);
	if (mac->next_netdev == NULL) {
		goto fail_to_init_2nd_port;
	}

	net_dev = mac->next_netdev;
	mac = netdev_priv(net_dev);

	mac->cpu_port = 0x1;    // soc0
	mac->lan_port = 0x2;    // forward to port 1
	mac->to_vlan = 0x2;     // vlan group: 1
	mac->vlan_id = 0x1;     // vlan group: 1

	mac_hw_addr_set(mac);   // Set MAC address for the second net device.
	//mac_hw_addr_print();
	return 0;

fail_to_init_2nd_port:
	ETH_ERR("[%s] Failed to initialize the 2nd port!\n", __func__);
#endif

	return 0;
}

static int l2sw_remove(struct platform_device *pdev)
{
	struct net_device *net_dev;
#ifdef CONFIG_DUAL_NIC
	struct net_device *net_dev2;
#endif
	struct l2sw_mac *mac;

	if ((net_dev = platform_get_drvdata(pdev)) == NULL) {
		return 0;
	}
	mac = netdev_priv(net_dev);

#ifdef CONFIG_DUAL_NIC
	net_dev2 = mac->next_netdev;
	if (net_dev2) {
		unregister_netdev(net_dev2);
		free_netdev(net_dev2);
	}
#endif

#ifndef INTERRUPT_IMMEDIATELY
	tasklet_kill(&mac->mac_comm->rx_tasklet);
	tasklet_kill(&mac->mac_comm->tx_tasklet);
#endif

	soc0_stop(mac);

	//mac_phy_remove(net_dev);
	//mdio_remove(mac);
	clk_disable(mac->mac_comm->clk);
	unregister_netdev(net_dev);
	kfree(mac->mac_comm);
	free_netdev(net_dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int l2sw_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	netif_device_detach(ndev);
	return 0;
}

static int l2sw_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	netif_device_attach(ndev);
	return 0;
}

static const struct dev_pm_ops l2sw_pm_ops = {
	.suspend = l2sw_suspend,
	.resume  = l2sw_resume,
};
#endif

#ifdef PHY_CONFIG
static const struct of_device_id sp_l2sw_of_match[] = {
	{ .compatible = "sunplus,sp7021-l2sw" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, sp_l2sw_of_match);
#endif

static struct platform_driver l2sw_driver = {
	.probe   = l2sw_probe,
	.remove  = l2sw_remove,
	.driver  = {
		.name  = "sp_l2sw",
		.owner = THIS_MODULE,
		.of_match_table = sp_l2sw_of_match,
#ifdef CONFIG_PM
		.pm = &l2sw_pm_ops, // not sure
#endif
	},
};



static int __init l2sw_init(void)
{
	u32 status;

	//ETH_INFO("[%s] IN\n", __func__);

	status = platform_driver_register(&l2sw_driver);

	return status;
}

static void __exit l2sw_exit(void)
{
	platform_driver_unregister(&l2sw_driver);
}


module_init(l2sw_init);
module_exit(l2sw_exit);

MODULE_LICENSE("GPL v2");

