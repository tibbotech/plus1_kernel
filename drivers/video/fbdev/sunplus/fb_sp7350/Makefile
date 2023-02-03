# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the SUNPLUS SP7350 SoC framebuffer driver
#
obj-$(CONFIG_FB_SP7350) += sp7350fb.o
sp7350fb-objs := sp7350fb_main.o
ifdef CONFIG_FB_SP7350_DEBUG
sp7350fb-objs += sp7350fb_debug.o
endif
