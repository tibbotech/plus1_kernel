#ifndef SPPCTL_PROCFS_H
#define SPPCTL_PROCFS_H

#include "sppctl_syshdrs.h"
#include "sppctl_defs.h"

#include "sppctl.h"
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>

void sppctl_procfs_init(  struct platform_device *_pdev);
void sppctl_procfs_clean( struct platform_device *_pdev);

#endif // SPPCTL_PROCFS_H
