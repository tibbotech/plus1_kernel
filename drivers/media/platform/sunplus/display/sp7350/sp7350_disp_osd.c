// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display driver for OSD layer
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#include "sp7350_disp_regs.h"
#include "sp7350_display.h"
#include "sp7350_osd_palette.h"
#include "sp7350_osd_header.h" //this is for test
#include <media/sunplus/disp/sp7350/sp7350_disp_osd.h>

int sp7350_disp_state = 1;
EXPORT_SYMBOL(sp7350_disp_state);

static struct sp7350_osd_region *sp_osd_region_vir;
static dma_addr_t sp_osd_region_phy;
static void *sp_osd_header_vir;
static dma_addr_t sp_osd_header_phy;

extern unsigned char osd0_data_array;
extern unsigned char osd1_data_array;
extern unsigned char osd2_data_array;
extern unsigned char osd3_data_array;

void sp7350_osd_init(void)
{
	
	struct sp_disp_device *disp_dev = gdisp_dev;

	init_waitqueue_head(&disp_dev->osd_wait);
	spin_lock_init(&disp_dev->osd_lock);

}

void sp7350_osd_decrypt_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3, value4;
	u32 value5, value6, value7, value8;
	u32 osd0_value, osd1_value, osd2_value, osd3_value;

	pr_info("OSD Layer info\n");
	pr_info("       OSD0       OSD1       OSD2       OSD3\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_EN);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_EN);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_EN);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_EN);
	pr_info("OSD_EN[%s] [%s] [%s] [%s]\n",
		FIELD_GET(GENMASK(0,0), value1)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value2)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value3)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value4)?"---En --":"---Dis--");

	value1 = readl(disp_dev->base + (0 << 7) + OSD_BIST_CTRL);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_BIST_CTRL);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_BIST_CTRL);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_BIST_CTRL);
	pr_info("BIST  0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value2, value3, value4);

	osd0_value = readl(disp_dev->base + (0 << 7) + OSD_CTRL);
	osd1_value = readl(disp_dev->base + (1 << 7) + OSD_CTRL);
	osd2_value = readl(disp_dev->base + (2 << 7) + OSD_CTRL);
	osd3_value = readl(disp_dev->base + (3 << 7) + OSD_CTRL);
	pr_info("CTRL  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd0_value, osd1_value, osd2_value, osd3_value);

	value1 = readl(disp_dev->base + (0 << 7) + OSD_BASE_ADDR);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_BASE_ADDR);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_BASE_ADDR);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_BASE_ADDR);
	pr_info("ADDR  0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value2, value3, value4);

	pr_info("HEADER list\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_BUS_MONITOR_H);
	value2 = readl(disp_dev->base + (0 << 7) + OSD_BUS_MONITOR_L);
	value3 = readl(disp_dev->base + (1 << 7) + OSD_BUS_MONITOR_H);
	value4 = readl(disp_dev->base + (1 << 7) + OSD_BUS_MONITOR_L);
	value5 = readl(disp_dev->base + (2 << 7) + OSD_BUS_MONITOR_H);
	value6 = readl(disp_dev->base + (2 << 7) + OSD_BUS_MONITOR_L);
	value7 = readl(disp_dev->base + (3 << 7) + OSD_BUS_MONITOR_H);
	value8 = readl(disp_dev->base + (3 << 7) + OSD_BUS_MONITOR_L);
	pr_info("HDR[0]0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value3, value5, value7);
	pr_info("HDR[1]0x%08x 0x%08x 0x%08x 0x%08x\n",
		value2, value4, value6, value8);

	pr_info("     Offset(W/H) (Width  /  Height)\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (0 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (0 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (0 << 7) + OSD_VVLD_HEIGHT);
	pr_info("OSD0 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (1 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (1 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (1 << 7) + OSD_VVLD_HEIGHT);
	pr_info("OSD1 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (2 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (2 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (2 << 7) + OSD_VVLD_HEIGHT);
	pr_info("OSD2 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (3 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (3 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (3 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_VVLD_HEIGHT);
	pr_info("OSD3 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

}
EXPORT_SYMBOL(sp7350_osd_decrypt_info);

void sp7350_osd_resolution_chk(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value1, value2, value3, value4;
	u32 value5, value6, value7, value8;
	u32 osd0_value, osd1_value, osd2_value, osd3_value;

	pr_info("OSD Layer resolution chk\n");
	pr_info("       OSD0       OSD1       OSD2       OSD3\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_EN);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_EN);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_EN);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_EN);
	pr_info("OSD_EN[%s] [%s] [%s] [%s]\n",
		FIELD_GET(GENMASK(0,0), value1)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value2)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value3)?"---En --":"---Dis--",
		FIELD_GET(GENMASK(0,0), value4)?"---En --":"---Dis--");

	value1 = readl(disp_dev->base + (0 << 7) + OSD_BIST_CTRL);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_BIST_CTRL);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_BIST_CTRL);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_BIST_CTRL);
	pr_info("BIST  0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value2, value3, value4);

	osd0_value = readl(disp_dev->base + (0 << 7) + OSD_CTRL);
	osd1_value = readl(disp_dev->base + (1 << 7) + OSD_CTRL);
	osd2_value = readl(disp_dev->base + (2 << 7) + OSD_CTRL);
	osd3_value = readl(disp_dev->base + (3 << 7) + OSD_CTRL);
	pr_info("CTRL  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd0_value, osd1_value, osd2_value, osd3_value);

	value1 = readl(disp_dev->base + (0 << 7) + OSD_BASE_ADDR);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_BASE_ADDR);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_BASE_ADDR);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_BASE_ADDR);
	pr_info("ADDR  0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value2, value3, value4);

	pr_info("HEADER list\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_BUS_MONITOR_H);
	value2 = readl(disp_dev->base + (0 << 7) + OSD_BUS_MONITOR_L);
	value3 = readl(disp_dev->base + (1 << 7) + OSD_BUS_MONITOR_H);
	value4 = readl(disp_dev->base + (1 << 7) + OSD_BUS_MONITOR_L);
	value5 = readl(disp_dev->base + (2 << 7) + OSD_BUS_MONITOR_H);
	value6 = readl(disp_dev->base + (2 << 7) + OSD_BUS_MONITOR_L);
	value7 = readl(disp_dev->base + (3 << 7) + OSD_BUS_MONITOR_H);
	value8 = readl(disp_dev->base + (3 << 7) + OSD_BUS_MONITOR_L);
	pr_info("HDR[0]0x%08x 0x%08x 0x%08x 0x%08x\n",
		value1, value3, value5, value7);
	pr_info("HDR[1]0x%08x 0x%08x 0x%08x 0x%08x\n",
		value2, value4, value6, value8);

	pr_info("     Offset(W/H) (Width   /   Height)\n");
	value1 = readl(disp_dev->base + (0 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (0 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (0 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (0 << 7) + OSD_VVLD_HEIGHT);
	pr_info("  OSD0 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (1 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (1 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (1 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (1 << 7) + OSD_VVLD_HEIGHT);
	pr_info("  OSD1 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (2 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (2 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (2 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (2 << 7) + OSD_VVLD_HEIGHT);
	pr_info("  OSD2 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

	value1 = readl(disp_dev->base + (3 << 7) + OSD_HVLD_OFFSET);
	value2 = readl(disp_dev->base + (3 << 7) + OSD_HVLD_WIDTH);
	value3 = readl(disp_dev->base + (3 << 7) + OSD_VVLD_OFFSET);
	value4 = readl(disp_dev->base + (3 << 7) + OSD_VVLD_HEIGHT);
	pr_info("  OSD3 %04d %04d, %04d(0x%04x) %04d(0x%04x)\n",
		value1, value3, value2, value2, value4, value4);

}
EXPORT_SYMBOL(sp7350_osd_resolution_chk);

void sp7350_osd_bist_info(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, value1, value2, value3, value4;
	u32 osd0_layer_en, osd1_layer_en, osd2_layer_en, osd3_layer_en;
	u32 osd0_bist_en, osd1_bist_en, osd2_bist_en, osd3_bist_en;
	u32 osd0_bist_mode, osd1_bist_mode, osd2_bist_mode, osd3_bist_mode;

	value = readl(disp_dev->base + (0 << 7) + OSD_EN);
	osd0_layer_en = FIELD_GET(GENMASK(0, 0), value);
	value = readl(disp_dev->base + (1 << 7) + OSD_EN);
	osd1_layer_en = FIELD_GET(GENMASK(0, 0), value);
	value = readl(disp_dev->base + (2 << 7) + OSD_EN);
	osd2_layer_en = FIELD_GET(GENMASK(0, 0), value);
	value = readl(disp_dev->base + (3 << 7) + OSD_EN);
	osd3_layer_en = FIELD_GET(GENMASK(0, 0), value);

	value1 = readl(disp_dev->base + (0 << 7) + OSD_BIST_CTRL);
	osd0_bist_en = FIELD_GET(GENMASK(7, 7), value1);
	osd0_bist_mode = FIELD_GET(GENMASK(6, 6), value1);

	value2 = readl(disp_dev->base + (1 << 7) + OSD_BIST_CTRL);
	osd1_bist_en = FIELD_GET(GENMASK(7, 7), value2);
	osd1_bist_mode = FIELD_GET(GENMASK(6, 6), value2);

	value3 = readl(disp_dev->base + (2 << 7) + OSD_BIST_CTRL);
	osd2_bist_en = FIELD_GET(GENMASK(7, 7), value3);
	osd2_bist_mode = FIELD_GET(GENMASK(6, 6), value3);

	value4 = readl(disp_dev->base + (3 << 7) + OSD_BIST_CTRL);
	osd3_bist_en = FIELD_GET(GENMASK(7, 7), value4);
	osd3_bist_mode = FIELD_GET(GENMASK(6, 6), value4);

	pr_info("OSD G189-G192 BIST info\n");
	pr_info("     OSD0[layer%s][bist%s]: %s\n",
		osd0_layer_en?"EN ":"DIS",
		osd0_bist_en?"ON ":"OFF",
		osd0_bist_mode?"Border":"COLORBAR");
		//osd0_bist_en?(osd0_bist_mode?"Border":"COLORBAR"):"--");
	pr_info("     OSD1[layer%s][bist%s]: %s\n",
		osd1_layer_en?"EN ":"DIS",
		osd1_bist_en?"ON ":"OFF",
		osd1_bist_mode?"Border":"COLORBAR");
		//osd1_bist_en?(osd1_bist_mode?"Border":"COLORBAR"):"--");
	pr_info("     OSD2[layer%s][bist%s]: %s\n",
		osd2_layer_en?"EN ":"DIS",
		osd2_bist_en?"ON ":"OFF",
		osd2_bist_mode?"Border":"COLORBAR");
		//osd2_bist_en?(osd2_bist_mode?"Border":"COLORBAR"):"--");
	pr_info("     OSD3[layer%s][bist%s]: %s\n",
		osd3_layer_en?"EN ":"DIS",
		osd3_bist_en?"ON ":"OFF",
		osd3_bist_mode?"Border":"COLORBAR");
		//osd3_bist_en?(osd3_bist_mode?"Border":"COLORBAR"):"--");

}
EXPORT_SYMBOL(sp7350_osd_bist_info);


void sp7350_osd_bist_set(int osd_layer_sel, int bist_en, int osd_bist_type)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	pr_info("sp7350_osd_bist_set: OSD%d\n", osd_layer_sel);
	value = readl(disp_dev->base + (osd_layer_sel << 7) + OSD_BIST_CTRL);
	value &= ~(SP7350_OSD_BIST_MASK);
	value |= SP7350_OSD_BIST_EN(bist_en) | SP7350_OSD_BIST_TYPE(osd_bist_type);
	writel(value, disp_dev->base + (osd_layer_sel << 7) + OSD_BIST_CTRL);

}
EXPORT_SYMBOL(sp7350_osd_bist_set);

void sp7350_osd_layer_onoff(int osd_layer_sel, int onoff)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value;

	pr_info("sp7350_osd_layer: OSD%d %s\n", osd_layer_sel, onoff?"ON":"OFF");
	value = readl(disp_dev->base + (osd_layer_sel << 7) + OSD_EN);
	value &= ~(SP7350_OSD_EN_MASK);
	value |= SP7350_OSD_EN(onoff);
	writel(value, disp_dev->base + (osd_layer_sel << 7) + OSD_EN);

}
EXPORT_SYMBOL(sp7350_osd_layer_onoff);

void sp7350_osd_header_init(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	int i;

	for (i = 0; i < SP_DISP_MAX_OSD_LAYER; i++) {
		disp_dev->osd_hdr[i] = dma_alloc_coherent(disp_dev->pdev,
				sizeof(struct sp7350_osd_header) + 1024,
				&disp_dev->osd_hdr_phy[i],
				GFP_KERNEL | __GFP_ZERO);

		//pr_info("  osd%d hdr VA 0x%px(PA 0x%llx) len %ld\n",
		//	i,
		//	disp_dev->osd_hdr[i],
		//	disp_dev->osd_hdr_phy[i],
		//	sizeof(struct sp7350_osd_header) + 1024);

		if (!disp_dev->osd_hdr[i]) {
			pr_err("malloc osd header fail\n");
			return;
		}
	}
	
}
EXPORT_SYMBOL(sp7350_osd_header_init);

void sp7350_osd_header_clear(int osd_layer_sel)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 *osd_header;
	//int i;

	if (!disp_dev->osd_hdr[osd_layer_sel])
		return;

	osd_header = (u32 *)disp_dev->osd_hdr[osd_layer_sel];

	/*
	 * only clear osd header first 32 bytes data
	 */
	//for (i = 0; i < 8; i++)
	//	osd_header[i] = 0;

}
EXPORT_SYMBOL(sp7350_osd_header_clear);

void sp7350_osd_header_show(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 *osd0_header, *osd1_header, *osd2_header, *osd3_header;
	int i;

	pr_info("  --- osd base addr ---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		readl(disp_dev->base + (0 << 7) + OSD_BASE_ADDR),
		readl(disp_dev->base + (1 << 7) + OSD_BASE_ADDR),
		readl(disp_dev->base + (2 << 7) + OSD_BASE_ADDR),
		readl(disp_dev->base + (3 << 7) + OSD_BASE_ADDR));

	osd0_header = (u32 *)disp_dev->osd_hdr[0];
	osd1_header = (u32 *)disp_dev->osd_hdr[1];
	osd2_header = (u32 *)disp_dev->osd_hdr[2];
	osd3_header = (u32 *)disp_dev->osd_hdr[3];
	pr_info("  --- osd header ---\n");
	for (i = 0; i < 8; i++)
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		SWAP32(osd0_header[i]), SWAP32(osd1_header[i]), SWAP32(osd2_header[i]), SWAP32(osd3_header[i]));		

	pr_info("  --- osd width ofs ---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		readl(disp_dev->base + (0 << 7) + OSD_HVLD_OFFSET),
		readl(disp_dev->base + (1 << 7) + OSD_HVLD_OFFSET),
		readl(disp_dev->base + (2 << 7) + OSD_HVLD_OFFSET),
		readl(disp_dev->base + (3 << 7) + OSD_HVLD_OFFSET));
	pr_info("  --- osd width ---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		readl(disp_dev->base + (0 << 7) + OSD_HVLD_WIDTH),
		readl(disp_dev->base + (1 << 7) + OSD_HVLD_WIDTH),
		readl(disp_dev->base + (2 << 7) + OSD_HVLD_WIDTH),
		readl(disp_dev->base + (3 << 7) + OSD_HVLD_WIDTH));
	pr_info("  --- osd height ofs ---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		readl(disp_dev->base + (0 << 7) + OSD_VVLD_OFFSET),
		readl(disp_dev->base + (1 << 7) + OSD_VVLD_OFFSET),
		readl(disp_dev->base + (2 << 7) + OSD_VVLD_OFFSET),
		readl(disp_dev->base + (3 << 7) + OSD_VVLD_OFFSET));
	pr_info("  --- osd height ---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		readl(disp_dev->base + (0 << 7) + OSD_VVLD_HEIGHT),
		readl(disp_dev->base + (1 << 7) + OSD_VVLD_HEIGHT),
		readl(disp_dev->base + (2 << 7) + OSD_VVLD_HEIGHT),
		readl(disp_dev->base + (3 << 7) + OSD_VVLD_HEIGHT));

}
EXPORT_SYMBOL(sp7350_osd_header_show);

u32 osd_base_addr_backup[SP_DISP_MAX_OSD_LAYER] = {0x00};
u32 osd_base_addr_backup1[SP_DISP_MAX_OSD_LAYER] = {0x00};
u32 osd0_header_bakup[32+256] __attribute__((aligned(1024))) = {0x00};
u32 osd1_header_bakup[32+256] __attribute__((aligned(1024))) = {0x00};
u32 osd2_header_bakup[32+256] __attribute__((aligned(1024))) = {0x00};
u32 osd3_header_bakup[32+256] __attribute__((aligned(1024))) = {0x00};
u32 osd_width_ofs_backup[SP_DISP_MAX_OSD_LAYER] = {0x00};
u32 osd_width_backup[SP_DISP_MAX_OSD_LAYER] = {0x00};
u32 osd_height_ofs_backup[SP_DISP_MAX_OSD_LAYER] = {0x00};
u32 osd_height_backup[SP_DISP_MAX_OSD_LAYER] = {0x00};

void sp7350_osd_header_save(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 *osd_header;
	int i, layer, value;

	for (layer = 0; layer < SP_DISP_MAX_OSD_LAYER; layer++) {
		value = readl(disp_dev->base + (layer << 7) + OSD_BASE_ADDR);
		osd_base_addr_backup[layer] = value;

		osd_header = (u32 *)disp_dev->osd_hdr[layer];
		for (i = 0; i < 8; i++) {
			if (layer == 0)
				osd0_header_bakup[i] = osd_header[i];
			else if (layer == 1)
				osd1_header_bakup[i] = osd_header[i];
			else if (layer == 2)
				osd2_header_bakup[i] = osd_header[i];
			else if (layer == 3)
				osd3_header_bakup[i] = osd_header[i];
		}

		osd_width_ofs_backup[layer] = readl(disp_dev->base + (layer << 7) + OSD_HVLD_OFFSET);
		osd_width_backup[layer] = readl(disp_dev->base + (layer << 7) + OSD_HVLD_WIDTH);
		osd_height_ofs_backup[layer] = readl(disp_dev->base + (layer << 7) + OSD_VVLD_OFFSET);
		osd_height_backup[layer] = readl(disp_dev->base + (layer << 7) + OSD_VVLD_HEIGHT);
	}
	#if 0
	pr_info("  --- osd base addr backup---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd_base_addr_backup[0], osd_base_addr_backup[1], osd_base_addr_backup[2], osd_base_addr_backup[3]);
	pr_info("  --- osd header backup---\n");
	for (i = 0; i < 8; i++)
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		SWAP32(osd0_header_bakup[i]), SWAP32(osd1_header_bakup[i]), SWAP32(osd2_header_bakup[i]), SWAP32(osd3_header_bakup[i]));

	pr_info("  --- osd width ofs backup---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd_width_ofs_backup[0], osd_width_ofs_backup[1], osd_width_ofs_backup[2], osd_width_ofs_backup[3]);
	pr_info("  --- osd width backup---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd_width_backup[0], osd_width_backup[1], osd_width_backup[2], osd_width_backup[3]);
	pr_info("  --- osd height ofs backup---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd_height_ofs_backup[0], osd_height_ofs_backup[1], osd_height_ofs_backup[2], osd_height_ofs_backup[3]);
	pr_info("  --- osd height backup---\n");
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		osd_height_backup[0], osd_height_backup[1], osd_height_backup[2], osd_height_backup[3]);
	#endif
}
EXPORT_SYMBOL(sp7350_osd_header_save);

void sp7350_osd_header_restore(int osd_layer_sel)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (!disp_dev->osd_hdr[osd_layer_sel])
		return;

	if (osd_base_addr_backup1[osd_layer_sel] != 0)
		writel(osd_base_addr_backup1[osd_layer_sel],
			disp_dev->base + (osd_layer_sel << 7) + OSD_BASE_ADDR);
	else if (osd_base_addr_backup[osd_layer_sel] != 0)
		writel(osd_base_addr_backup[osd_layer_sel],
			disp_dev->base + (osd_layer_sel << 7) + OSD_BASE_ADDR);
	else
		;//TBD

}
EXPORT_SYMBOL(sp7350_osd_header_restore);

void sp7350_osd_header_update(struct sp7350fb_info *info, int osd_layer_sel)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 *osd_header;

	if (!disp_dev->osd_hdr[osd_layer_sel])
		return;

	osd_header = (u32 *)disp_dev->osd_hdr[osd_layer_sel];

	osd_header[7] = SWAP32((u32)info->buf_addr_phy);

}
EXPORT_SYMBOL(sp7350_osd_header_update);

void sp7350_osd_layer_set(struct sp7350fb_info *info, int osd_layer_sel)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 tmp_width, tmp_height, tmp_color_mode;
	u32 tmp_alpha = 0, value = 0;
	u32 *osd_header, *osd_palette;
	int i;

	//pr_info("%s for osd%d\n", __func__, osd_layer_sel);
	if (!disp_dev->osd_hdr[osd_layer_sel])
		return;

	osd_header = (u32 *)disp_dev->osd_hdr[osd_layer_sel];
	osd_palette = (u32 *)disp_dev->osd_hdr[osd_layer_sel] + 32;

	/*
	 * Fill OSD Layer Header info
	 */
	tmp_color_mode = info->color_mode;
	//tmp_alpha = SP7350_OSD_HDR_BL | SP7350_OSD_HDR_ALPHA;
	value |= (tmp_color_mode << 24) | SP7350_OSD_HDR_BS | tmp_alpha;

	if (info->color_mode == SP7350_OSD_COLOR_MODE_8BPP)
		value |= SP7350_OSD_HDR_CULT;

	osd_header[0] = SWAP32(value);

	//if (disp_dev->osd_res[osd_layer_sel].color_mode == SP7350_OSD_COLOR_MODE_8BPP)
	//	osd_header[0] = SWAP32(0x80001000 | tmp_color_mode << 24 | tmp_alpha);
	//else
	//	osd_header[0] = SWAP32(0x00001000 | tmp_color_mode << 24 | tmp_alpha);

	tmp_width = info->width;
	tmp_height = info->height;
	osd_header[1] = SWAP32(tmp_height << 16 | tmp_width << 0);

	osd_header[2] = 0;
	osd_header[3] = 0;
	osd_header[4] = 0;

	value = 0;
	value |= tmp_width;

	if (info->color_mode == SP7350_OSD_COLOR_MODE_YUY2)
		value |= SP7350_OSD_HDR_CSM_SET(SP7350_OSD_CSM_BYPASS);
	else
		value |= SP7350_OSD_HDR_CSM_SET(SP7350_OSD_CSM_RGB_BT601);

	osd_header[5] = SWAP32(value);

	osd_header[6] = SWAP32(0xFFFFFFE0);

	/* OSD buffer data address */
	osd_header[7] = SWAP32((u32)info->buf_addr_phy);

	/*
	 * update sp7350 osd layer register
	 */
	value = 0;
	value = OSD_CTRL_COLOR_MODE_RGB
		| OSD_CTRL_CLUT_FMT_ARGB
		| OSD_CTRL_LATCH_EN
		| OSD_CTRL_A32B32_EN
		| OSD_CTRL_FIFO_DEPTH;
	writel(value, disp_dev->base + (osd_layer_sel << 7) + OSD_CTRL);

	writel(disp_dev->osd_hdr_phy[osd_layer_sel],
		disp_dev->base + (osd_layer_sel << 7) + OSD_BASE_ADDR);

	writel(disp_dev->osd_res[osd_layer_sel].x_ofs,
		disp_dev->base + (osd_layer_sel << 7) + OSD_HVLD_OFFSET);
	writel(disp_dev->osd_res[osd_layer_sel].y_ofs,
		disp_dev->base + (osd_layer_sel << 7) + OSD_VVLD_OFFSET);
#if 0
	writel(disp_dev->osd_res[osd_layer_sel].width,
		disp_dev->base + (osd_layer_sel << 7) + OSD_HVLD_WIDTH);
	writel(disp_dev->osd_res[osd_layer_sel].height,
		disp_dev->base + (osd_layer_sel << 7) + OSD_VVLD_HEIGHT);
#else
	writel(disp_dev->out_res.width,
		disp_dev->base + (osd_layer_sel << 7) + OSD_HVLD_WIDTH);
	writel(disp_dev->out_res.height,
		disp_dev->base + (osd_layer_sel << 7) + OSD_VVLD_HEIGHT);
#endif

	writel(0, disp_dev->base + (osd_layer_sel << 7) + OSD_BIST_CTRL);
	writel(0, disp_dev->base + (osd_layer_sel << 7) + OSD_3D_H_OFFSET);
	writel(0, disp_dev->base + (osd_layer_sel << 7) + OSD_SRC_DECIMATION_SEL);
	writel(1, disp_dev->base + (osd_layer_sel << 7) + OSD_EN);

	//GPOST bypass
	writel(0, disp_dev->base + (osd_layer_sel << 7) + GPOST_CONFIG);
	writel(0, disp_dev->base + (osd_layer_sel << 7) + GPOST_MASTER_EN);
	writel(0x8010, disp_dev->base + (osd_layer_sel << 7) + GPOST_BG1);
	writel(0x0080, disp_dev->base + (osd_layer_sel << 7) + GPOST_BG2);

	//GPOST PQ disable
	writel(0, disp_dev->base + (osd_layer_sel << 7) + GPOST_CONTRAST_CONFIG);

	/*
	 * in case of color_mode 8bpp, load grey or color palette
	 */
	if ((disp_dev->dev[osd_layer_sel]) &&
		(info->color_mode == SP7350_OSD_COLOR_MODE_8BPP)) {
		if (disp_dev->dev[osd_layer_sel]->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY) {
			
			for (i = 0; i < 256; i++)
				osd_palette[i] = SWAP32(disp_osd_8bpp_pal_grey[i]); //8bpp grey scale table (argb)

		} else if (disp_dev->dev[osd_layer_sel]->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_PAL8) {

			for (i = 0; i < 256; i++)
				osd_palette[i] = SWAP32(disp_osd_8bpp_pal_color[i]); //8bpp 256 color table (argb)

		}
	}

	#if 0
	pr_info("  --- osd%d header ---\n", osd_layer_sel);
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		SWAP32(osd_header[0]), SWAP32(osd_header[1]), SWAP32(osd_header[2]), SWAP32(osd_header[3]));
	pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
		SWAP32(osd_header[4]), SWAP32(osd_header[5]), SWAP32(osd_header[6]), SWAP32(osd_header[7]));
	//for (i = 0; i < 8; i++)
	//	pr_info("osd_header[%d] 0x%08x\n", i, SWAP32(osd_header[i]));

	//pr_info("  --- osd%d palette ---\n", osd_layer_sel);
	//pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
	//	SWAP32(osd_palette[0]), SWAP32(osd_palette[1]), SWAP32(osd_palette[2]), SWAP32(osd_palette[3]));
	//pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
	//	SWAP32(osd_palette[4]), SWAP32(osd_palette[5]), SWAP32(osd_palette[6]), SWAP32(osd_palette[7]));
	//pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
	//	SWAP32(osd_palette[8]), SWAP32(osd_palette[9]), SWAP32(osd_palette[10]), SWAP32(osd_palette[11]));
	//pr_info("  0x%08x 0x%08x 0x%08x 0x%08x\n",
	//	SWAP32(osd_palette[12]), SWAP32(osd_palette[13]), SWAP32(osd_palette[14]), SWAP32(osd_palette[15]));
	//for (i = 0; i < 256; i++)
	//for (i = 0; i < 8; i++)
	//	if (i%32 == 0)
	//		pr_info("osd_palette[%d] 0x%08x\n", i, SWAP32(osd_palette[i]));
	#endif
}
EXPORT_SYMBOL(sp7350_osd_layer_set);

int sp7350_osd_resolution_init(struct sp_disp_device *disp_dev)
{
	struct sp7350fb_info info;
	int i;

	if (((disp_dev->osd_res[0].width == 240) && (disp_dev->osd_res[0].height == 320)) ||
		((disp_dev->osd_res[1].width == 240) && (disp_dev->osd_res[1].height == 320)) ||
		((disp_dev->osd_res[2].width == 240) && (disp_dev->osd_res[2].height == 320)) ||
		((disp_dev->osd_res[3].width == 240) && (disp_dev->osd_res[3].height == 320)))
			return 0;

	sp7350_osd_header_init();

	//pr_info("  osd0_data_array VA 0x%px(PA 0x%llx)\n",
	//	&osd0_data_array, virt_to_phys(&osd0_data_array));
	//pr_info("  osd1_data_array VA 0x%px(PA 0x%llx)\n",
	//	&osd1_data_array, virt_to_phys(&osd1_data_array));
	//pr_info("  osd2_data_array VA 0x%px(PA 0x%llx)\n",
	//	&osd2_data_array, virt_to_phys(&osd2_data_array));
	//pr_info("  osd3_data_array VA 0x%px(PA 0x%llx)\n",
	//	&osd3_data_array, virt_to_phys(&osd3_data_array));

	for (i = 0; i < SP_DISP_MAX_OSD_LAYER; i++) {
		info.width = disp_dev->osd_res[i].width;
		info.height = disp_dev->osd_res[i].height;
		info.color_mode = disp_dev->osd_res[i].color_mode;

		if (i == 0)
			info.buf_addr_phy = (u32)virt_to_phys(&osd0_data_array);
		else if (i == 1)
			info.buf_addr_phy = (u32)virt_to_phys(&osd1_data_array);
		else if (i == 2)
			info.buf_addr_phy = (u32)virt_to_phys(&osd2_data_array);
		else if (i == 3)
			info.buf_addr_phy = (u32)virt_to_phys(&osd3_data_array);

		sp7350_osd_layer_set(&info, i);
	}

	sp7350_osd_header_save();

	return 0;
}
EXPORT_SYMBOL(sp7350_osd_resolution_init);

static const char * const sp_osd_cmod_str[] = {
	"none", "none", "256color index", "none",
	"YUY2", "none", "none", "none",
	"RGB565", "ARGB1555", "RGBA4444", "ARGB4444",
	"none", "RGBA8888", "ARGB8888", "none"};

static const int sp_osd_cmod[16][10] = {
	/* bpp nonstd rgba_len rgba_ofs*/
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{ 8, 0, 8, 8, 8, 8,  8, 16, 24,  0},	/* 8bpp */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{16, 1, 0, 0, 0, 0,  0,  0,  0,  0},	/* YUY2 */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{16, 0, 5, 6, 5, 0, 11,  5,  0,  0},	/* RGB565 */
	{16, 0, 5, 5, 5, 1, 10,  5,  0, 15},	/* ARGB1555 */
	{16, 0, 4, 4, 4, 4, 12,  8,  4,  0},	/* RGBA4444 */
	{16, 0, 4, 4, 4, 4,  8,  4,  0, 12},	/* ARGB4444 */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0},	/* none */
	{32, 0, 8, 8, 8, 8, 24, 16,  8,  0},	/* RGBA8888 */
	{32, 0, 8, 8, 8, 8, 16,  8,  0, 24},	/* ARGB8888 */
	{ 0, 0, 0, 0, 0, 0,  0,  0,  0,  0}	/* none */
	};

int sp7350_osd_get_fbinfo(struct sp7350fb_info *pinfo)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (!disp_dev)
		return 1;

	//pr_info("sp7350_osd_get_fbinfo\n");
	/* get sp7350fb resolution from dts */
	#if 1
	pinfo->width = disp_dev->osd_res[0].width;
	pinfo->height = disp_dev->osd_res[0].height;
	pinfo->color_mode = disp_dev->osd_res[0].color_mode;
	#else
	pinfo->width = disp_dev->UIRes.width;
	pinfo->height = disp_dev->UIRes.height;
	pinfo->color_mode = disp_dev->UIFmt;
	#endif
	pinfo->buf_num = 2;
	pinfo->buf_align = 4096;
	strcpy(pinfo->cmod.name, sp_osd_cmod_str[pinfo->color_mode]);

	pinfo->cmod.bits_per_pixel	= sp_osd_cmod[pinfo->color_mode][0];
	pinfo->cmod.nonstd		= sp_osd_cmod[pinfo->color_mode][1];

	pinfo->cmod.red.length		= sp_osd_cmod[pinfo->color_mode][2];
	pinfo->cmod.green.length	= sp_osd_cmod[pinfo->color_mode][3];
	pinfo->cmod.blue.length		= sp_osd_cmod[pinfo->color_mode][4];
	pinfo->cmod.transp.length	= sp_osd_cmod[pinfo->color_mode][5];

	pinfo->cmod.red.offset		= sp_osd_cmod[pinfo->color_mode][6];
	pinfo->cmod.green.offset	= sp_osd_cmod[pinfo->color_mode][7];
	pinfo->cmod.blue.offset		= sp_osd_cmod[pinfo->color_mode][8];
	pinfo->cmod.transp.offset	= sp_osd_cmod[pinfo->color_mode][9];

	//pr_info("  sp7350fb_info\n");
	//pr_info("    W %d H %d CMOD %d(%s)\n", pinfo->width, pinfo->height,
	//	pinfo->color_mode, pinfo->cmod.name);
	//pr_info("    BUF_NUM %d ALIGN %d\n", pinfo->buf_num, pinfo->buf_align);
	//pr_info("    BPP %d NONSTD %d\n", pinfo->cmod.bits_per_pixel, pinfo->cmod.nonstd);
	//pr_info("    len/ofs RGBA (%d %d %d %d) (%d %d %d %d)\n",
	//	pinfo->cmod.red.length, pinfo->cmod.green.length,
	//	pinfo->cmod.blue.length, pinfo->cmod.transp.length,
	//	pinfo->cmod.red.offset, pinfo->cmod.green.offset,
	//	pinfo->cmod.blue.offset, pinfo->cmod.transp.offset);
	//pr_info("    buf_addr     0x%llx\n", pinfo->buf_addr);
	//pr_info("    buf_addr_pal 0x%px\n", pinfo->buf_addr_pal);
	//pr_info("    buf_size %d\n", pinfo->buf_size);
	//pr_info("    handle %d\n", pinfo->handle);

	return 0;
}
EXPORT_SYMBOL(sp7350_osd_get_fbinfo);

int sp7350_osd_region_create(struct sp7350fb_info *pinfo)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 value, tmp_value;
	u32 *osd_header;
	//__be32 data_be = 0;

	//tmp_value = be32_to_cpu(data_be);
	/*
	 * alloc sp7350 osd header/region memory
	 */
	sp_osd_header_vir = dma_alloc_coherent(disp_dev->pdev,
			sizeof(struct sp7350_osd_header) + 1024,
			&sp_osd_header_phy,
			GFP_KERNEL | __GFP_ZERO);

	//pr_info("  osd hdr VA 0x%px(PA 0x%llx) len %ld\n",
	//	sp_osd_header_vir,
	//	sp_osd_header_phy,
	//	sizeof(struct sp7350_osd_header) + 1024);

	if (!sp_osd_header_vir) {
		pr_err("malloc osd header fail\n");
		return -ENOMEM;
	}

	sp_osd_region_vir = dma_alloc_coherent(disp_dev->pdev,
			sizeof(struct sp7350_osd_region),
			&sp_osd_region_phy,
			GFP_KERNEL | __GFP_ZERO);

	//pr_info("  osd rgn VA 0x%px(PA 0x%llx) len %ld\n",
	//	sp_osd_region_vir,
	//	sp_osd_region_phy,
	//	sizeof(struct sp7350_osd_region));

	if (!sp_osd_region_vir) {
		pr_err("malloc osd region fail\n");
		return -ENOMEM;
	}

	/*
	 * update sp7350 osd region info
	 */
	//todo
	//sp_osd_region_vir->RegionInfo

	sp_osd_region_vir->color_mode = pinfo->color_mode;
	sp_osd_region_vir->buf_num = pinfo->buf_num;
	sp_osd_region_vir->buf_align = pinfo->buf_align;

	tmp_value = pinfo->width * pinfo->height * (pinfo->cmod.bits_per_pixel >> 3);
	sp_osd_region_vir->buf_size = EXTENDED_ALIGNED(tmp_value, pinfo->buf_align);
	sp_osd_region_vir->buf_addr_phy = pinfo->buf_addr;
	sp_osd_region_vir->buf_addr_vir = (u8 *)pinfo->buf_addr;

	sp_osd_region_vir->hdr = (struct sp7350_osd_header *)sp_osd_header_vir;
	sp_osd_region_vir->hdr_pal = sp_osd_header_vir + sizeof(struct sp7350_osd_header);

	sp_osd_region_vir->pal = (u8 *)pinfo->buf_addr_pal;

	//pr_info("  osd pal VA 0x%px(PA 0x%llx) len %d\n",
	//	sp_osd_region_vir->pal,
	//	virt_to_phys(sp_osd_region_vir->pal),
	//	1024);

	//pr_info("osd_header=0x%px 0x%llx addr=0x%llx\n",
	//		sp_osd_header_vir,
	//		sp_osd_header_phy,
	//		pinfo->buf_addr);

	/*
	 * update sp7350 osd header info
	 */
	osd_header = (u32 *)sp_osd_header_vir;

	if (pinfo->color_mode == SP7350_OSD_COLOR_MODE_8BPP)
		osd_header[0] = SWAP32(0x82001000);
	else
		osd_header[0] = SWAP32(0x00001000 | (pinfo->color_mode << 24));

	tmp_value = min(pinfo->height, disp_dev->out_res.height) << 16 |
		min(pinfo->width, disp_dev->out_res.width) << 0;
	//osd_header[1] = SWAP32(tmp_value);
	osd_header[1] = be32_to_cpu(tmp_value);
	osd_header[2] = 0x00000000;
	osd_header[3] = 0x00000000;
	osd_header[4] = 0x00000000;
	if (pinfo->color_mode == SP7350_OSD_COLOR_MODE_YUY2)
		osd_header[5] = SWAP32(0x00040000 | pinfo->width);
	else
		osd_header[5] = SWAP32(0x00010000 | pinfo->width);
	osd_header[6] = SWAP32(0xFFFFFFE0);
	osd_header[7] = SWAP32(pinfo->buf_addr);

	/*
	 * update sp7350 osd layer register
	 */
	value = OSD_CTRL_COLOR_MODE_RGB
		| OSD_CTRL_CLUT_FMT_ARGB
		| OSD_CTRL_LATCH_EN
		| OSD_CTRL_A32B32_EN
		| OSD_CTRL_FIFO_DEPTH;
	writel(value, disp_dev->base + OSD_CTRL);
	writel(sp_osd_header_phy, disp_dev->base + OSD_BASE_ADDR);
	writel(0, disp_dev->base + OSD_HVLD_OFFSET);
	writel(0, disp_dev->base + OSD_VVLD_OFFSET);
	writel(disp_dev->out_res.width, disp_dev->base + OSD_HVLD_WIDTH);
	writel(disp_dev->out_res.height, disp_dev->base + OSD_VVLD_HEIGHT);
	writel(0, disp_dev->base + OSD_BIST_CTRL);
	writel(0, disp_dev->base + OSD_3D_H_OFFSET);
	writel(0, disp_dev->base + OSD_SRC_DECIMATION_SEL);
	writel(1, disp_dev->base + OSD_EN);

	//GPOST bypass
	writel(0, disp_dev->base + GPOST_CONFIG);
	writel(0, disp_dev->base + GPOST_MASTER_EN);
	writel(0x8010, disp_dev->base + GPOST_BG1);
	writel(0x0080, disp_dev->base + GPOST_BG2);

	//GPOST PQ disable
	writel(0, disp_dev->base + GPOST_CONTRAST_CONFIG);

	return 0;

}
EXPORT_SYMBOL(sp7350_osd_region_create);

