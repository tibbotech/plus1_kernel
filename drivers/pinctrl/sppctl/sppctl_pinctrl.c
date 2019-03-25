/*
 * SC7021 pinmux controller driver.
 * Copyright (C) SunPlus Tech/Tibbo Tech. 2019
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
 */

#include "sppctl_pinctrl.h"
#include "../core.h"
#include "../pinctrl-utils.h"
#include "../devicetree.h"

#include "sc7021_gpio_ops.h"

// pmux: pin names - char *
const char * const sc7021pmux_list_names[] = {
 //D_PIS(0,0), D_PIS(0,1), D_PIS(0,2), D_PIS(0,3), D_PIS(0,4), D_PIS(0,5), D_PIS(0,6), D_PIS(0,7),
 D_PIS(1,0), D_PIS(1,1), D_PIS(1,2), D_PIS(1,3), D_PIS(1,4), D_PIS(1,5), D_PIS(1,6), D_PIS(1,7),
 D_PIS(2,0), D_PIS(2,1), D_PIS(2,2), D_PIS(2,3), D_PIS(2,4), D_PIS(2,5), D_PIS(2,6), D_PIS(2,7),
 D_PIS(3,0), D_PIS(3,1), D_PIS(3,2), D_PIS(3,3), D_PIS(3,4), D_PIS(3,5), D_PIS(3,6), D_PIS(3,7),
 D_PIS(4,0), D_PIS(4,1), D_PIS(4,2), D_PIS(4,3), D_PIS(4,4), D_PIS(4,5), D_PIS(4,6), D_PIS(4,7),
 D_PIS(5,0), D_PIS(5,1), D_PIS(5,2), D_PIS(5,3), D_PIS(5,4), D_PIS(5,5), D_PIS(5,6), D_PIS(5,7),
 D_PIS(6,0), D_PIS(6,1), D_PIS(6,2), D_PIS(6,3), D_PIS(6,4), D_PIS(6,5), D_PIS(6,6), D_PIS(6,7),
 D_PIS(7,0), D_PIS(7,1), D_PIS(7,2), D_PIS(7,3), D_PIS(7,4), D_PIS(7,5), D_PIS(7,6), D_PIS(7,7),
 D_PIS(8,0), D_PIS(8,1), D_PIS(8,2), D_PIS(8,3), D_PIS(8,4), D_PIS(8,5), D_PIS(8,6), D_PIS(8,7),
};
// gpio: is defined in sc7021_gpio_inf.c

#define D_PIN(x,y) 8*x+y
// function: GPIO. list of groups (pins)
const unsigned sc7021pins_G[] = {
 D_PIN(0,0), D_PIN(0,1), D_PIN(0,2), D_PIN(0,3), D_PIN(0,4), D_PIN(0,5), D_PIN(0,6), D_PIN(0,7),
 D_PIN(1,0), D_PIN(1,1), D_PIN(1,2), D_PIN(1,3), D_PIN(1,4), D_PIN(1,5), D_PIN(1,6), D_PIN(1,7),
 D_PIN(2,0), D_PIN(2,1), D_PIN(2,2), D_PIN(2,3), D_PIN(2,4), D_PIN(2,5), D_PIN(2,6), D_PIN(2,7),
 D_PIN(3,0), D_PIN(3,1), D_PIN(3,2), D_PIN(3,3), D_PIN(3,4), D_PIN(3,5), D_PIN(3,6), D_PIN(3,7),
 D_PIN(4,0), D_PIN(4,1), D_PIN(4,2), D_PIN(4,3), D_PIN(4,4), D_PIN(4,5), D_PIN(4,6), D_PIN(4,7),
 D_PIN(5,0), D_PIN(5,1), D_PIN(5,2), D_PIN(5,3), D_PIN(5,4), D_PIN(5,5), D_PIN(5,6), D_PIN(5,7),
 D_PIN(6,0), D_PIN(6,1), D_PIN(6,2), D_PIN(6,3), D_PIN(6,4), D_PIN(6,5), D_PIN(6,6), D_PIN(6,7),
 D_PIN(7,0), D_PIN(7,1), D_PIN(7,2), D_PIN(7,3), D_PIN(7,4), D_PIN(7,5), D_PIN(7,6), D_PIN(7,7),
 D_PIN(8,0), D_PIN(8,1), D_PIN(8,2), D_PIN(8,3), D_PIN(8,4), D_PIN(8,5), D_PIN(8,6), D_PIN(8,7),
};
// function: non-gpio. list of groups (pins)
const unsigned sc7021pins_M[] = {
 //D_PIN(0,0), D_PIN(0,1), D_PIN(0,2), D_PIN(0,3), D_PIN(0,4), D_PIN(0,5), D_PIN(0,6), D_PIN(0,7),
 D_PIN(1,0), D_PIN(1,1), D_PIN(1,2), D_PIN(1,3), D_PIN(1,4), D_PIN(1,5), D_PIN(1,6), D_PIN(1,7),
 D_PIN(2,0), D_PIN(2,1), D_PIN(2,2), D_PIN(2,3), D_PIN(2,4), D_PIN(2,5), D_PIN(2,6), D_PIN(2,7),
 D_PIN(3,0), D_PIN(3,1), D_PIN(3,2), D_PIN(3,3), D_PIN(3,4), D_PIN(3,5), D_PIN(3,6), D_PIN(3,7),
 D_PIN(4,0), D_PIN(4,1), D_PIN(4,2), D_PIN(4,3), D_PIN(4,4), D_PIN(4,5), D_PIN(4,6), D_PIN(4,7),
 D_PIN(5,0), D_PIN(5,1), D_PIN(5,2), D_PIN(5,3), D_PIN(5,4), D_PIN(5,5), D_PIN(5,6), D_PIN(5,7),
 D_PIN(6,0), D_PIN(6,1), D_PIN(6,2), D_PIN(6,3), D_PIN(6,4), D_PIN(6,5), D_PIN(6,6), D_PIN(6,7),
 D_PIN(7,0), D_PIN(7,1), D_PIN(7,2), D_PIN(7,3), D_PIN(7,4), D_PIN(7,5), D_PIN(7,6), D_PIN(7,7),
 D_PIN(8,0), D_PIN(8,1), D_PIN(8,2), D_PIN(8,3), D_PIN(8,4), D_PIN(8,5), D_PIN(8,6), D_PIN(8,7),
};

