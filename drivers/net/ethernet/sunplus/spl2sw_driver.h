/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SPL2SW_DRIVER_H__
#define __SPL2SW_DRIVER_H__

#include "spl2sw_define.h"
#include "spl2sw_register.h"
#include "spl2sw_int.h"
#include "spl2sw_mdio.h"
#include "spl2sw_mac.h"
#include "spl2sw_desc.h"

#define NEXT_TX(N)	((N) = (((N)+1) == TX_DESC_NUM) ? 0 : (N)+1)
#define NEXT_RX(QUE, N)	((N) = (((N)+1) == mac->comm->rx_desc_num[QUE]) ? 0 : (N)+1)

#define RX_NAPI_WEIGHT	64

void print_packet(struct sk_buff *skb);

#endif
