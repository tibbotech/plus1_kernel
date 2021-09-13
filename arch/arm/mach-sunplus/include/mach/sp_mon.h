/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */
#ifndef _SP_MON_H_
#define _SP_MON_H_

#ifdef CONFIG_MAGIC_SYSRQ
/*
 * Code can be shared between Linux and U-Boot
 */
#define BUF_SIZE	256

//#define DBG_PRN	dbg_printf
#define DBG_PRN(...)

/* Write 4 bytes to a specific register in designated group */
void write_regs(unsigned int regGroupNum, unsigned int regIndex, unsigned int value);
/* Print specific group of registers */
void prn_regs(unsigned int regGroupNum);
/* Dump requested physical memory data */
int dumpPhysMem(unsigned int physAddr, unsigned int dumpSize);
/* Write 4 bytes to a specific memory address */
int writeToMem(unsigned int physAddr, unsigned int value);
/* Show task info */
void showTaskInfo(pid_t pid);
/* Query if we're in MON shell */
int is_in_mon_shell(void);
/* Print to MON debug port */
int dbg_printf(const char *fmt, ...);

#endif	/* CONFIG_MAGIC_SYSRQ */
#endif	/* _SP_MON_H_ */
