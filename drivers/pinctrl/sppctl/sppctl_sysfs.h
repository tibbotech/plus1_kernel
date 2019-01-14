#ifndef SPPCTL_SYSFS_H
#define SPPCTL_SYSFS_H

#include "sppctl_syshdrs.h"

#include "sppctl.h"
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>

typedef struct sppctl_sdata_T {
 uint8_t i;
 sppctl_pdata_t *pdata;
} sppctl_sdata_t;

void sppctl_sysfs_init(  struct platform_device *_pdev);
void sppctl_sysfs_clean( struct platform_device *_pdev);

#endif // SPPCTL_SYSFS_H