#define XPIN(x,y) PINCTRL_PIN(8*x+y,D_PIS(x,y))
const struct pinctrl_pin_desc sc7021pmux_list_pins[] = {
 XPIN(0,0), XPIN(0,1), XPIN(0,2), XPIN(0,3), XPIN(0,4), XPIN(0,5), XPIN(0,6), XPIN(0,7),
 XPIN(1,0), XPIN(1,1), XPIN(1,2), XPIN(1,3), XPIN(1,4), XPIN(1,5), XPIN(1,6), XPIN(1,7),
 XPIN(2,0), XPIN(2,1), XPIN(2,2), XPIN(2,3), XPIN(2,4), XPIN(2,5), XPIN(2,6), XPIN(2,7),
 XPIN(3,0), XPIN(3,1), XPIN(3,2), XPIN(3,3), XPIN(3,4), XPIN(3,5), XPIN(3,6), XPIN(3,7),
 XPIN(4,0), XPIN(4,1), XPIN(4,2), XPIN(4,3), XPIN(4,4), XPIN(4,5), XPIN(4,6), XPIN(4,7),
 XPIN(5,0), XPIN(5,1), XPIN(5,2), XPIN(5,3), XPIN(5,4), XPIN(5,5), XPIN(5,6), XPIN(5,7),
 XPIN(6,0), XPIN(6,1), XPIN(6,2), XPIN(6,3), XPIN(6,4), XPIN(6,5), XPIN(6,6), XPIN(6,7),
 XPIN(7,0), XPIN(7,1), XPIN(7,2), XPIN(7,3), XPIN(7,4), XPIN(7,5), XPIN(7,6), XPIN(7,7),
 XPIN(8,0), XPIN(8,1), XPIN(8,2), XPIN(8,3), XPIN(8,4), XPIN(8,5), XPIN(8,6), XPIN(8,7),
};

int stpctl_c_p_get( struct pinctrl_dev *_pd, unsigned _pin, unsigned long *_cfg) {
 sppctl_pdata_t *pctrl = pinctrl_dev_get_drvdata( _pd);
 unsigned int param = pinconf_to_config_param( *_cfg);
 unsigned int arg = 0;
// KINF( _pd, "%s(%d)\n", __FUNCTION__, _pin);
 switch ( param) {
   case PIN_CONFIG_DRIVE_OPEN_DRAIN:
        if ( !sc7021gpio_u_isodr( &( pctrl->gpiod->chip), _pin)) return( -EINVAL);
        break;
   case PIN_CONFIG_OUTPUT:
        if ( !sc7021gpio_u_gfrst( &( pctrl->gpiod->chip), _pin)) return( -EINVAL);
        if ( !sc7021gpio_u_magpi( &( pctrl->gpiod->chip), _pin)) return( -EINVAL);
        if ( sc7021gpio_f_gdi( &( pctrl->gpiod->chip), _pin) != 0) return( -EINVAL);
        arg = sc7021gpio_f_get( &( pctrl->gpiod->chip), _pin);
        break;
   default:
//       KINF( _pd, "%s(%d) wtf:%X\n", __FUNCTION__, _pin, param);
       return( -ENOTSUPP);  break;
 }
 *_cfg = pinconf_to_config_packed( param, arg);
 return( 0);  }
