/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef __UNCOMPRESS_H_
#define __UNCOMPRESS_H_

#include <mach/ll_uart.h>

struct regs_uart_t {
	u32 uart_data;
	u32 uart_lsr;
	u32 uart_msr;
	u32 uart_lcr;
	u32 uart_mcr;
	u32 uart_div_l;
	u32 uart_div_h;
	u32 uart_isc;
	u32 uart_tx_residue;
	u32 uart_rx_residue;
	u32 uart_rx_threshold;
	u32 uart_clk_src;
};

static struct regs_uart_t *uart_base = ((struct regs_uart_t *)(LL_UART_PADDR));

static void putc(int c)
{
	while (!(uart_base->uart_lsr & SP_UART_LSR_TX))
		barrier();
	uart_base->uart_data = c;
}

static inline void flush(void)
{
}

#define arch_decomp_setup()	/* Do nothing */

#endif /* __UNCOMPRESS_H_ */
