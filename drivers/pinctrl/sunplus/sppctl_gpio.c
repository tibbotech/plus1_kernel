// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO Driver for Sunplus/Tibbo SP7021 controller
 * Copyright (C) 2020 Sunplus Tech./Tibbo Tech.
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
#include <linux/io.h>

#include "sppctl_gpio_ops.h"
#include "sppctl_gpio.h"

__attribute((unused))
static irqreturn_t gpio_int_0(int irq, void *data)
{
	pr_info("register gpio int0 trigger\n");
	return IRQ_HANDLED;
}

#ifndef SPPCTL_H
int sppctl_gpio_resmap(struct platform_device *_pd, struct sppctlgpio_chip_t *_pc)
{
	struct resource *rp;

	// res0
	rp = platform_get_resource(_pd, IORESOURCE_MEM, 1);
	if (IS_ERR(rp)) {
		KERR(&(_pd->dev), "%s get res#0 ERR\n", __func__);
		return PTR_ERR(rp);
	}
	KDBG(&(_pd->dev), "mres #0:%p\n", rp);
	if (!rp)
		return -EFAULT;
	KDBG(&(_pd->dev), "mapping [%pa-%pa]\n", &rp->start, &rp->end);

	_pc->base0 = devm_ioremap_resource(&(_pd->dev), rp);
	if (IS_ERR(_pc->base0)) {
		KERR(&(_pd->dev), "%s map res#0 ERR\n", __func__);
		return PTR_ERR(_pc->base0);
	}

#if defined(CONFIG_PINCTRL_SPPCTL_Q645) || defined(CONFIG_PINCTRL_SPPCTL_Q654)
	// res2
	rp = platform_get_resource(_pd, IORESOURCE_MEM, 2);
	if (IS_ERR(rp)) {
		KERR(&(_pd->dev), "%s get res#2 ERR\n", __func__);
		return PTR_ERR(rp);
	}
	KDBG(&(_pd->dev), "mres #2:%p\n", rp);
	if (!rp)
		return -EFAULT;
	KDBG(&(_pd->dev), "mapping [%pa-%pa]\n", &rp->start, &rp->end);

	_pc->base2 = devm_ioremap_resource(&(_pd->dev), rp);
	if (IS_ERR(_pc->base2)) {
		KERR(&(_pd->dev), "%s map res#2 ERR\n", __func__);
		return PTR_ERR(_pc->base2);
	}
#else
	// res1
	rp = platform_get_resource(_pd, IORESOURCE_MEM, 2);
	if (IS_ERR(rp)) {
		KERR(&(_pd->dev), "%s get res#1 ERR\n", __func__);
		return PTR_ERR(rp);
	}
	KDBG(&(_pd->dev), "mres #1:%p\n", rp);
	if (!rp)
		return -EFAULT;
	KDBG(&(_pd->dev), "mapping [%pa-%pa]\n", &rp->start, &rp->end);

	_pc->base1 = devm_ioremap_resource(&(_pd->dev), rp);
	if (IS_ERR(_pc->base1)) {
		KERR(&(_pd->dev), "%s map res#1 ERR\n", __func__);
		return PTR_ERR(_pc->base1);
	}

	// res2
	rp = platform_get_resource(_pd, IORESOURCE_MEM, 3);
	if (IS_ERR(rp)) {
		KERR(&(_pd->dev), "%s get res#2 ERR\n", __func__);
		return PTR_ERR(rp);
	}
	KDBG(&(_pd->dev), "mres #2:%p\n", rp);
	if (!rp)
		return -EFAULT;
	KDBG(&(_pd->dev), "mapping [%pa-%pa]\n", &rp->start, &rp->end);

	_pc->base2 = devm_ioremap_resource(&(_pd->dev), rp);
	if (IS_ERR(_pc->base2)) {
		KERR(&(_pd->dev), "%s map res#2 ERR\n", __func__);
		return PTR_ERR(_pc->base2);
	}
#endif

	return 0;
}
#endif // SPPCTL_H

