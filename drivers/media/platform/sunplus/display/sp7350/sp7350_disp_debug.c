// SPDX-License-Identifier: GPL-2.0
/*
 * Sunplus SP7350 SoC Display function debug
 *
 * Author: Hammer Hsieh <hammer.hsieh@sunplus.com>
 */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "sp7350_display.h"
#include "sp7350_disp_regs.h"

static char *spmon_skipspace(char *p)
{
	int i = 256;

	if (p == NULL)
		return NULL;
	while (i) {
	//while (1) {
		int c = p[0];

		i--;
		if (c == ' ' || c == '\t' || c == '\v')
			p++;
		else
			break;
	}
	return p;
}

#if 0
static char *spmon_readint(char *p, int *x)
{
	int base = 10;
	int cnt, retval;

	if (x == NULL)
		return p;

	*x = 0;

	if (p == NULL)
		return NULL;

	p = spmon_skipspace(p);

	if (p[0] == '0' && p[1] == 'x') {
		base = 16;
		p += 2;
	}
	if (p[0] == '0' && p[1] == 'o') {
		base = 8;
		p += 2;
	}
	if (p[0] == '0' && p[1] == 'b') {
		base = 2;
		p += 2;
	}

	retval = 0;
	for (cnt = 0; 1; cnt++) {
		int c = *p++;
		int val;

		// translate 0~9, a~z, A~Z
		if (c >= '0' && c <= '9')
			val = c - '0';
		else if (c >= 'a' && c <= 'z')
			val = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			val = c - 'A' + 10;
		else
			break;
		if (val >= base)
			break;

		retval = retval * base + val;
	}

	if (cnt == 0)
		return p;			// no translation is done??

	*(unsigned int *) x = retval;		// store it
	return p;
}
#endif

/* sunplus_debug_cmd
 *
 *  <Resolution check>
 *    echo "dispmon reso" > /proc/disp_mon
 *
 *  <bist output check>
 *    echo "dispmon bist info" > /proc/disp_mon
 *    echo "dispmon bist img off" > /proc/disp_mon
 *    echo "dispmon bist img bar" > /proc/disp_mon
 *    echo "dispmon bist img bor" > /proc/disp_mon
 *    echo "dispmon bist vscl off" > /proc/disp_mon
 *    echo "dispmon bist vscl bar" > /proc/disp_mon
 *    echo "dispmon bist vscl bor" > /proc/disp_mon
 *    echo "dispmon bist osd0 off" > /proc/disp_mon
 *    echo "dispmon bist osd0 bar" > /proc/disp_mon
 *    echo "dispmon bist osd0 bor" > /proc/disp_mon
 *    echo "dispmon bist dmix region" > /proc/disp_mon
 *    echo "dispmon bist dmix bar" > /proc/disp_mon
 *    echo "dispmon bist dmix barv" > /proc/disp_mon
 *    echo "dispmon bist dmix bor" > /proc/disp_mon
 *    echo "dispmon bist dmix bor black" > /proc/disp_mon
 *    echo "dispmon bist dmix bor red" > /proc/disp_mon
 *    echo "dispmon bist dmix bor green" > /proc/disp_mon
 *    echo "dispmon bist dmix bor blue" > /proc/disp_mon
 *    echo "dispmon bist dmix bor0" > /proc/disp_mon
 *    echo "dispmon bist dmix bor1" > /proc/disp_mon
 *    echo "dispmon bist dmix snow" > /proc/disp_mon
 *    echo "dispmon bist dmix snowhalf" > /proc/disp_mon
 *    echo "dispmon bist dmix snowmax" > /proc/disp_mon
 *
 *  <dmix layer check/set>
 *    echo "dispmon layer info" > /proc/disp_mon
 *    echo "dispmon layer vpp0 blend" > /proc/disp_mon
 *    echo "dispmon layer vpp0 trans" > /proc/disp_mon
 *    echo "dispmon layer vpp0 opaci" > /proc/disp_mon
 *    echo "dispmon layer osd0 blend" > /proc/disp_mon
 *    echo "dispmon layer osd0 trans" > /proc/disp_mon
 *    echo "dispmon layer osd0 opaci" > /proc/disp_mon
 *
 *  <dump each block info>
 *    echo "dispmon dump vpp" > /proc/disp_mon
 *    echo "dispmon dump osd" > /proc/disp_mon
 *    echo "dispmon dump tgen" > /proc/disp_mon
 *    echo "dispmon dump dmix" > /proc/disp_mon
 *    echo "dispmon dump tcon" > /proc/disp_mon
 *    echo "dispmon dump mipitx" > /proc/disp_mon
 *    echo "dispmon dump all" > /proc/disp_mon
 *
 */
