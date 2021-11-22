// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "spl2sw_define.h"
#include "spl2sw_int.h"
#include "spl2sw_driver.h"
#include "spl2sw_register.h"

static void spl2sw_port_status_change(struct spl2sw_mac *mac)
{
	struct net_device *ndev = (struct net_device *)mac->ndev;
	struct spl2sw_common *comm = mac->comm;
	u32 reg;

	reg = readl(comm->l2sw_reg_base + L2SW_PORT_ABILITY);
	if (mac->comm->dual_nic) {
		if (!netif_carrier_ok(ndev) && (reg & PORT_ABILITY_LINK_ST_P0)) {
			netif_carrier_on(ndev);
			netif_start_queue(ndev);
		} else if (netif_carrier_ok(ndev) && !(reg & PORT_ABILITY_LINK_ST_P0)) {
			netif_carrier_off(ndev);
			netif_stop_queue(ndev);
		}

		if (mac->next_ndev) {
			struct net_device *ndev2 = mac->next_ndev;

			if (!netif_carrier_ok(ndev2) && (reg & PORT_ABILITY_LINK_ST_P1)) {
				netif_carrier_on(ndev2);
				netif_start_queue(ndev2);
			} else if (netif_carrier_ok(ndev2) && !(reg & PORT_ABILITY_LINK_ST_P1)) {
				netif_carrier_off(ndev2);
				netif_stop_queue(ndev2);
			}
		}
	} else {
		if (!netif_carrier_ok(ndev) &&
		    (reg & (PORT_ABILITY_LINK_ST_P1 | PORT_ABILITY_LINK_ST_P0))) {
			netif_carrier_on(ndev);
			netif_start_queue(ndev);
		} else if (netif_carrier_ok(ndev) &&
		    !(reg & (PORT_ABILITY_LINK_ST_P1 | PORT_ABILITY_LINK_ST_P0))) {
			netif_carrier_off(ndev);
			netif_stop_queue(ndev);
		}
	}
}

static void spl2sw_rx_skb(struct spl2sw_mac *mac, struct sk_buff *skb)
{
	mac->dev_stats.rx_packets++;
	mac->dev_stats.rx_bytes += skb->len;

	netif_rx(skb);
}

static void spl2sw_rx_interrupt(struct spl2sw_mac *mac)
{
	struct spl2sw_common *comm = mac->comm;
	struct net_device_stats *dev_stats;
	struct sk_buff *skb, *new_skb;
	struct spl2sw_skb_info *sinfo;
	struct spl2sw_mac_desc *desc;
	struct spl2sw_mac_desc *h_desc;
	u32 rx_pos, pkg_len;
	u32 cmd;
	u32 num, rx_count;
	s32 queue;
	int ndev2_pkt;

	// Process high-priority queue and then low-priority queue.
	for (queue = 0; queue < RX_DESC_QUEUE_NUM; queue++) {
		rx_pos = comm->rx_pos[queue];
		rx_count = comm->rx_desc_num[queue];
		//pr_info(" queue = %d, rx_pos = %d, rx_count = %d\n", queue, rx_pos, rx_count);

		for (num = 0; num < rx_count; num++) {
			sinfo = comm->spl2sw_rx_skb_info[queue] + rx_pos;
			desc = comm->rx_desc[queue] + rx_pos;
			cmd = desc->cmd1;
			//pr_info(" rx_pos = %d, RX: cmd1 = %08x, cmd2 = %08x\n", rx_pos, cmd, desc->cmd2);

			if (cmd & OWN_BIT) {
				//pr_info(" RX: is owned by NIC, rx_pos = %d, desc = %px", rx_pos, desc);
				break;
			}

			if ((comm->dual_nic) && ((cmd & PKTSP_MASK) == PKTSP_PORT1)) {
				struct spl2sw_mac *mac2;

				ndev2_pkt = 1;
				mac2 = (mac->next_ndev) ? netdev_priv(mac->next_ndev) : NULL;
				dev_stats = (mac2) ? &mac2->dev_stats : &mac->dev_stats;
			} else {
				ndev2_pkt = 0;
				dev_stats = &mac->dev_stats;
			}

			pkg_len = cmd & LEN_MASK;
			if (unlikely((cmd & ERR_CODE) || (pkg_len < 64))) {
				dev_stats->rx_length_errors++;
				dev_stats->rx_dropped++;
				goto spl2sw_rx_poll_next;
			}

			if (unlikely(cmd & RX_IP_CHKSUM_BIT)) {
				//pr_info(" RX IP Checksum error!\n");
				dev_stats->rx_crc_errors++;
				dev_stats->rx_dropped++;
				goto spl2sw_rx_poll_next;
			}

			/* Allocate an skbuff for receiving. */
			new_skb = __dev_alloc_skb(comm->rx_desc_buff_size + RX_OFFSET,
						  GFP_ATOMIC | GFP_DMA);
			if (unlikely(!new_skb)) {
				dev_stats->rx_errors++;
				desc->cmd2 = (rx_pos == comm->rx_desc_num[queue] - 1) ?
					     EOR_BIT : 0;
				goto spl2sw_rx_poll_err;
			}
			new_skb->dev = mac->ndev;

			dma_unmap_single(&comm->pdev->dev, sinfo->mapping,
					 comm->rx_desc_buff_size, DMA_FROM_DEVICE);

			skb = sinfo->skb;
			skb->ip_summed = CHECKSUM_NONE;

			/* skb_put will judge if tail exceeds end, but __skb_put won't. */
			__skb_put(skb, (pkg_len - 4 > comm->rx_desc_buff_size) ?
				       comm->rx_desc_buff_size : pkg_len - 4);

			sinfo->mapping = dma_map_single(&comm->pdev->dev, new_skb->data,
							comm->rx_desc_buff_size,
							DMA_FROM_DEVICE);
			if (dma_mapping_error(&comm->pdev->dev, sinfo->mapping)) {
				dev_kfree_skb(new_skb);
				dev_stats->rx_errors++;
				desc->cmd2 = (rx_pos == comm->rx_desc_num[queue] - 1) ?
					     EOR_BIT : 0;
				goto spl2sw_rx_poll_err;
			}
			sinfo->skb = new_skb;
			//print_packet(skb);

			if (ndev2_pkt) {
				struct net_device *netdev2 = mac->next_ndev;

				if (netdev2) {
					skb->protocol = eth_type_trans(skb, netdev2);
					spl2sw_rx_skb(netdev_priv(netdev2), skb);
				}
			} else {
				skb->protocol = eth_type_trans(skb, mac->ndev);
				spl2sw_rx_skb(mac, skb);
			}

			desc->addr1 = sinfo->mapping;

spl2sw_rx_poll_next:
			desc->cmd2 = (rx_pos == comm->rx_desc_num[queue] - 1) ?
				     EOR_BIT | MAC_RX_LEN_MAX : MAC_RX_LEN_MAX;
spl2sw_rx_poll_err:
			wmb();	/* Set OWN_BIT after other fields of descriptor are effective. */
			desc->cmd1 = OWN_BIT;

			NEXT_RX(queue, rx_pos);

			/* If there are packets in high-priority queue,
			 * stop processing low-priority queue.
			 */
			if ((queue == 1) && ((h_desc->cmd1 & OWN_BIT) == 0))
				break;
		}

		comm->rx_pos[queue] = rx_pos;

		/* Save pointer to last rx descriptor of high-priority queue. */
		if (queue == 0)
			h_desc = comm->rx_desc[queue] + rx_pos;
	}
}

