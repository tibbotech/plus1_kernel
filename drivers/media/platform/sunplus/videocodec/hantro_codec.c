// SPDX-License-Identifier: GPL-2.0
/*
 * Hantro video codec driver
 * Copyright (C) 2021 Sunplus Technology Co.,Ldt.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/init.h>

#include <linux/mm.h>

#include <linux/slab.h>

#include <linux/fs.h>

#include <linux/errno.h>

#include <linux/moduleparam.h>

#include <linux/sched.h>

#include <asm/io.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <linux/version.h>

#include <linux/platform_device.h>  
#include <linux/clk.h>
#include <linux/reset.h>

#define VCODEC_DEV_NAME  "hantro_codec"

static int codec_reset_release(struct platform_device *dev, const char *id) 
{  
    struct reset_control *rstc;
    int ret = 0;

	rstc = devm_reset_control_get(&dev->dev, id);

	if (IS_ERR(rstc)) {
		return PTR_ERR(rstc);
	} else {
		ret = reset_control_deassert(rstc);
		if (ret) {
			dev_err(&dev->dev, "failed to deassert reset line\n");
			return ret;
		}
        dev_info(&dev->dev, "Video codec reset okay\n");
	}

    return ret;
}

static int codec_clock_enable(struct platform_device *dev, const char *id) 
{  
    struct clk *clk;
    int ret = 0;

    clk = devm_clk_get(&dev->dev, id);

    if (IS_ERR(clk)) {
        return PTR_ERR(clk);
    } else {
        ret = clk_prepare_enable(clk);
        if (ret) {
            dev_err(&dev->dev, "enabled clock failed\n");
            return ret;
        }
        dev_info(&dev->dev, "Video codec clock enabled\n");
    }
    return ret;
}

static int codec_chrdev_probe(struct platform_device *dev) 
{  
    if (codec_clock_enable(dev, "clk_vcodec")) goto PROBE_ERR;
    if (codec_reset_release(dev, "rstc_vcodec")) goto PROBE_ERR;
   
    return 0;  
      
PROBE_ERR:
    
    return -1;  
}  
  
static int codec_chrdev_remove (struct platform_device *dev) 
{  
    dev_info(&dev->dev, " chrdev remove!\n");  
    return 0;  
}

static const struct of_device_id codec_dev_of_match[] = {
	{ .compatible = "sunplus,q645-hantro-codec", },
	{ .compatible = "sunplus,sp7350-hantro-codec", },
	{},
};

static struct platform_driver codec_chrdev_platform_driver = {
    .probe  =   codec_chrdev_probe,  
    .remove =   codec_chrdev_remove,  
    .driver =   {  
        .name   =   VCODEC_DEV_NAME,  
        .owner  =   THIS_MODULE,
        .of_match_table = codec_dev_of_match,
    },  
};  

int __init hx280codec_init(void)
{     
    int ret = 0;
    ret = platform_driver_register(&codec_chrdev_platform_driver);  
    if (ret)
        pr_err("hx280codec: module init failed!\n");  
    return ret;  
}

void __exit hx280codec_exit(void)
{
    platform_driver_unregister(&codec_chrdev_platform_driver); 
    pr_info("hx280codec: module removed\n");
}

module_init(hx280codec_init);
module_exit(hx280codec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jackyhsieh");
MODULE_DESCRIPTION("Driver module for Hantro codec");
