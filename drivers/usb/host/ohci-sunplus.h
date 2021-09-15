/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __OHCI_PLATFORM_H
#define __OHCI_PLATFORM_H


#ifdef CONFIG_PM
extern struct dev_pm_ops const ohci_sunplus_pm_ops;
#endif

extern int ohci_sunplus_probe(struct platform_device *dev);
extern int ohci_sunplus_remove(struct platform_device *dev);

extern void usb_hcd_platform_shutdown(struct platform_device *dev);

#endif