#ifndef INTERRUPT_IMMEDIATELY
void rx_do_tasklet(unsigned long data)
{
	struct spl2sw_mac *mac = (struct spl2sw_mac *)data;

	spl2sw_rx_interrupt(mac);
}
#endif

#ifdef RX_POLLING
int rx_poll(struct napi_struct *napi, int budget)
{
	struct spl2sw_mac *mac = container_of(napi, struct spl2sw_mac, napi);

	spl2sw_rx_interrupt(mac);
	napi_complete(napi);

	return 0;
}
#endif

static void spl2sw_tx_interrupt(struct spl2sw_mac *mac)
{
	u32 tx_done_pos;
	u32 cmd;
	struct spl2sw_skb_info *skbinfo;
	struct spl2sw_mac *smac;
	struct spl2sw_common *comm = mac->comm;

	tx_done_pos = comm->tx_done_pos;
	//pr_info(" tx_done_pos = %d\n", tx_done_pos);
	while ((tx_done_pos != comm->tx_pos) || (comm->tx_desc_full == 1)) {
		cmd = comm->tx_desc[tx_done_pos].cmd1;
		if (cmd & OWN_BIT)
			break;

		//pr_info(" tx_done_pos = %d\n", tx_done_pos);
		//pr_info(" TX2: cmd1 = %08x, cmd2 = %08x\n", cmd, comm->tx_desc[tx_done_pos].cmd2);

		skbinfo = &comm->tx_temp_skb_info[tx_done_pos];
		if (unlikely(skbinfo->skb == NULL))
			pr_err(" skb is null!\n");

		smac = mac;
		if ((mac->next_ndev) && ((cmd & TO_VLAN_MASK) == TO_VLAN_GROUP1))
			smac = netdev_priv(mac->next_ndev);

		if (unlikely(cmd & (ERR_CODE))) {
			//pr_err(" TX Error = %x\n", cmd);

			smac->dev_stats.tx_errors++;
		} else {
			smac->dev_stats.tx_packets++;
			smac->dev_stats.tx_bytes += skbinfo->len;
		}

		dma_unmap_single(&mac->pdev->dev, skbinfo->mapping, skbinfo->len, DMA_TO_DEVICE);
		skbinfo->mapping = 0;
		dev_kfree_skb_irq(skbinfo->skb);
		skbinfo->skb = NULL;

		NEXT_TX(tx_done_pos);
		if (comm->tx_desc_full == 1)
			comm->tx_desc_full = 0;
	}

	comm->tx_done_pos = tx_done_pos;
	if (!comm->tx_desc_full) {
		if (netif_queue_stopped(mac->ndev))
			netif_wake_queue(mac->ndev);

		if (mac->next_ndev) {
			if (netif_queue_stopped(mac->next_ndev))
				netif_wake_queue(mac->next_ndev);
		}
	}
}

