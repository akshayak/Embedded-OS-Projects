#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "misc_ioctl.h"
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/delay.h>
#include <linux/kthread.h>
#define MAX_DEVICES 100

#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)   // This has been obtained from an outside source
{                                                   // this is assembly code to obtain the Time stamp counter value
                                                    // based on the platform.
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#elif defined(__powerpc__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#else

#error "Counter is not present"

#endif

static int nodevices=1;
module_param (nodevices, int,0000);

static int irq_no;
int ultra_velocity= 346;
long long unsigned int  distance=0;
long secs=0, nsecs=0;
struct op_buffer *buff;


struct output_buffer{
    unsigned long long int time_stamp;
     unsigned long long int distance;
};

//per device structure
struct hcsr_device
{
    struct miscdevice misc_hcsr;
    char name[20]; //name of the device
    int trigger_pin;
    int echo_pin;
    int trigger_level_pin;
    int echo_level_pin;
    int trigger_mux_pin1;
    int trigger_mux_pin2;
    int echo_mux_pin1;
    int echo_mux_pin2;
    int m_val;
    int delta;
    int measure_flag;
    struct output_buffer o_buf[5]; //max size of buffer is 5
    int buf_pointer;
    struct hcsr_device *next; // to create a list of devices
    int count; //this count is to maintain the fifo element count
    struct task_struct *task;
} *hcsr_dev[MAX_DEVICES];

typedef struct hcsr_device* hcsr_d;
//struct for device list
struct hcsr_device_list
{
    hcsr_d head;
} *hcsr_list;

static int misc_open(struct inode *inode, struct file *file)
{
    int minor=iminor(inode);
    hcsr_d hcsr_dev=hcsr_list->head;
    printk("Inside open function");
    
    printk("Mino no: %d",minor);
    
    //getting the context of the opened device from the list of devices
    while(hcsr_dev->next !=NULL)
    {
        if(minor==hcsr_dev->misc_hcsr.minor)
            break;
        hcsr_dev=hcsr_dev->next;
    } 
    pr_info("Opening device : %s\n",hcsr_dev->name);
    file->private_data=hcsr_dev;
    return 0;
}

static int misc_close(struct inode *inode, struct file *file)
{
    struct hcsr_device *hcsr_dev=file->private_data;
    printk("Inside close function");
    

    //freeing the interrupt line
    free_irq(gpio_to_irq(hcsr_dev->echo_pin), (void *)hcsr_dev );

    //freeing the gpio trigger and edge pins
    
    gpio_set_value_cansleep(hcsr_dev->trigger_pin, 0);
    gpio_set_value_cansleep(hcsr_dev->echo_pin, 0);
    gpio_free(hcsr_dev->echo_pin); 
    gpio_free(hcsr_dev->trigger_pin); 

    //freeing up the extra pins
    if(hcsr_dev->trigger_level_pin!=-1)
        gpio_free(hcsr_dev->trigger_level_pin);

    if(hcsr_dev->trigger_mux_pin1!=-1)
        gpio_free(hcsr_dev->trigger_mux_pin1);

    if(hcsr_dev->trigger_mux_pin2!=-1)
        gpio_free(hcsr_dev->trigger_mux_pin2);

    if(hcsr_dev->echo_level_pin!=-1)
        gpio_free(hcsr_dev->echo_level_pin);

    if(hcsr_dev->echo_mux_pin1!=-1)
        gpio_free(hcsr_dev->echo_mux_pin1);

    if(hcsr_dev->echo_mux_pin2!=-1)
        gpio_free(hcsr_dev->echo_mux_pin2);

    pr_info("Releasing: %s\n",hcsr_dev->name);
    return 0;
}

int populate_buffer(struct hcsr_device *hcsr_dev, unsigned long long int time_val, unsigned long long int dist)
{

    hcsr_dev-> buf_pointer=((hcsr_dev-> buf_pointer)+1)%5; //overwrite the earlist msg

    printk("Populating buffer : time: %llu, distance : %llu, pointer :%d\n",time_val,dist,hcsr_dev-> buf_pointer);
    hcsr_dev->o_buf[hcsr_dev-> buf_pointer].time_stamp=time_val;
    hcsr_dev->o_buf[hcsr_dev-> buf_pointer].distance=dist;

    hcsr_dev->count++; //incrementing count for read operation
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

        printk("Max = %llu  Min= %llu \n",max,min);
        msleep(hcsr_dev->delta); //sleeping for delta millisecs   

    }



    //subtract the max and min values - 2 outliers
    printk("Final Max = %llu Final Min= %llu Final dist = %llu\n",max,min,final_dist);
    final_dist-=max;
    final_dist-=min;
    
    do_div(final_dist,hcsr_dev->m_val);
    printk("Final distance measure: %llu\n",final_dist);
    populate_buffer(hcsr_dev,rdtsc(), final_dist);
    hcsr_dev->measure_flag=0;
    return 0;
}

