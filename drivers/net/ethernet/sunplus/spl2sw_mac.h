/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SPL2SW_MAC_H__
#define __SPL2SW_MAC_H__

#include "spl2sw_define.h"
#include "spl2sw_register.h"
#include "spl2sw_define.h"
#include "spl2sw_desc.h"

//calculate the empty tx descriptor number
#define TX_DESC_AVAIL(mac) \
	(((mac)->tx_pos != (mac)->tx_done_pos) ? \
	(((mac)->tx_done_pos < (mac)->tx_pos) ? \
	(TX_DESC_NUM - ((mac)->tx_pos - (mac)->tx_done_pos)) : \
	((mac)->tx_done_pos - (mac)->tx_pos)) : \
	((mac)->tx_desc_full ? 0 : TX_DESC_NUM))

void spl2sw_mac_hw_stop(struct spl2sw_mac *mac);
void spl2sw_mac_hw_start(struct spl2sw_mac *mac);
void spl2sw_mac_addr_set(struct spl2sw_mac *mac);
void spl2sw_mac_addr_del(struct spl2sw_mac *mac);
void spl2sw_mac_addr_table_del_all(struct spl2sw_mac *mac);
void spl2sw_mac_addr_print(struct spl2sw_mac *mac);
void spl2sw_mac_hw_init(struct spl2sw_mac *mac);
void spl2sw_mac_switch_mode(struct spl2sw_mac *mac);
void spl2sw_mac_disable_port_sa_learning(struct spl2sw_mac *mac);
void spl2sw_spl2sw_mac_enable_port_sa_learning(struct spl2sw_mac *mac);
void spl2sw_mac_rx_mode_set(struct spl2sw_mac *mac);
void spl2sw_mac_enable_port(struct spl2sw_mac *mac);
bool spl2sw_mac_init(struct spl2sw_mac *mac);
void spl2sw_mac_soft_reset(struct spl2sw_mac *mac);
void spl2sw_mac_regs_print(struct spl2sw_mac *mac);

#endif
