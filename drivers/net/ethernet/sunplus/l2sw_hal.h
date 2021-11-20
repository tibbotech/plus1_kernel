/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __L2SW_HAL_H__
#define __L2SW_HAL_H__

#include "l2sw_register.h"
#include "l2sw_define.h"
#include "l2sw_desc.h"

#define MDIO_RW_TIMEOUT_RETRY_NUMBERS 500

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
void rx_mode_set(struct net_device *ndev);
int  mdio_read(struct l2sw_mac *mac, int phy_id, int regnum);
int  mdio_write(struct l2sw_mac *mac, int phy_id, int regnum, u16 val);
void tx_trigger(struct l2sw_mac *mac);
void write_sw_int_mask0(struct l2sw_mac *mac, u32 value);
void write_sw_int_status0(struct l2sw_mac *mac, u32 value);
u32  read_sw_int_status0(struct l2sw_mac *mac);
u32  read_port_ability(struct l2sw_mac *mac);
int  phy_cfg(struct l2sw_mac *mac);
void l2sw_enable_port(struct l2sw_mac *mac);
void regs_print(struct l2sw_mac *mac);

#endif
