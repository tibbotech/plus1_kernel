#ifndef __L2SW_DESC_H__
#define __L2SW_DESC_H__

#include "l2sw_define.h"


void rx_descs_flush(struct l2sw_mac_common *mac);

void tx_descs_clean(struct l2sw_mac_common *mac);

void rx_descs_clean(struct l2sw_mac_common *mac);

void descs_clean(struct l2sw_mac_common *mac);

void descs_free(struct l2sw_mac_common *mac);

u32 tx_descs_init(struct l2sw_mac_common *mac);

u32 rx_descs_init(struct l2sw_mac_common *mac);

u32 descs_alloc(struct l2sw_mac_common *mac);

u32 descs_init(struct l2sw_mac_common *mac);


#endif
