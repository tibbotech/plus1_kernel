#include "l2sw_desc.h"
#include "l2sw_define.h"


void rx_descs_flush(struct l2sw_common *mac)
{
	u32 i, j;
	struct mac_desc *rx_desc;
	struct skb_info *rx_skbinfo;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		rx_desc = mac->rx_desc[i];
		rx_skbinfo = mac->rx_skb_info[i];
		for (j = 0; j < mac->rx_desc_num[i]; j++) {
			rx_desc[j].addr1 = rx_skbinfo[j].mapping;
			rx_desc[j].cmd2 = (j == mac->rx_desc_num[i] - 1)? EOR_BIT|mac->rx_desc_buff_size: mac->rx_desc_buff_size;
			wmb();
			rx_desc[j].cmd1 = OWN_BIT;
		}
	}
}

void tx_descs_clean(struct l2sw_common *mac)
{
	u32 i;
	s32 buflen;

	if (mac->tx_desc == NULL) {
		return;
	}

	for (i = 0; i < TX_DESC_NUM; i++) {
		mac->tx_desc[i].cmd1 = 0;
		wmb();
		mac->tx_desc[i].cmd2 = 0;
		mac->tx_desc[i].addr1 = 0;
		mac->tx_desc[i].addr2 = 0;

		if (mac->tx_temp_skb_info[i].mapping) {
			buflen = (mac->tx_temp_skb_info[i].skb != NULL)? mac->tx_temp_skb_info[i].skb->len: MAC_TX_BUFF_SIZE;
			dma_unmap_single(&mac->pdev->dev, mac->tx_temp_skb_info[i].mapping, buflen, DMA_TO_DEVICE);
			mac->tx_temp_skb_info[i].mapping = 0;
		}

		if (mac->tx_temp_skb_info[i].skb) {
			dev_kfree_skb(mac->tx_temp_skb_info[i].skb);
			mac->tx_temp_skb_info[i].skb = NULL;
		}
	}
}

void rx_descs_clean(struct l2sw_common *mac)
{
	u32 i, j;
	struct skb_info *skbinfo;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		if (mac->rx_skb_info[i] == NULL) {
			continue;
		}

		skbinfo = mac->rx_skb_info[i];
		for (j = 0; j < mac->rx_desc_num[i]; j++) {
			mac->rx_desc[i][j].cmd1 = 0;
			wmb();
			mac->rx_desc[i][j].cmd2 = 0;
			mac->rx_desc[i][j].addr1 = 0;

			if (skbinfo[j].skb) {
				dma_unmap_single(&mac->pdev->dev, skbinfo[j].mapping, mac->rx_desc_buff_size, DMA_FROM_DEVICE);
				dev_kfree_skb(skbinfo[j].skb);
				skbinfo[j].skb = NULL;
				skbinfo[j].mapping = 0;
			}
		}

		kfree(mac->rx_skb_info[i]);
		mac->rx_skb_info[i] = NULL;
	}
}

void descs_clean(struct l2sw_common *mac)
{
	rx_descs_clean(mac);
	tx_descs_clean(mac);
}

void descs_free(struct l2sw_common *mac)
{
	u32 i;

	descs_clean(mac);
	mac->tx_desc = NULL;
	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		mac->rx_desc[i] = NULL;
	}

	/*  Free descriptor area  */
	if (mac->desc_base != NULL) {
		dma_free_coherent(&mac->pdev->dev, mac->desc_size, mac->desc_base, mac->desc_dma);
		mac->desc_base = NULL;
		mac->desc_dma = 0;
		mac->desc_size = 0;
	}
}

u32 tx_descs_init(struct l2sw_common *mac)
{
	memset(mac->tx_desc, '\0', sizeof(struct mac_desc) * (TX_DESC_NUM+MAC_GUARD_DESC_NUM));
	return 0;
}