#ifndef INTERRUPT_IMMEDIATELY
void tx_do_tasklet(unsigned long data)
{
	struct spl2sw_mac *mac = (struct spl2sw_mac *)data;

	spl2sw_tx_interrupt(mac);
}
#endif

irqreturn_t spl2sw_ethernet_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev;
	struct spl2sw_mac *mac;
	struct spl2sw_common *comm;
	u32 status;

	//pr_info("[%s] IN\n", __func__);
	ndev = (struct net_device *)dev_id;
	if (unlikely(ndev == NULL)) {
		pr_err(" ndev is null!\n");
		return -1;
	}

	mac = netdev_priv(ndev);
	comm = mac->comm;

	spin_lock(&comm->lock);

	/* Mask all interrupts */
	writel(0xffffffff, comm->l2sw_reg_base + L2SW_SW_INT_MASK_0);

	status = readl(comm->l2sw_reg_base + L2SW_SW_INT_STATUS_0);
	//pr_info(" Int Status = %08x\n", status);
	if (unlikely(status == 0)) {
		pr_err(" Interrput status is null!\n");
		goto OUT;
	}
	writel(status, comm->l2sw_reg_base + L2SW_SW_INT_STATUS_0);

#ifdef RX_POLLING
	if (napi_schedule_prep(&comm->napi))
		__napi_schedule(&comm->napi);
#else /* RX_POLLING */
	if (status & MAC_INT_RX) {
		if (unlikely(status & MAC_INT_RX_DES_ERR)) {
			pr_err(" Illegal RX Descriptor!\n");
			mac->dev_stats.rx_fifo_errors++;
		}
#ifdef INTERRUPT_IMMEDIATELY
		spl2sw_rx_interrupt(mac);
#else
		tasklet_schedule(&comm->rx_tasklet);
#endif
	}
#endif /* RX_POLLING */

	if (status & MAC_INT_TX) {
		if (unlikely(status & MAC_INT_TX_DES_ERR)) {
			pr_err(" Illegal TX Descriptor Error\n");
			mac->dev_stats.tx_fifo_errors++;
			spl2sw_mac_soft_reset(mac);
		} else {
#ifdef INTERRUPT_IMMEDIATELY
			spl2sw_tx_interrupt(mac);
			writel(status & MAC_INT_TX,
			       comm->l2sw_reg_base + L2SW_SW_INT_STATUS_0);
#else
			tasklet_schedule(&comm->tx_tasklet);
#endif
		}
	}

	if (status & MAC_INT_PORT_ST_CHG)	/* link status changed */
		spl2sw_port_status_change(mac);

	//if (status & MAC_INT_RX_H_DESCF)
	//	pr_info(" RX High-priority Descriptor Full!\n");
	//if (status & MAC_INT_RX_L_DESCF)
	//	pr_info(" RX Low-priority Descriptor Full!\n");
	//if (status & MAC_INT_TX_LAN0_QUE_FULL)
	//	pr_info(" Lan Port 0 Queue Full!\n");
	//if (status & MAC_INT_TX_LAN1_QUE_FULL)
	//	pr_info(" Lan Port 1 Queue Full!\n");
	//if (status & MAC_INT_RX_SOC_QUE_FULL)
	//	pr_info(" CPU Port RX Queue Full!\n");
	//if (status & MAC_INT_TX_SOC_PAUSE_ON)
	//	pr_info(" CPU Port TX Pause On!\n");
	//if (status & MAC_INT_GLOBAL_QUE_FULL)
	//	pr_info(" Global Queue Full!\n");

OUT:
	wmb();	/* make sure settings are effective. */
	writel(MAC_INT_MASK_DEF, comm->l2sw_reg_base + L2SW_SW_INT_MASK_0);
	spin_unlock(&comm->lock);
	return IRQ_HANDLED;
}

int spl2sw_get_irq(struct platform_device *pdev, struct spl2sw_common *comm)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		pr_err(" No IRQ resource found!\n");
		return -1;
	}

	pr_debug(" res->name = \"%s\", res->start = %pa\n", res->name, &res->start);
	comm->irq = res->start;
	return 0;
}

int spl2sw_request_irq(struct platform_device *pdev, struct spl2sw_common *comm,
		     struct net_device *ndev)
{
	int rc;

	rc = devm_request_irq(&pdev->dev, comm->irq, spl2sw_ethernet_interrupt, 0,
			      ndev->name, ndev);
	if (rc != 0) {
		pr_err(" Failed to request irq #%d for \"%s\" (rc = %d)!\n",
		       ndev->irq, ndev->name, rc);
	}
	return rc;
}
