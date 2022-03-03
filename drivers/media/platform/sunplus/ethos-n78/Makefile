# SPDX-License-Identifier: GPL-2.0

# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_driver.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_device.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_core.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_buffer.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_dma.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_dma_carveout.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_dma_iommu.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_log.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_network.o
# obj-$(CONFIG_ETHOS_N78_NPU) += ethosn_smc.o

#ccflags-y += -Wall -g -DDEBUG -DETHOSN_NS
#ccflags-y += -Wall -g -Werror -DDEBUG -DETHOSN_NS
#ccflags-y += -Wall -g -Werror -DDEBUG $(EXTRA_CCFLAGS)
#ccflags-y += -Wall -g -Werror -DDEBUG
ccflags-y += -Wall -Werror $(EXTRA_CCFLAGS)


obj-$(CONFIG_ETHOS_N78_NPU) += ethosn.o
#obj-$(CONFIG_ETHOSN_TEST) += ../internal/kernel-module/tests/ethosn-tests.o
ethosn-objs := ethosn_core.o \
             ethosn_backport.o \
             ethosn_buffer.o \
             ethosn_debug.o \
             ethosn_device.o \
             ethosn_dma.o \
             ethosn_dma_carveout.o \
             ethosn_dma_iommu.o \
             ethosn_driver.o \
             ethosn_log.o \
             ethosn_network.o \
             ethosn_smc.o
