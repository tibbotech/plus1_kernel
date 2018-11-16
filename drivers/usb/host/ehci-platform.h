#ifndef __EHCI_PLATFORM_H
#define __EHCI_PLATFORM_H

extern int ehci_platform_probe(struct platform_device *dev);

extern int ehci_platform_remove(struct platform_device *dev);


#ifdef CONFIG_PM_WARP
int sp_ehci_fb_resume(struct platform_device *dev);

int sp_ehci_fb_suspend(struct platform_device *dev, pm_message_t state);
#endif

#endif




