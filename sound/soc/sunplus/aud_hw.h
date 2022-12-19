#ifndef __AUD_HW_H__
#define __AUD_HW_H__

#if IS_ENABLED(CONFIG_SND_SOC_AUD628)
#include "spsoc_util.h"
#elif defined(CONFIG_SND_SOC_AUD645)
#include "spsoc_util-645.h"
#endif

#define PLLA_FRE 147456000
#define DPLL_FRE 45158400

void AUDHW_pin_mx(void);
void AUDHW_Mixer_Setting(void *auddrvdata);
void AUDHW_SystemInit(void *auddrvdata);
void snd_aud_config(void *auddrvdata);
#endif

