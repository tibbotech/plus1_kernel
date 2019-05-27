#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>



#define DRIVER_NAME "thermaldev"
static unsigned int thermaldev_alloc_major=0;
static unsigned int num_of_dev=1;
static struct cdev  thermaldev_alloc_cdev;

static struct class *thermaldev_class;


extern int  sp_thermal_get_temp(int *temp);


//ret = read(fd, buf, 16);

static ssize_t thermaldev_alloc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{

     int temp=0;
	 int ret;

     ret = sp_thermal_get_temp(&temp);


	 if (ret == 0) {
		 //printk(KERN_ALERT "[thermal_dev] read OK");
		 ret = copy_to_user(buf, &temp, count);
	     //printk(KERN_ALERT "[thermal_dev] copy_to_user");
	 }


     return ret;

}

static int thermaldev_alloc_open(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "Call thermaldev_alloc_open\n");
	return 0;
}

static int thermaldev_alloc_close(struct inode *inode, struct file *file)
{
        printk(KERN_ALERT "Call thermaldev_alloc_close\n");
        return 0;
}


struct file_operations fops = {
	.owner = THIS_MODULE,
        .open = thermaldev_alloc_open,
        .read = thermaldev_alloc_read,
        .release = thermaldev_alloc_close,
};

static int thermaldev_alloc_init ( void ) 
{ 
    dev_t dev = MKDEV(thermaldev_alloc_major, 0);
    int alloc_ret=0;
    int cdev_ret=0;

    alloc_ret=alloc_chrdev_region(&dev, 0, num_of_dev, DRIVER_NAME);
    if(alloc_ret)
	goto error;
    thermaldev_alloc_major = MAJOR(dev);

    cdev_init(&thermaldev_alloc_cdev, &fops);
    cdev_ret=cdev_add(&thermaldev_alloc_cdev,dev, num_of_dev);
 
   if (cdev_ret)
           goto error;

    printk(KERN_ALERT"%s driver(major %d) installed.\n", DRIVER_NAME,thermaldev_alloc_major);


	thermaldev_class = class_create(THIS_MODULE, "thermaldev");

	if (IS_ERR(thermaldev_class)) 
		goto error;


    device_create(thermaldev_class, NULL, MKDEV(thermaldev_alloc_major, 0), NULL, DRIVER_NAME);



	return 0;

	
error:
        if (cdev_ret == 0)
                cdev_del(&thermaldev_alloc_cdev);

        if (alloc_ret == 0)
                unregister_chrdev_region(dev,num_of_dev);
	return -1;
} 

static void thermaldev_alloc_exit ( void ) 
{
        dev_t dev = MKDEV(thermaldev_alloc_major, 0);
       
        cdev_del(&thermaldev_alloc_cdev);
        unregister_chrdev_region(dev,num_of_dev);
	    printk(KERN_ALERT"%s driver removed\n",DRIVER_NAME); 
} 

module_init ( thermaldev_alloc_init ) ; 
module_exit ( thermaldev_alloc_exit ) ;

MODULE_LICENSE ( "Dual BSD/GPL" ) ; 
MODULE_AUTHOR("jeff Kuo");
MODULE_DESCRIPTION("This is alloc_thermaldev_region  module");
MODULE_VERSION("1.0");
