#ifndef __IOP_HAL_H__
#define __IOP_HAL_H__

#include <linux/types.h>
#include <linux/module.h>
#include "reg_iop.h"




void hal_iop_init(void __iomem *iopbase);


void hal_iop_suspend(void __iomem *iopbase, void __iomem *ioppmcbase);

#endif /* __IOP_HAL_H__ */