static void sunplus_debug_cmd(char *tmpbuf)
{

	tmpbuf = spmon_skipspace(tmpbuf);

	pr_info("run disp debug cmd\n");

	if (!strncasecmp(tmpbuf, "bist", 4)) {
		tmpbuf = spmon_skipspace(tmpbuf + 4);
		pr_info("bist all info\n");
		if (!strncasecmp(tmpbuf, "info", 4)) {
			sp7350_vpp_bist_info();
			sp7350_osd_bist_info();
			sp7350_dmix_bist_info();
			sp7350_tcon_bist_info();
		} else if (!strncasecmp(tmpbuf, "img", 3)) {
			tmpbuf = spmon_skipspace(tmpbuf + 3);
			pr_info("bist vpp0 imgread cmd\n");
			sp7350_vpp_layer_onoff(1);
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist vpp0 imgread off\n");
				sp7350_vpp_bist_set(0, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist vpp0 imgread bar\n");
				sp7350_vpp_bist_set(0, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist vpp0 imgread bor\n");
				sp7350_vpp_bist_set(0, 1, 1);
			} else
				pr_info("bist vpp0 imgread cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "vscl", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist vpp0 vscl cmd\n");
			sp7350_vpp_layer_onoff(1);
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist vpp0 vscl off\n");
				sp7350_vpp_bist_set(1, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist vpp0 vscl bar\n");
				sp7350_vpp_bist_set(1, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist vpp0 vscl bor\n");
				sp7350_vpp_bist_set(1, 1, 1);
			} else
				pr_info("bist vpp0 vscl cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd0", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist osd0 cmd\n");
			sp7350_osd_layer_onoff(SP7350_LAYER_OSD0, 1); //OSD0 on
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_BLENDING);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist osd0 off\n");
				sp7350_osd_bist_set(0, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist osd0 bar\n");
				sp7350_osd_bist_set(0, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist osd0 bor\n");
				sp7350_osd_bist_set(0, 1, 1);
			} else
				pr_info("bist osd0 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd1", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist osd1 cmd\n");
			sp7350_osd_layer_onoff(SP7350_LAYER_OSD1, 1); //OSD1 on
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_BLENDING);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist osd1 off\n");
				sp7350_osd_bist_set(1, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist osd1 bar\n");
				sp7350_osd_bist_set(1, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist osd1 bor\n");
				sp7350_osd_bist_set(1, 1, 1);
			} else
				pr_info("bist osd1 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd2", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist osd2 cmd\n");
			sp7350_osd_layer_onoff(SP7350_LAYER_OSD2, 1); //OSD2 on
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_BLENDING);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist osd2 off\n");
				sp7350_osd_bist_set(2, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist osd2 bar\n");
				sp7350_osd_bist_set(2, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist osd2 bor\n");
				sp7350_osd_bist_set(2, 1, 1);
			} else
				pr_info("bist osd2 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd3", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist osd3 cmd\n");
			sp7350_osd_layer_onoff(SP7350_LAYER_OSD3, 1); //OSD3 on
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_BLENDING);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist osd3 off\n");
				sp7350_osd_bist_set(3, 0, 0);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist osd3 bar\n");
				sp7350_osd_bist_set(3, 1, 0);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				pr_info("bist osd3 bor\n");
				sp7350_osd_bist_set(3, 1, 1);
			} else
				pr_info("bist osd3 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "dmix", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist dmix cmd\n");
			sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			if (!strncasecmp(tmpbuf, "region", 6)) {
				pr_info("bist dmix region\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_REGION, 0x29f06e);	
			} else if (!strncasecmp(tmpbuf, "snowhalf", 8)) {
				pr_info("bist dmix snowhalf\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_SNOW_HALF, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "snowmax", 7)) {
				pr_info("bist dmix snowmax\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_SNOW_MAX, 0x29f06e);		
			} else if (!strncasecmp(tmpbuf, "snow", 4)) {
				pr_info("bist dmix snow\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_SNOW, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "barv", 4)) {
				pr_info("bist dmix bar rot90\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_COLORBAR_ROT90, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "bar", 3)) {
				pr_info("bist dmix bar rot0\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_COLORBAR_ROT0, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "bor0", 4)) {
				pr_info("bist dmix bor pix0\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER_NONE, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "bor1", 4)) {
				pr_info("bist dmix bor pix1\n");
				sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER_ONE, 0x29f06e);
			} else if (!strncasecmp(tmpbuf, "bor", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				pr_info("bist dmix bor pix7\n");
				if (!strncasecmp(tmpbuf, "black", 5)) {
					pr_info("bist dmix bor black\n");
					sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER, 0x108080); //BG black
				} else if (!strncasecmp(tmpbuf, "red", 3)) {
					pr_info("bist dmix bor red\n");
					sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER, 0x4040f0); //BG red
				} else if (!strncasecmp(tmpbuf, "green", 5)) {
					pr_info("bist dmix bor green\n");
					sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER, 0x101010); //BG green
				} else if (!strncasecmp(tmpbuf, "blue", 4)) {
					pr_info("bist dmix bor blue\n");
					sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER, 0x29f06e); //BG blue
				} else {
					pr_info("bist dmix bor blue(def)\n");
					sp7350_dmix_ptg_set(SP7350_DMIX_BIST_BORDER, 0x29f06e); //BG blue
				}
			} else
				pr_info("bist dmix cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "tcon", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("bist tcon cmd\n");
			if (!strncasecmp(tmpbuf, "off", 3)) {
				pr_info("bist tcon off\n");
				sp7350_tcon_bist_set(0, 0);
			} else if (!strncasecmp(tmpbuf, "hor1", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon H_1_COLORBAR internal\n");
					sp7350_tcon_bist_set(1, 0);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon H_1_COLORBAR external\n");
					sp7350_tcon_bist_set(2, 0);
				} else
					pr_info("bist tcon hor1 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hor2", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon H_2_RAMP internal\n");
					sp7350_tcon_bist_set(1, 1);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon H_2_RAMP external\n");
					sp7350_tcon_bist_set(2, 1);
				} else
					pr_info("bist tcon hor2 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hor3", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon H_3_ODDEVEN internal\n");
					sp7350_tcon_bist_set(1, 2);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon H_3_ODDEVEN external\n");
					sp7350_tcon_bist_set(2, 2);
				} else
					pr_info("bist tcon hor3 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "ver1", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon V_1_COLORBAR internal\n");
					sp7350_tcon_bist_set(1, 3);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon V_1_COLORBAR external\n");
					sp7350_tcon_bist_set(2, 3);
				} else
					pr_info("bist tcon ver1 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "ver2", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon V_2_RAMP internal\n");
					sp7350_tcon_bist_set(1, 4);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon V_2_RAMP external\n");
					sp7350_tcon_bist_set(2, 4);
				} else
					pr_info("bist tcon ver2 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "ver3", 4)) {
				tmpbuf = spmon_skipspace(tmpbuf + 4);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon V_3_ODDEVEN internal\n");
					sp7350_tcon_bist_set(1, 5);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon V_3_ODDEVEN external\n");
					sp7350_tcon_bist_set(2, 5);
				} else
					pr_info("bist tcon ver3 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hv1", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon HV_1_CHECK internal\n");
					sp7350_tcon_bist_set(1, 6);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon HV_1_CHECK external\n");
					sp7350_tcon_bist_set(2, 6);
				} else
					pr_info("bist tcon hv1 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hv2", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon HV_2_FRAME internal\n");
					sp7350_tcon_bist_set(1, 7);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon HV_2_FRAME external\n");
					sp7350_tcon_bist_set(2, 7);
				} else
					pr_info("bist tcon hv2 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hv3", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon HV_3_MOIRE_A internal\n");
					sp7350_tcon_bist_set(1, 8);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon HV_3_MOIRE_A external\n");
					sp7350_tcon_bist_set(2, 8);
				} else
					pr_info("bist tcon hv3 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hv4", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon HV_4_MOIRE_B internal\n");
					sp7350_tcon_bist_set(1, 9);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon HV_4_MOIRE_B external\n");
					sp7350_tcon_bist_set(2, 9);
				} else
					pr_info("bist tcon hv4 cmd undef\n");

			} else if (!strncasecmp(tmpbuf, "hv5", 3)) {
				tmpbuf = spmon_skipspace(tmpbuf + 3);
				if (!strncasecmp(tmpbuf, "inter", 5)) {
					pr_info("bist tcon HV_5_CONTRAST internal\n");
					sp7350_tcon_bist_set(1, 10);
				} else if (!strncasecmp(tmpbuf, "exter", 5)) {
					pr_info("bist tcon HV_5_CONTRAST external\n");
					sp7350_tcon_bist_set(2, 10);
				} else
					pr_info("bist tcon hv5 cmd undef\n");

			} else
				pr_info("bist tcon cmd undef\n");

		}
	} else if (!strncasecmp(tmpbuf, "layer", 5)) {
		tmpbuf = spmon_skipspace(tmpbuf + 5);

		if (!strncasecmp(tmpbuf, "info", 4)) {
			pr_info("layer info cmd\n");
			sp7350_dmix_all_layer_info();
		} else if (!strncasecmp(tmpbuf, "vpp0", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("layer vpp0 cmd\n");
			if (!strncasecmp(tmpbuf, "blend", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_BLENDING);
			} else if (!strncasecmp(tmpbuf, "trans", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_TRANSPARENT);
			} else if (!strncasecmp(tmpbuf, "opaci", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L1, SP7350_DMIX_VPP0, SP7350_DMIX_OPACITY);
			} else
				pr_info("layer vpp0 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd0", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("layer osd0 cmd\n");
			if (!strncasecmp(tmpbuf, "blend", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_BLENDING);
			} else if (!strncasecmp(tmpbuf, "trans", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_TRANSPARENT);
			} else if (!strncasecmp(tmpbuf, "opaci", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L6, SP7350_DMIX_OSD0, SP7350_DMIX_OPACITY);
			} else
				pr_info("layer osd0 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd1", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("layer osd1 cmd\n");
			if (!strncasecmp(tmpbuf, "blend", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_BLENDING);
			} else if (!strncasecmp(tmpbuf, "trans", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_TRANSPARENT);
			} else if (!strncasecmp(tmpbuf, "opaci", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L5, SP7350_DMIX_OSD1, SP7350_DMIX_OPACITY);
			} else
				pr_info("layer osd1 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd2", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("layer osd2 cmd\n");
			if (!strncasecmp(tmpbuf, "blend", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_BLENDING);
			} else if (!strncasecmp(tmpbuf, "trans", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_TRANSPARENT);
			} else if (!strncasecmp(tmpbuf, "opaci", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L4, SP7350_DMIX_OSD2, SP7350_DMIX_OPACITY);
			} else
				pr_info("layer osd2 cmd undef\n");

		} else if (!strncasecmp(tmpbuf, "osd3", 4)) {
			tmpbuf = spmon_skipspace(tmpbuf + 4);
			pr_info("layer osd3 cmd\n");
			if (!strncasecmp(tmpbuf, "blend", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_BLENDING);
			} else if (!strncasecmp(tmpbuf, "trans", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_TRANSPARENT);
			} else if (!strncasecmp(tmpbuf, "opaci", 5)) {
				sp7350_dmix_layer_init(SP7350_DMIX_L3, SP7350_DMIX_OSD3, SP7350_DMIX_OPACITY);
			} else
				pr_info("layer osd3 cmd undef\n");

		} else
			pr_info("layer cmd undef\n");

	} else if (!strncasecmp(tmpbuf, "dump", 4)) {
		tmpbuf = spmon_skipspace(tmpbuf + 4);

		if (!strncasecmp(tmpbuf, "vpp", 3)) {
			pr_info("dump vpp path info\n");
			sp7350_vpp_decrypt_info();
		} else if (!strncasecmp(tmpbuf, "osdh", 4)) {
			pr_info("dump osd header info\n");
			sp7350_osd_header_show();
		} else if (!strncasecmp(tmpbuf, "osd", 3)) {
			pr_info("dump osd path info\n");
			sp7350_osd_decrypt_info();
		} else if (!strncasecmp(tmpbuf, "tgen", 4)) {
			pr_info("dump tgen info\n");
			sp7350_tgen_decrypt_info();
		} else if (!strncasecmp(tmpbuf, "dmix", 4)) {
			pr_info("dump dmix info\n");
			sp7350_dmix_decrypt_info();
		} else if (!strncasecmp(tmpbuf, "tcon", 4)) {
			pr_info("dump tcon info\n");
			sp7350_tcon_decrypt_info();
		} else if (!strncasecmp(tmpbuf, "mipitx", 6)) {
			pr_info("dump mipitx info\n");
			sp7350_mipitx_decrypt_info();
			sp7350_mipitx_pllclk_get();
			sp7350_mipitx_txpll_get();
		} else if (!strncasecmp(tmpbuf, "all", 3)) {
			pr_info("dump all info\n");
			sp7350_vpp_decrypt_info();
			sp7350_osd_decrypt_info();
			sp7350_tgen_decrypt_info();
			sp7350_dmix_decrypt_info();
			sp7350_tcon_decrypt_info();
			sp7350_mipitx_decrypt_info();
			sp7350_mipitx_pllclk_get();
			sp7350_mipitx_txpll_get();
		} else if (!strncasecmp(tmpbuf, "reso", 4)) {
			pr_info("dump resolution\n");
			sp7350_vpp_imgread_resolution_chk();
			sp7350_vpp_vscl_resolution_chk();
			sp7350_osd_resolution_chk();
			sp7350_tgen_resolution_chk();
			sp7350_tcon_resolution_chk();
			sp7350_mipitx_resolution_chk();
			sp7350_mipitx_pllclk_get();
			sp7350_mipitx_txpll_get();
		} else if (!strncasecmp(tmpbuf, "timing", 6)) {
			pr_info("dump timing\n");
			sp7350_tgen_timing_get();
			sp7350_tcon_timing_get();
			sp7350_mipitx_timing_get();
			sp7350_mipitx_pllclk_get();
			sp7350_mipitx_txpll_get();
		}

	} else
		pr_err("unknown command:%s\n", tmpbuf);

	(void)tmpbuf;
}

int sunplus_proc_open_show(struct seq_file *m, void *v)
{
	//pr_info("%s\n", __func__);
	//pr_info("SP7350 DISPLAY DEBUG TEST\n");

	return 0;
}

int sunplus_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sunplus_proc_open_show, NULL);
}

static ssize_t sunplus_proc_write(struct file *filp, const char __user *buffer,
				size_t len, loff_t *f_pos)
{
	char pbuf[256];
	char *tmpbuf = pbuf;

	if (len > sizeof(pbuf)) {
		pr_err("intput error len:%d!\n", (int)len);
		return -ENOSPC;
	}

	if (copy_from_user(tmpbuf, buffer, len)) {
		pr_err("intput error!\n");
		return -EFAULT;
	}

	if (len == 0)
		tmpbuf[len] = '\0';
	else
		tmpbuf[len - 1] = '\0';

	if (!strncasecmp(tmpbuf, "dispmon", 7))
		sunplus_debug_cmd(tmpbuf + 7);

	return len;
}

//static const struct proc_ops sp_disp_proc_ops = {
const struct proc_ops sp_disp_proc_ops = {
	.proc_open = sunplus_proc_open,
	.proc_write = sunplus_proc_write,
	.proc_read = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int set_debug_cmd(const char *val, const struct kernel_param *kp)
{
	sunplus_debug_cmd((char *)val);

	return 0;
}

static struct kernel_param_ops debug_param_ops = {
	.set = set_debug_cmd,
};
module_param_cb(debug, &debug_param_ops, NULL, 0644);
MODULE_PARM_DESC(debug, "SP7350 display debug test");

/*
 * sp7350 debug fs reference
 *
 * /sys/kernel/debug/f8005c80.display/
 *   -- sp7350_dump_regs
 *   -- /regs/vpp
 *
 */
static int sp7350_debug_dump_regs(struct sp_disp_device *disp_dev,
				  struct seq_file *m)
{
	//pr_info("%s test\n", __func__);

	return 0;
}

static int sp7350_dump_regs_show(struct seq_file *m, void *p)
{
	struct sp_disp_device *disp_dev = m->private;

	return sp7350_debug_dump_regs(disp_dev, m);
}
DEFINE_SHOW_ATTRIBUTE(sp7350_dump_regs);

static int sp7350_debug_dump_vpp_regs(struct sp_disp_device *disp_dev,
				  struct seq_file *m)
{
	//pr_info("%s test\n", __func__);

	return 0;
}

static int sp7350_dump_vpp_regs_show(struct seq_file *m, void *p)
{
	struct sp_disp_device *disp_dev = m->private;

	return sp7350_debug_dump_vpp_regs(disp_dev, m);
}
DEFINE_SHOW_ATTRIBUTE(sp7350_dump_vpp_regs);

void sp7350_debug_init(struct sp_disp_device *disp_dev)
{
	struct sp7350_debug *debug = &disp_dev->debug;
	struct dentry *regs_dir;

	debug->debugfs_dir = debugfs_create_dir(dev_name(disp_dev->pdev), NULL);

	debugfs_create_file("sp7350_dump_regs", 0444, debug->debugfs_dir, disp_dev,
			    &sp7350_dump_regs_fops);

	regs_dir = debugfs_create_dir("regs", debug->debugfs_dir);

	debugfs_create_file("vpp", 0444, regs_dir, disp_dev,
			    &sp7350_dump_vpp_regs_fops);

}

void sp7350_debug_cleanup(struct sp_disp_device *disp_dev)
{
	debugfs_remove_recursive(disp_dev->debug.debugfs_dir);
}
