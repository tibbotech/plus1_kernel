SUPPORT_CHECK_CODING_STYLE=0

#check linux kernel coding style start
ifeq ($(SUPPORT_CHECK_CODING_STYLE), 1)
CHECK_IGNORE_FILES = $(src)/pwm-sc7021.h
CHECK_SCRIPT_CMD := $(srctree)/scripts/checkpatch.pl -q -f
CHECK_STYLE = $(shell $(CHECK_SCRIPT_CMD) $(1) 1>/dev/null 2>&1; echo $$?)
#$(foreach i, $(filter-out $(wildcard $(src)/*.mod.c) $(CHECK_IGNORE_FILES), $(wildcard $(src)/*.c $(src)/*.h)), $(if $(filter-out $(call CHECK_STYLE, $i), 0),$(error $(CHECK_SCRIPT_CMD) $(srctree)/$i), ))
$(foreach i, $(filter-out $(wildcard $(src)/*.mod.c) $(CHECK_IGNORE_FILES), $(wildcard $(src)/*.c $(src)/*.h)), $(if $(filter-out $(call CHECK_STYLE, $i), 0),$(error coding style error: $i), ))
endif
#check linux kernel coding style end

obj-$(CONFIG_PWM_SC7021)	+= pwm-sc7021.o

