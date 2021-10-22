ifeq ($(CONFIG_SND_SOC_AUD628), y)
snd-soc-aud3502-objs := aud_codec.o
snd-soc-spsoc-tdm-objs := spsoc-tdm.o
snd-soc-spsoc-pdm-objs := spsoc-pdm.o
snd-soc-spsoc_pcm-objs := spsoc_pcm.o
snd-soc-spsoc_aud3502-objs := spsoc_aud.o
snd-soc-spsoc_util-objs := spsoc_util.o
snd-soc-aud_hw-objs := aud_hw.o
snd-soc-spdif-objs := spdif.o
snd_soc_audio-objs := sunplus_auido.o

obj-$(CONFIG_SND_SOC_AUD628)	+= snd_soc_audio.o
obj-$(CONFIG_SND_SOC_AUD628)	+= snd-soc-aud3502.o


obj-y	+= snd-soc-spsoc_pcm.o
obj-y	+= cpu_dai.o
obj-y	+= snd-soc-spsoc-tdm.o
obj-y	+= snd-soc-spsoc-pdm.o
obj-y	+= snd-soc-spsoc_aud3502.o
obj-y	+= snd-soc-spsoc_util.o
obj-y	+= snd-soc-aud_hw.o
obj-y	+= snd-soc-spdif.o
endif

ifeq ($(CONFIG_SND_SOC_AUD645), y)
snd-soc-aud3502-objs := aud_codec-645.o
snd-soc-spsoc-tdm-objs := spsoc_tdm-645.o
snd-soc-spsoc_pcm-objs := spsoc_pcm-645.o
snd-soc-spsoc_aud3502-objs := spsoc_aud-645.o
snd-soc-spsoc_util-objs := spsoc_util.o
snd-soc-aud_hw-objs := aud_hw.o
snd-soc-spdif-objs := spdif.o
snd_soc_audio-objs := sunplus_auido.o

obj-$(CONFIG_SND_SOC_AUD645)	+= snd_soc_audio.o
obj-$(CONFIG_SND_SOC_AUD645)	+= snd-soc-aud3502.o


obj-y				+= snd-soc-spsoc_pcm.o
obj-y				+= cpu_dai-645.o
obj-y				+= snd-soc-spsoc-tdm.o
obj-y				+= snd-soc-spsoc_aud3502.o
obj-y				+= snd-soc-spsoc_util.o
obj-y				+= snd-soc-aud_hw.o
obj-y				+= snd-soc-spdif.o
endif