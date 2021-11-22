/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __SPL2SW_INT_H__
#define __SPL2SW_INT_H__

#ifndef INTERRUPT_IMMEDIATELY
void rx_do_tasklet(unsigned long data);
void tx_do_tasklet(unsigned long data);
#endif
#ifdef RX_POLLING
int rx_poll(struct napi_struct *napi, int budget);
#endif
irqreturn_t spl2sw_ethernet_interrupt(int irq, void *dev_id);
int spl2sw_get_irq(struct platform_device *pdev, struct spl2sw_common *comm);
int spl2sw_request_irq(struct platform_device *pdev, struct spl2sw_common *comm,
		       struct net_device *ndev);

#endif
