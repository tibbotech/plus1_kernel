/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __EHCI_PLATFORM_H
#define __EHCI_PLATFORM_H

#ifdef CONFIG_PM
extern struct dev_pm_ops const ehci_sunplus_pm_ops;
#endif

extern int ehci_sunplus_probe(struct platform_device *dev);
extern int ehci_sunplus_remove(struct platform_device *dev);

extern void usb_hcd_platform_shutdown(struct platform_device *dev);
#endif

