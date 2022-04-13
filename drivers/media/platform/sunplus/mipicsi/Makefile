# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Sunplus MIPI/CSI-RX drivers.
#
sp-vin-objs = sp-core.o sp-dma.o sp-v4l2.o

obj-$(CONFIG_MIPICSI_RX_SUNPLUS_SP7021)     += sp-mipi-sp7021.o sp-videobuf2-dma-contig.o
obj-$(CONFIG_MIPICSI_RX_SUNPLUS_SP7021_NEW) += sp-mipi-sp7021n.o
obj-$(CONFIG_MIPICSI_RX_SUNPLUS_Q645)       += sp-mipi-q645.o
obj-$(CONFIG_VIDEO_SUNPLUS_Q654_CSI2)       += sp-csi2.o
obj-$(CONFIG_VIDEO_SUNPLUS_Q654_VIN)        += sp-vin.o