u32 rx_descs_init(struct l2sw_common *mac)
{
	struct sk_buff *skb;
	u32 i, j;
	struct mac_desc *rxdesc;
	struct skb_info *rx_skbinfo;

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		mac->rx_skb_info[i] = (struct skb_info*)kmalloc(sizeof(struct skb_info) * mac->rx_desc_num[i], GFP_KERNEL);
		if (mac->rx_skb_info[i] == NULL) {
			goto MEM_ALLOC_FAIL;
		}

		rx_skbinfo = mac->rx_skb_info[i];
		rxdesc = mac->rx_desc[i];
		for (j = 0; j < mac->rx_desc_num[i]; j++) {
			skb = __dev_alloc_skb(mac->rx_desc_buff_size + RX_OFFSET, GFP_ATOMIC | GFP_DMA);
			if (!skb) {
				goto MEM_ALLOC_FAIL;
			}

			skb->dev = mac->net_dev;
			skb_reserve(skb, RX_OFFSET);/* +data +tail */

			rx_skbinfo[j].skb = skb;
			rx_skbinfo[j].mapping = dma_map_single(&mac->pdev->dev, skb->data, mac->rx_desc_buff_size, DMA_FROM_DEVICE);
			rxdesc[j].addr1 = rx_skbinfo[j].mapping;
			rxdesc[j].addr2 = 0;
			rxdesc[j].cmd2 = (j == mac->rx_desc_num[i] - 1)? EOR_BIT|mac->rx_desc_buff_size: mac->rx_desc_buff_size;
			wmb();
			rxdesc[j].cmd1 = OWN_BIT;
		}
	}

	return 0;

MEM_ALLOC_FAIL:
	rx_descs_clean(mac);
	return -ENOMEM;
}

u32 descs_alloc(struct l2sw_common *mac)
{
	u32 i;
	s32 desc_size;

	/* Alloc descriptor area  */
	desc_size = (TX_DESC_NUM + MAC_GUARD_DESC_NUM) * sizeof(struct mac_desc);
	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		desc_size += mac->rx_desc_num[i] * sizeof(struct mac_desc);
	}

	mac->desc_base = dma_alloc_coherent(&mac->pdev->dev, desc_size, &mac->desc_dma, GFP_KERNEL);
	if (mac->desc_base == NULL) {
		return -ENOMEM;
	}
	mac->desc_size = desc_size;

	/* Setup Tx descriptor */
	mac->tx_desc = (struct mac_desc*)mac->desc_base;

	/* Setup Rx descriptor */
	mac->rx_desc[0] = &mac->tx_desc[TX_DESC_NUM + MAC_GUARD_DESC_NUM];
	for (i = 1; i < RX_DESC_QUEUE_NUM; i++) {
		mac->rx_desc[i] = mac->rx_desc[i-1] + mac->rx_desc_num[i - 1];
	}

	return 0;
}

u32 descs_init(struct l2sw_common *mac)
{
	u32 i, rc;

	// Initialize rx descriptor's data
	mac->rx_desc_num[0] = RX_QUEUE0_DESC_NUM;
#if RX_DESC_QUEUE_NUM > 1
	mac->rx_desc_num[1] = RX_QUEUE1_DESC_NUM;
#endif

	for (i = 0; i < RX_DESC_QUEUE_NUM; i++) {
		mac->rx_desc[i] = NULL;
		mac->rx_skb_info[i] = NULL;
		mac->rx_pos[i] = 0;
	}
	mac->rx_desc_buff_size = MAC_RX_LEN_MAX;

	// Initialize tx descriptor's data
	mac->tx_done_pos = 0;
	mac->tx_desc = NULL;
	mac->tx_pos = 0;
	mac->tx_desc_full = 0;
	for (i = 0; i < TX_DESC_NUM; i++) {
		mac->tx_temp_skb_info[i].skb = NULL;
	}

	// Allocate tx & rx descriptors.
	rc = descs_alloc(mac);
	if (rc) {
		ETH_ERR("[%s] Failed to allocate mac descriptors!\n", __func__);
		return rc;
	}

	rc = tx_descs_init(mac);
	if (rc) {
		return rc;
	}

	return rx_descs_init(mac);
}

