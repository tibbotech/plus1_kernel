#ifndef __IOP_HAL_H__
#define __IOP_HAL_H__

#include <linux/types.h>
#include <linux/module.h>
#include "reg_iop.h"




void hal_iop_init(void __iomem *iopbase);
void hal_iop_load_normal_code(void __iomem *iopbase);
void hal_iop_load_standby_code(void __iomem *iopbase);
void hal_iop_normalmode(void __iomem *iopbase);
void hal_iop_standbymode(void __iomem *iopbase);

void hal_iop_get_iop_data(void __iomem *iopbase);
void hal_iop_set_iop_data(void __iomem *iopbase, unsigned int num, unsigned int value);
void hal_iop_suspend(void __iomem *iopbase, void __iomem *ioppmcbase);
void hal_iop_shutdown(void __iomem *iopbase, void __iomem *ioppmcbase);
#endif /* __IOP_HAL_H__ */
