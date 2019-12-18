/*
 * GPIO Driver for SunPlus/Tibbo SP7021 controller
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
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <asm/io.h>

#include "sp7021_gpio_ops.h"
#include "sp7021_gpio.h"

#if 0 //Test code for GPIO_INT0
static irqreturn_t gpio_int_0(int irq, void *data)
{
 printk(KERN_INFO "register gpio int0 trigger \n");
 return( IRQ_HANDLED);  }
#endif

int sp7021_gpio_new( struct platform_device *_pd, void *_datap) {
 struct device_node *np = _pd->dev.of_node, *npi;
 sp7021gpio_chip_t *pc = NULL;
 struct gpio_chip *gchip = NULL;
 int err = 0, i = 0, npins;
 sppctl_pdata_t *_pctrlp = ( sppctl_pdata_t *)_datap;
 if ( !np) {
   KERR( &( _pd->dev), "invalid devicetree node\n");
   return( -EINVAL);  }
 if ( !of_device_is_available( np)) {
   KERR( &( _pd->dev), "devicetree status is not available\n");
   return( -ENODEV);  }
 // print_device_tree_node( np, 0);
 for_each_child_of_node( np, npi) {
   if ( of_find_property( npi, "gpio-controller", NULL)) {  i = 1;  break;  }
 }
 if ( of_find_property( np, "gpio-controller", NULL)) i = 1;
 if ( i == 0) {
   KERR( &( _pd->dev), "is not gpio-controller\n");
   return( -ENODEV);  }
 if ( !( pc = devm_kzalloc( &( _pd->dev), sizeof( *pc), GFP_KERNEL))) return( -ENOMEM);
 gchip = &( pc->chip);
 pc->base0 = _pctrlp->base0;
 pc->base1 = _pctrlp->base1;
 pc->base2 = _pctrlp->base2;
 _pctrlp->gpiod = pc;
 gchip->label =             MNAME;
 gchip->parent =            &( _pd->dev);
 gchip->owner =             THIS_MODULE;
 gchip->request =           gpiochip_generic_request; // place new calls there
 gchip->free =              gpiochip_generic_free;
 gchip->get_direction =     sp7021gpio_f_gdi;
 gchip->direction_input =   sp7021gpio_f_sin;
 gchip->direction_output =  sp7021gpio_f_sou;
 gchip->get =               sp7021gpio_f_get;
 gchip->set =               sp7021gpio_f_set;
 gchip->set_config =        sp7021gpio_f_scf;
 gchip->dbg_show =          sp7021gpio_f_dsh;
 gchip->base =              0; // it is main platform GPIO controller
 gchip->ngpio =             GPIS_listSZ;
 gchip->names =             sp7021gpio_list_s;
 gchip->can_sleep =         0;
#if defined(CONFIG_OF_GPIO)
 gchip->of_node =           np;
 gchip->of_gpio_n_cells =   2;
 gchip->of_xlate =          sp7021gpio_xlate;
#endif
 gchip->to_irq =            sp7021gpio_i_map;
 _pctrlp->gpio_range.npins = gchip->ngpio;
 _pctrlp->gpio_range.base =  gchip->base;
 _pctrlp->gpio_range.name =  gchip->label;
 _pctrlp->gpio_range.gc =    gchip;
 // FIXME: can't set pc globally
 if ( ( err = devm_gpiochip_add_data( &( _pd->dev), gchip, pc)) < 0) {
   KERR( &( _pd->dev), "gpiochip add failed\n");
   return( err);  }
 npins = platform_irq_count( _pd);
 for ( i = 0; i < npins && i < SP7021_GPIO_IRQS; i++) {
   pc->irq[ i] = irq_of_parse_and_map( np, i);
   KDBG( &( _pd->dev), "setting up irq#%d -> %d\n", i, pc->irq[ i]);
 }
#if 0 //Test code for GPIO_INT0
 err = devm_request_irq(&( _pd->dev), pc->irq[ 0], gpio_int_0, IRQF_TRIGGER_RISING, "sppctl_gpio_int0", _pd);
 KDBG( &( _pd->dev), "register gpio int0 irq \n");
 if (err) {
   KDBG( &( _pd->dev), "register gpio int0 irq err \n");
   return( err);  }
#endif
 spin_lock_init( &( pc->lock));
 return( 0);  }

int sp7021_gpio_del( struct platform_device *_pd, void *_datap) {
 //sp7021gpio_chip_t *cp;
 // FIXME: can't use globally now
 //if ( ( cp = platform_get_drvdata( _pd)) == NULL) return( -ENODEV);
 //gpiochip_remove( &( cp->chip));
 // FIX: remove spinlock_t ?
 return( 0);  }