int stpctl_c_p_set( struct pinctrl_dev *_pd, unsigned _pin, unsigned long *_ca, unsigned _clen) {
 sppctl_pdata_t *pctrl = pinctrl_dev_get_drvdata( _pd);
 int i = 0;
 KDBG( _pd->dev, "%s(%d,%ld,%d)\n", __FUNCTION__, _pin, *_ca, _clen);
 for ( i = 0; i < _clen; i++) {
   if ( _ca[ i] & SC7021_PCTL_L_OUT) {  KDBG( _pd->dev, "%d:OUT\n", i);  sc7021gpio_f_sou( &( pctrl->gpiod->chip), _pin, 0);  }
   if ( _ca[ i] & SC7021_PCTL_L_OU1) {  KDBG( _pd->dev, "%d:OU1\n", i);  sc7021gpio_f_sou( &( pctrl->gpiod->chip), _pin, 1);  }
   if ( _ca[ i] & SC7021_PCTL_L_INV) {  KDBG( _pd->dev, "%d:INV\n", i);  sc7021gpio_u_siinv( &( pctrl->gpiod->chip), _pin);  }
   if ( _ca[ i] & SC7021_PCTL_L_ODR) {  KDBG( _pd->dev, "%d:INV\n", i);  sc7021gpio_u_soinv( &( pctrl->gpiod->chip), _pin);  }
   // FIXME: add PIN_CONFIG_DRIVE_OPEN_DRAIN
 }
 return( 0);  }
int stpctl_c_g_get( struct pinctrl_dev *_pd, unsigned _gid, unsigned long *_config) {
// KINF( _pd->dev, "%s(%d)\n", __FUNCTION__, _gid);
 // FIXME: add data
 return( 0);  }
int stpctl_c_g_set( struct pinctrl_dev *_pd, unsigned _gid, unsigned long *_configs, unsigned _num_configs) {
// KINF( _pd->dev, "%s(%d,,%d)\n", __FUNCTION__, _gid, _num_configs);
 // FIXME: delete
 return( 0);  }
#ifdef CONFIG_DEBUG_FS
void stpctl_c_d_show( struct pinctrl_dev *_pd, struct seq_file *s, unsigned _off) {
// KINF( _pd->dev, "%s(%d)\n", __FUNCTION__, _off);
 seq_printf(s, " %s", dev_name( _pd->dev));
 return;  }
void stpctl_c_d_group_show( struct pinctrl_dev *_pd, struct seq_file *s, unsigned _gid) {
 // group: freescale/pinctrl-imx.c, 448
// KINF( _pd->dev, "%s(%d)\n", __FUNCTION__, _gid);
 return;  }
void stpctl_c_d_config_show( struct pinctrl_dev *_pd, struct seq_file *s, unsigned long _config) {
// KINF( _pd->dev, "%s(%ld)\n", __FUNCTION__, _config);
 return;  }
#else
#define stpctl_c_d_show NULL
#define stpctl_c_d_group_show NULL
#define stpctl_c_d_config_show NULL
#endif
 
static struct pinconf_ops sppctl_pconf_ops = {
 .is_generic =                  true,
 .pin_config_get =              stpctl_c_p_get,
 .pin_config_set =              stpctl_c_p_set,
// .pin_config_group_get =        stpctl_c_g_get,
// .pin_config_group_set =        stpctl_c_g_set,
 .pin_config_dbg_show =         stpctl_c_d_show,
 .pin_config_group_dbg_show =   stpctl_c_d_group_show,
 .pin_config_config_dbg_show =  stpctl_c_d_config_show,
};

int stpctl_m_req( struct pinctrl_dev *_pd, unsigned _pin) {
 KDBG( _pd->dev, "%s(%d)\n", __FUNCTION__, _pin);
 // FIXME: define
 return( 0);  }
int stpctl_m_fre( struct pinctrl_dev *_pd, unsigned _pin) {
 KDBG( _pd->dev, "%s(%d)\n", __FUNCTION__, _pin);
 // FIXME: define
 return( 0);  }