int sppctl_gpio_new(struct platform_device *_pd, void *_datap)
{
	struct device_node *np = _pd->dev.of_node, *npi;
	struct sppctlgpio_chip_t *pc = NULL;
	struct gpio_chip *gchip = NULL;
	int err = 0, i = 0, npins;
#ifdef SPPCTL_H
	struct sppctl_pdata_t *_pctrlp = (struct sppctl_pdata_t *)_datap;
#endif

	if (!np) {
		KERR(&(_pd->dev), "invalid devicetree node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		KERR(&(_pd->dev), "devicetree status is not available\n");
		return -ENODEV;
	}

	// print_device_tree_node(np, 0);
	for_each_child_of_node(np, npi) {
		if (of_find_property(npi, "gpio-controller", NULL)) {
			i = 1;
			break;
		}
	}

	if (of_find_property(np, "gpio-controller", NULL))
		i = 1;
	if (i == 0) {
		KERR(&(_pd->dev), "is not gpio-controller\n");
		return -ENODEV;
	}

	pc = devm_kzalloc(&(_pd->dev), sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;
	gchip = &(pc->chip);

#ifdef SPPCTL_H
	pc->base0 = _pctrlp->base0;
	pc->base1 = _pctrlp->base1;
	pc->base2 = _pctrlp->base2;
	_pctrlp->gpiod = pc;
#else
	err = sppctl_gpio_resmap(_pd, pc);
	if (err != 0)
		return err;
#endif

	gchip->label =             MNAME;
	gchip->parent =            &(_pd->dev);
	gchip->owner =             THIS_MODULE;
#ifdef SPPCTL_H
	gchip->request =           gpiochip_generic_request; // place new calls there
	gchip->free =              gpiochip_generic_free;
#else
	gchip->request =           sppctlgpio_f_req;
	gchip->free =              sppctlgpio_f_fre;
#endif
	gchip->get_direction =     sppctlgpio_f_gdi;
	gchip->direction_input =   sppctlgpio_f_sin;
	gchip->direction_output =  sppctlgpio_f_sou;
	gchip->get =               sppctlgpio_f_get;
	gchip->set =               sppctlgpio_f_set;
	gchip->set_config =        sppctlgpio_f_scf;
	gchip->dbg_show =          sppctlgpio_f_dsh;
	gchip->base =              0; // it is main platform GPIO controller
	gchip->ngpio =             GPIS_listSZ;
	gchip->names =             sppctlgpio_list_s;
	gchip->can_sleep =         0;
#if defined(CONFIG_OF_GPIO)
	gchip->of_node =           np;
#ifdef CONFIG_PINCTRL_SPPCTL
	gchip->of_gpio_n_cells =   2;
#endif
#endif
	gchip->to_irq =            sppctlgpio_i_map;

#ifdef SPPCTL_H
	_pctrlp->gpio_range.npins = gchip->ngpio;
	_pctrlp->gpio_range.base =  gchip->base;
	_pctrlp->gpio_range.name =  gchip->label;
	_pctrlp->gpio_range.gc =    gchip;
#endif

	// FIXME: can't set pc globally
	err = devm_gpiochip_add_data(&(_pd->dev), gchip, pc);
	if (err < 0) {
		KERR(&(_pd->dev), "gpiochip add failed\n");
		return err;
	}

	npins = platform_irq_count(_pd);
	for (i = 0; i < npins && i < SPPCTL_GPIO_IRQS; i++) {
		pc->irq[i] = irq_of_parse_and_map(np, i);
		KDBG(&(_pd->dev), "setting up irq#%d -> %d\n", i, pc->irq[i]);
	}

	spin_lock_init(&(pc->lock));

#ifndef SPPCTL_H
	pr_info(M_NAM " by " M_ORG " " M_CPR);
#endif

	return 0;
}

int sppctl_gpio_del(struct platform_device *_pd, void *_datap)
{
	//struct sppctlgpio_chip_t *cp;

	// FIXME: can't use globally now
	//cp = platform_get_drvdata(_pd);
	//if (cp == NULL)
	//	return -ENODEV;
	//gpiochip_remove(&(cp->chip));
	// FIX: remove spinlock_t ?
	return 0;
}

#ifndef SPPCTL_H
static const struct of_device_id sppctl_gpio_of_match[] = {
#if defined(CONFIG_PINCTRL_SPPCTL)
	{ .compatible = "sunplus,sp7021-gpio" },
#elif defined(CONFIG_PINCTRL_SPPCTL_Q645)
	{ .compatible = "sunplus,q645-gpio" },
#elif defined(CONFIG_PINCTRL_SPPCTL_Q654)
	{ .compatible = "sunplus,q654-gpio" },
#endif
	{ /* null */ }
};

static int sppctl_gpio_probe(struct platform_device *_pd)
{
	return sppctl_gpio_new(_pd, NULL);
}

static int sppctl_gpio_remove(struct platform_device *_pd)
{
	return sppctl_gpio_del(_pd, NULL);
}
MODULE_DEVICE_TABLE(of, sppctl_gpio_of_match);
MODULE_ALIAS("platform:" MNAME);

static struct platform_driver sppctl_gpio_driver = {
	.driver = {
		.name           = MNAME,
		.owner          = THIS_MODULE,
		.of_match_table = sppctl_gpio_of_match,
	},
	.probe  = sppctl_gpio_probe,
	.remove = sppctl_gpio_remove,
};
module_platform_driver(sppctl_gpio_driver);

static int __init sppctl_gpio_drv_reg(void)
{
	return platform_driver_register(&sppctl_gpio_driver);
}
postcore_initcall(sppctl_gpio_drv_reg);

static void __exit sppctl_gpio_exit(void)
{
	platform_driver_unregister(&sppctl_gpio_driver);
}
module_exit(sppctl_gpio_exit);

MODULE_LICENSE(M_LIC);
MODULE_AUTHOR(M_AUT1);
MODULE_AUTHOR(M_AUT2);
MODULE_DESCRIPTION(M_NAM);

#endif // SPPCTL_H