void sp7350_osd_region_release(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;

	if (!sp_osd_header_vir || !sp_osd_region_vir)
		return;

	dma_free_coherent(disp_dev->pdev,
			sizeof(struct sp7350_osd_header) + 1024,
			sp_osd_header_vir,
			sp_osd_header_phy);

	dma_free_coherent(disp_dev->pdev,
			sizeof(struct sp7350_osd_region),
			sp_osd_region_vir,
			sp_osd_region_phy);
}
EXPORT_SYMBOL(sp7350_osd_region_release);

void sp7350_osd_region_update(void)
{
	struct sp7350_osd_region *region = sp_osd_region_vir;
	struct sp_disp_device *disp_dev = gdisp_dev;
	struct sp7350_osd_header *osd_hdr;
	u32 region_ofs;

	if (!region)
		return;

	spin_lock(&disp_dev->osd_lock);

	osd_hdr = (struct sp7350_osd_header *)region->hdr;

	if (osd_hdr && (region->dirty_flag & REGION_ADDR_DIRTY)) {
		region->dirty_flag &= ~REGION_ADDR_DIRTY;

		region_ofs = (u32)((u32)region->buf_addr_phy +
			region->buf_size * (region->buf_cur_id & 0xf));
		osd_hdr->osd_header[7] = SWAP32(region_ofs);

		if (region->pal)
			memcpy(region->hdr_pal, region->pal, 256 * 4);
	}

	if (disp_dev->osd_fe_protect) {
		disp_dev->osd_fe_protect &= ~(1 << SP7350_LAYER_OSD0);
		wake_up_interruptible(&disp_dev->osd_wait);
	}

	spin_unlock(&disp_dev->osd_lock);
}

