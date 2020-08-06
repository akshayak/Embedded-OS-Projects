
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <linux/export.h>
#include <net/genetlink.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/delay.h>    
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/param.h>
#include <linux/workqueue.h>

#include "genl_header.h"

static struct timer_list timer;
static struct genl_family genl_test_family;

#define device_name "DotMatrixLED"
#define maj_num 154 //for spidev

//per device structure
struct spidevice {
    struct spi_device *spi;
    dev_t  devt;
    int trig_pin;
    int trig_pin_num;
    int trigger_level_pin;
    int trigger_mux_pin1;
    int trigger_mux_pin2;
    int echo_pin;
    int echo_pin_num;
    int echo_level_pin;
    int echo_mux_pin1;
    int echo_mux_pin2;
    int cs_pin;
    int cs_pin_num;
    int m_request;
    int pattern;
    long long unsigned int  dist;
    struct task_struct *led_task;
    struct task_struct *hcsr_task;
    int isInitial;
};

static struct spi_message msg;
static struct spidevice *spid;
static struct class *spiclass;
int ultra_velocity= 346;  //ultrasonic speed velocity in ait
long secs=0, nsecs=0; //for ktime functions
long long unsigned int  distance=0;
static int irq_no;

int mosi_mux1=44;  //should be high
int mosi_mux2=72;
int mosi_ls = 24;
int mosi_pullup=25;
int mosi_gpio=5;

int sck_mux1=46; // should be high
int sck_ls=30;
int sck_pullup=31;
int sck_gpio=7;

//pattern for creating a butterfly flying animation on the dot matrix. 0th row contains the hex code for butterfly with wings and 1st row contains the hex code for butterfly without wings
uint8_t pat_arr[2][24]= {
    {0x0C, 0x01, 0x09, 0x00, 0x0A, 0x0F, 0x0B, 0x07, 0x01, 0x54, 0x02, 0xba, 0x03, 0xba, 0x04, 0xba, 0x05, 0x7c, 0x06, 0x7c, 0x07, 0xba, 0x08, 0xd6,},
    {0x0C, 0x01, 0x09, 0x00, 0x0A, 0x0F, 0x0B, 0x07, 0x01, 0x10, 0x02, 0x38, 0x03, 0x38, 0x04, 0x38, 0x05, 0x38, 0x06, 0x38, 0x07, 0x38, 0x08, 0x10,},
}; // before lighting up the led, we set the initial configuration of the led in the order shutdown , intensity, decode mode and finally scan limit.

uint8_t trans_arr[2]; //to transmit to the led
unsigned int design[16];
//for message exchange
struct spi_transfer spi_trans = {

    .tx_buf = trans_arr,
    .rx_buf = 0,
    .len = 2,
    .bits_per_word = 8,
    .speed_hz = 10000000,
    .delay_usecs = 1,
    .cs_change = 1,
};

static void greet_group(unsigned int group)
{   
    
    void *hdr;
    int res, flags = GFP_ATOMIC;
    struct sk_buff* skb = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);
    printk("Greeting group : %d\n",group);

    if (!skb) {
        printk(KERN_ERR "%d: OOM!!", __LINE__);
        return;
    }

    hdr = genlmsg_put(skb, 0, 0, &genl_test_family, flags, GENL_TEST_C_MSG);
    if (!hdr) {
        printk(KERN_ERR "%d: Unknown err !", __LINE__);
        goto nlmsg_fail;
    }

    //sending the distance measured from hcsr device to the user space
    res = nla_put_u32(skb, GENL_DISTANCE_ATTR, spid->dist);
    if (res) {
        printk(KERN_ERR "%d: err %d ", __LINE__, res);
        goto nlmsg_fail;
    }

    genlmsg_end(skb, hdr);
    printk("Multicasting...\n");
    genlmsg_multicast(&genl_test_family, skb, 0, group, flags);
    return;

nlmsg_fail:
    genlmsg_cancel(skb, hdr);
    nlmsg_free(skb);
    return;
}


