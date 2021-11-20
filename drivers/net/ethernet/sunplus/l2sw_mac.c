// SPDX-License-Identifier: GPL-2.0
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#include "l2sw_mac.h"

bool mac_init(struct l2sw_mac *mac)
{
	u32 i;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++)
		mac->comm->rx_pos[i] = 0;
	mb();	/* make sure settings are effective. */

	mac_hw_init(mac);

	return 1;
}

void mac_soft_reset(struct l2sw_mac *mac)
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

	mac_hw_stop(mac);

	//descs_clean(mac->comm);
	rx_descs_flush(mac->comm);
	mac->comm->tx_pos = 0;
	mac->comm->tx_done_pos = 0;
	mac->comm->tx_desc_full = 0;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++)
		mac->comm->rx_pos[i] = 0;
	mb();	/* make sure settings are effective. */

	mac_hw_init(mac);
	mac_hw_start(mac);

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
