
#ifndef _DT_BINDINGS_PINCTRL_SC7021_H
#define _DT_BINDINGS_PINCTRL_SC7021_H

#define IOP_G_MASTE 0x01<<0
#define IOP_G_FIRST 0x01<<1

#define SC7021_PCTL_G_PMUX (       0x00|IOP_G_MASTE)
#define SC7021_PCTL_G_GPIO (IOP_G_FIRST|IOP_G_MASTE)
#define SC7021_PCTL_G_IOPP (IOP_G_FIRST|0x00)

#define SC7021_PCTL_L_OUT 0x01<<0
#define SC7021_PCTL_L_OU1 0x01<<1
#define SC7021_PCTL_L_INV 0x01<<2
#define SC7021_PCTL_L_ODR 0x01<<3

#define SC7021_PCTLE_P(v) ((v)<<24)
#define SC7021_PCTLE_G(v) ((v)<<16)
#define SC7021_PCTLE_F(v) ((v)<<8)
#define SC7021_PCTLE_L(v) ((v)<<0)

#define SC7021_PCTLD_P(v) (((v)>>24) & 0xFF)
#define SC7021_PCTLD_G(v) (((v)>>16) & 0xFF)
#define SC7021_PCTLD_F(v) (((v) >>8) & 0xFF)
#define SC7021_PCTLD_L(v) (((v) >>0) & 0xFF)

/* pack into 32-bit value :
  pin#{8bit} . iop{8bit} . function{8bit} . flags{8bit}
 */
#define SC7021_IOPAD(pin,iop,fun,fls) (((pin)<<24)|((iop)<<16)|((fun)<<8)|(fls))

#endif