//*************HCSR trigger function*******************
int trigger_measurement(struct spidevice *spid)
{
    int i;
    static unsigned long long final_dist=0;
    static unsigned long long max=0;
    static unsigned long long min= 100000000;
    printk("Trigger function. Value of trig pin: %d\n",gpio_get_value(spid->trig_pin));


    for(i=0;i<5;i++) //sampling 3+2 =5 measurements
    {
        printk("Triggering %d th measurement\n",i);
        gpio_set_value_cansleep(spid->trig_pin,1);
        mdelay(5000);
        gpio_set_value_cansleep(spid->trig_pin,0);
        mdelay(1);
        final_dist+=distance;

        if(distance<min)
            min=distance;

        if(distance > max)
            max=distance;

        //printk("Max = %llu  Min= %llu\n ",max,min);
        msleep(5000); //sleeping for 5000 millisecs   

    }



    //subtract the max and min values - 2 outliers
    //printk("Final Max = %llu Final Min= %llu Final dist = %llu\n",max,min,final_dist);
    final_dist-=max;
    final_dist-=min;
    
    do_div(final_dist,3); //to take average of m values, here m=5
    printk("Final distance measure: %llu\n",final_dist);
    spid->dist=final_dist;
    printk("DISTANCE MEASURE: %llu",spid->dist);
    greet_group(GENL_TEST_MCGRP0);
    greet_group(GENL_TEST_MCGRP1);
    greet_group(GENL_TEST_MCGRP2);
    return 0;
}


ktime_t start;
ktime_t end;