void sp7350_osd_region_wait_sync(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	struct sp7350_osd_region *region = sp_osd_region_vir;

	if (!region)
		return;

	if (region->dirty_flag & REGION_ADDR_DIRTY)
		disp_dev->osd_fe_protect |= 1 << SP7350_LAYER_OSD0;

	wait_event_interruptible_timeout(disp_dev->osd_wait,
					!disp_dev->osd_fe_protect,
					msecs_to_jiffies(50));
}
EXPORT_SYMBOL(sp7350_osd_region_wait_sync);

u32 sp7350_osd_region_buf_fetch_done(u32 buf_id)
{
	struct sp7350_osd_region *region = sp_osd_region_vir;

	if (!region)
		return 0;

	region->dirty_flag |= REGION_ADDR_DIRTY;
	region->buf_cur_id = buf_id;

	return 0;
}
EXPORT_SYMBOL(sp7350_osd_region_buf_fetch_done);

void sp7350_osd_region_irq_disable(void)
{
	sp7350_disp_state = 1;

}
EXPORT_SYMBOL(sp7350_osd_region_irq_disable);

void sp7350_osd_region_irq_enable(void)
{
	sp7350_disp_state = 0;

}
EXPORT_SYMBOL(sp7350_osd_region_irq_enable);

void sp7350_osd_store(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 i, j;

	for(j = 0; j < 32 ; j++) {
		for(i = 0; i < SP_DISP_MAX_OSD_LAYER ; i++) {
			disp_dev->tmp_osd[i].reg[j] =
				readl(disp_dev->base + DISP_OSD_REG + (i << 7) + j * 4);
			disp_dev->tmp_gpost[i].reg[j] =
				readl(disp_dev->base + DISP_GPOST_REG + (i << 7) + j * 4);
		}
	}
}

