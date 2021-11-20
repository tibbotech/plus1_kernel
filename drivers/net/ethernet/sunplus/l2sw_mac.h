/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __L2SW_MAC_H__
#define __L2SW_MAC_H__

#include "l2sw_define.h"
#include "l2sw_register.h"
#include "l2sw_define.h"
#include "l2sw_desc.h"

//calculate the empty tx descriptor number
#define TX_DESC_AVAIL(mac) \
	(((mac)->tx_pos != (mac)->tx_done_pos) ? \
	(((mac)->tx_done_pos < (mac)->tx_pos) ? \
	(TX_DESC_NUM - ((mac)->tx_pos - (mac)->tx_done_pos)) : \
	((mac)->tx_done_pos - (mac)->tx_pos)) : \
	((mac)->tx_desc_full ? 0 : TX_DESC_NUM))

void mac_hw_stop(struct l2sw_mac *mac);
void mac_hw_start(struct l2sw_mac *mac);
void mac_hw_addr_set(struct l2sw_mac *mac);
void mac_hw_addr_del(struct l2sw_mac *mac);
void mac_addr_table_del_all(struct l2sw_mac *mac);
void mac_hw_addr_print(struct l2sw_mac *mac);
void mac_hw_init(struct l2sw_mac *mac);
void mac_switch_mode(struct l2sw_mac *mac);
void mac_disable_port_sa_learning(struct l2sw_mac *mac);
void mac_enable_port_sa_learning(struct l2sw_mac *mac);
void mac_rx_mode_set(struct l2sw_mac *mac);
void mac_enable_port(struct l2sw_mac *mac);
bool mac_init(struct l2sw_mac *mac);
void mac_soft_reset(struct l2sw_mac *mac);
void mac_regs_print(struct l2sw_mac *mac);

#endif
