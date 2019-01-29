/*
 * GPIO Driver for SunPlus/Tibbo SC7021 controller
 * Copyright (C) 2019 SunPlus Tech./Tibbo Tech.
 * Author: Dvorkin Dmitry <dvorkin@tibbo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * TODO: irq hanling, pinconf (inv), MASTER/FIRST state save
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <asm/io.h>

#include "sc7021_gpio.h"

static int sc7021gpio_f_gdi( struct gpio_chip *_c, unsigned _n);

// (/16)*4
#define R16_ROF(r) (((r)>>4)<<2)
#define R16_BOF(r) ((r)%16)
// (/32)*4
#define R32_ROF(r) (((r)>>5)<<2)
#define R32_BOF(r) ((r)%32)
#define R32_VAL(r,boff) (((r)>>(boff)) & BIT(0))

// --- not in callbacks

// who is first: GPIO(1) | MUX(0)
static int sc7021gpio_u_gfrst( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = readl( pc->base2 + SC7021_GPIO_OFF_GFR + R32_ROF(_n));
 return( R32_VAL(r,R32_BOF(_n)));  }

// who is master: GPIO(1) | IOP(0)
static int sc7021gpio_u_magpi( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = readl( pc->base0 + SC7021_GPIO_OFF_CTL + R16_ROF(_n));
 return( R32_VAL(r,R16_BOF(_n)));  }

// is inv: INVERTED(1) | NORMAL(0)
static int sc7021gpio_u_isinv( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 u16 inv_off = SC7021_GPIO_OFF_IINV;
 if ( sc7021gpio_f_gdi( _c, _n) == 0) inv_off = SC7021_GPIO_OFF_OINV;
 r = readl( pc->base1 + inv_off + R16_ROF(_n));
 return( R32_VAL(r,R16_BOF(_n)) ^ BIT(0));  }

// is open-drain: YES(1) | NON(0)
static int sc7021gpio_u_isodr( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = readl( pc->base1 + SC7021_GPIO_OFF_OD + R16_ROF(_n));
 return( R32_VAL(r,R16_BOF(_n)));  }

// --- in callbacks

// take pin (export/open for ex.): set GPIO_FIRST=1,GPIO_MASTER=1
// FIX: how to prevent gpio to take over the mux if mux is the default?
// FIX: idea: save state of MASTER/FIRST and return back after _fre?
static int sc7021gpio_f_req( struct gpio_chip *_c, unsigned _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 // get GPIO_FIRST:32
 r = readl( pc->base2 + SC7021_GPIO_OFF_GFR + R32_ROF(_n));
 // set GPIO_FIRST(1):32
 r |= BIT(R32_BOF(_n));
 writel( r, pc->base2 + SC7021_GPIO_OFF_GFR + R32_ROF(_n));
 // set GPIO_MASTER(1):m16,v:16
 r = (BIT(R16_BOF(_n))<<16) | BIT(R16_BOF(_n));
 writel( r, pc->base0 + SC7021_GPIO_OFF_CTL + R16_ROF(_n));
 return( 0);  }

// gave pin back: set GPIO_MASTER=0,GPIO_FIRST=0
static void sc7021gpio_f_fre( struct gpio_chip *_c, unsigned _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 // set GPIO_MASTER(1):m16,v:16 - doesn't matter now: gpio mode is default
// r = (BIT(R16_BOF(_n))<<16) | BIT(R16_BOF(_n);
// writel( r, pc->base0 + SC7021_GPIO_OFF_CTL + R16_ROF(_n));
 // get GPIO_FIRST:32
 r = readl( pc->base2 + SC7021_GPIO_OFF_GFR + R32_ROF(_n));
 // set GPIO_FIRST(0):32
 r &= ~BIT(R32_BOF(_n));
 writel( r, pc->base2 + SC7021_GPIO_OFF_GFR + R32_ROF(_n));
 return;  }

// get dir: 0=out, 1=in, -E =err (-EINVAL for ex): OE inverted on ret
static int sc7021gpio_f_gdi( struct gpio_chip *_c, unsigned _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = readl( pc->base0 + SC7021_GPIO_OFF_OE + R16_ROF(_n));
 return( R32_VAL(r,R16_BOF(_n)) ^ BIT(0));  }
 
// set to input: 0:ok: OE=0
static int sc7021gpio_f_sin( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = (BIT(R16_BOF(_n))<<16);
 writel( r, pc->base0 + SC7021_GPIO_OFF_OE + R16_ROF(_n));
 return( 0);  }

// set to output: 0:ok: OE=1,O=_v
static int sc7021gpio_f_sou( struct gpio_chip *_c, unsigned int _n, int _v) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = (BIT(R16_BOF(_n))<<16) | BIT(R16_BOF(_n));
 writel( r, pc->base0 + SC7021_GPIO_OFF_OE + R16_ROF(_n));
 r = (BIT(R16_BOF(_n))<<16) | ( ( _v & BIT(0)) << R16_BOF(_n));
 writel( r, pc->base0 + SC7021_GPIO_OFF_OUT + R16_ROF(_n));
 return( 0);  }

// get value for signal: 0=low | 1=high | -err
static int sc7021gpio_f_get( struct gpio_chip *_c, unsigned int _n) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 if ( sc7021gpio_f_gdi( _c, _n) == 1) {
   r = readl( pc->base0 + SC7021_GPIO_OFF_IN + R32_ROF(_n));
   return( R32_VAL(r,R32_BOF(_n)));  }
 // OUT
 r = readl( pc->base0 + SC7021_GPIO_OFF_OUT + R16_ROF(_n));
 return( R32_VAL(r,R16_BOF(_n)));  }

// OUT only: can't call set on IN pin: protected by gpio_chip layer
static void sc7021gpio_f_set( struct gpio_chip *_c, unsigned int _n, int _v) {
 u32 r;
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 r = (BIT(R16_BOF(_n))<<16) | ( _v & 0x0001) << R16_BOF(_n);
 writel( r, pc->base0 + SC7021_GPIO_OFF_OUT + R16_ROF(_n));
 return;  }

 // FIX: define later
static int sc7021gpio_f_scf( struct gpio_chip *_c, unsigned _n, unsigned long _conf) {
 u32 r;
 enum pin_config_param cp = pinconf_to_config_param( _conf);
 u16 ca = pinconf_to_config_argument( _conf);
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 KDBG( "%s(%03d,%lX) p:%d a:%d\n", __FUNCTION__, _n, _conf, cp, ca);
 switch ( cp) {
   case PIN_CONFIG_DRIVE_OPEN_DRAIN:
          r = (BIT(R16_BOF(_n))<<16) | BIT(R16_BOF(_n));
          writel( r, pc->base1 + SC7021_GPIO_OFF_OD + R16_ROF(_n));
          break;
   case PIN_CONFIG_INPUT_ENABLE:
          KERR( "f_scf(%03d,%lX) input enable arg:%d\n", _n, _conf, ca);
          break;
   case PIN_CONFIG_OUTPUT:
          return( sc7021gpio_f_sou( _c, _n, 0));
          break;
   default:
       KERR( "f_scf(%03d,%lX) unknown pinconf:%d\n", _n, _conf, cp);
       return( -EINVAL);  break;
 }
 return( 0);  }

 // dbg show
#ifdef CONFIG_DEBUG_FS
static void sc7021gpio_f_dsh( struct seq_file *_s, struct gpio_chip *_c) {
 int i;
 const char *label;
 for ( i = 0; i < _c->ngpio; i++) {
   if ( !( label = gpiochip_is_requested( _c, i))) label = "";
   seq_printf( _s, " gpio-%03d (%-16.16s | %-16.16s)", i + _c->base, _c->names[ i], label);
   seq_printf( _s, " %c", sc7021gpio_f_gdi( _c, i) == 0 ? 'O' : 'I');
   seq_printf( _s, ":%d", sc7021gpio_f_get( _c, i));
   seq_printf( _s, " %s", ( sc7021gpio_u_gfrst( _c, i) ? "gpi" : "mux"));
   seq_printf( _s, " %s", ( sc7021gpio_u_magpi( _c, i) ? "gpi" : "iop"));
   seq_printf( _s, " %s", ( sc7021gpio_u_isinv( _c, i) ? "inv" : "   "));
   seq_printf( _s, " %s", ( sc7021gpio_u_isodr( _c, i) ? "oDr" : ""));
   seq_printf( _s, "\n");
 }
 return;  }
#else
#define sc7021gpio_f_dsh NULL
#endif

#ifdef CONFIG_OF_GPIO
static int sc7021gpio_xlate( struct gpio_chip *_c,
        const struct of_phandle_args *_a, u32 *_flags) {
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 KERR( "%s(%lX)\n", __FUNCTION__, *_flags);
 return;  }
#endif

static int sc7021gpio_i_map( struct gpio_chip *_c, unsigned _off) {
 sc7021gpio_chip_t *pc = ( sc7021gpio_chip_t *)gpiochip_get_data( _c);
 if ( _off >= 8 && _off < 15) return( pc->irq[ _off - 8]);
 // immediately after this callback:
 // genirq: Setting trigger mode 3 for irq 120 failed (sp_intc_set_type+0x1/0xd4)
 return( -ENXIO);  }

static irqreturn_t sc7021gpio_f_irq( int _irq, void *_dp) {
 KDBG("%s(%03d)\n", __FUNCTION__, _irq);
 return( IRQ_HANDLED);  }

int sc7021_gpio_resmap( struct platform_device *_pd, sc7021gpio_chip_t *_pc) {
 struct resource *rp;
 if ( IS_ERR( rp = platform_get_resource( _pd, IORESOURCE_MEM, 0))) {
   dev_err( &( _pd->dev), "getres regs#0\n");
   return( PTR_ERR( rp));  }
 KDBG( "%s() 0 res %X-%X\n", __FUNCTION__, rp->start, rp->end);
 if ( IS_ERR( _pc->base0 = devm_ioremap_resource( &( _pd->dev), rp))) {
   KDBG( "mapping regs#0\n");
   return( PTR_ERR( _pc->base0));  }
 if ( IS_ERR( rp = platform_get_resource( _pd, IORESOURCE_MEM, 1))) {
   dev_err( &( _pd->dev), "getres regs#1\n");
   return( PTR_ERR( rp));  }
 KDBG( "%s() 1 res %X-%X\n", __FUNCTION__, rp->start, rp->end);
 if ( IS_ERR( _pc->base1 = devm_ioremap_resource( &( _pd->dev), rp))) {
   KDBG( "mapping regs#1\n");
   return( PTR_ERR( _pc->base1));  }
 if ( IS_ERR( rp = platform_get_resource( _pd, IORESOURCE_MEM, 2))) {
   dev_err( &( _pd->dev), "getres regs2\n");
   return( PTR_ERR( rp));  }
 KDBG( "%s() 2 res %X-%X\n", __FUNCTION__, rp->start, rp->end);
 if ( IS_ERR( _pc->base2 = devm_ioremap_resource( &( _pd->dev), rp))) {
   KDBG( "mapping regs#2\n");
   return( PTR_ERR( _pc->base2));  }
 return( 0);  }
 
static int sc7021_gpio_probe( struct platform_device *_pd) {
 struct device_node *np = _pd->dev.of_node;
 sc7021gpio_chip_t *pc;
 struct gpio_chip *gchip;
 struct resource *rp;
 int err = 0, i, npins;
 if ( !np) {
   dev_err( &_pd->dev, "invalid devicetree node\n");
   return( -EINVAL);  }
 if ( !of_device_is_available( np)) {
   dev_err( &_pd->dev, "devicetree status is not available\n");
   return( -ENODEV);  }
 if ( !( pc = devm_kzalloc( &( _pd->dev), sizeof( *pc), GFP_KERNEL))) return( -ENOMEM);
 gchip = &( pc->chip);
 if ( ( err = sc7021_gpio_resmap( _pd, pc)) != 0) {
   devm_kfree( &( _pd->dev), pc);
   return( err);  }
 _pd->dev.platform_data = pc;
 gchip->label = MNAME;
 gchip->parent = &( _pd->dev);
 gchip->owner = THIS_MODULE;
 gchip->request =           sc7021gpio_f_req;
 gchip->free =              sc7021gpio_f_fre;
 gchip->get_direction =     sc7021gpio_f_gdi;
 gchip->direction_input =   sc7021gpio_f_sin;
 gchip->direction_output =  sc7021gpio_f_sou;
 gchip->get =               sc7021gpio_f_get;
 gchip->set =               sc7021gpio_f_set;
 gchip->set_config =        sc7021gpio_f_scf;
 gchip->dbg_show =          sc7021gpio_f_dsh;
 gchip->base = 0; // it is main platform GPIO controller
 gchip->ngpio = ARRAY_SIZE( sc7021gpio_list_names);
 gchip->names = sc7021gpio_list_names;
 gchip->can_sleep = 0;
#if defined(CONFIG_OF_GPIO)
 gchip->of_node = np;
 gchip->of_gpio_n_cells = 2;
 gchip->of_xlate =          sc7021gpio_xlate;
#endif
 gchip->to_irq =            sc7021gpio_i_map;
 if ( ( err = devm_gpiochip_add_data( &( _pd->dev), gchip, pc)) < 0) {
   dev_err( &_pd->dev, "gpiochip add failed\n");
   return( err);  }
 dev_info( &_pd->dev, "initialized\n");
 npins = platform_irq_count( _pd);
 for ( i = 0; i < npins; i++) {
    pc->irq[ i] = irq_of_parse_and_map( np, i);
    KDBG( "setting up irq#%d -> %d\n", i, pc->irq[ i]);
 }
//for ( i = 0; i < npins; i++) {
//   KINF( "setting up irq#%d\n", i);
//   if ( !( rp = platform_get_resource( _pd, IORESOURCE_IRQ, i))) {
//     KERR( "No irq#%d res\n", i);  continue;  }
//   if ( ( err = request_irq( rp->start, sc7021gpio_f_irq, 0, MNAME, rp))) {
//     KERR( "irq#%d req err:%d\n", i, err);  }
// }
 spin_lock_init( &( pc->lock));
 return( 0);  }

static int sc7021_gpio_remove( struct platform_device *_pd) {
 sc7021gpio_chip_t *cp;
 if ( ( cp = platform_get_drvdata( _pd)) == NULL) return( -ENODEV);
 gpiochip_remove( &( cp->chip));
 // FIX: remove spinlock_t
 return( 0);  }

static const struct of_device_id sc7021_gpio_of_match[] = {
 { .compatible = "sunplus,sp_gpio", },
 { .compatible = "pentagram,sp_gpio", },
 { .compatible = "tps,spgpio", },
 { .compatible = "tibbo,spgpio", },
 { .compatible = "sc7021,sp_gpio", },
 { /* null */ }
};
MODULE_DEVICE_TABLE(of, sc7021_gpio_of_match);
MODULE_ALIAS( "platform:" MNAME);
static struct platform_driver sc7021_gpio_driver = {
 .driver = {
	.name	= MNAME,
	.owner	= THIS_MODULE,
	.of_match_table = sc7021_gpio_of_match,
 },
 .probe		= sc7021_gpio_probe,
 .remove	= sc7021_gpio_remove,
};
module_platform_driver(sc7021_gpio_driver);

static int __init sc7021_gpio_drv_reg( void) {
 return platform_driver_register( &sc7021_gpio_driver);  }
postcore_initcall( sc7021_gpio_drv_reg);
static void __exit sc7021_gpio_exit( void) {
 platform_driver_unregister( &sc7021_gpio_driver);  }
module_exit(sc7021_gpio_exit);

MODULE_LICENSE(M_LIC);
MODULE_AUTHOR(M_AUT);
MODULE_DESCRIPTION(M_NAM);