int stpctl_m_f_cnt( struct pinctrl_dev *_pd) {  return( sizeof_listF - 2);  }
const char *stpctl_m_f_nam( struct pinctrl_dev *_pd, unsigned _fid) {  return( list_funcs[ _fid]);  }
int stpctl_m_f_grp( struct pinctrl_dev *_pd, unsigned _fid, const char * const **groups, unsigned *_galen) {
 if ( _fid == 0 || _fid == 1) {
   *_galen = sizeof_listG - 1;
   *groups = sc7021gpio_list_names;
 } else {
   *_galen = ARRAY_SIZE( sc7021pmux_list_names);
   *groups = sc7021pmux_list_names;
 }
 KDBG( _pd->dev, "%s(_fid:%d) %d\n", __FUNCTION__, _fid, *_galen);
 return( 0);  }
int stpctl_m_mux( struct pinctrl_dev *_pd, unsigned _fid, unsigned _gid) {
 int i = -1;
 sppctl_pdata_t *pctrl = pinctrl_dev_get_drvdata( _pd);
 KINF( _pd->dev, "%s(fun:%d,pin:%d)\n", __FUNCTION__, _fid, _gid);
 // set function
 if ( _fid > 1) {
   sc7021gpio_u_magpi_set( &( pctrl->gpiod->chip), _gid, muxF_M, muxMKEEP);
   sppctl_pin_set( pctrl, _gid, _fid - 2);    // pin, fun
   return( 0);  }
 // set IOP
 if ( _fid == 1) {
   sc7021gpio_u_magpi_set( &( pctrl->gpiod->chip), _gid, muxF_G, muxM_I);
   return( 0);  }
 // set GPIO and detouch from all funcs
 sc7021gpio_u_magpi_set( &( pctrl->gpiod->chip), _gid, muxF_G, muxM_G);
 while ( list_funcs[ ++i]) {
   if ( sppctl_fun_get( pctrl, i) != _gid) continue;
   sppctl_pin_set( pctrl, _gid, i);  }
 return( 0);  }
int stpctl_m_gpio_req( struct pinctrl_dev *_pd, struct pinctrl_gpio_range *range, unsigned _off) {
 KDBG( _pd->dev, "%s(%d)\n", __FUNCTION__, _off);
 return( 0);  }
void stpctl_m_gpio_fre( struct pinctrl_dev *_pd, struct pinctrl_gpio_range *range, unsigned _off) {
 KDBG( _pd->dev, "%s(%d)\n", __FUNCTION__, _off);
 return;  }
int stpctl_m_gpio_sdir( struct pinctrl_dev *_pd, struct pinctrl_gpio_range *range, unsigned _off, bool _in) {
 KDBG( _pd->dev, "%s(%d,%d)\n", __FUNCTION__, _off, _in);
 return( 0);  }

static struct pinmux_ops sppctl_pinmux_ops = {
 .request =                 stpctl_m_req,
 .free =                    stpctl_m_fre,
 .get_functions_count =     stpctl_m_f_cnt,
 .get_function_name =       stpctl_m_f_nam,
 .get_function_groups =     stpctl_m_f_grp,
 .set_mux =                 stpctl_m_mux,
 .gpio_request_enable =     stpctl_m_gpio_req,
 .gpio_disable_free =       stpctl_m_gpio_fre,
 .gpio_set_direction =      stpctl_m_gpio_sdir,
 .strict =      1,
};

// all groups
int stpctl_o_g_cnt( struct pinctrl_dev *_pd) {
 return( sizeof_listG);  }
const char *stpctl_o_g_nam( struct pinctrl_dev *_pd, unsigned _gid) {
 return( sc7021gpio_list_names[ _gid]);  }
int stpctl_o_g_pins( struct pinctrl_dev *_pd, unsigned _gid, const unsigned **pins, unsigned *num_pins) {
 *num_pins = 1;
 *pins = &sc7021pins_G[ _gid];
 return( 0);  }
// /sys/kernel/debug/pinctrl/sppctl/pins add: gpio_first and ctrl_sel
#ifdef CONFIG_DEBUG_FS
void stpctl_o_show( struct pinctrl_dev *_pd, struct seq_file *_s, unsigned _n) {
 seq_printf( _s, "%s", dev_name( _pd->dev));
 // FIXME: add: gpio_first and ctrl_sel
 return;  }
#else
#define stpctl_ops_show NULL
#endif