void sp7350_osd_restore(void)
{
	struct sp_disp_device *disp_dev = gdisp_dev;
	u32 i;

	for(i = 0; i < SP_DISP_MAX_OSD_LAYER ; i++) {
		writel(disp_dev->tmp_gpost[i].reg[0], disp_dev->base + (i << 7) + GPOST_CONFIG);
		writel(disp_dev->tmp_gpost[i].reg[18], disp_dev->base + (i << 7) + GPOST_MASTER_EN);
		writel(disp_dev->tmp_gpost[i].reg[5], disp_dev->base + (i << 7) + GPOST_BG1);
		writel(disp_dev->tmp_gpost[i].reg[6], disp_dev->base + (i << 7) + GPOST_BG2);
		writel(disp_dev->tmp_gpost[i].reg[7], disp_dev->base + (i << 7) + GPOST_CONTRAST_CONFIG);

		writel(disp_dev->tmp_osd[i].reg[0], disp_dev->base + (i << 7) + OSD_CTRL);
		writel(disp_dev->tmp_osd[i].reg[2], disp_dev->base + (i << 7) + OSD_BASE_ADDR);
		writel(disp_dev->tmp_osd[i].reg[16], disp_dev->base + (i << 7) + OSD_HVLD_OFFSET);
		writel(disp_dev->tmp_osd[i].reg[17], disp_dev->base + (i << 7) + OSD_HVLD_WIDTH);
		writel(disp_dev->tmp_osd[i].reg[18], disp_dev->base + (i << 7) + OSD_VVLD_OFFSET);
		writel(disp_dev->tmp_osd[i].reg[19], disp_dev->base + (i << 7) + OSD_VVLD_HEIGHT);
		writel(disp_dev->tmp_osd[i].reg[21], disp_dev->base + (i << 7) + OSD_BIST_CTRL);
		writel(disp_dev->tmp_osd[i].reg[1], disp_dev->base + (i << 7) + OSD_EN);
	}

}
