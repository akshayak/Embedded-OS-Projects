#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <asm/delay.h>
#include <linux/platform_device.h>
#include "hcsr_dev.h"

#define MAX_DEVICES 100

static int nodevices=1;
module_param (nodevices, int,0000);


//defining 2 platform devices
static struct p_device plt_device1 =
{
		.dev_name	= "HCSR_0",
		.dev_num 	= 20,
		.p_dev = {
			.name	= "HCSR_0",
			.id	= -1,
		}
}; 

static struct p_device plt_device2 =
{
		.dev_name	= "HCSR_1",
		.dev_num 	=55 ,
		.p_dev = {
			.name	= "HCSR_1",
			.id	= -1,
		}
}; 



static int pdevice_init(void)
{

	int ret=0;
	/* Register the device */
	platform_device_register(&plt_device1.p_dev);
	
	printk(KERN_ALERT "Platform device 0 is registered in init \n");

	platform_device_register(&plt_device2.p_dev);

	printk(KERN_ALERT "Platform device 1 is registered in init \n");
	
	
	return ret;
}

static void pdevice_exit(void)
{
    platform_device_unregister(&plt_device1.p_dev);

	platform_device_unregister(&plt_device2.p_dev);

	printk(KERN_ALERT "Goodbye, unregister the device\n");
}

module_init(pdevice_init);
module_exit(pdevice_exit);
MODULE_LICENSE("GPL");