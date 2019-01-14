#ifndef SPPCTL_H
#define SPPCTL_H

#define MNAME "sppctl"
#define M_LIC "GPL v2"
#define M_AUT "Dvorkin Dmitry dvorkin@tibbo.com"
#define M_NAM "SunPlus PinCtl module"
#define M_ORG "Tibbo Tech."
#define M_CPR "(C) 2019-2019"

#define FW_DEFNAME "sppctl.bin"

#include "sppctl_syshdrs.h"
#include "sppctl_defs.h"

#include "sppctl_procfs.h"
#include "sppctl_sysfs.h"

#include <mach/io_map.h>

//#define MOON_REG_BASE 0x9C000000
//#define MOON_REG_N(n) 0x80*(n)+MOON_REG_BASE
#define MOON_REG_N(n) PA_IOB_ADDR(0x80*(n))

void sppctl_pin_set( sppctl_pdata_t *_p, uint8_t _pin, uint8_t _fun);
uint8_t sppctl_fun_get( sppctl_pdata_t *_p, uint8_t _pin);
void sppctl_loadfw( struct device *_dev, const char *_fwname);

extern char *list_funcs[];
extern const size_t sizeof_listF;


#endif // SPPCTL_H
