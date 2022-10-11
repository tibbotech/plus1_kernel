obj-$(CONFIG_VIDEO_ESP876) += esp876.o

ifdef CONFIG_VIDEO_CAMERA
KBUILD_CFLAGS += -DESP876_FOR_NVIDIA_TX2
esp876-y := esp876_core_tx2.o esp876_api.o
else
esp876-y := esp876_core.o esp876_api.o
endif
