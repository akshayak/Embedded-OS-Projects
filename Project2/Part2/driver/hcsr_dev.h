#include <linux/platform_device.h>
#include <linux/miscdevice.h>

//output buffer

struct output_buffer{
    unsigned long long int time_stamp;
     unsigned long long int distance;
};

//per device structure
//per device structure
struct hcsr_device
{
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
    unsigned long long int recent_distance;
    int enable_flag;
    struct task_struct *task;
};


struct p_device {
		char 	*dev_name;
		int			dev_num;
		struct platform_device 	p_dev;
		struct hcsr_device *hcsr_dev;
	    struct device *hcsr_device;
	    struct miscdevice misc_device;
};