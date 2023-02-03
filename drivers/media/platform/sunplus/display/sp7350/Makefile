# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the sunplus sp7350 video device drivers.
#
obj-$(CONFIG_VIDEO_SP7350_DISP) += display_sp7350.o
display_sp7350-objs := sp7350_display.o
display_sp7350-objs += sp7350_disp_tcon.o
display_sp7350-objs += sp7350_disp_mipitx.o
display_sp7350-objs += sp7350_disp_dmix.o
display_sp7350-objs += sp7350_disp_tgen.o
display_sp7350-objs += sp7350_disp_osd.o
display_sp7350-objs += sp7350_disp_vpp.o
display_sp7350-objs += sp7350_disp_v4l2.o
ifdef CONFIG_VIDEO_SP7350_DISP_DEBUG
display_sp7350-objs += sp7350_disp_debug.o
endif
