#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "hcsr_dev.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/delay.h>
#include <linux/kthread.h>


static int irq_no;
long long unsigned int  distance=0;
static struct p_device *pdev[10]; //per device structure for platform devices
struct class *hcsr_dev_class; 
int ultra_velocity= 346;  //ultrasonic speed velocity in ait
static dev_t dev_count =1; //device number
static int no_devices=0; //number of platform devices
long secs=0, nsecs=0; //for ktime functions


static int hcsr_driver_open(struct inode *inode, struct file *file)
{
	printk("Inside open function");
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file)
{
	printk("Inside close function");
	return 0;
}

static ssize_t hcsr_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	printk("Inside read function");
	return 0;
}

static ssize_t hcsr_driver_write(struct file *file, const char __user *buf,
		       size_t len, loff_t *ppos)
{

	printk("Inside write function");
	return 0;
}


long hcsr_driver_ioctl(struct file *file, unsigned int operation, unsigned long arg)
{
	printk("Inside ioctl function");
	return 0;
}

int trigger_measurement(struct hcsr_device *hcsr_dev)
{
    int i;
    static unsigned long long final_dist=0;
    static unsigned long long max=0;
    static unsigned long long min= 100000000;
    printk("Trigger function. Value of trig pin: %d\n",gpio_get_value(hcsr_dev->trigger_pin));

    //starting the measurement, so setting the measurement flag=1
    hcsr_dev->measure_flag=1;

    for(i=0;i<hcsr_dev->m_val+2;i++)
    {
        printk("Triggering %d th measurement\n",i);
        gpio_set_value_cansleep(hcsr_dev->trigger_pin,1);
        mdelay(5000);
        gpio_set_value_cansleep(hcsr_dev->trigger_pin,0);
        mdelay(1);
        final_dist+=distance;

        if(distance<min)
            min=distance;

        if(distance > max)
            max=distance;

        //printk("Max = %llu  Min= %llu\n ",max,min);
        msleep(hcsr_dev->delta); //sleeping for delta millisecs   

    }



    //subtract the max and min values - 2 outliers
    //printk("Final Max = %llu Final Min= %llu Final dist = %llu\n",max,min,final_dist);
    final_dist-=max;
    final_dist-=min;
    
    do_div(final_dist,hcsr_dev->m_val); //to take average of m values
    printk("Final distance measure: %llu\n",final_dist);
    hcsr_dev->recent_distance=distance; //to write to distance attribute in sysfs structure
    hcsr_dev->measure_flag=0; //resetting measurement flag - measurement is completed
    return 0;
}


ktime_t start;
ktime_t end;

static irq_handler_t gpio_irq_handler(unsigned int irq, void *dev_id) // interrupt handler
{
    
    struct hcsr_device *hcsr_dev=((struct hcsr_device *)dev_id);
    int gpio_val=gpio_get_value(hcsr_dev->echo_pin);
    
    printk("Inside handler \n");
    if(gpio_val==0)
    {
        long long int pulse_width;
        end=ktime_set(secs,nsecs);  
        end=ktime_get();              
        
        pulse_width = ktime_to_us(ktime_sub(end,start)); // pulse width= difference of rising and falling edges
        //printk("Pin is low\n");
        distance = ((pulse_width*ultra_velocity)/2); // using formula : distance = (pulse width * ultrasonic spreading velocity in air) / 2
        //printk("Distance before division: %llu\n",distance);
        do_div(distance,100000);
        //do_div(distance,100000);
        printk("Distance: %llu\n",distance);
        irq_set_irq_type(irq_no,IRQ_TYPE_EDGE_RISING);

    }
    else 
    {
        //printk("Pin is high\n");
        
        start=ktime_set(secs,nsecs);
        start= ktime_get();
        irq_set_irq_type(irq_no,IRQ_TYPE_EDGE_FALLING);
    }

    return (irq_handler_t)IRQ_HANDLED; 

}

