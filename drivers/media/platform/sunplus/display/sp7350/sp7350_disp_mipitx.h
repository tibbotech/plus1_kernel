/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Sunplus SP7350 SoC Display driver for MIPITX block
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#ifndef __SP7350_DISP_MIPITX_H__
#define __SP7350_DISP_MIPITX_H__

#define SP7350_MIPITX_DSI       0
#define SP7350_MIPITX_CSI       1

/*
 * Init SP7350 MIPITX Setting
 */
void sp7350_mipitx_init(void);

/*
 * Show SP7350 MIPITX Info
 */
void sp7350_mipitx_reg_info(void);
void sp7350_mipitx_decrypt_info(void);
void sp7350_mipitx_resolution_chk(void);

#endif	//__SP7350_DISP_MIPITX_H__