//********************HCSR HANDLER*********************************
static irq_handler_t gpio_irq_handler(unsigned int irq, void *dev_id) // interrupt handler
{
    
    struct spidevice *spid=((struct spidevice *)dev_id);
    int gpio_val=gpio_get_value(spid->echo_pin);
    
    //printk("Inside handler \n");
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
        printk("Distance from handler: %llu\n",distance);
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

//******************************THREAD FOR HCSR************************
int distance_measure_thread(void * data)  
{
        struct spidevice *spid;
        spid=data;
        while(!kthread_should_stop())
        {
            //printk("Inside hcsr thread\n");
         trigger_measurement(spid);
         msleep(3000);
        }
         return 0;

}

//***********LED LIGHTUP FUCNTION*************************************
int displayLEDPattern(struct spidevice *spid){
    
    int j;
    //printk("\n inside thread function of spi display\n");

    printk("Pattern:%d\n",spid->pattern);

    if(spid->pattern ==2)
    {
        trans_arr[0] = 0x0c;
        trans_arr[1] = 0x00;
        spi_message_init(&msg);
        spi_message_add_tail((void *)&spi_trans, &msg);
        gpio_set_value(15,0);
        spi_sync(spid->spi, &msg);
        gpio_set_value(15,1);
    }
    else
    {
      
     for(j=0;j<24;j=j+2)
     {
                //printk("j value :%d\n",j);
                trans_arr[0] = pat_arr[spid->pattern][j];
                trans_arr[1] = pat_arr[spid->pattern][j+1];
                
                //printk("Msg values: %u, %u\n", trans_arr[0],trans_arr[1]);
                
                spi_message_init(&msg);
                spi_message_add_tail((void *)&spi_trans, &msg);
                gpio_set_value(15,0);
                spi_sync(spid->spi, &msg);
                gpio_set_value(15,1);
                
    }   

    }
   
    return 0;   
}


//******************************THREAD FOR LED************************
int led_lightup_thread(void * data)  
{

        struct spidevice *spid;
        spid=data;
        while(!kthread_should_stop())
        {
            //printk("Inside lightup thread\n");
            displayLEDPattern(spid);
            msleep(3000);
            
        }
         return 0;

}

int set_trig_pin( int pin_num)
{
    switch(pin_num)
    {
        case 0:

            if( gpio_request(11, "gpio_out_11") != 0 )  printk("gpio_out_11 error!\n");
            if( gpio_request(32, "dir_out_32") != 0 )  printk("dir_out_32 error!\n");

            spid->trig_pin = 11;
            spid->trigger_level_pin=32;

            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(32, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;

        case 1:
            if( gpio_request(12, "gpio_out_12") != 0 )  printk("gpio_out_12 error!\n");
            if( gpio_request(28, "dir_out_28") != 0 )  printk("dir_out_28 error!\n");
            if( gpio_request(45, "gpio_pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");            

            spid->trig_pin = 12;
            spid->trigger_level_pin=28;
            spid->trigger_mux_pin1=45;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(28, 0);
            gpio_direction_output(45, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;
        case 2:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(13, "gpio_out_13") != 0 )  printk("gpio_out_13 error!\n");
            if( gpio_request(34, "dir_out_34") != 0 )  printk("dir_out_34 error!\n");
            if( gpio_request(77, "gpio_pin_mux_77") != 0 )  printk("pin_mux_77 error!\n");
            spid->trig_pin = 13;
            spid->trigger_level_pin=34;
            spid->trigger_mux_pin1=77;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(34, 0);
            gpio_direction_output(77, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;
        case 3:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(14, "gpio_out_14") != 0 )  printk("gpio_out_14 error!\n");
            if( gpio_request(16, "gpio_out_16") != 0 )  printk("gpio_out_16 error!\n");
            if( gpio_request(76, "dir_out_76") != 0 )  printk("dir_out_76 error!\n");
            if( gpio_request(64, "gpio_pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            spid->trig_pin = 14;
            spid->trigger_level_pin=16;
            spid->trigger_mux_pin1=76;
            spid->trigger_mux_pin2=64;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(16, 0);
            gpio_direction_output(76, 0);
            gpio_direction_output(64, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;           
        case 4:
            if( gpio_request(6, "gpio_out_6") != 0 )  printk("gpio_out_6 error!\n");
            if( gpio_request(36, "dir_out_36") != 0 )  printk("dir_out_36 error!\n");

            spid->trig_pin = 6;
            spid->trigger_level_pin=36;

            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(36, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;
        case 5:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(0, "gpio_out_0") != 0 )  printk("gpio_out_0 error!\n");
            if( gpio_request(18, "dir_out_18") != 0 )  printk("dir_out_18 error!\n");
            if( gpio_request(66, "gpio_pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            spid->trig_pin = 0;
            spid->trigger_level_pin=18;
            spid->trigger_mux_pin1=66;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(18, 0);
            gpio_direction_output(66, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;
        case 6:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(1, "gpio_out_1") != 0 )  printk("gpio_out_1 error!\n");
            if( gpio_request(20, "dir_out_20") != 0 )  printk("dir_out_20 error!\n");
            if( gpio_request(21, "gpio_pin_mux_21") != 0 )  printk("pin_mux_21 error!\n");
            spid->trig_pin = 1;
            spid->trigger_level_pin=20;
            spid->trigger_mux_pin1=21;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(20, 0);
            gpio_direction_output(21, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;    
        case 7:

            if( gpio_request(38, "gpio_out_38") != 0 )  printk("gpio_out_38 error!\n");
            spid->trig_pin = 38;
            gpio_direction_output(spid->trig_pin, 0);
            break;  

        case 8:

            if( gpio_request(40, "gpio_out_40") != 0 )  printk("gpio_out_40 error!\n");
            spid->trig_pin = 40;
            gpio_direction_output(spid->trig_pin, 0);
            break; 

        case 9:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(4, "gpio_out_4") != 0 )  printk("gpio_out_4 error!\n");
            if( gpio_request(22, "dir_out_22") != 0 )  printk("dir_out_22 error!\n");
            if( gpio_request(70, "gpio_pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            spid->trig_pin = 4;
            spid->trigger_level_pin=22;
            spid->trigger_mux_pin1=70;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(22, 0);
            gpio_direction_output(70, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;
        case 10:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(10, "gpio_out_10") != 0 )  printk("gpio_out_10 error!\n");
            if( gpio_request(26, "dir_out_26") != 0 )  printk("dir_out_26 error!\n");
            if( gpio_request(74, "gpio_pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            spid->trig_pin = 10;
            spid->trigger_level_pin=26;
            spid->trigger_mux_pin1=74;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(26, 0);
            gpio_direction_output(74, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;
        case 11:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(5, "gpio_out_5") != 0 )  printk("gpio_out_5 error!\n");
            if( gpio_request(24, "gpio_out_24") != 0 )  printk("gpio_out_24 error!\n");
            if( gpio_request(44, "dir_out_44") != 0 )  printk("dir_out_44 error!\n");
            if( gpio_request(72, "gpio_pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            spid->trig_pin = 5;
            spid->trigger_level_pin=24;
            spid->trigger_mux_pin1=44;
            spid->trigger_mux_pin2=72;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(24, 0);
            gpio_direction_output(44, 0);
            gpio_direction_output(72, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break; 
        case 12:
            if( gpio_request(15, "gpio_out_15") != 0 )  printk("gpio_out_15 error!\n");
            if( gpio_request(42, "dir_out_42") != 0 )  printk("dir_out_42 error!\n");

            spid->trig_pin = 15;
            spid->trigger_level_pin=42;

            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(42, 0);
            gpio_direction_output(spid->trig_pin, 0);

            break;
        case 13:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(7, "gpio_out_7") != 0 )  printk("gpio_out_7 error!\n");
            if( gpio_request(30, "dir_out_30") != 0 )  printk("dir_out_30 error!\n");
            if( gpio_request(46, "gpio_pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            spid->trig_pin = 7;
            spid->trigger_level_pin=30;
            spid->trigger_mux_pin1=46;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(30, 0);
            gpio_direction_output(46, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;

        case 14:
            if( gpio_request(48, "gpio_out_48") != 0 )  printk("gpio_out_48 error!\n");
            spid->trig_pin = 48;
            gpio_direction_output(spid->trig_pin, 0);
            break; 

        case 15:
            if( gpio_request(50, "gpio_out_48") != 0 )  printk("gpio_out_50 error!\n");
            spid->trig_pin = 50;
            gpio_direction_output(spid->trig_pin, 0);
            break; 

        case 16:
            if( gpio_request(52, "gpio_out_52") != 0 )  printk("gpio_out_52 error!\n");
            spid->trig_pin = 52;
            gpio_direction_output(spid->trig_pin, 0);
            break; 

        case 17:
            if( gpio_request(54, "gpio_out_54") != 0 )  printk("gpio_out_54 error!\n");
            spid->trig_pin = 54;
            gpio_direction_output(spid->trig_pin, 0);
            break; 

        case 18:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(56, "gpio_out_56") != 0 )  printk("gpio_out_56 error!\n");
            if( gpio_request(60, "dir_out_60") != 0 )  printk("dir_out_60 error!\n");
            if( gpio_request(78, "gpio_pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            spid->trig_pin = 56;
            spid->trigger_mux_pin1=60;
            spid->trigger_mux_pin2=78;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(60, 0);
            gpio_direction_output(78, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;

        case 19:
            //passing trigger pin as 2 and echo pin as 10
            if( gpio_request(58, "gpio_out_58") != 0 )  printk("gpio_out_58 error!\n");
            if( gpio_request(60, "dir_out_60") != 0 )  printk("dir_out_60 error!\n");
            if( gpio_request(79, "gpio_pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            spid->trig_pin = 58;
            spid->trigger_mux_pin1=60;
            spid->trigger_mux_pin2=79;
            printk("Setting trigger pin as %d\n",spid->trig_pin);

            gpio_direction_output(60, 0);
            gpio_direction_output(79, 0);
            gpio_direction_output(spid->trig_pin, 0);
            break;

    }
    return 0;
}

int set_echo_pin(int pin_num)
{
    int ret;
    switch(pin_num)
    {

        case 0:
            
            spid->echo_pin = 11;
            spid->echo_level_pin=32;

            if( gpio_request(11, "gpio_in_11") != 0 )  printk("gpio_in_11 error!\n");
            if( gpio_request(32, "dir_in_32") != 0 )  printk("dir_in_32 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            break;

        case 1:
            
            spid->echo_pin = 12;
            spid->echo_level_pin=28;
            spid->echo_mux_pin1=45;
            if( gpio_request(12, "gpio_in_12") != 0 )  printk("gpio_in_12 error!\n");
            if( gpio_request(28, "dir_in_28") != 0 )  printk("dir_in_28 error!\n");
            if( gpio_request(45, "pin_mux_45") != 0 )  printk("pin_mux_45 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;

        case 2:
            
            spid->echo_pin = 13;
            spid->echo_level_pin=34;
            spid->echo_mux_pin1=77;
            if( gpio_request(13, "gpio_in_12") != 0 )  printk("gpio_in_13 error!\n");
            if( gpio_request(34, "dir_in_28") != 0 )  printk("dir_in_34 error!\n");
            if( gpio_request(77, "pin_mux_45") != 0 )  printk("pin_mux_77 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;

        case 3:
            //setting pin 10 as edge pin
            spid->echo_pin = 14;
            spid->echo_level_pin=16;
            spid->echo_mux_pin1=76;
            spid->echo_mux_pin2=64;
            if( gpio_request(14, "gpio_in_14") != 0 )  printk("gpio_in_14 error!\n");
            if( gpio_request(16, "dir_in_16") != 0 )  printk("dir_in_16 error!\n");
            if( gpio_request(76, "pin_mux_76") != 0 )  printk("pin_mux_76 error!\n");
            if( gpio_request(64, "pin_mux_64") != 0 )  printk("pin_mux_64 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            gpio_set_value_cansleep(spid->echo_mux_pin2, 0);
            break;

        case 4:
            //setting pin 10 as edge pin
            spid->echo_pin = 6;
            spid->echo_level_pin=36;

            if( gpio_request(6, "gpio_in_6") != 0 )  printk("gpio_in_6 error!\n");
            if( gpio_request(36, "dir_in_36") != 0 )  printk("dir_in_36 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            break;

        case 5:
            //setting pin 10 as edge pin
            spid->echo_pin = 0;
            spid->echo_level_pin=18;
            spid->echo_mux_pin1=66;
            if( gpio_request(0, "gpio_in_0") != 0 )  printk("gpio_in_0 error!\n");
            if( gpio_request(18, "dir_in_18") != 0 )  printk("dir_in_18 error!\n");
            if( gpio_request(66, "pin_mux_66") != 0 )  printk("pin_mux_66 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;

        case 6:
            //setting pin 10 as edge pin
            spid->echo_pin = 1;
            spid->echo_level_pin=20;
            spid->echo_mux_pin1=68;
            if( gpio_request(1, "gpio_in_1") != 0 )  printk("gpio_in_1 error!\n");
            if( gpio_request(20, "dir_in_20") != 0 )  printk("dir_in_20 error!\n");
            if( gpio_request(68, "pin_mux_68") != 0 )  printk("pin_mux_68 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;

         case 7:
            //setting pin 10 as edge pin
            spid->echo_pin = 38;


            if( gpio_request(38, "dir_in_38") != 0 )  printk("dir_in_38 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break; 

        case 8:
            //setting pin 10 as edge pin
            spid->echo_pin = 40;

            if( gpio_request(40, "dir_in_40") != 0 )  printk("dir_in_40 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break;   

        case 9:
            //setting pin 10 as edge pin
            spid->echo_pin = 4;
            spid->echo_level_pin=22;
            spid->echo_mux_pin1=70;
            if( gpio_request(4, "gpio_in_4") != 0 )  printk("gpio_in_4 error!\n");
            if( gpio_request(22, "dir_in_22") != 0 )  printk("dir_in_22 error!\n");
            if( gpio_request(70, "pin_mux_70") != 0 )  printk("pin_mux_70 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;    

        case 10:
            //setting pin 10 as edge pin
            spid->echo_pin = 10;
            spid->echo_level_pin=26;
            spid->echo_mux_pin1=74;
            if( gpio_request(10, "gpio_in_10") != 0 )  printk("gpio_in_10 error!\n");
            if( gpio_request(26, "dir_in_26") != 0 )  printk("dir_in_26 error!\n");
            if( gpio_request(74, "pin_mux_74") != 0 )  printk("pin_mux_74 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(26,1);
            gpio_set_value_cansleep(74, 0);
            break;

        case 11:
            //setting pin 10 as edge pin
            spid->echo_pin = 5;
            spid->echo_level_pin=24;
            spid->echo_mux_pin1=44;
            spid->echo_mux_pin2=72;
            if( gpio_request(5, "gpio_in_5") != 0 )  printk("gpio_in_5 error!\n");
            if( gpio_request(24, "dir_in_24") != 0 )  printk("dir_in_24 error!\n");
            if( gpio_request(44, "pin_mux_44") != 0 )  printk("pin_mux_44 error!\n");
            if( gpio_request(72, "pin_mux_72") != 0 )  printk("pin_mux_72 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            gpio_set_value_cansleep(spid->echo_mux_pin2, 0);
            break;

        case 12:
            //setting pin 10 as edge pin
            spid->echo_pin = 15;
            spid->echo_level_pin=42;

            if( gpio_request(15, "gpio_in_15") != 0 )  printk("gpio_in_15 error!\n");
            if( gpio_request(42, "dir_in_42") != 0 )  printk("dir_in_42 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            break;

        case 13:
            //setting pin 10 as edge pin
            spid->echo_pin = 7;
            spid->echo_level_pin=30;
            spid->echo_mux_pin1=46;
            if( gpio_request(7, "gpio_in_7") != 0 )  printk("gpio_in_7 error!\n");
            if( gpio_request(30, "dir_in_30") != 0 )  printk("dir_in_30 error!\n");
            if( gpio_request(46, "pin_mux_46") != 0 )  printk("pin_mux_46 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;   

        case 14:
            //setting pin 10 as edge pin
            spid->echo_pin = 48;

            if( gpio_request(48, "dir_in_48") != 0 )  printk("dir_in_48 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break;  

        case 15:
            //setting pin 10 as edge pin
            spid->echo_pin = 50;

            if( gpio_request(50, "dir_in_50") != 0 )  printk("dir_in_50 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break; 

        case 16:
            //setting pin 10 as edge pin
            spid->echo_pin = 52;

            if( gpio_request(52, "dir_in_52") != 0 )  printk("dir_in_52 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break; 

        case 17:
            //setting pin 10 as edge pin
            spid->echo_pin = 54;

            if( gpio_request(54, "dir_in_54") != 0 )  printk("dir_in_54 error!\n");

            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            break;

        case 18:
            //setting pin 10 as edge pin
            spid->echo_pin = 56;
            spid->echo_mux_pin1=60;
            spid->echo_mux_pin2=78;
            if( gpio_request(56, "gpio_in_56") != 0 )  printk("gpio_in_56 error!\n");
            if( gpio_request(60, "dir_in_60") != 0 )  printk("dir_in_60 error!\n");
            if( gpio_request(78, "pin_mux_78") != 0 )  printk("pin_mux_78 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;  

         case 19:
            //setting pin 10 as edge pin
            spid->echo_pin = 58;
            spid->echo_mux_pin1=60;
            spid->echo_mux_pin2=79;
            if( gpio_request(58, "gpio_in_58") != 0 )  printk("gpio_in_58 error!\n");
            if( gpio_request(60, "dir_in_60") != 0 )  printk("dir_in_60 error!\n");
            if( gpio_request(79, "pin_mux_79") != 0 )  printk("pin_mux_79 error!\n");
            irq_no = gpio_to_irq(spid->echo_pin);
            if(irq_no<0)  printk("IRQ NUMBER ERROR\n");
            printk("irq number: %d\n",irq_no);
            ret = request_irq(irq_no, (irq_handler_t) gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_interrupt", (void *)spid);
            if (ret < 0)   printk("Error in request_irq\n");
            
            gpio_direction_input(spid->echo_pin);
            gpio_set_value_cansleep(spid->echo_level_pin,1);
            gpio_set_value_cansleep(spid->echo_mux_pin1, 0);
            break;             

    }
    return 0;
}


int init_gpios(int trigger,int echo,int cs)
{
    //int ret;
    printk(KERN_INFO" initializing the gpio pins for the spi LED matrix\n");
    //IO11 is conencted to DIn fo dot matrix LED

    if( gpio_request(mosi_ls, "gpio_24") != 0 )  printk("gpio_24 error!\n");
    if( gpio_request(mosi_pullup, "gpio_25") != 0 )  printk("gpio_25 error!\n");
    if( gpio_request(mosi_mux1, "gpio_44") != 0 )  printk("gpio_44 error!\n");
    if( gpio_request(mosi_mux2, "gpio_72") != 0 )  printk("gpio_72 error!\n");
    if( gpio_request(mosi_gpio, "gpio_5") != 0 )  printk("gpio_5 error!\n");
    gpio_direction_output(mosi_gpio,1);
    gpio_set_value(mosi_gpio,1);
    gpio_set_value(mosi_mux2,0);
    gpio_direction_output(mosi_ls,1);
    gpio_set_value(mosi_ls,0);
    gpio_direction_output(mosi_pullup,1);
    gpio_set_value(mosi_pullup,0);
    gpio_direction_output(mosi_mux1,1);
    gpio_set_value(mosi_mux1,1);

    //IO13 is connected to CLK pin of dot matrix LED

    if( gpio_request(sck_ls, "gpio_30") != 0 )  printk("gpio_30 error!\n");
    if( gpio_request(sck_pullup, "gpio_31") != 0 )  printk("gpio_31 error!\n");
    if( gpio_request(sck_mux1, "gpio_46") != 0 )  printk("gpio_46 error!\n");
    if( gpio_request(sck_gpio, "gpio_7") != 0 )  printk("gpio_7 error!\n");
    gpio_direction_output(sck_gpio,1);
    gpio_set_value(sck_gpio,1);
    gpio_direction_output(sck_mux1,1);
    gpio_set_value(sck_mux1,1);
    gpio_direction_output(sck_ls,1);
    gpio_set_value(sck_ls,0);
    gpio_direction_output(sck_pullup,1);
    gpio_set_value(sck_pullup,0);

    //IO12 is connected to CS pin of dot matrix LED
    if(cs==12)
    {
    if( gpio_request(42, "gpio_42") != 0 )  printk("gpio_42 error!\n");
    if( gpio_request(43, "gpio_43") != 0 )  printk("gpio_43 error!\n");
    if( gpio_request(15, "gpio_15") != 0 )  printk("gpio_15 error!\n");
    gpio_direction_output(42,1);
    gpio_set_value(42,0);
    gpio_direction_output(43,1);
    gpio_set_value(43,0);
    gpio_direction_output(15,1);
    gpio_set_value(15,0);
    }


    printk(KERN_INFO" initializing the gpio pins for the HCSR sensor\n");
    //configuring trigger pin as 2
    set_trig_pin(trigger);

    //configuring echo pin
    set_echo_pin(echo);

    return 0;
}

int uninit_gpios(void)
{
    printk(KERN_INFO"\n EXIT: Unexport and free gpio pins\n");
    free_irq(gpio_to_irq(spid->echo_pin), (void *)spid);

    gpio_free(mosi_ls);
    gpio_free(mosi_pullup);
    gpio_free(mosi_mux1);
    gpio_free(mosi_mux2);
    gpio_free(mosi_gpio);

    gpio_free(sck_ls);
    gpio_free(sck_pullup);
    gpio_free(sck_mux1);
    gpio_free(sck_gpio);

    //free cs pin=12
    gpio_free(42);
    gpio_free(43);
    gpio_free(15);

    //freeing gpios for HCSR trigger pin
    if(spid->trig_pin!=-1)
        gpio_free(spid->trig_pin);
    if(spid->trigger_mux_pin1!=-1)
        gpio_free(spid->trigger_mux_pin1);
    if(spid->trigger_mux_pin2!=-1)
        gpio_free(spid->trigger_mux_pin2);
    if(spid->trigger_level_pin!=-1)
        gpio_free(spid->trigger_level_pin);

    //freeing gpios for HCSR echo pin
    if(spid->echo_pin!=-1)
        gpio_free(spid->echo_pin);
    if(spid->echo_level_pin!=-1)
        gpio_free(spid->echo_level_pin);
    if(spid->echo_mux_pin1!=-1)
        gpio_free(spid->echo_mux_pin1);
    if(spid->echo_mux_pin2!=-1)
        gpio_free(spid->echo_mux_pin2);

    return 0;
}


static int genl_test_rx_msg(struct sk_buff* skb, struct genl_info* info)
{

    spid->isInitial =nla_get_u32(info->attrs[GENL_INITIAL_ATTR]);
    printk("*****MSG RECEIVED*****\n");
    
    //If its the initial call as indicated by the variable isInitial, then we spawn 2 threads one for hcsr measurement and other for led display
    if(spid->isInitial==1) //if its the initial msg
    {
        printk("**INITIAL MSG**\n");
        spid->trig_pin_num=nla_get_u32(info->attrs[GENL_TRIGGER_PIN_ATTR]);
        spid->echo_pin_num=nla_get_u32(info->attrs[GENL_ECHO_PIN_ATTR]);
        spid->cs_pin_num=nla_get_u32(info->attrs[GENL_CS_PIN_ATTR]);
        spid->m_request=nla_get_u32(info->attrs[GENL_REQUEST_ATTR]);
        spid->pattern =nla_get_u32(info->attrs[GENL_PATTERN_ATTR]);

        printk(KERN_NOTICE "%u isInitial:  %d \n", info->snd_portid,spid->isInitial); 
        printk(KERN_NOTICE "%u trigger pin:  %d \n", info->snd_portid,spid->trig_pin_num); 
        printk(KERN_NOTICE "%u echo pin:  %d \n", info->snd_portid,spid->echo_pin_num); 
        printk(KERN_NOTICE "%u cs pin:  %d \n", info->snd_portid,spid->cs_pin_num); 
        printk(KERN_NOTICE "%u Measurement req:  %d \n", info->snd_portid,spid->m_request); 
        printk(KERN_NOTICE "%u Pattern:  %d \n", info->snd_portid,spid->pattern); 
        
        init_gpios(spid->trig_pin_num,spid->echo_pin_num,spid->cs_pin_num);
        spid->led_task = kthread_run(led_lightup_thread, (void *)spid,"kthread_spi_led");
        if(spid->led_task)
            printk("Created led thread successfully !\n");
        spid->hcsr_task=kthread_run(distance_measure_thread, (void *) spid ,"kthread_hcsr");   
        if(spid->hcsr_task)
            printk("Created hcsr thread successfully !\n");

    }
    else if(spid->isInitial==0) //if it is not the initial call, then get only the updated pattern from the netlink socket
    {
        printk("**NON-INITIAL MSG**\n");
        spid->pattern =nla_get_u32(info->attrs[GENL_PATTERN_ATTR]);

        printk(KERN_NOTICE "%u isInitial:  %d \n", info->snd_portid,spid->isInitial); 
        printk(KERN_NOTICE "%u trigger pin:  %d \n", info->snd_portid,spid->trig_pin_num); 
        printk(KERN_NOTICE "%u echo pin:  %d \n", info->snd_portid,spid->echo_pin_num); 
        printk(KERN_NOTICE "%u cs pin:  %d \n", info->snd_portid,spid->cs_pin_num); 
        printk(KERN_NOTICE "%u Measurement req:  %d \n", info->snd_portid,spid->m_request); 
        printk(KERN_NOTICE "%u Pattern:  %d \n", info->snd_portid,spid->pattern); 
    }

    return 0;
}

static const struct genl_ops genl_test_ops[] = {
    {
        .cmd = GENL_TEST_C_MSG,
        .policy = genl_test_policy,
        .doit = genl_test_rx_msg,
        .dumpit = NULL,
    },
};

static const struct genl_multicast_group genl_test_mcgrps[] = {
    [GENL_TEST_MCGRP0] = { .name = GENL_TEST_MCGRP0_NAME, },
    [GENL_TEST_MCGRP1] = { .name = GENL_TEST_MCGRP1_NAME, },
    [GENL_TEST_MCGRP2] = { .name = GENL_TEST_MCGRP2_NAME, },
};

static struct genl_family genl_test_family = {
    .name = GENL_TEST_FAMILY_NAME,
    .version = 1,
    .maxattr = GENL_TEST_ATTR_MAX,
    .netnsok = false,
    .module = THIS_MODULE,
    .ops = genl_test_ops,
    .n_ops = ARRAY_SIZE(genl_test_ops),
    .mcgrps = genl_test_mcgrps,
    .n_mcgrps = ARRAY_SIZE(genl_test_mcgrps),
};


int spi_open(struct inode *i, struct file *fp){
    printk("Inside open fucntion");
    return 0;
}


int spi_release(struct inode *i, struct file *fp){
    printk("Inside release function");
    return 0;
}

ssize_t spidevice_write(struct file *f,const char *spim, size_t sz, loff_t *loffptr){
    
    return 0;   
}


static const struct file_operations spi_fops = {
    .open = spi_open,
    .release = spi_release,
    .write  = spidevice_write,
    
};



//Display test function- lights up all the LEDS
int lightUp(void)
{
    printk("Lighting up LEDS - display Test mode!!");
    trans_arr[0] = 0x0F;
    trans_arr[1] = 0x01;
                
    spi_message_init(&msg);
    spi_message_add_tail((void *)&spi_trans, &msg);
    gpio_set_value(15,0);
    spi_sync(spid->spi, &msg);
    gpio_set_value(15,1);   
    return 0;
}




static int spi_probe(struct spi_device *spi)
{
    struct device *dev;
    //printk("Inside probe function\n");
    //allocate per device structure
    spid = kzalloc(sizeof(*spid), GFP_KERNEL);
    if(!spid)
    {
        return -ENOMEM;
    }
    spid->spi = spi;

    spid->devt = MKDEV(maj_num, 0); //major number =154, minor number=0
    spid->dist=0;

    dev = device_create(spiclass, &spi->dev, spid->devt, spid, device_name);

    if(dev == NULL)
    {
        printk(" Failed while creating device\n");
        kfree(spid);
        return -1;
    }
    printk("Probe done..\n");
    spid->pattern=0;

    return 0;
}

static int spi_remove(struct spi_device *spi){
    return 0;
}

struct spi_device_id device_table[] = {
{"spidev",0},
{}
};

static struct spi_driver dotLED_spi_driver = {
         .driver = {
            .name =         "spidev",
            .owner =        THIS_MODULE,
         },
         .id_table =   device_table,
         .probe =        spi_probe,
         .remove =       spi_remove,
        
};

 int __init spi_module_init(void)
{

    int ret;
    int rc;
    //printk("Inside spi init\n");
    
    //Registering device
    ret = register_chrdev(maj_num, device_name, &spi_fops);
    if(ret < 0)
    {
        printk(" Failed to register device\n");
        return -1;
    }
    //creating class
    spiclass = class_create(THIS_MODULE, "DotMatrixLED");
    if(spiclass == NULL)
    {
        printk("Failed to create class\n");
        unregister_chrdev(maj_num, dotLED_spi_driver.driver.name);
        return -1;
    }
    //registering driver
    ret = spi_register_driver(&dotLED_spi_driver);
    if(ret < 0)
    {
        printk("Driver Registraion Failed\n");
        class_destroy(spiclass);
        unregister_chrdev(maj_num, dotLED_spi_driver.driver.name);
        return -1;
    }
    
    printk("Device and driver initialized.\n");

    
    printk(KERN_INFO "Initializing netlink\n");

    rc = genl_register_family(&genl_test_family);
    if (rc)
        printk(KERN_DEBUG "genl_test: error occurred in %s\n", __func__);

    return 0;

}


void spi_module_exit(void)
{
    printk("Switching off LED\n");
    //to switch off the led while removing module
    spid->pattern=2;
    msleep(2000);
    printk("Disabling measurement\n");
    if(spid->led_task)
        kthread_stop(spid->led_task);  //kill the led thread
    if(spid->hcsr_task)
        kthread_stop(spid->hcsr_task);  //kill the hcsr thread
    spid->pattern=2;

    uninit_gpios();
    spi_unregister_driver(&dotLED_spi_driver);
    unregister_chrdev(maj_num, dotLED_spi_driver.driver.name);
    device_destroy(spiclass, spid->devt);
    kfree(spid);
    class_destroy(spiclass);
    del_timer(&timer);
    genl_unregister_family(&genl_test_family);
    printk("Device and Driver deregistered\n");
    printk("Exiting kernel module, Bubye !\n");
}

module_init(spi_module_init);
module_exit(spi_module_exit);

MODULE_AUTHOR("Ahmed Zaki <anzaki@gmail.com>");
MODULE_LICENSE("GPL");