int set_output_pin(struct hcsr_device *hcsr_dev, int pin_num)
{
    switch(pin_num)
    {
        case 0:

            if( gpio_request(11, "gpio_out_11") != 0 )  printk("gpio_out_11 error!\n");
            if( gpio_request(32, "dir_out_32") != 0 )  printk("dir_out_32 error!\n");

            hcsr_dev->trigger_pin = 11;
            hcsr_dev->trigger_level_pin=32;

            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(32, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;

        case 1:
            if( gpio_request(12, "gpio_out_12") != 0 )  printk("gpio_out_12 error!\n");
            if( gpio_request(28, "dir_out_28") != 0 )  printk("dir_out_28 error!\n");
            if( gpio_request(45, "gpio_pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");            

            hcsr_dev->trigger_pin = 12;
            hcsr_dev->trigger_level_pin=28;
            hcsr_dev->trigger_mux_pin1=45;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(28, 0);
            gpio_direction_output(45, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;
        case 2:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(13, "gpio_out_13") != 0 )  printk("gpio_out_13 error!\n");
            if( gpio_request(34, "dir_out_34") != 0 )  printk("dir_out_34 error!\n");
            if( gpio_request(77, "gpio_pin_mux_77") != 0 )  printk("pin_mux_77 error!\n");
            hcsr_dev->trigger_pin = 13;
            hcsr_dev->trigger_level_pin=34;
            hcsr_dev->trigger_mux_pin1=77;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(34, 0);
            gpio_direction_output(77, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;
        case 3:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(14, "gpio_out_14") != 0 )  printk("gpio_out_14 error!\n");
            if( gpio_request(16, "gpio_out_16") != 0 )  printk("gpio_out_16 error!\n");
            if( gpio_request(76, "dir_out_76") != 0 )  printk("dir_out_76 error!\n");
            if( gpio_request(64, "gpio_pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            hcsr_dev->trigger_pin = 14;
            hcsr_dev->trigger_level_pin=16;
            hcsr_dev->trigger_mux_pin1=76;
            hcsr_dev->trigger_mux_pin2=64;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(16, 0);
            gpio_direction_output(76, 0);
            gpio_direction_output(64, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;           
        case 4:
            if( gpio_request(6, "gpio_out_6") != 0 )  printk("gpio_out_6 error!\n");
            if( gpio_request(36, "dir_out_36") != 0 )  printk("dir_out_36 error!\n");

            hcsr_dev->trigger_pin = 6;
            hcsr_dev->trigger_level_pin=36;

            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(36, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;
        case 5:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(0, "gpio_out_0") != 0 )  printk("gpio_out_0 error!\n");
            if( gpio_request(18, "dir_out_18") != 0 )  printk("dir_out_18 error!\n");
            if( gpio_request(66, "gpio_pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            hcsr_dev->trigger_pin = 0;
            hcsr_dev->trigger_level_pin=18;
            hcsr_dev->trigger_mux_pin1=66;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(18, 0);
            gpio_direction_output(66, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;
        case 6:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(1, "gpio_out_1") != 0 )  printk("gpio_out_1 error!\n");
            if( gpio_request(20, "dir_out_20") != 0 )  printk("dir_out_20 error!\n");
            if( gpio_request(21, "gpio_pin_mux_21") != 0 )  printk("pin_mux_21 error!\n");
            hcsr_dev->trigger_pin = 1;
            hcsr_dev->trigger_level_pin=20;
            hcsr_dev->trigger_mux_pin1=21;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(20, 0);
            gpio_direction_output(21, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;    
        case 7:

            if( gpio_request(38, "gpio_out_38") != 0 )  printk("gpio_out_38 error!\n");
            hcsr_dev->trigger_pin = 38;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;  

        case 8:

            if( gpio_request(40, "gpio_out_40") != 0 )  printk("gpio_out_40 error!\n");
            hcsr_dev->trigger_pin = 40;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break; 

        case 9:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(4, "gpio_out_4") != 0 )  printk("gpio_out_4 error!\n");
            if( gpio_request(22, "dir_out_22") != 0 )  printk("dir_out_22 error!\n");
            if( gpio_request(70, "gpio_pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            hcsr_dev->trigger_pin = 4;
            hcsr_dev->trigger_level_pin=22;
            hcsr_dev->trigger_mux_pin1=70;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(22, 0);
            gpio_direction_output(70, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;
        case 10:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(10, "gpio_out_10") != 0 )  printk("gpio_out_10 error!\n");
            if( gpio_request(26, "dir_out_26") != 0 )  printk("dir_out_26 error!\n");
            if( gpio_request(74, "gpio_pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            hcsr_dev->trigger_pin = 10;
            hcsr_dev->trigger_level_pin=26;
            hcsr_dev->trigger_mux_pin1=74;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(26, 0);
            gpio_direction_output(74, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;
        case 11:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(5, "gpio_out_5") != 0 )  printk("gpio_out_5 error!\n");
            if( gpio_request(24, "gpio_out_24") != 0 )  printk("gpio_out_24 error!\n");
            if( gpio_request(44, "dir_out_44") != 0 )  printk("dir_out_44 error!\n");
            if( gpio_request(72, "gpio_pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            hcsr_dev->trigger_pin = 5;
            hcsr_dev->trigger_level_pin=24;
            hcsr_dev->trigger_mux_pin1=44;
            hcsr_dev->trigger_mux_pin2=72;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(24, 0);
            gpio_direction_output(44, 0);
            gpio_direction_output(72, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break; 
        case 12:
            if( gpio_request(15, "gpio_out_15") != 0 )  printk("gpio_out_15 error!\n");
            if( gpio_request(42, "dir_out_42") != 0 )  printk("dir_out_42 error!\n");

            hcsr_dev->trigger_pin = 15;
            hcsr_dev->trigger_level_pin=42;

            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(42, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);

            break;
        case 13:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(7, "gpio_out_7") != 0 )  printk("gpio_out_7 error!\n");
            if( gpio_request(30, "dir_out_30") != 0 )  printk("dir_out_30 error!\n");
            if( gpio_request(46, "gpio_pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            hcsr_dev->trigger_pin = 7;
            hcsr_dev->trigger_level_pin=30;
            hcsr_dev->trigger_mux_pin1=46;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(30, 0);
            gpio_direction_output(46, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;

        case 14:
            if( gpio_request(48, "gpio_out_48") != 0 )  printk("gpio_out_48 error!\n");
            hcsr_dev->trigger_pin = 48;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break; 

        case 15:
            if( gpio_request(50, "gpio_out_48") != 0 )  printk("gpio_out_50 error!\n");
            hcsr_dev->trigger_pin = 50;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break; 

        case 16:
            if( gpio_request(52, "gpio_out_52") != 0 )  printk("gpio_out_52 error!\n");
            hcsr_dev->trigger_pin = 52;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break; 

        case 17:
            if( gpio_request(54, "gpio_out_54") != 0 )  printk("gpio_out_54 error!\n");
            hcsr_dev->trigger_pin = 54;
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break; 

        case 18:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(56, "gpio_out_56") != 0 )  printk("gpio_out_56 error!\n");
            if( gpio_request(60, "dir_out_60") != 0 )  printk("dir_out_60 error!\n");
            if( gpio_request(78, "gpio_pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            hcsr_dev->trigger_pin = 56;
            hcsr_dev->trigger_mux_pin1=60;
            hcsr_dev->trigger_mux_pin2=78;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(60, 0);
            gpio_direction_output(78, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;

        case 19:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(58, "gpio_out_58") != 0 )  printk("gpio_out_58 error!\n");
            if( gpio_request(60, "dir_out_60") != 0 )  printk("dir_out_60 error!\n");
            if( gpio_request(79, "gpio_pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            hcsr_dev->trigger_pin = 58;
            hcsr_dev->trigger_mux_pin1=60;
            hcsr_dev->trigger_mux_pin2=79;
            printk("Setting trigger pin as %d\n",hcsr_dev->trigger_pin);

            gpio_direction_output(60, 0);
            gpio_direction_output(79, 0);
            gpio_direction_output(hcsr_dev->trigger_pin, 0);
            break;

    }
    return 0;
}

int set_input_pin(struct hcsr_device *hcsr_dev, int pin_num)
{
    int ret;
    switch(pin_num)
    {

        case 0:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 11;
            hcsr_dev->echo_level_pin=32;

            if( gpio_request(11, "gpio_in_11") != 0 )  printk("gpio_in_11 error!\n");
            if( gpio_request(32, "dir_in_32") != 0 )  printk("dir_in_32 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            break;

        case 1:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 12;
            hcsr_dev->echo_level_pin=28;
            hcsr_dev->echo_mux_pin1=45;
            if( gpio_request(12, "gpio_in_12") != 0 )  printk("gpio_in_12 error!\n");
            if( gpio_request(28, "dir_in_28") != 0 )  printk("dir_in_28 error!\n");
            if( gpio_request(45, "pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;

        case 2:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 13;
            hcsr_dev->echo_level_pin=34;
            hcsr_dev->echo_mux_pin1=77;
            if( gpio_request(13, "gpio_in_12") != 0 )  printk("gpio_in_13 error!\n");
            if( gpio_request(34, "dir_in_28") != 0 )  printk("dir_in_34 error!\n");
            if( gpio_request(77, "pin_mux_45") != 0 )  printk("pin_mux_77 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;

        case 3:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 14;
            hcsr_dev->echo_level_pin=16;
            hcsr_dev->echo_mux_pin1=76;
            hcsr_dev->echo_mux_pin2=64;
            if( gpio_request(14, "gpio_in_14") != 0 )  printk("gpio_in_14 error!\n");
            if( gpio_request(16, "dir_in_16") != 0 )  printk("dir_in_16 error!\n");
            if( gpio_request(76, "pin_mux_76") != 0 )  printk("pin_mux_76 error!\n");
            if( gpio_request(64, "pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin2, 0);
            break;

        case 4:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 6;
            hcsr_dev->echo_level_pin=36;

            if( gpio_request(6, "gpio_in_6") != 0 )  printk("gpio_in_6 error!\n");
            if( gpio_request(36, "dir_in_36") != 0 )  printk("dir_in_36 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            break;

        case 5:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 0;
            hcsr_dev->echo_level_pin=18;
            hcsr_dev->echo_mux_pin1=66;
            if( gpio_request(0, "gpio_in_0") != 0 )  printk("gpio_in_0 error!\n");
            if( gpio_request(18, "dir_in_18") != 0 )  printk("dir_in_18 error!\n");
            if( gpio_request(66, "pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;

        case 6:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 1;
            hcsr_dev->echo_level_pin=20;
            hcsr_dev->echo_mux_pin1=68;
            if( gpio_request(1, "gpio_in_1") != 0 )  printk("gpio_in_1 error!\n");
            if( gpio_request(20, "dir_in_20") != 0 )  printk("dir_in_20 error!\n");
            if( gpio_request(68, "pin_mux_68") != 0 )  printk("pin_mux_68 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;

         case 7:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 38;


            if( gpio_request(38, "dir_in_38") != 0 )  printk("dir_in_38 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break; 

        case 8:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 40;

            if( gpio_request(40, "dir_in_40") != 0 )  printk("dir_in_40 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break;   

        case 9:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 4;
            hcsr_dev->echo_level_pin=22;
            hcsr_dev->echo_mux_pin1=70;
            if( gpio_request(4, "gpio_in_4") != 0 )  printk("gpio_in_4 error!\n");
            if( gpio_request(22, "dir_in_22") != 0 )  printk("dir_in_22 error!\n");
            if( gpio_request(70, "pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;    

        case 10:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 10;
            hcsr_dev->echo_level_pin=26;
            hcsr_dev->echo_mux_pin1=74;
            if( gpio_request(10, "gpio_in_10") != 0 )  printk("gpio_in_10 error!\n");
            if( gpio_request(26, "dir_in_26") != 0 )  printk("dir_in_26 error!\n");
            if( gpio_request(74, "pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(26,1);
            gpio_set_value_cansleep(74, 0);
            break;

        case 11:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 5;
            hcsr_dev->echo_level_pin=24;
            hcsr_dev->echo_mux_pin1=44;
            hcsr_dev->echo_mux_pin2=72;
            if( gpio_request(5, "gpio_in_5") != 0 )  printk("gpio_in_5 error!\n");
            if( gpio_request(24, "dir_in_24") != 0 )  printk("dir_in_24 error!\n");
            if( gpio_request(44, "pin_mux_44") != 0 )  printk("pin_mux_44 error!\n");
            if( gpio_request(72, "pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin2, 0);
            break;

        case 12:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 15;
            hcsr_dev->echo_level_pin=42;

            if( gpio_request(15, "gpio_in_15") != 0 )  printk("gpio_in_15 error!\n");
            if( gpio_request(42, "dir_in_42") != 0 )  printk("dir_in_42 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            break;

        case 13:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 7;
            hcsr_dev->echo_level_pin=30;
            hcsr_dev->echo_mux_pin1=46;
            if( gpio_request(7, "gpio_in_7") != 0 )  printk("gpio_in_7 error!\n");
            if( gpio_request(30, "dir_in_30") != 0 )  printk("dir_in_30 error!\n");
            if( gpio_request(46, "pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;   

        case 14:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 48;

            if( gpio_request(48, "dir_in_48") != 0 )  printk("dir_in_48 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break;  

        case 15:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 50;

            if( gpio_request(50, "dir_in_50") != 0 )  printk("dir_in_50 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break; 

        case 16:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 52;

            if( gpio_request(52, "dir_in_52") != 0 )  printk("dir_in_52 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break; 

        case 17:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 54;

            if( gpio_request(54, "dir_in_54") != 0 )  printk("dir_in_54 error!\n");

            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            break;

        case 18:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 56;
            hcsr_dev->echo_mux_pin1=60;
            hcsr_dev->echo_mux_pin2=78;
            if( gpio_request(56, "gpio_in_56") != 0 )  printk("gpio_in_56 error!\n");
            if( gpio_request(60, "dir_in_60") != 0 )  printk("dir_in_60 error!\n");
            if( gpio_request(78, "pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;  

         case 19:
            //setting pin 10 as edge pin
            hcsr_dev->echo_pin = 58;
            hcsr_dev->echo_mux_pin1=60;
            hcsr_dev->echo_mux_pin2=79;
            if( gpio_request(58, "gpio_in_58") != 0 )  printk("gpio_in_58 error!\n");
            if( gpio_request(60, "dir_in_60") != 0 )  printk("dir_in_60 error!\n");
            if( gpio_request(79, "pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            irq_no = gpio_to_irq(hcsr_dev->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)hcsr_dev);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(hcsr_dev->echo_pin);
            gpio_set_value_cansleep(hcsr_dev->echo_level_pin,1);
            gpio_set_value_cansleep(hcsr_dev->echo_mux_pin1, 0);
            break;             

    }
    return 0;
}

int set_pin(int trig_or_echo,struct hcsr_device *hcsr_dev, int pin_num)
{
if(trig_or_echo==1)
    set_output_pin(hcsr_dev,pin_num); //setting output trigger pin
else //setting input echo pin
    set_input_pin(hcsr_dev,pin_num);

    return 0;
}



int distance_measure_thread(void * data)  
{

        struct hcsr_device *hcsr_dev;
        hcsr_dev=data;
        while(!kthread_should_stop())
        {
         trigger_measurement(hcsr_dev);
        }
         return 0;

}

//to display trigger pin attribute
static ssize_t trigger_show(struct device *dev,struct device_attribute *attr,char *buf) 
{     
        
        struct p_device *pdev=dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "%d\n",pdev->hcsr_dev->trigger_pin);
        
}

//to store the trigger pin attribute
static ssize_t trigger_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) 
{
        int trigpin;
        struct p_device *pdev=dev_get_drvdata(dev);
        
        sscanf(buf, "%d", &trigpin);
   		pdev->hcsr_dev->trigger_pin=trigpin;
        printk("Setting trigger pin to %d \n",pdev->hcsr_dev->trigger_pin);

        return PAGE_SIZE;
}

//to display echo pin attribute
static ssize_t echo_show(struct device *dev,struct device_attribute *attr,char *buf) 
{     
        
        struct p_device *pdev=dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "%d\n",pdev->hcsr_dev->echo_pin);
        
}

//to store echo pin attribute
static ssize_t echo_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) 
{
        int echopin;
        struct p_device *pdev=dev_get_drvdata(dev);
        
        sscanf(buf, "%d", &echopin);
   		pdev->hcsr_dev->echo_pin=echopin;
        printk("Setting echo pin to %d \n",pdev->hcsr_dev->echo_pin);

        return PAGE_SIZE;
}

//to display number of samples attribute
static ssize_t number_samples_show(struct device *dev,struct device_attribute *attr,char *buf) 
{     
        
        struct p_device *pdev=dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "%d\n",pdev->hcsr_dev->m_val);
        
}

//to store number of samples attribute
static ssize_t number_samples_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) 
{
        int mval;
        struct p_device *pdev=dev_get_drvdata(dev);
        
        sscanf(buf, "%d", &mval);
   		pdev->hcsr_dev->m_val=mval;
        printk("Setting m value to %d \n",pdev->hcsr_dev->m_val);

        return PAGE_SIZE;
}

//to display sampling period attribute
static ssize_t sampling_period_show(struct device *dev,struct device_attribute *attr,char *buf) 
{     
        
        struct p_device *pdev=dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "%d\n",pdev->hcsr_dev->delta);
        
}

//to store sampling period attribute
static ssize_t sampling_period_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) 
{
        int del;
        struct p_device *pdev=dev_get_drvdata(dev);
        
        sscanf(buf, "%d", &del);
   		pdev->hcsr_dev->delta=del;
        printk("Setting delta to %d \n",pdev->hcsr_dev->delta);

        return PAGE_SIZE;
}
static ssize_t  distance_show(struct device *dev,struct device_attribute *attr,char *buf) //used to display the distance
{     
        struct p_device *pdev=dev_get_drvdata(dev);
            
        return snprintf(buf, PAGE_SIZE, "%lld\n",pdev->hcsr_dev->recent_distance);                      
}

//to display sampling period attribute
static ssize_t enable_show(struct device *dev,struct device_attribute *attr,char *buf) 
{     
        
        struct p_device *pdev=dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "%d\n",pdev->hcsr_dev->enable_flag);
        
}

//to store sampling period attribute
static ssize_t enable_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count) 
{
        int enbl;
        struct p_device *pdev=dev_get_drvdata(dev);
        
        sscanf(buf, "%d", &enbl);
   		pdev->hcsr_dev->enable_flag=enbl;

        if(enbl==1)
        {
            printk("Enabling measurement\n ");    

            set_pin(1, pdev->hcsr_dev,pdev->hcsr_dev->trigger_pin);
            set_pin(0,pdev->hcsr_dev,pdev->hcsr_dev->echo_pin);
            
            pdev->hcsr_dev->task=kthread_run(distance_measure_thread, (void *) pdev->hcsr_dev , pdev->hcsr_dev->name);//spawn a thread to measure the process


            }

        if(enbl==0)
            {
                printk("Disabling measurement\n");
                kthread_stop(pdev->hcsr_dev->task);  //kill the thread
            }


        printk("Setting enable flag to %d \n",pdev->hcsr_dev->enable_flag);

        return PAGE_SIZE;
}

static DEVICE_ATTR(trigger, S_IRUSR | S_IWUSR, trigger_show, trigger_store);
static DEVICE_ATTR(echo, S_IRUSR | S_IWUSR, echo_show, echo_store);
static DEVICE_ATTR(number_samples, S_IRUSR | S_IWUSR, number_samples_show, number_samples_store);
static DEVICE_ATTR(sampling_period, S_IRUSR | S_IWUSR, sampling_period_show, sampling_period_store);
static DEVICE_ATTR(distance, S_IRUSR, distance_show, NULL);
static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR, enable_show, enable_store);


	static struct file_operations hcsr_fops = {
	    .owner		= THIS_MODULE,          
	    .open		= hcsr_driver_open,        
	    .release	= hcsr_driver_close,    
	    .write		= hcsr_driver_write,       
	    .read		= hcsr_driver_read,       
	    .unlocked_ioctl        = hcsr_driver_ioctl,
	};



int hcsr_driver_init(struct p_device* plt)   
{

	char dev_name[20];
	int ret;
	printk("Inside init, no of devices = %d",no_devices);
	

	pdev[no_devices]=plt;
	pdev[no_devices]->hcsr_dev=(struct hcsr_device *)kmalloc(sizeof(struct hcsr_device), GFP_KERNEL);
	pdev[no_devices]->hcsr_dev->trigger_pin=-1;
    pdev[no_devices]->hcsr_dev->echo_pin=-1;
    pdev[no_devices]->hcsr_dev->trigger_level_pin=-1;
    pdev[no_devices]->hcsr_dev->echo_level_pin=-1;
    pdev[no_devices]->hcsr_dev->trigger_mux_pin1=-1;
    pdev[no_devices]->hcsr_dev->trigger_mux_pin2=-1;
    pdev[no_devices]->hcsr_dev->echo_mux_pin1=-1;
    pdev[no_devices]->hcsr_dev->echo_mux_pin2=-1;
    pdev[no_devices]->hcsr_dev->m_val=-1;
    pdev[no_devices]->hcsr_dev->delta=-1;
    pdev[no_devices]->hcsr_dev->buf_pointer=-1;

    pdev[no_devices]->hcsr_dev->recent_distance=0; //for the latest distance
    pdev[no_devices]->hcsr_dev->enable_flag=0;

    if(no_devices==0) //create class if its the first init call
    {
    	hcsr_dev_class=class_create(THIS_MODULE,"HCSR");
    }

	pdev[no_devices]->misc_device.minor =MISC_DYNAMIC_MINOR;

	sprintf(pdev[no_devices]->hcsr_dev->name,pdev[no_devices]->dev_name);
	sprintf(dev_name,pdev[no_devices]->dev_name);
	pdev[no_devices]->misc_device.name=dev_name;

	pdev[no_devices]->misc_device.fops=&hcsr_fops;

	printk("Registering %s\n", pdev[no_devices]->misc_device.name);

	ret = misc_register(&(pdev[no_devices]->misc_device));
	if(ret <0)
	{
		printk("Error registering the misc device");
		return -EFAULT;
	}

	// create platform devices
	pdev[no_devices]->hcsr_device = device_create(hcsr_dev_class, NULL, dev_count, NULL, pdev[no_devices]->dev_name);
	dev_count+=1;

    if (!pdev[no_devices]->hcsr_device) {
                printk("ERROR Device cannot be created\n");
                return -EFAULT;
     }
    dev_set_drvdata(pdev[no_devices]->hcsr_device,(void *) pdev[no_devices]);


    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_trigger); //creating trigger attribute
    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_echo); //creating echo attribute
    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_number_samples); //creating number samples attribute
    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_sampling_period); //creating number samples attribute
    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_distance); //creating distance attribute
    ret = device_create_file(pdev[no_devices]->hcsr_device, &dev_attr_enable); //creating enable attribute

    return 0;
}

static const struct platform_device_id plt_id_table[] = {            
         { "HCSR_0", 0 },  
         { "HCSR_1", 0 },

	 { },
};


static int pltdriver_probe(struct platform_device *dev_found)		
{
	struct p_device *pdevice;
	
	pdevice = container_of(dev_found, struct p_device, p_dev);
	
	printk(KERN_ALERT "Found the device -- %s  %d \n", pdevice->dev_name, pdevice->dev_num);

	hcsr_driver_init(pdevice); 		//calling init for the found device
	no_devices+=1;
	
			
	return 0;
};

void hcsr_exit(void)
{

	printk("inside exit function\n");
    no_devices-=1;
    dev_count-=1;
    

    //freeing irq
    free_irq(gpio_to_irq(pdev[no_devices]->hcsr_dev->echo_pin), (void *)pdev[no_devices]->hcsr_dev);


    if(pdev[no_devices]->hcsr_dev->echo_pin!=-1)
    {
    //freeing irq
    free_irq(gpio_to_irq(pdev[no_devices]->hcsr_dev->echo_pin), (void *)pdev[no_devices]->hcsr_dev);
    gpio_free(pdev[no_devices]->hcsr_dev->echo_pin); 
    }
    if(pdev[no_devices]->hcsr_dev->trigger_pin!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->trigger_pin); 


    if(pdev[no_devices]->hcsr_dev->trigger_level_pin!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->trigger_level_pin);
    if(pdev[no_devices]->hcsr_dev->trigger_mux_pin1!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->trigger_mux_pin1);
    if(pdev[no_devices]->hcsr_dev->trigger_mux_pin2!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->trigger_mux_pin2);    
    if(pdev[no_devices]->hcsr_dev->echo_level_pin!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->echo_level_pin);
    if(pdev[no_devices]->hcsr_dev->echo_mux_pin1!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->echo_mux_pin1);
    if(pdev[no_devices]->hcsr_dev->echo_mux_pin2!=-1)
        gpio_free(pdev[no_devices]->hcsr_dev->echo_mux_pin2);

	kfree(pdev[no_devices]->hcsr_dev);
	device_destroy(hcsr_dev_class, dev_count);

	printk("Deregistering %s\n",pdev[no_devices]->dev_name);
	misc_deregister(&(pdev[no_devices]->misc_device));
	
	if(no_devices==0)	
    {
	   class_unregister(hcsr_dev_class);
        class_destroy(hcsr_dev_class);
    }
}

static int pltdriver_remove(struct platform_device *pdev)
{
	hcsr_exit();
	return 0;
};


static struct platform_driver plt_driver = {
	.driver		= {
		.name	= "hcsr_driver",
		.owner	= THIS_MODULE,
	},
	.probe		= pltdriver_probe,
	.remove		= pltdriver_remove,
	.id_table	= plt_id_table,
};

module_platform_driver(plt_driver);
MODULE_LICENSE("GPL");