static ssize_t misc_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
    struct hcsr_device *hcsr_dev=file->private_data;
    int index;
    printk("Inside read function");

    while(hcsr_dev->buf_pointer<0); //in case the buffer is empty, wait till the buffer gets a value


    if(hcsr_dev->count<4) //if the buffer has not even filled once fully, then return the 0th element of the FIFO queue
        index=0;
    else
        index=hcsr_dev->buf_pointer+1; //if buffer is already filled atleast once, the next index of buff pointer will be the oldest element
    buff=kmalloc(sizeof(struct op_buffer), GFP_KERNEL);


    buff->time_stamp=hcsr_dev->o_buf[index].time_stamp;
    buff->distance=hcsr_dev->o_buf[index].distance;
    printk("Time = %llu\n",buff->time_stamp);
    printk("distance = %llu\n",buff->distance);
    copy_to_user((struct op_buffer *)buf, buff , sizeof(struct op_buffer));
    

    return 0;
}

int distance_measure_thread(void * data)  
{
        
        struct hcsr_device *hcsr_dev;
        printk("Inside thread\n");
        hcsr_dev=data;
        while(!kthread_should_stop())
        {
         trigger_measurement(hcsr_dev);
        }
         return 0;

}

static ssize_t misc_write(struct file *file, const char __user *buf,
		       size_t len, loff_t *ppos)
{
    struct hcsr_device *hcsr_dev=file->private_data;

    int write_int;
    printk("Inside write function, measure flag :%d\n",hcsr_dev->measure_flag);


    copy_from_user(&write_int, buf,len);

    printk("Write param : %d",write_int);

    if (write_int==1) 
    {
        if(hcsr_dev->measure_flag==0) //trigger measurement if one already is not in progress
        {
            printk("Starting thread:\n");
            hcsr_dev->buf_pointer=-1; //clearing buffer
            hcsr_dev->task=kthread_run(distance_measure_thread, (void *) hcsr_dev , hcsr_dev->name);//spawn a thread to measure the process
        }
        else 
            return -EINVAL; //return if measurement is in progress

        //trigger_measurement(hcsr_dev);
    }

    else if(write_int==0)
    {
        printk("Stopping thread:\n");
        kthread_stop(hcsr_dev->task); 
    }

 


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
        printk("Pin is low\n");
        distance = ((pulse_width*ultra_velocity)/2);
        printk("Distance before division: %llu\n",distance);
        do_div(distance,100000);
        printk("Distance: %llu\n",distance);
        irq_set_irq_type(irq_no,IRQ_TYPE_EDGE_RISING);

    }
    else 
    {
        printk("Pin is high\n");
        
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



long misc_ioctl(struct file *file, unsigned int operation, unsigned long arg)
{
    int val1,val2;
    void *tempargs = NULL;
    struct hcsr_device *hcsr_dev=file->private_data;
    printk("Inside ioctl function\n");
    tempargs=kmalloc(_IOC_SIZE(operation), GFP_KERNEL);
    copy_from_user(tempargs,(unsigned long *)(arg), _IOC_SIZE(operation));

    val1=((i_args)tempargs)->val1;
    val2=((i_args)tempargs)->val2;
    printk("val 1: %d\n",val1);
    printk("val 2: %d\n",val2);
    if(val1<0 || val2<0)
        return -EINVAL;
    switch(operation)
    {
        case CONFIG_PINS: 
            //setting trigger pin as output. first param to set_pin is 1 for output, 0 for input
            set_pin(1, hcsr_dev,val1);
            set_pin(0,hcsr_dev,val2);
            printk("Pins configured: trigger pin:%d echo pin:%d \n",hcsr_dev->trigger_pin,hcsr_dev->echo_pin);
        break;
        case SET_PARAMETERS:
            //setting parameter values 
            hcsr_dev->m_val=val1;
            hcsr_dev->delta=val2;
            printk("Params configured: mval: %d delta:%d \n",hcsr_dev->m_val,hcsr_dev->delta);
        break;
    }

    return 0;
}



static const struct file_operations misc_fops = {
    .owner			= THIS_MODULE,
    .write			= misc_write,
    .open			= misc_open,
    .release		= misc_close,
    .llseek 		= no_llseek,
    .unlocked_ioctl = misc_ioctl,
    .read           = misc_read,
};


struct miscdevice misc_hcsr[MAX_DEVICES];
static int __init misc_init(void)
{
    int error,i;
    char dev_name[20];

    pr_info("I'm in\n");
    printk(KERN_ALERT "No of devices: %d\n", nodevices);

    //Allocate memory for the device list
    hcsr_list=kmalloc(sizeof(struct hcsr_device_list), GFP_KERNEL);

    for(i=0;i<nodevices;i++)
    {
        
        /* Allocate memory for the per-device structure */
        hcsr_dev[i] = kmalloc(sizeof(struct hcsr_device), GFP_KERNEL);
        

        snprintf(dev_name,20,"HCSR_%d", i);
        sprintf(hcsr_dev[i]->name, dev_name); //storing device name is per device structure
        misc_hcsr[i].name=dev_name;
        printk("Device name:%s\n",hcsr_dev[i]->name);
        misc_hcsr[i].fops=&misc_fops;
        misc_hcsr[i].minor = MISC_DYNAMIC_MINOR;
        printk("Minor number:%d\n",misc_hcsr[i].minor);
        //register misc device
        error = misc_register(&misc_hcsr[i]);

        if (error) {
            pr_err("can't misc_register :(\n");
            return error;
        }
        hcsr_dev[i]->misc_hcsr=misc_hcsr[i];
        hcsr_dev[i]->buf_pointer = -1;
        hcsr_dev[i]->count = 0;
        hcsr_dev[i]->measure_flag=0;
        printk("Created device : %d",i);
    }
    
    printk("Done registering devices");
    //creating a list of n devices
    hcsr_list-> head=hcsr_dev[0];

    for(i=0;i<nodevices-1;i++)
    {
        hcsr_dev[i]->next=hcsr_dev[i+1];
        printk("dev: %s",hcsr_dev[i]->name);
    }
    hcsr_dev[nodevices-1]->next=NULL; 
    return 0;
}



static void __exit misc_exit(void)
{
    int i;
    for(i=0;i<nodevices;i++)
    {
        hcsr_dev[i]->next = NULL;
        misc_deregister(&misc_hcsr[i]);
        printk("Destroying device : %s",hcsr_dev[i]->name);
        kfree(hcsr_dev[i]);
    }
    //clearing the list head
    hcsr_list->head = NULL;
    kfree(hcsr_list);


    pr_info("I'm out\n");
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_DESCRIPTION("Misc Driver for HCSR Sensors");
MODULE_AUTHOR("Akshaya Kumar");
MODULE_LICENSE("GPL");