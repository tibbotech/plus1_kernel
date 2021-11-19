/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __L2SW_DESC_H__
#define __L2SW_DESC_H__

#include "l2sw_define.h"

void rx_descs_flush(struct l2sw_common *comm);

void tx_descs_clean(struct l2sw_common *comm);

void rx_descs_clean(struct l2sw_common *comm);

void descs_clean(struct l2sw_common *comm);

void descs_free(struct l2sw_common *comm);

void tx_descs_init(struct l2sw_common *comm);

int  rx_descs_init(struct l2sw_common *comm);

int  descs_alloc(struct l2sw_common *comm);

int  descs_init(struct l2sw_common *comm);

#endif