int stpctl_o_n2map( struct pinctrl_dev *_pd, struct device_node *_dn, struct pinctrl_map **_map, unsigned *_nm) {
 struct device_node *parent;
 u32 dt_pin;
 u8 p_p, p_g, p_f, p_l;
 unsigned long *configs;
 int i, size = 0;
 const __be32 *list = of_get_property( _dn, "sppctl,pins", &size);
// KDBG( _pd->dev, "%s() np_c->n:%s ->t:%s ->fn:%s\n", __FUNCTION__, _dn->name, _dn->type, _dn->full_name);
// int ret = pinconf_generic_dt_node_to_map_pin( _pd, _dn, _map, num_maps);
 parent = of_get_parent( _dn);
 *_nm = size/sizeof( *list);
 *_map = devm_kcalloc( _pd->dev, *_nm, sizeof( **_map), GFP_KERNEL);
 configs = devm_kcalloc( _pd->dev, *_nm, sizeof( *configs), GFP_KERNEL);
 for ( i = 0; i < ( *_nm); i++) {
    dt_pin = be32_to_cpu( list[ i]);
    p_p = SC7021_PCTLD_P(dt_pin);
    p_g = SC7021_PCTLD_G(dt_pin);
    p_f = SC7021_PCTLD_F(dt_pin);
    p_l = SC7021_PCTLD_L(dt_pin);
    (* _map)[ i].name = parent->name;
    KDBG( _pd->dev, "map [%d]=%d p=%d g=%d f=%d l=%d\n", i, dt_pin, p_p, p_g, p_f, p_l);
    if ( p_g == SC7021_PCTL_G_GPIO) {
      (* _map)[ i].type = PIN_MAP_TYPE_CONFIGS_PIN;
      (* _map)[ i].data.configs.num_configs = 1;
      (* _map)[ i].data.configs.group_or_pin = pin_get_name( _pd, p_p);
      configs[ i] = p_l;
      (* _map)[ i].data.configs.configs = &( configs[ i]);
      KDBG( _pd->dev, "%s(%d) = %X\n", (* _map)[ i].data.configs.group_or_pin, p_p, p_l);
    } else {
      (* _map)[ i].type = PIN_MAP_TYPE_MUX_GROUP;
      (* _map)[ i].data.mux.function = list_funcs[ p_f];
      (* _map)[ i].data.mux.group = pin_get_name( _pd, p_p);
      KDBG( _pd->dev, "f->p: %s(%d)->%s(%d)\n", (* _map)[ i].data.mux.function, p_f, (* _map)[ i].data.mux.group, p_p);
    }
 }
 of_node_put( parent);
// parse_dt_cfg(np, pctldev->desc->custom_params,
 KDBG( _pd->dev, "%d pins mapped\n", *_nm);
 return( 0);  }
void stpctl_o_mfre( struct pinctrl_dev *_pd, struct pinctrl_map *_map, unsigned num_maps) {
 KINF( _pd->dev, "%s(%d)\n", __FUNCTION__, num_maps);
 // FIXME: test
 pinctrl_utils_free_map( _pd, _map, num_maps);
 return;  }

static struct pinctrl_ops sppctl_pctl_ops = {
 .get_groups_count =    stpctl_o_g_cnt,
 .get_group_name =      stpctl_o_g_nam,
 .get_group_pins =      stpctl_o_g_pins,
 .pin_dbg_show =        stpctl_o_show,
 .dt_node_to_map =      stpctl_o_n2map,
 .dt_free_map =         stpctl_o_mfre,
};

// ---------- main (exported) functions
void sppctl_pinctrl_init( struct platform_device *_pd) {
 int err;
 struct device *dev = &_pd->dev;
 struct device_node *np = of_node_get( dev->of_node);
 sppctl_pdata_t *_p = ( sppctl_pdata_t *)_pd->dev.platform_data;
 // init pdesc
 _p->pdesc.owner = THIS_MODULE;
 _p->pdesc.name = dev_name( &( _pd->dev));
 _p->pdesc.pins = &( sc7021pmux_list_pins[ 0]);
 _p->pdesc.npins = ARRAY_SIZE( sc7021pmux_list_pins);
 _p->pdesc.pctlops = &sppctl_pctl_ops;
 _p->pdesc.confops = &sppctl_pconf_ops;
 _p->pdesc.pmxops = &sppctl_pinmux_ops;
 
 if ( ( err = devm_pinctrl_register_and_init( &( _pd->dev), &( _p->pdesc), _p, &( _p->pcdp)))) {
   KERR( &( _pd->dev), "Failed to register\n");
   of_node_put( np);  }
 pinctrl_enable( _p->pcdp);
 return;  }

void sppctl_pinctrl_clea( struct platform_device *_pd) {
 sppctl_pdata_t *_p = ( sppctl_pdata_t *)_pd->dev.platform_data;
 devm_pinctrl_unregister( &( _pd->dev), _p->pcdp);
 return;  }
