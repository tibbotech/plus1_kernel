obj-$(CONFIG_VIDEO_EYS3D) += eys3d.o

ifdef CONFIG_VIDEO_CAMERA
KBUILD_CFLAGS += -DEYS3D_FOR_NVIDIA_TX2
eys3d-y := eys3d_core_tx2.o eys3d_api.o eys3d_esp876.o eys3d_p2c.o
else
eys3d-y := eys3d_core.o eys3d_api.o eys3d_esp876.o eys3d_p2c.o
endif
