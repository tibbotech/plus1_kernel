#ifndef __ISP_API_S_H
#define __ISP_API_S_H

#include "isp_api.h"
#include "reg_mipi.h"

void ispVsyncIntCtrl(struct mipi_isp_info *isp_info, u8 on);
void ispVsyncInt(struct mipi_isp_info *isp_info);
void videoStartMode(struct mipi_isp_info *isp_info);
void videoStopMode(struct mipi_isp_info *isp_info);
void isp_setting_s(struct mipi_isp_info *isp_info);

#endif /* __ISP_API_S_H */
