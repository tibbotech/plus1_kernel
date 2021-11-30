#ifndef __LL_UART_H_
#define __LL_UART_H_

#include <mach/io_map.h>

#define LL_UART_PADDR		PA_IOB_ADDR(18 * 32 * 4)
#define LL_UART_VADDR		VA_IOB_ADDR(18 * 32 * 4)

#define SP_UART_DATA		0x00
#define SP_UART_LSR		0x04
#define SP_UART_LSR_TX		(1 << 0)
#define SP_UART_LSR_TXE		(1 << 6)

#endif
