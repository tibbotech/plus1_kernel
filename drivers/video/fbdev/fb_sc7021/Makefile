#
# Makefile for the SC7021 framebuffer driver
#
SUPPORT_CHECK_CODING_STYLE=1

#check linux kernel coding style start
ifeq ($(SUPPORT_CHECK_CODING_STYLE), 1)
CHECK_SCRIPT_CMD := $(srctree)/scripts/checkpatch.pl -q -f
CHECK_STYLE = $(shell $(CHECK_SCRIPT_CMD) $(1) 1>/dev/null 2>&1; echo $$?)
$(foreach i, $(filter-out $(wildcard $(src)/*.mod.c), $(wildcard $(src)/*.c $(src)/*.h)), $(if $(filter-out $(call CHECK_STYLE, $i), 0),$(error $(CHECK_SCRIPT_CMD) $(srctree)/$i), ))
#$(foreach i, $(filter-out $(wildcard $(src)/*.mod.c), $(wildcard $(src)/*.c $(src)/*.h)), $(if $(filter-out $(call CHECK_STYLE, $i), 0),$(error coding style error: $i), ))
endif
#check linux kernel coding style end

ccflags-y += -Werror
ccflags-y += -Wformat-security

obj-$(CONFIG_FB_SC7021) += fb_sc7021.o
fb_sc7021-objs := fb_sc7021_main.o

ifdef CONFIG_FB_SC7021_DEBUG
	fb_sc7021-objs += fb_sc7021_debug.o
endif

